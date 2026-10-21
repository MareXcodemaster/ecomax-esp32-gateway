#include "transport.hpp"
#include "board.hpp"
#include "frame_parser.hpp"
#include "state.hpp"
#include "pellet_commands.hpp"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <atomic>
#include <algorithm>

namespace transport {
static std::atomic<unsigned> requested_baud{board::baud};
static QueueHandle_t events;
static SemaphoreHandle_t call_mutex, exchange_mutex, completed;
static std::atomic<bool> active{false}, duplicate{false};
static std::atomic<uint64_t> last_econet_us{0}, tx_count{0};
static bool hello_pending=false;
struct Exchange { uint8_t frame[64]{}; size_t length=0; uint8_t expected=0;
    uint64_t deadline=0, sent_us=0; Reply reply; bool pending=false, waiting=false; } exchange;
bool enabled() { return active.load(); }
bool conflict() { return duplicate.load(); }
uint64_t sent_bytes() { return tx_count.load(); }
bool enable() {
    auto now=static_cast<uint64_t>(esp_timer_get_time());
    if(now<30000000 || now-last_econet_us.load()<30000000 || requested_baud.load()!=board::baud) return false;
    duplicate=false; active=true; return true;
}
void disable() { active=false; }
static bool readonly_allowed(uint8_t type,const uint8_t *payload,size_t size,uint8_t response) {
    if(type==86) return response==214&&payload&&
        ((size==1&&(payload[0]==0||payload[0]==3||payload[0]==9))||(size==3&&payload[0]==4&&
         ((payload[1]==255&&payload[2]==0)||(payload[1]==1&&payload[2]<168))));
    if(size==0) return (type==57&&response==185)||(type==64&&response==192)||
        (type==84&&response==212)||(type==85&&response==213)||(type==54&&response==182);
    if(size!=2||!payload) return false;
    if(type==97)return payload[0]==255&&payload[1]==0&&response==225;
    if(type==61) return payload[0]==0&&payload[1]==10&&response==189;
    if(type==49) return payload[0]==32&&payload[1]<=96&&payload[1]%32==0&&response==177;
    if(payload[1]!=0) return false;
    if(type==50||type==81||type==83) return payload[0]==16&&response==type+128;
    if(type==80||type==82) return payload[0]==32&&response==type+128;
    return (type==92||type==94||type==95)&&payload[0]==255&&response==type+128;
}
static bool request_allowed(uint8_t type,const uint8_t *payload,size_t size,uint8_t response) {
    return readonly_allowed(type,payload,size,response)||(size==0&&type==25&&response==0)||
        (type==112&&pellet_commands::write_allowed(payload,size,response))||
        (type==93&&size==2&&payload&&payload[0]==1&&(payload[1]&127)<=7&&response==221)||
        (type==55&&size==46&&payload&&response==0&&payload[0]==1&&payload[1]<=9&&payload[2]<=1&&payload[3]<=100)||
        (type==86&&size==4&&payload&&payload[0]==5&&payload[1]==1&&payload[2]<168&&response==214)||
        (type==93&&size==3&&payload&&(payload[0]==2||payload[0]==3||payload[0]==10||payload[0]==11)&&response==221&&
          (payload[1]|(payload[2]<<8))>=100&&(payload[1]|(payload[2]<<8))<=350) ||
        (type==93&&size==3&&payload&&payload[0]==12&&response==221&&
          (payload[1]|(payload[2]<<8))>=50&&(payload[1]|(payload[2]<<8))<=300) ||
        (type==93&&size==2&&payload&&response==221&&
          ((payload[0]==4&&payload[1]>=60&&payload[1]<=140)||
           (payload[0]>=5&&payload[0]<=8&&payload[1]>=1&&payload[1]<=60)||
           (payload[0]==9&&payload[1]>=2&&payload[1]<=50))) ||
        (type==52&&size==3&&payload&&payload[0]==0&&payload[1]==0&&payload[2]>=40&&payload[2]<=65&&response==180) ||
        (type==59&&size==1&&payload&&payload[0]<=1&&response==187) ||
        (type==87&&size==6&&payload&&payload[0]==1&&payload[1]==0&&payload[2]==4&&payload[4]==5&&
          ((payload[3]==0&&payload[5]>=70&&payload[5]<=88)||(payload[3]==1&&payload[5]>=35&&payload[5]<=65)));
}
static bool thermostat_allowed(uint8_t type,const uint8_t *payload,size_t size,uint8_t response) {
    return (type==88&&size==0&&response==216)||(type==89&&payload&&size==46&&response==217&&
        payload[0]==1&&payload[1]==16&&payload[2]<=1&&(payload[3]<=100||payload[3]==255));
}
static bool request_to(uint8_t recipient,uint8_t type, const uint8_t *payload, size_t size, uint8_t response, Reply &reply) {
    reply={};
    bool allowed=recipient==69?request_allowed(type,payload,size,response):recipient==81&&thermostat_allowed(type,payload,size,response);
    if(!allowed || !active || !call_mutex) return false;
    xSemaphoreTake(call_mutex,portMAX_DELAY);
    while(xSemaphoreTake(completed,0)==pdTRUE) {}
    xSemaphoreTake(exchange_mutex,portMAX_DELAY);
    exchange={}; exchange.length=size+10; exchange.expected=response;
    auto *f=exchange.frame; f[0]=0x68; f[1]=exchange.length; f[2]=0; f[3]=recipient; f[4]=86; f[5]=48; f[6]=5; f[7]=type;
    if(size) memcpy(f+8,payload,size);
    uint8_t sum=0; for(size_t i=0;i<exchange.length-2;++i) sum^=f[i];
    f[exchange.length-2]=sum; f[exchange.length-1]=0x16;
    exchange.deadline=esp_timer_get_time()+3000000; exchange.pending=true;
    xSemaphoreGive(exchange_mutex);
    xSemaphoreTake(completed,pdMS_TO_TICKS(3200));
    xSemaphoreTake(exchange_mutex,portMAX_DELAY);
    reply=exchange.reply; exchange.pending=exchange.waiting=false;
    xSemaphoreGive(exchange_mutex); xSemaphoreGive(call_mutex);
    return reply.sent && (!response||reply.length>0);
}
bool request(uint8_t type,const uint8_t *payload,size_t size,uint8_t response,Reply &reply) {
    return request_to(69,type,payload,size,response,reply);
}
bool request_thermostat(uint8_t type,const uint8_t *payload,size_t size,uint8_t response,Reply &reply) {
    return request_to(81,type,payload,size,response,reply);
}
bool read_thermostat_schedule(Reply &reply) { return request_thermostat(88,nullptr,0,216,reply); }
bool request_readonly(uint8_t type,const uint8_t *payload,size_t size,uint8_t response,Reply &reply) {
    reply={};
    if(!readonly_allowed(type,payload,size,response))return false;
    if(request(type,payload,size,response,reply))return true;
    if(!enabled()||conflict())return false;
    // Only this read-only whitelist may retry. Setting writes still dispatch once.
    vTaskDelay(pdMS_TO_TICKS(150));
    return request(type,payload,size,response,reply);
}
void lock_tx() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << board::de;
    config.mode = GPIO_MODE_INPUT_OUTPUT; config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(board::de, 0));
    config.pin_bit_mask = 1ULL << board::tx; config.mode = GPIO_MODE_OUTPUT;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config)); ESP_ERROR_CHECK(gpio_set_level(board::tx, 1));
}
bool set_baud(unsigned value) {
    if(active) return false;
    if (value != 9600 && value != 19200 && value != 38400 && value != 57600 && value != 115200) return false;
    requested_baud.store(value); return true;
}
static bool reply_matches(const uint8_t *request,size_t request_size,uint8_t expected,const uint8_t *bytes,size_t length) {
    if(request_size<10||length<10||(bytes[3]!=86&&bytes[3]!=0)||bytes[7]!=expected) return false;
    if(request[3]==81) {
        if(bytes[4]!=81)return false;
        if(request[7]==88)return expected==216;
        if(request[7]!=89||expected!=217||request_size!=56)return false;
        return length==10||(length>=60&&bytes[8]==16&&bytes[9]==1&&bytes[10]>0&&bytes[11]==request[9]);
    }
    bool menu_reply=request[7]==86&&expected==214&&request_size>=11&&length>=11&&bytes[8]==request[8];
    if(menu_reply&&request[8]==5) menu_reply=request_size==14&&length==13&&bytes[9]==1&&bytes[10]==request[10];
    if(menu_reply&&request[8]==4&&request_size==13&&request[9]==1) menu_reply=length==16&&bytes[9]==1&&bytes[10]==request[10];
    bool alarm_reply=request[7]==61&&expected==189&&request_size==12&&length>=13&&bytes[9]==request[8]&&bytes[10]<=request[9]&&bytes[9]+bytes[10]<=bytes[8]&&length==13+bytes[10]*9;
    return (bytes[4]==69||((menu_reply||alarm_reply)&&bytes[4]==80))&&(request[7]!=86||menu_reply)&&(request[7]!=61||alarm_reply);
}
static void captured(const uint8_t *bytes, size_t length, void *) {
    if(length>=10 && bytes[4]==86) {
        last_econet_us=esp_timer_get_time();
        if(active.exchange(false)) { duplicate=true; state::log("RS485 address 0x56 conflict: TX disabled, no command retry"); }
    }
    xSemaphoreTake(exchange_mutex,portMAX_DELAY);
    if(active && length==10 && bytes[4]==69 && bytes[3]==86 && bytes[7]==48) hello_pending=true;
    if(exchange.waiting && reply_matches(exchange.frame,exchange.length,exchange.expected,bytes,length) &&
       static_cast<uint64_t>(esp_timer_get_time())<exchange.deadline) {
        memcpy(exchange.reply.bytes,bytes,length); exchange.reply.length=length;
        exchange.reply.latency_ms=(esp_timer_get_time()-exchange.sent_us)/1000;
        exchange.waiting=false; xSemaphoreGive(completed);
    }
    xSemaphoreGive(exchange_mutex);
    state::frame(bytes, length);
}
static void dispatch(bool idle) {
    xSemaphoreTake(exchange_mutex,portMAX_DELAY);
    if(hello_pending && idle && active && !exchange.pending) {
        // PyPlumIO DeviceAvailableResponse: disconnected Ethernet, local wireless link.
        uint8_t f[48]={0x68,48,0,69,86,48,5,176,1};
        f[13]=255; f[14]=255; f[15]=255;
        f[26]=255; f[27]=255; f[28]=255;
        f[34]=1; f[35]=4; f[36]=100; f[37]=1;
        f[42]=3; f[43]='E'; f[44]='S'; f[45]='P';
        for(size_t i=0;i<46;++i) f[46]^=f[i];
        f[47]=0x16;
        gpio_set_level(board::de,1); int written=uart_write_bytes(board::uart,f,sizeof(f));
        uart_wait_tx_done(board::uart,pdMS_TO_TICKS(100)); gpio_set_level(board::de,0);
        tx_count+=std::max(written,0); hello_pending=false;
    }
    if(exchange.pending && (!active || static_cast<uint64_t>(esp_timer_get_time())>=exchange.deadline)) {
        exchange.pending=false; xSemaphoreGive(completed);
    }
    if(exchange.pending && idle && active) {
        exchange.pending=false;
        // RX task owns direction changes; release DE only after UART completion.
        gpio_set_level(board::de,1);
        int written=uart_write_bytes(board::uart,exchange.frame,exchange.length);
        auto finished=uart_wait_tx_done(board::uart,pdMS_TO_TICKS(100));
        gpio_set_level(board::de,0);
        exchange.reply.sent=written>0; tx_count+=std::max(written,0);
        exchange.sent_us=esp_timer_get_time();
        if(written==static_cast<int>(exchange.length)&&finished==ESP_OK) {
            exchange.waiting=exchange.expected!=0;
            if(!exchange.waiting) xSemaphoreGive(completed);
        } else { active=false; state::log("RS485 UART TX failed: bytes=%d expected=%u status=%d; command not retried",written,static_cast<unsigned>(exchange.length),finished); xSemaphoreGive(completed); }
    }
    xSemaphoreGive(exchange_mutex);
}
static void receive(void *) {
    ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
    FrameParser parser;
    uint8_t buffer[256]; unsigned current = board::baud;
    for (;;) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        if (gpio_get_level(board::de)) {
            gpio_set_level(board::de, 0); state::log("ERROR: DE was not LOW; restart"); esp_restart();
        }
        if (requested_baud.load() != current) {
            current = requested_baud.load(); ESP_ERROR_CHECK(uart_set_baudrate(board::uart, current));
            uart_flush_input(board::uart); parser.reset(); state::baud(current);
            state::log("Passive UART baud=%u, RX=18, TX disconnected, DE=LOW", current);
        }
        int n = uart_read_bytes(board::uart, buffer, sizeof(buffer), pdMS_TO_TICKS(active?10:30));
        auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        if (n > 0) { state::raw(buffer, n); parser.feed(buffer, n, now, captured, nullptr); }
        else parser.expire(now);
        size_t buffered=0; uart_get_buffered_data_len(board::uart,&buffered);
        dispatch(n==0 && buffered==0 && !parser.partial());
        uart_event_t event;
        while (xQueueReceive(events, &event, 0) == pdTRUE) {
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                state::uart_error(true); parser.reset(); uart_flush_input(board::uart);
                state::log("UART overflow: RX gap, increase capture/export frequency");
            } else if (event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR) state::uart_error(false);
        }
        state::parser_stats(parser.valid, parser.invalid, parser.noise);
    }
}
void start() {
    uart_config_t config{}; config.baud_rate = board::baud;
    config.data_bits = UART_DATA_8_BITS; config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1; config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(board::uart, &config));
    call_mutex=xSemaphoreCreateMutex(); exchange_mutex=xSemaphoreCreateMutex(); completed=xSemaphoreCreateBinary();
    configASSERT(call_mutex&&exchange_mutex&&completed);
    ESP_ERROR_CHECK(uart_set_pin(board::uart, board::tx, board::rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(board::uart, 8192, 0, 64, &events, 0));
    configASSERT(xTaskCreate(receive, "rs485_rx", 12288, nullptr, 12, nullptr) == pdPASS);
    state::log("RS485: 115200 8N1, RX=18, TX=17 guarded, DE=21 LOW; active role disabled by default");
}
bool self_test() {
    const uint8_t frame[] = {0x68,0x0c,0,0,0x56,0x30,5,0x40,0xb0,0x0b,0xfc,0x16};
    FrameParser parser; unsigned count = 0;
    auto cb = [](const uint8_t *, size_t n, void *ctx) { if (n == 12) ++*static_cast<unsigned *>(ctx); };
    for (size_t split = 0; split <= sizeof(frame); ++split) {
        parser.feed(frame, split, 1, cb, &count);
        parser.feed(frame + split, sizeof(frame) - split, 2, cb, &count);
    }
    uint8_t bad[sizeof(frame)]; memcpy(bad, frame, sizeof(frame)); bad[10] ^= 1;
    parser.feed(bad, sizeof(bad), 3, cb, &count);
    parser.feed(frame, sizeof(frame), 4, cb, &count);
    parser.feed(frame, 4, 5, cb, &count); parser.expire(300);
    parser.feed(frame, sizeof(frame), 301, cb, &count);
    const uint8_t noise[] = {1,2,0x68,9,0,0x68,0xff,0xff};
    parser.feed(noise, sizeof(noise), 302, cb, &count);
    parser.feed(frame, sizeof(frame), 303, cb, &count);
    const uint8_t room[]={10,229,0},night[]={11,228,0},unknown[]={13,228,0},too_low[]={10,99,0},too_high[]={10,95,1},read[]={255,0};
    bool guards=request_allowed(93,room,3,221)&&request_allowed(93,night,3,221)&&
        !request_allowed(93,unknown,3,221)&&!request_allowed(93,too_low,3,221)&&!request_allowed(93,too_high,3,221)&&
        !request_allowed(93,room,2,221)&&!request_allowed(93,room,3,215)&&!request_allowed(93,nullptr,3,221)&&
        request_allowed(92,read,2,220)&&!request_allowed(92,read,2,212);
    const uint8_t party[]={2,232,0},holiday[]={3,213,0},mode[]={1,2,0},mixer[]={0,0,46},bad_mixer[]={0,1,46},other_mixer[]={1,0,46};
    guards=guards&&request_allowed(93,party,3,221)&&request_allowed(93,holiday,3,221)&&
        !request_allowed(93,mode,3,221)&&request_allowed(52,mixer,3,180)&&
        !request_allowed(52,bad_mixer,3,180)&&!request_allowed(52,other_mixer,3,180)&&
        !request_allowed(52,mixer,3,179)&&!request_allowed(52,mixer,2,180)&&
        !readonly_allowed(52,mixer,3,180)&&!readonly_allowed(93,party,3,221);
    const uint8_t mixer_read[]={16,0},boiler_read[]={32,0},alarms_read[]={0,10};
    bool reads=readonly_allowed(50,mixer_read,2,178)&&readonly_allowed(49,boiler_read,2,177)&&
        readonly_allowed(61,alarms_read,2,189)&&readonly_allowed(54,nullptr,0,182)&&
        readonly_allowed(92,read,2,220)&&!readonly_allowed(50,mixer_read,2,177)&&
        !readonly_allowed(93,room,3,221)&&!readonly_allowed(59,room,1,187)&&
        !readonly_allowed(87,room,3,215)&&!readonly_allowed(25,nullptr,0,0)&&
        !readonly_allowed(50,nullptr,2,178)&&!readonly_allowed(50,read,2,178);
    const uint8_t menu_versions[]={0},menu_config[]={3},menu_values[]={4,255,0},menu_write[]={5,1,0,46};
    reads=reads&&readonly_allowed(86,menu_versions,1,214)&&readonly_allowed(86,menu_config,1,214)&&
        readonly_allowed(86,menu_values,3,214)&&!readonly_allowed(86,menu_write,4,214)&&
        !readonly_allowed(86,menu_values,3,215)&&!readonly_allowed(86,nullptr,1,214);
    guards=guards&&request_allowed(86,menu_write,4,214)&&!request_allowed(86,menu_write,3,214)&&!request_allowed(86,menu_write,4,215);
    uint8_t schedule_write[46]={1,1,1,5};
    uint8_t room_schedule[46]={1,16,1,0},mode_write[]={1,2};
    guards=guards&&thermostat_allowed(88,nullptr,0,216)&&!request_allowed(88,nullptr,0,216)&&
        thermostat_allowed(89,room_schedule,46,217)&&!thermostat_allowed(89,room_schedule,45,217)&&
        !thermostat_allowed(89,room_schedule,46,216)&&!thermostat_allowed(93,mode_write,2,221)&&request_allowed(93,mode_write,2,221);
    mode_write[1]=128; guards=guards&&request_allowed(93,mode_write,2,221); mode_write[1]=8; guards=guards&&!request_allowed(93,mode_write,2,221);
    room_schedule[3]=255; guards=guards&&thermostat_allowed(89,room_schedule,46,217); room_schedule[3]=4; guards=guards&&thermostat_allowed(89,room_schedule,46,217);
    room_schedule[3]=3; guards=guards&&thermostat_allowed(89,room_schedule,46,217);
    room_schedule[3]=101; guards=guards&&!thermostat_allowed(89,room_schedule,46,217);
    room_schedule[3]=0; room_schedule[1]=17; guards=guards&&!thermostat_allowed(89,room_schedule,46,217);
    uint8_t antifreeze[]={12,50,0},correction[]={4,109},timer[]={5,6},hysteresis[]={9,6};
    guards=guards&&request_allowed(93,antifreeze,3,221)&&request_allowed(93,correction,2,221)&&request_allowed(93,timer,2,221)&&request_allowed(93,hysteresis,2,221);
    antifreeze[1]=49;guards=guards&&!request_allowed(93,antifreeze,3,221);correction[1]=141;guards=guards&&!request_allowed(93,correction,2,221);
    timer[1]=61;guards=guards&&!request_allowed(93,timer,2,221);hysteresis[1]=1;guards=guards&&!request_allowed(93,hysteresis,2,221);
    guards=guards&&request_allowed(55,schedule_write,46,0)&&!readonly_allowed(55,schedule_write,46,0)&&!request_allowed(55,schedule_write,45,0)&&!request_allowed(55,schedule_write,46,183);
    schedule_write[1]=16; guards=guards&&!request_allowed(55,schedule_write,46,0); schedule_write[1]=1;
    schedule_write[2]=2; guards=guards&&!request_allowed(55,schedule_write,46,0); schedule_write[2]=1;
    schedule_write[3]=101; guards=guards&&!request_allowed(55,schedule_write,46,0);
    uint8_t bad_menu_write[]={5,2,47,46}; guards=guards&&!request_allowed(86,bad_menu_write,4,214);
    bad_menu_write[1]=1; bad_menu_write[2]=168; guards=guards&&!request_allowed(86,bad_menu_write,4,214);
    for(unsigned type:{51,52,55,59,87,89,93,96,97,98,99,100,101,102})
        reads=reads&&!readonly_allowed(type,boiler_read,2,type+128)&&!readonly_allowed(type,nullptr,0,type+128);
    const uint8_t pellet_config[]={255,0};uint8_t pellet_point[3]{};
    reads=reads&&readonly_allowed(97,pellet_config,2,225)&&!readonly_allowed(112,pellet_config,2,240);
    guards=guards&&pellet_commands::point(100,pellet_point)&&request_allowed(112,pellet_point,3,240);
    pellet_point[1]=141;guards=guards&&!request_allowed(112,pellet_point,3,240);
    uint8_t menu_request[11]{},panel_reply[12]{},ack_request[13]{},ack_reply[10]{};
    menu_request[7]=86; menu_request[8]=3; panel_reply[4]=80; panel_reply[7]=214; panel_reply[8]=3;
    reads=reads&&reply_matches(menu_request,11,214,panel_reply,12);
    panel_reply[8]=4; reads=reads&&!reply_matches(menu_request,11,214,panel_reply,12); panel_reply[8]=3;
    panel_reply[4]=69; reads=reads&&reply_matches(menu_request,11,214,panel_reply,12);
    ack_request[7]=52; ack_reply[4]=80; ack_reply[7]=180;
    reads=reads&&!reply_matches(ack_request,13,180,ack_reply,10); ack_reply[4]=69;
    reads=reads&&reply_matches(ack_request,13,180,ack_reply,10)&&!reply_matches(menu_request,10,214,panel_reply,12);
    uint8_t write_request[14]{},write_reply[13]{};
    write_request[7]=86; write_request[8]=5; write_request[9]=1; write_request[10]=47;
    write_reply[4]=80; write_reply[7]=214; write_reply[8]=5; write_reply[9]=1; write_reply[10]=47;
    guards=guards&&reply_matches(write_request,14,214,write_reply,13);
    write_reply[10]=48; guards=guards&&!reply_matches(write_request,14,214,write_reply,13); write_reply[10]=47;
    write_reply[9]=2; guards=guards&&!reply_matches(write_request,14,214,write_reply,13); write_reply[9]=1;
    guards=guards&&!reply_matches(write_request,14,214,write_reply,11);
    uint8_t point[]={4,1,47},point_request[13]{},point_reply[16]{};
    reads=reads&&readonly_allowed(86,point,3,214); point[2]=168; reads=reads&&!readonly_allowed(86,point,3,214); point[2]=47;
    point[1]=2; reads=reads&&!readonly_allowed(86,point,3,214); point[1]=1;
    point_request[7]=86; memcpy(point_request+8,point,3); point_reply[4]=80; point_reply[7]=214; memcpy(point_reply+8,point,3);
    reads=reads&&reply_matches(point_request,13,214,point_reply,16);
    point_reply[10]=48; reads=reads&&!reply_matches(point_request,13,214,point_reply,16); point_reply[10]=47;
    point_reply[9]=2; reads=reads&&!reply_matches(point_request,13,214,point_reply,16); point_reply[9]=1;
    reads=reads&&!reply_matches(point_request,13,214,point_reply,15);
    const uint8_t alarm_request[]={0x68,12,0,69,86,48,5,61,0,10,0,0x16};
    uint8_t alarm_reply[]={0x68,13,0,0,80,252,177,189,0,0,0,197,0x16};
    reads=reads&&reply_matches(alarm_request,12,189,alarm_reply,13);
    alarm_reply[4]=81; reads=reads&&!reply_matches(alarm_request,12,189,alarm_reply,13); alarm_reply[4]=80;
    alarm_reply[9]=1; reads=reads&&!reply_matches(alarm_request,12,189,alarm_reply,13); alarm_reply[9]=0;
    alarm_reply[10]=11; reads=reads&&!reply_matches(alarm_request,12,189,alarm_reply,13); alarm_reply[10]=0;
    uint8_t room_request[10]{},room_reply[60]{}; room_request[3]=81;room_request[7]=88;room_reply[3]=86;room_reply[4]=81;room_reply[7]=216;
    reads=reads&&reply_matches(room_request,10,216,room_reply,60); room_reply[4]=69;reads=reads&&!reply_matches(room_request,10,216,room_reply,60);
    room_reply[4]=81;room_reply[3]=69;reads=reads&&!reply_matches(room_request,10,216,room_reply,60);
    return guards && reads && count == 16 && parser.valid == 16 && parser.invalid >= 4;
}
}

