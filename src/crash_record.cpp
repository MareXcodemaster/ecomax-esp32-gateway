#include "crash_record.hpp"
#include "esp_attr.h"
#include "esp_private/panic_internal.h"
#include "esp_idf_version.h"
#include "xtensa_context.h"
#include <cstdint>

static_assert(ESP_IDF_VERSION_MAJOR==5 && ESP_IDF_VERSION_MINOR==5,"Review panic recorder for this IDF version");
namespace {
struct Crash { uint32_t magic,core,exception,pc,sp; char reason[160],description[160],abort_details[192]; };
RTC_NOINIT_ATTR volatile Crash saved;
constexpr uint32_t magic=0x4c415a52;
void IRAM_ATTR copy_text(volatile char *out,unsigned size,const char *source) {
    unsigned i=0;
    if(source) for(;i+1<size && source[i];++i) out[i]=source[i];
    out[i]=0;
}
}
// IDF 5.5 enables cache before this entry. Retain diagnostics only, then use its original panic handler.
extern "C" void __attribute__((noreturn)) __real_esp_panic_handler(panic_info_t *info);
extern "C" void IRAM_ATTR __attribute__((noreturn)) __wrap_esp_panic_handler(panic_info_t *info) {
    saved.magic=0;
    saved.core=info->core; saved.exception=info->exception;
    saved.pc=reinterpret_cast<uintptr_t>(info->addr);
    saved.sp=info->frame?static_cast<const XtExcFrame *>(info->frame)->a1:0;
    copy_text(saved.reason,sizeof(saved.reason),info->reason);
    copy_text(saved.description,sizeof(saved.description),info->description);
    copy_text(saved.abort_details,sizeof(saved.abort_details),g_panic_abort?g_panic_abort_details:nullptr);
    saved.magic=magic;
    __real_esp_panic_handler(info);
}
namespace crash_record {
cJSON *snapshot() {
    if(saved.magic!=magic) return cJSON_CreateNull();
    Crash value{}; value.core=saved.core; value.exception=saved.exception; value.pc=saved.pc; value.sp=saved.sp;
    for(unsigned i=0;i<sizeof(value.reason);++i) value.reason[i]=saved.reason[i];
    for(unsigned i=0;i<sizeof(value.description);++i) value.description[i]=saved.description[i];
    for(unsigned i=0;i<sizeof(value.abort_details);++i) value.abort_details[i]=saved.abort_details[i];
    value.reason[159]=value.description[159]=value.abort_details[191]=0;
    auto *out=cJSON_CreateObject(); cJSON_AddNumberToObject(out,"core",value.core); cJSON_AddNumberToObject(out,"exception",value.exception);
    cJSON_AddNumberToObject(out,"pc",value.pc); cJSON_AddNumberToObject(out,"sp",value.sp);
    cJSON_AddStringToObject(out,"reason",value.reason); cJSON_AddStringToObject(out,"description",value.description);
    cJSON_AddStringToObject(out,"abort_details",value.abort_details); return out;
}
}
