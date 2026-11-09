// Included inside panel's worker namespace. All I/O and recovery are serialized
// with ordinary setting writes; snapshot readers only see the published status.
bool run_week(const J *j,schedule_run::Week &w){
    if(!valid_schedule(j))return false;
    for(int d=0;d<7;++d)for(int b=0;b<6;++b)w.days[d][b]=num(item(item(const_cast<J*>(j),d),b));
    for(int b=0;b<4;++b)w.meta[b]=num(item(item(const_cast<J*>(j),7),b));
    return true;
}
J *run_json(const schedule_run::Week &w){
    auto *j=cJSON_CreateArray();
    for(int d=0;d<8;++d){auto *row=cJSON_CreateArray();for(int b=0;b<(d==7?4:6);++b)cJSON_AddItemToArray(row,cJSON_CreateNumber(d==7?w.meta[b]:w.days[d][b]));cJSON_AddItemToArray(j,row);}
    return j;
}
bool run_clock(int64_t &civil){
    uint8_t p[26];uint64_t received=0;
    if(!transport::clock_sample(p,received)||now()<received||now()-received>=10000||!boiler_clock_codec::valid(p,sizeof(p)))return false;
    civil=boiler_clock_codec::civil(p)+(now()-received)/1000;return true;
}
std::string run_label(int64_t civil){time_t t=civil;tm value{};gmtime_r(&t,&value);char s[32];strftime(s,sizeof(s),"%d.%m.%Y %H:%M",&value);return s;}
const char *run_reason(schedule_run::Result r){
    switch(r){
    case schedule_run::Result::missing_window:return "Harmonogram nie ma zadnego okresu pracy. Ustaw godziny w zakladce Harmonogram";
    case schedule_run::Result::missing_end:return "Nie mozna zachowac automatycznego konca pracy w tym tygodniowym planie. Ustaw dodatkowy okres przerwy";
    case schedule_run::Result::invalid:return "Nieprawidlowy harmonogram lub zegar kotla";
    default:return "";
    }
}
const char *run_guard(const J *sensor,bool identity,bool enabled){
    if(const char *guard=control_guard(sensor,true,identity,enabled,false))return guard;
    auto *s=get(sensor,"sensors");int mode=num(get(s,"state"),-1);
    if(mode!=0&&mode!=5&&mode!=6)return "Wczesniejsze wlaczenie wymaga wylaczonego kotla, przerwy lub postoju";
    if(num(get(s,"pending_alerts"))!=0||!cJSON_IsFalse(get(get(sensor,"outputs"),"alarm")))return "Kociol zglasza alarm lub brak potwierdzenia alarmow";
    return nullptr;
}
bool run_persist(const schedule_run::Record &record){
    nvs_handle_t h;if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK)return fail("Nie zapisano pamieci jednorazowego uruchomienia");
    auto err=nvs_set_blob(h,"schedule_run",&record,sizeof(record));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);
    return err==ESP_OK||fail("Nie zapisano pamieci jednorazowego uruchomienia");
}
void run_publish(const char *message){
    auto *status=cJSON_CreateObject();flag(status,"active",schedule_run_pending);text(status,"message",message);
    if(schedule_run_pending){text(status,"until",run_label(schedule_run_record.plan.end));text(status,"resume",run_label(schedule_run_record.plan.resume));flag(status,"restored",schedule_run_record.stage==schedule_run::Stage::watching);}
    Lock lock;put(data,"schedule_run",status);
}
bool run_stage(schedule_run::Stage stage){
    auto next=schedule_run_record;next.stage=stage;if(!run_persist(next))return false;schedule_run_record=next;return true;
}
bool run_finish(const char *message){
    nvs_handle_t h;if(nvs_open("eco_panel",NVS_READWRITE,&h)!=ESP_OK)return false;
    auto err=nvs_erase_key(h,"schedule_run");if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);
    if(err!=ESP_OK){run_publish("Harmonogram sprawdzony; oczekiwanie na utrwalenie zakonczenia w ESP");return false;}
    schedule_run_pending=false;run_publish(message);state::log("One-shot schedule: %s",message);return true;
}
// Absolute restoration can retry only AFTER a fresh read still equals our own
// extended week. It never replays START and never overwrites another week.
bool run_write(const J *target,const schedule_run::Week &week){
    Owned value{run_json(week)};uint8_t bytes[46]{};
    if(!encode_schedule(target,value.p,bytes))return fail("Nieprawidlowe kodowanie harmonogramu");
    transport::Reply reply;transport::request(55,bytes,sizeof(bytes),0,reply);
    if(reply.sent){Lock lock;number(data,"write_count",num(get(data,"write_count"),0)+1);}
    for(unsigned i=0;reply.sent&&i<3;++i){
        if(i)vTaskDelay(pdMS_TO_TICKS(150));
        if(!rs_schedule_refresh())continue;
        Owned observed{schedule_target("schedule:boilerWorkTZ")};
        if(cJSON_Compare(get(observed.p,"before"),value.p,true))return true;
    }
    return fail("Brak potwierdzenia tygodnia po zapisie. Nie ponowiono uruchomienia");
}
uint64_t next_run_check=0,run_started_ms=0;
bool run_reconcile(bool cancel=false){
    if(!schedule_run_pending)return true;
    if(cancel&&!run_stage(schedule_run::Stage::restoring))return false;
    if(selected_mode!="rs485"||!transport::enabled()||transport::conflict()){
        run_publish("Oczekiwanie na RS485 do przywrocenia harmonogramu; godzina zakonczenia pozostaje w kotle");return false;
    }
    // Do not issue any writes after an identity/firmware change.
    J *t=nullptr;if(!inspect_generic("schedule:boilerWorkTZ",t)){run_publish("Oczekiwanie na odczyt harmonogramu kotla");return false;}Owned target{t};
    if(uid!=schedule_run_record.identity){run_publish("Inny kociol: zapamietany harmonogram nie zostanie wyslany");return false;}
    schedule_run::Week actual;if(!run_week(get(t,"before"),actual))return false;
    int64_t civil=0;bool clock=run_clock(civil);Owned sensor{boiler::snapshot()};
    int mode=num(get(get(sensor.p,"sensors"),"state"),-1);
    // Extinguishing (7) also occurs normally when heat demand is satisfied.
    // Only OFF cancels implicitly; the explicit STOP path cancels immediately.
    bool stopped=cJSON_IsTrue(get(sensor.p,"fresh"))&&mode==0&&(!run_started_ms||now()-run_started_ms>20000);
    auto decision=schedule_run::decide(schedule_run_record,actual,clock,civil,stopped);
    if(decision==schedule_run::Decision::external_change)return run_finish("Harmonogram zmieniono na innym urzadzeniu. Zachowano nowy plan");
    if(decision==schedule_run::Decision::finish)return run_finish("Sterowanie wedlug harmonogramu");
    if(decision==schedule_run::Decision::restore){
        bool watch=!cancel&&schedule_run_record.stage==schedule_run::Stage::active&&clock&&civil>=schedule_run_record.plan.resume&&civil<schedule_run_record.plan.end&&!stopped;
        // Recovery writes require a newly read identity and week, not just the
        // usual identity cache. A replacement controller must never get this plan.
        if(!rs_identify()||uid!=schedule_run_record.identity){run_publish("Przywrocenie czeka na potwierdzenie tozsamosci kotla");return false;}
        J *checked=nullptr;if(!inspect_generic("schedule:boilerWorkTZ",checked))return false;Owned final_target{checked};
        schedule_run::Week final_week;if(!run_week(get(checked,"before"),final_week))return false;
        if(!schedule_run::same(final_week,schedule_run_record.plan.extended))return run_finish("Harmonogram zmienil sie przed przywroceniem. Zachowano nowy plan");
        if(!run_stage(schedule_run::Stage::restoring))return false;
        if(!run_write(checked,schedule_run_record.plan.before)){run_publish("Oczekiwanie na potwierdzenie przywrocenia planu; bez ponawiania START");return false;}
        if(!watch)return run_finish("Przywrocono harmonogram");
        if(!run_stage(schedule_run::Stage::watching))return false;
    }else if(decision==schedule_run::Decision::watch&&schedule_run_record.stage!=schedule_run::Stage::watching){
        if(!run_stage(schedule_run::Stage::watching))return false;
    }
    run_publish("Wczesniejsze wlaczenie aktywne; zakonczenie wedlug zegara kotla");return true;
}
void run_tick(){
    if(!schedule_run_pending||now()<next_run_check||updates::installing())return;
    bool idle=false;if(!busy.compare_exchange_strong(idle,true))return;
    next_run_check=now()+10000;auto saved=error_text;run_reconcile();error_text=saved;busy=false;
}
// Only the already verified native schedule family participates. Other models
// retain their ordinary START/STOP path and do not inherit this wire mapping.
bool run_supported(const J *sys){return controller_profile::reference(str(get(sys,"controllerID")),str(get(sys,"moduleASoftVer")),str(get(sys,"modulePanelSoftVer")));}
bool run_prepare(J *ticket_value,int &mode){
    if(!rs_identify())return false;
    if(!run_supported(get(raw,"sysParams")))return inspect_control(true,true,mode);
    if(schedule_run_pending)return fail("Jednorazowe wlaczenie jest juz aktywne");
    J *t=nullptr;if(!inspect_generic("schedule:boilerWorkTZ",t))return false;Owned target{t};
    schedule_run::Week week;if(!run_week(get(t,"before"),week))return fail("Brak aktualnego harmonogramu pracy kotla");
    if(!week.meta[0])return inspect_control(true,true,mode);
    int64_t civil=0;if(!run_clock(civil))return fail("Oczekiwanie na swiezy zegar kotla do sprawdzenia harmonogramu");
    schedule_run::Plan plan;auto result=schedule_run::plan(week,civil,plan);
    if(result==schedule_run::Result::already_allowed)return inspect_control(true,true,mode);
    if(result!=schedule_run::Result::ready)return fail(run_reason(result));
    Owned sensor{boiler::snapshot()};if(auto *e=run_guard(sensor.p,native_identity&&now()-native_identity_ms<60000,transport::enabled()))return fail(e);
    mode=num(get(get(sensor.p,"sensors"),"state"));
    auto *p=cJSON_CreateObject();copy(p,"before",get(t,"before"));put(p,"extended",run_json(plan.extended));number(p,"start",plan.start);number(p,"resume",plan.resume);number(p,"end",plan.end);text(p,"until",run_label(plan.end));text(p,"identity",uid);
    put(ticket_value,"schedule_run",p);return true;
}
bool run_begin(const J *held,int &mode){
    Owned fresh{cJSON_CreateObject()};if(!run_prepare(fresh.p,mode))return false;
    auto *expected=get(held,"schedule_run"),*current=get(fresh.p,"schedule_run");
    if(!current||strcmp(str(get(current,"identity")),str(get(expected,"identity")))||
       !cJSON_Compare(get(current,"before"),get(expected,"before"),true)||num(get(current,"resume"))!=num(get(expected,"resume"))||num(get(current,"end"))!=num(get(expected,"end"))||mode!=num(get(held,"before_mode")))return fail("Plan lub stan kotla zmienil sie. Nacisnij Wlacz kociol ponownie");
    schedule_run::Record next{};snprintf(next.identity,sizeof(next.identity),"%s",uid.c_str());
    schedule_run::Week week;run_week(get(current,"before"),week);if(schedule_run::plan(week,num(get(current,"start")),next.plan)!=schedule_run::Result::ready)return false;
    if(!run_persist(next))return false;
    schedule_run_record=next;schedule_run_pending=true;run_started_ms=now();
    run_publish("Zapisywanie wczesniejszego okresu pracy");
    phase("Zapisywanie rozszerzenia harmonogramu","dispatch");
    Owned target{schedule_target("schedule:boilerWorkTZ")};
    if(!run_write(target.p,next.plan.extended)){run_publish("Zapis okresu niepotwierdzony; sprawdzanie i przywracanie harmonogramu");return false;}
    if(!run_stage(schedule_run::Stage::active))return false;
    run_publish("Praca do konca najblizszego okresu harmonogramu");return true;
}
