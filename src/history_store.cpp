#include "history_store.hpp"
#include "state.hpp"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_littlefs.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <atomic>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace history_store {
namespace {
using history_data::Sample;using history_data::Point;
constexpr uint32_t offset=0x620000,bytes=0x400000,magic=0x3148534c,record_magic=0x3152534c;
constexpr const char *label="history",*base="/hist";
struct Record { uint32_t magic;Sample sample;uint32_t crc; };
struct Header { uint32_t magic,version,record_size,fields,schema,hour,crc; };
struct Hour { uint32_t hour,first=0,last=0,count=0; };
static_assert(sizeof(Record)==172&&sizeof(Header)==28,"Archive ABI changed");
const esp_partition_t *partition=nullptr;
SemaphoreHandle_t mutex=nullptr;
QueueHandle_t queue=nullptr;
std::vector<Hour> hours;
bool ready=false,loading=true,restored=false,remount_verified=false;
size_t total_bytes=0,used_bytes=0;
uint32_t bad_records=0,last_stored=0;
uint32_t signature=0;
std::atomic<uint32_t> dropped{0};
char error[128]{};
struct Lock { Lock(){xSemaphoreTake(mutex,portMAX_DELAY);}~Lock(){xSemaphoreGive(mutex);} };
void fail(const char *message){snprintf(error,sizeof(error),"%s",message);state::log("HISTORY archive: %s",message);}
uint32_t crc(const void *data,size_t size){return esp_rom_crc32_le(0,static_cast<const uint8_t *>(data),size);}
bool valid(const Record &r){return r.magic==record_magic&&r.crc==crc(&r,offsetof(Record,crc))&&r.sample.time>=1577836800&&r.sample.time<=4102444800U&&r.sample.native<=1;}
Header header(uint32_t hour){Header h{magic,1,sizeof(Record),history_data::fields,signature,hour,0};h.crc=crc(&h,offsetof(Header,crc));return h;}
bool valid_header(const Header &h,uint32_t hour){auto expected=header(hour);return memcmp(&h,&expected,sizeof(h))==0;}
void path(char (&out)[40],uint32_t hour){snprintf(out,sizeof(out),"%s/h%010lu.bin",base,static_cast<unsigned long>(hour));}
bool filename(const char *name,uint32_t &hour){if(strlen(name)!=15||name[0]!='h'||strcmp(name+11,".bin"))return false;for(unsigned i=1;i<11;++i)if(name[i]<'0'||name[i]>'9')return false;unsigned long value=0;int end=0;if(sscanf(name,"h%10lu.bin%n",&value,&end)!=1||end!=15||value>4102444800UL||value%3600)return false;hour=value;return true;}
esp_err_t mount(){esp_vfs_littlefs_conf_t conf{};conf.base_path=base;conf.partition_label=label;conf.format_if_mount_failed=false;return esp_vfs_littlefs_register(&conf);}
bool erased(){uint8_t buffer[1024];for(uint32_t at=0;at<bytes;at+=sizeof(buffer)){if(esp_partition_read(partition,at,buffer,sizeof(buffer))!=ESP_OK)return false;for(auto value:buffer)if(value!=255)return false;if(!(at%65536))vTaskDelay(1);}return true;}
bool reserve(){
    uint32_t size=0;if(esp_flash_get_size(nullptr,&size)!=ESP_OK||size<offset+bytes){fail("Flash za mala na archiwum 3 dni");return false;}
    partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,label);
    if(partition){if(partition->flash_chip!=esp_flash_default_chip||partition->address!=offset||partition->size!=bytes||partition->encrypted||partition->readonly){fail("Niezgodny obszar archiwum; bez zapisu");return false;}}
    else if(esp_partition_register_external(nullptr,offset,bytes,label,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,&partition)!=ESP_OK){fail("Obszar archiwum koliduje z partycjami; bez zapisu");return false;}
    return true;
}
bool probe(){
    constexpr char name[]="/hist/.probe";uint32_t input[]={0x31545348,0x12345678,0x89abcdef,0},output[4]{};input[3]=crc(input,12);
    auto *file=fopen(name,"wb");if(!file)return false;
    bool ok=fwrite(input,sizeof(input),1,file)==1&&fflush(file)==0&&fsync(fileno(file))==0;if(fclose(file))ok=false;
    if(!ok)return false;
    if(esp_vfs_littlefs_unregister(label)!=ESP_OK||mount()!=ESP_OK)return false;
    file=fopen(name,"rb");if(!file)return false;ok=fread(output,sizeof(output),1,file)==1&&memcmp(input,output,sizeof(input))==0;fclose(file);unlink(name);return ok;
}
bool scan(){
    hours.clear();hours.reserve(96);auto *dir=opendir(base);if(!dir)return false;
    while(auto *entry=readdir(dir)){uint32_t hour;if(filename(entry->d_name,hour))hours.push_back({hour});if(hours.size()>96){closedir(dir);fail("Zbyt wiele plikow archiwum; bez usuwania danych");return false;}}
    closedir(dir);std::sort(hours.begin(),hours.end(),[](const Hour &a,const Hour &b){return a.hour<b.hour;});
    for(auto &info:hours){char name[40];path(name,info.hour);auto *file=fopen(name,"rb");if(!file)return false;Header h{};
        if(fread(&h,sizeof(h),1,file)!=1||!valid_header(h,info.hour)){
            bool empty=fseek(file,0,SEEK_END)==0&&ftell(file)>=0&&ftell(file)<static_cast<long>(sizeof(Header));fclose(file);
            if(empty&&unlink(name)==0){info.hour=0;++bad_records;continue;}
            fail("Nieznany format pliku historii; bez nadpisywania");return false;
        }
        Record record{};size_t n;while((n=fread(&record,1,sizeof(record),file))>0){if(n!=sizeof(record)||!valid(record)||record.sample.time/3600*3600!=info.hour){++bad_records;continue;}if(!info.first)info.first=record.sample.time;info.last=record.sample.time;++info.count;}
        bool ok=!ferror(file);fclose(file);if(!ok)return false;last_stored=std::max(last_stored,info.last);vTaskDelay(1);
    }
    hours.erase(std::remove_if(hours.begin(),hours.end(),[](const Hour &h){return h.hour==0;}),hours.end());
    restored=last_stored!=0;return true;
}
bool initialise(){
    if(!reserve())return false;
    auto result=mount();
    if(result!=ESP_OK){
        // Only a completely erased, non-overlapping area may be initialised.
        if(!erased()){fail("Nie mozna otworzyc archiwum; dane nie zostaly sformatowane");return false;}
        if(esp_littlefs_format(label)!=ESP_OK||mount()!=ESP_OK){fail("Nie utworzono archiwum LittleFS");return false;}
    }
    remount_verified=probe();if(!remount_verified){fail("Niepotwierdzony zapis/ponowne otwarcie LittleFS");return false;}
    if(!scan()){if(!*error)fail("Nie odczytano katalogu historii");return false;}
    if(esp_littlefs_info(label,&total_bytes,&used_bytes)!=ESP_OK){fail("Nie odczytano stanu pamieci historii");return false;}
    state::log("HISTORY archive mounted: %u hours, last=%lu; remount PASS; no boiler commands",static_cast<unsigned>(hours.size()),static_cast<unsigned long>(last_stored));return true;
}
bool prune(uint32_t time){
    const auto cutoff=time>history_data::retention_seconds?time-history_data::retention_seconds:0;
    while(!hours.empty()&&hours.front().hour+3600<=cutoff){char name[40];path(name,hours.front().hour);if(unlink(name)){fail("Nie usunieto najstarszej godziny historii");return false;}hours.erase(hours.begin());vTaskDelay(1);}
    return true;
}
bool persist(const Sample &sample){
    if(last_stored&&sample.time<last_stored+history_data::archive_interval)return true;
    const uint32_t hour=sample.time/3600*3600;
    auto found=std::find_if(hours.begin(),hours.end(),[&](const Hour &h){return h.hour==hour;});bool create=found==hours.end();
    if(create){if(!prune(sample.time))return false;if(hours.size()>=74){fail("Limit plikow historii; zapis zatrzymany");return false;}}
    char name[40];path(name,hour);auto *file=fopen(name,create?"wb":"r+b");if(!file){fail("Nie otwarto pliku historii do zapisu");return false;}
    bool ok=true;
    if(create){auto h=header(hour);ok=fwrite(&h,sizeof(h),1,file)==1;}
    else {Header h{};ok=fread(&h,sizeof(h),1,file)==1&&valid_header(h,hour);if(ok){ok=fseek(file,0,SEEK_END)==0;long size=ftell(file);if(size<static_cast<long>(sizeof(Header)))ok=false;else if(ok&&(size-sizeof(Header))%sizeof(Record)){auto complete=size-(size-sizeof(Header))%sizeof(Record);ok=ftruncate(fileno(file),complete)==0&&fseek(file,complete,SEEK_SET)==0;}}}
    Record record{};record.magic=record_magic;record.sample=sample;record.crc=crc(&record,offsetof(Record,crc));
    if(ok)ok=fwrite(&record,sizeof(record),1,file)==1&&fflush(file)==0&&fsync(fileno(file))==0;
    if(fclose(file))ok=false;
    if(!ok){fail("Nie potwierdzono zapisu historii; bez formatowania");return false;}
    if(create){hours.push_back({hour});found=hours.end()-1;esp_littlefs_info(label,&total_bytes,&used_bytes);}
    if(!found->first)found->first=sample.time;
    found->last=sample.time;++found->count;last_stored=sample.time;return true;
}
bool codec_test(){Record r{};r.magic=record_magic;r.sample.time=1700000000;r.sample.native=1;r.sample.mode=2;r.sample.values[0]=0;r.sample.values[1]=NAN;r.crc=crc(&r,offsetof(Record,crc));bool ok=valid(r);r.sample.time^=1;ok=ok&&!valid(r);auto h=header(1699999200);return ok&&valid_header(h,1699999200)&&!valid_header(h,1700002800);}
void task(void *){
    {Lock lock;ready=codec_test()&&initialise();loading=false;if(!ready&&!*error)fail("Test formatu historii nie powiodl sie");}
    Sample sample{};for(;;){if(xQueueReceive(queue,&sample,portMAX_DELAY)!=pdTRUE)continue;Lock lock;if(!ready){++dropped;continue;}if(!persist(sample)){ready=false;++dropped;}}
}
}
void start(uint32_t schema_signature){
    signature=schema_signature;
    mutex=xSemaphoreCreateMutex();queue=xQueueCreate(8,sizeof(Sample));
    if(!mutex||!queue){snprintf(error,sizeof(error),"Brak pamieci archiwum");loading=false;return;}
    if(xTaskCreate(task,"history_flash",6144,nullptr,1,nullptr)!=pdPASS){vQueueDelete(queue);queue=nullptr;loading=false;snprintf(error,sizeof(error),"Nie uruchomiono archiwum");}
}
bool enqueue(const Sample &sample){if(!queue)return false;if(xQueueSend(queue,&sample,0)!=pdTRUE){++dropped;return false;}return true;}
size_t read(uint32_t from,uint32_t to,Point *points,size_t capacity){
    if(!mutex||!points||!capacity)return 0;
    Lock lock;if(!ready)return 0;
    uint32_t first=0;for(const auto &hour:hours)if(hour.first){first=hour.first;break;}
    const auto actual_from=std::max(from,first),actual_to=std::min(to,last_stored);if(!first||actual_from>actual_to)return 0;
    size_t count=0;uint32_t bucket=0,previous=0,previous_native=0;const auto step=std::max<uint32_t>(60,history_data::step(actual_from,actual_to,capacity));
    for(const auto &hour:hours){if(hour.hour>to||hour.hour+3600<=from)continue;char name[40];path(name,hour.hour);auto *file=fopen(name,"rb");if(!file)continue;Header h{};if(fread(&h,sizeof(h),1,file)!=1||!valid_header(h,hour.hour)){fclose(file);continue;}
        Record r{};while(fread(&r,sizeof(r),1,file)==1){if(!valid(r)||r.sample.time<from||r.sample.time>to)continue;auto time=r.sample.time;const uint32_t next=(time-actual_from)/step;Point point{r.sample,static_cast<uint32_t>(previous&&(time-previous>90||previous_native!=r.sample.native))};previous=time;previous_native=r.sample.native;
            if(count&&next==bucket){point.gap_before|=points[count-1].gap_before;points[count-1]=point;}else if(count<capacity){points[count++]=point;bucket=next;}
        }fclose(file);vTaskDelay(1);
    }
    return count;
}
cJSON *status(){
    auto *out=cJSON_CreateObject();if(!out)return nullptr;
    if(!mutex){cJSON_AddBoolToObject(out,"ready",false);cJSON_AddStringToObject(out,"error",error);return out;}
    Lock lock;uint32_t count=0,first=0;for(auto &h:hours){count+=h.count;if(!first&&h.first)first=h.first;}
    cJSON_AddBoolToObject(out,"ready",ready);cJSON_AddBoolToObject(out,"loading",loading);cJSON_AddBoolToObject(out,"restored",restored);cJSON_AddBoolToObject(out,"remount_verified",remount_verified);
    cJSON_AddNumberToObject(out,"retention_days",3);cJSON_AddNumberToObject(out,"interval_seconds",60);cJSON_AddNumberToObject(out,"samples",count);cJSON_AddNumberToObject(out,"oldest_epoch",first);cJSON_AddNumberToObject(out,"newest_epoch",last_stored);
    cJSON_AddNumberToObject(out,"total_bytes",total_bytes);cJSON_AddNumberToObject(out,"used_bytes",used_bytes);cJSON_AddNumberToObject(out,"bad_records",bad_records);cJSON_AddNumberToObject(out,"dropped",dropped.load());cJSON_AddStringToObject(out,"error",error);return out;
}
}
