#include "protocol.hpp"
#include "protocol_catalog.hpp"
#include "protocol_fixtures.hpp"
#include "sensor_decoder.hpp"
#include "thermostat_decoder.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace protocol {
namespace {
struct Reader {
    const uint8_t *data; size_t size, at = 0; bool ok = true;
    const uint8_t *take(size_t count) {
        if (!ok || count > size - at) { ok = false; return nullptr; }
        auto *value = data + at; at += count; return value;
    }
    uint8_t u8() { auto *p = take(1); return p ? *p : 0; }
    uint16_t u16() { auto *p = take(2); return p ? p[0] | (p[1] << 8) : 0; }
    uint32_t u32() { auto *p = take(4); return p ? static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24) : 0; }
    bool done() const { return ok && at == size; }
};
struct Field { uint16_t id; uint8_t type; };
struct Schema {
    Field fields[256]{}; unsigned count = 0;
    uint16_t version = 0; bool bound = false, recorded = false;
};
struct Outcome { const char *state = "reference_only"; unsigned fields = 0; };
struct Counts { uint64_t decoded = 0, failed = 0; const char *state = "not_seen"; unsigned fields = 0; };
static Schema schema;
static Counts counts[256];
static uint16_t latest_version = 0; static bool version_seen = false;
static SemaphoreHandle_t mutex;
struct Lock { Lock() { xSemaphoreTake(mutex, portMAX_DELAY); } ~Lock() { xSemaphoreGive(mutex); } };

const char *extra_name(unsigned type) {
    switch (type) {
        case 80: return "GET_PARAMETER_CONFIG"; case 81: return "GET_MIXER_CONFIG";
        case 82: return "GET_PARAMETER_STRUCTURE"; case 83: return "GET_MIXER_STRUCTURE";
        case 84: return "GET_CURRENT_PARAMETERS"; case 86: return "REMOTE_MENU";
        case 87: return "SET_CURRENT_PARAMETER"; case 88: return "GET_THERMOSTAT_SCHEDULES";
        case 89: return "SET_THERMOSTAT_SCHEDULE"; case 217: return "THERMOSTAT_SCHEDULE_WRITE_REPLY";
        case 94: return "GET_THERMOSTAT_CONFIG"; case 95: return "GET_THERMOSTAT_STRUCTURE";
        case 184: return "SETTINGS_REPLY"; case 208: return "PARAMETER_CONFIG_REPLY";
        case 209: return "MIXER_CONFIG_REPLY"; case 210: return "PARAMETER_STRUCTURE_REPLY";
        case 215: return "SET_CURRENT_PARAMETER_ACK";
        case 211: return "MIXER_STRUCTURE_REPLY"; case 214: return "REMOTE_MENU_REPLY";
        case 216: return "THERMOSTAT_SCHEDULES_REPLY"; case 222: return "THERMOSTAT_CONFIG_REPLY";
        case 223: return "THERMOSTAT_STRUCTURE_REPLY"; default: return nullptr;
    }
}
const char *name(unsigned type) { auto *value = reference_name(type); return value ? value : extra_name(type); }
const char *origin(unsigned type) { return reference_name(type) ? "PyPlumIO_033c15781" : extra_name(type) ? "ecoNET_reference_gist" : "not_documented"; }
void number(cJSON *out, const char *key, double value) { if (out) cJSON_AddNumberToObject(out, key, value); }
void boolean(cJSON *out, const char *key, bool value) { if (out) cJSON_AddBoolToObject(out, key, value); }
cJSON *object(cJSON *array) { if (!array) return nullptr; auto *out = cJSON_CreateObject(); cJSON_AddItemToArray(array, out); return out; }
cJSON *array(cJSON *out, const char *key) { return out ? cJSON_AddArrayToObject(out, key) : nullptr; }

void alert_date(cJSON *out,const char *key,uint32_t seconds) {
    // Controller dates use twelve 31-day months, not Unix seconds.
    unsigned year=2000+seconds/32140800; seconds%=32140800;
    unsigned month=1+seconds/2678400; seconds%=2678400;
    unsigned day=1+seconds/86400; seconds%=86400;
    unsigned hour=seconds/3600,minute=(seconds%3600)/60,second=seconds%60;
    const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap=year%4==0&&(year%100!=0||year%400==0);
    if(month>12||day>days[month-1]+(month==2&&leap?1:0)) { if(out)cJSON_AddNullToObject(out,key); return; }
    char value[24]; snprintf(value,sizeof(value),"%04u-%02u-%02uT%02u:%02u:%02u",year,month,day,hour,minute,second);
    if(out)cJSON_AddStringToObject(out,key,value);
}

bool schema_read(Reader &r, Schema &next, cJSON *out) {
    next.count = r.u16(); if (!r.ok || !next.count || next.count > 256 || r.size != 2 + next.count * 3) return false;
    auto *items = array(out, "fields");
    for (unsigned i = 0; i < next.count; ++i) {
        auto &field = next.fields[i]; field.type = r.u8(); field.id = r.u16();
        if (field.type > 16) return false;
        auto *item = object(items); number(item, "id", field.id); number(item, "wire_type", field.type);
    }
    number(out, "field_count", next.count); return r.done();
}
bool versions(Reader &r, uint16_t &version, bool &found) {
    unsigned count = r.u8(); found = false;
    if (!r.ok || count > (r.size - r.at) / 3) return false;
    for (unsigned i = 0; i < count; ++i) {
        unsigned type = r.u8(); auto value = r.u16();
        if (type == 85) { version = value; found = true; }
    }
    return r.ok;
}

bool scalar(Reader &r, unsigned type, unsigned &bit, cJSON *out, const char *key) {
    if (type != 10 && bit) { r.take(1); bit = 0; }
    if (type == 0 || type == 8) { if (out) cJSON_AddNullToObject(out, key); return r.ok; }
    if (type == 10) {
        if (!r.ok || r.at >= r.size) return false;
        boolean(out, key, (r.data[r.at] & (1U << bit)) != 0);
        if (++bit == 8) { r.take(1); bit = 0; }
        return r.ok;
    }
    if (type == 11 || type == 12) {
        size_t start = r.at;
        while (r.ok && r.at < r.size && r.data[r.at]) r.take(1);
        if (r.at >= r.size) return false;
        size_t length = r.at - start; r.take(1);
        if (out) {
            char value[1001]; memcpy(value, r.data + start, length); value[length] = 0;
            // Preserve ASCII text; unverified wire encoding is not interpreted as Unicode.
            for (size_t i = 0; i < length; ++i) if (static_cast<unsigned char>(value[i]) >= 128) value[i] = '?';
            cJSON_AddStringToObject(out, key, value);
        }
        return r.ok;
    }
    constexpr unsigned sizes[] = {0,1,2,4,1,2,4,4,0,8,0,0,0,8,8,4,16};
    if (type > 16) return false;
    auto *p = r.take(sizes[type]); if (!p) return false;
    double value = 0;
    switch (type) {
        case 1: value = static_cast<int8_t>(p[0]); break;
        case 2: { int16_t v; memcpy(&v, p, 2); value = v; break; }
        case 3: { int32_t v; memcpy(&v, p, 4); value = v; break; }
        case 4: value = p[0]; break;
        case 5: { uint16_t v; memcpy(&v, p, 2); value = v; break; }
        case 6: { uint32_t v; memcpy(&v, p, 4); value = v; break; }
        case 7: { float v; memcpy(&v, p, 4); value = v; break; }
        case 9: memcpy(&value, p, 8); break;
        case 13: case 14: {
            if (out) {
                char text[32];
                if (type == 13) { int64_t v; memcpy(&v, p, 8); snprintf(text, sizeof(text), "%lld", static_cast<long long>(v)); }
                else { uint64_t v; memcpy(&v, p, 8); snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(v)); }
                cJSON_AddStringToObject(out, key, text);
            }
            return true;
        }
        case 15: case 16: {
            if (out) {
                char text[48];
                if (type == 15) snprintf(text, sizeof(text), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
                else snprintf(text, sizeof(text), "%x:%x:%x:%x:%x:%x:%x:%x", (p[0]<<8)|p[1], (p[2]<<8)|p[3], (p[4]<<8)|p[5], (p[6]<<8)|p[7], (p[8]<<8)|p[9], (p[10]<<8)|p[11], (p[12]<<8)|p[13], (p[14]<<8)|p[15]);
                cJSON_AddStringToObject(out, key, text);
            }
            return true;
        }
        default: return false;
    }
    if (out) {
        if (std::isfinite(value)) number(out, key, value);
        else cJSON_AddNullToObject(out, key);
    }
    return true;
}

Outcome decode(const uint8_t *raw, size_t length, const Schema &context, cJSON *out) {
    if (length < 10 || length > 1000 || raw[0] != 0x68 || raw[length-1] != 0x16 || (raw[1] | (raw[2]<<8)) != length) return {"payload_mismatch"};
    uint8_t sum = 0; for (size_t i = 0; i < length-2; ++i) sum ^= raw[i];
    if (sum != raw[length-2]) return {"payload_mismatch"};
    unsigned type = raw[7]; Reader r{raw+8, length-10}; unsigned fields = 0;
    bool supported = true;
    switch (type) {
        case 8: {
            r.take(2); unsigned minor = r.u8(), major = r.u8();
            uint16_t version = 0; bool found = false;
            if (minor != 0 || major != 1 || !versions(r, version, found)) return {"payload_mismatch"};
            if (!context.count || !context.bound || !found || version != context.version || raw[4] != 69) return {"waiting_schema"};
            if (context.recorded && (raw[8] != protocol_fixtures::frame_8[8] || raw[9] != protocol_fixtures::frame_8[9])) return {"waiting_schema"};
            auto *items = array(out, "fields"); unsigned bit = 0;
            for (unsigned i = 0; i < context.count; ++i) {
                const auto &field = context.fields[i]; auto *item = object(items);
                number(item, "id", field.id); number(item, "wire_type", field.type);
                if (!scalar(r, field.type, bit, item, "value")) return {"payload_mismatch"};
                ++fields;
            }
            if (bit) r.take(1);
            number(out, "schema_version", version); boolean(out, "recorded_schema", context.recorded);
            break;
        }
        case 53: {
            plum::Sensors sensors;
            if (!plum::decode_sensors(raw+8, length-10, sensors)) return {"payload_mismatch"};
            r.at = r.size; fields = 1; if (out) cJSON_AddStringToObject(out, "sensor_data_endpoint", "/api/ecomax"); break;
        }
        case 213: {
            Schema next; if (!schema_read(r, next, out)) return {"payload_mismatch"}; fields = next.count; break;
        }
        case 177: case 178: {
            unsigned marker = r.u8(), start = r.u8(), count = r.u8(), mixers = type == 178 ? r.u8() : 1;
            if (!r.ok || start+count > 256 || mixers > 16 || r.size != r.at + mixers*count*3) return {"payload_mismatch"};
            number(out, "marker", marker); number(out, "start", start); number(out, "count", count);
            auto *items = array(out, "parameters");
            for (unsigned m = 0; m < mixers; ++m) for (unsigned i = 0; i < count; ++i) {
                unsigned v = r.u8(), min = r.u8(), max = r.u8(); auto *item = object(items);
                number(item, "index", start+i); if (type == 178) number(item, "mixer", m);
                boolean(item, "present", v != 255 || min != 255 || max != 255);
                number(item, "value", v); number(item, "min", min); number(item, "max", max); ++fields;
            }
            break;
        }
        case 212: {
            unsigned count = r.u16(); if (!r.ok || count > 256) return {"payload_mismatch"};
            auto *items = array(out, "parameters");
            for (unsigned i = 0; i < count; ++i) {
                unsigned t = r.u8(), id = r.u16(); if (!r.ok || t > 16) return {"payload_mismatch"};
                auto *item = object(items); number(item, "id", id); number(item, "wire_type", t);
                if (t != 0 && t != 8 && t != 10 && t != 11 && t != 12) {
                    unsigned bit = 0;
                    if (!scalar(r, t, bit, item, "value") || !scalar(r, t, bit, item, "min") || !scalar(r, t, bit, item, "max")) return {"payload_mismatch"};
                } else boolean(item, "values_reported", false);
                ++fields;
            }
            break;
        }
        case 208: case 209: case 222: {
            unsigned start=r.u8(),count=r.u8(),slots=type==209?r.u8():1;
            if(!r.ok||start+count>256||!slots||slots>16||r.size!=r.at+slots*count*3) return {"payload_mismatch"};
            number(out,"start",start); number(out,"count",count); number(out,"slots",slots);
            auto *items=array(out,"parameters"); const double powers[]={1,10,100,1000};
            for(unsigned slot=0;slot<slots;++slot) for(unsigned i=0;i<count;++i) {
                unsigned flags=r.u8(),coefficient=r.u8(); int shift=static_cast<int8_t>(r.u8());
                unsigned unit=flags&31,exponent=(flags>>5)&3;
                double multiplier=unit==31?coefficient:(flags&128?coefficient/powers[exponent]:coefficient*powers[exponent]);
                auto *entry=object(items); number(entry,"index",start+i); if(type==209) number(entry,"mixer",slot);
                number(entry,"unit",unit); number(entry,"mult",multiplier); number(entry,"offset",shift); ++fields;
            }
            break;
        }
        case 214: {
            unsigned command=r.u8(); number(out,"command",command);
            if(command==0) {
                unsigned minor=r.u8(),major=r.u8(),count=r.u8();
                if(!r.ok||r.size!=4+count*3) return {"payload_mismatch"};
                number(out,"minor",minor); number(out,"major",major); auto *items=array(out,"commands");
                for(unsigned i=0;i<count;++i) { auto *entry=object(items); number(entry,"command",r.u8()); number(entry,"version",r.u16()); ++fields; }
            } else if(command==3) {
                unsigned count=r.u8(); if(!r.ok||r.size!=2+count*3) return {"payload_mismatch"};
                number(out,"count",count); auto *items=array(out,"parameters"); const double powers[]={1,10,100,1000};
                for(unsigned i=0;i<count;++i) {
                    unsigned flags=r.u8(),coefficient=r.u8(); int shift=static_cast<int8_t>(r.u8());
                    unsigned unit=flags&31,exponent=(flags>>5)&3;
                    double multiplier=unit==31?coefficient:(flags&128?coefficient/powers[exponent]:coefficient*powers[exponent]);
                    auto *entry=object(items); number(entry,"index",i); number(entry,"unit",unit); number(entry,"mult",multiplier); number(entry,"offset",shift); ++fields;
                }
            } else if(command==4) {
                unsigned count=r.u8(),start=r.u8(); if(!r.ok||start+count>256||r.size!=3+count*3) return {"payload_mismatch"};
                number(out,"start",start); number(out,"count",count); auto *items=array(out,"parameters");
                for(unsigned i=0;i<count;++i) {
                    unsigned value=r.u8(),low=r.u8(),high=r.u8(); auto *entry=object(items);
                    number(entry,"index",start+i); boolean(entry,"present",value!=255||low!=255||high!=255);
                    number(entry,"value",value); number(entry,"min",low); number(entry,"max",high); ++fields;
                }
            } else if(command==5) {
                unsigned count=r.u8(); if(!r.ok||count>168||r.size!=2+count) return {"payload_mismatch"};
                number(out,"count",count); auto *indices=array(out,"indices");
                for(unsigned i=0;i<count;++i) { unsigned index=r.u8(); if(index>=168) return {"payload_mismatch"}; if(indices) cJSON_AddItemToArray(indices,cJSON_CreateNumber(index)); ++fields; }
            } else if(command==9) {
                unsigned count=r.u16(); if(!r.ok||count>493||!count||r.size!=3+count*2) return {"payload_mismatch"};
                number(out,"wire_count",count); auto *items=array(out,"structure");
                bool removed[494]{}; unsigned parent=0,cursor=0;
                for(unsigned i=0;i<count;++i) {
                    unsigned attributes=r.u8(),index=r.u8(),kind=attributes&7,pass=attributes>>5;
                    if(attributes&8) return {"payload_mismatch"};
                    bool locked=attributes&16; unsigned data_id=index,lock_index=256;
                    if(kind==7&&i>2) ++cursor;
                    if(cursor>=494||parent>=494) return {"payload_mismatch"};
                    if(kind==0||kind==2||kind==4||kind==5) {
                        if(kind==4||removed[cursor]) removed[parent]=true;
                        ++parent;
                    }
                    if(kind==3) { if(i+1>=count)return {"payload_mismatch"}; r.u8(); data_id|=r.u8()<<8; ++i; }
                    if(locked&&i+2<count) { r.u8(); lock_index=r.u8(); ++i; }
                    if(kind==4||removed[cursor]) continue;
                    auto *entry=object(items); number(entry,"type",kind); number(entry,"index",index);
                    number(entry,"pass_index",pass); boolean(entry,"lock",locked);
                    if(kind==3) { char id[12]; snprintf(id,sizeof(id),"%u",data_id); if(entry)cJSON_AddStringToObject(entry,"data_id",id); }
                    if(lock_index<256) number(entry,"lock_index",lock_index);
                    ++fields;
                }
            } else return {"reference_only"};
            break;
        }
        case 220: {
            plum::ThermostatSettings settings;
            if(!plum::decode_thermostats(raw+8,length-10,settings)) return {"payload_mismatch"};
            number(out,"thermostat_slots",settings.slots); number(out,"parameters_per_thermostat",plum::thermostat_parameters);
            auto *items=array(out,"parameters");
            for(unsigned slot=0;slot<settings.slots;++slot) for(unsigned i=0;i<plum::thermostat_parameters;++i) {
                const auto &v=settings.values[slot][i]; auto *item=object(items);
                number(item,"thermostat",slot); number(item,"wire_index",1+slot*plum::thermostat_parameters+i);
                number(item,"size",plum::thermostat_sizes[i]); boolean(item,"present",v.present);
                if(item) cJSON_AddStringToObject(item,"name",plum::thermostat_names[i]);
                number(item,"value",v.value); number(item,"min",v.low); number(item,"max",v.high); ++fields;
            }
            r.at=r.size; break;
        }
        case 217: if(r.size==0)break; [[fallthrough]];
        case 216: case 182: {
            unsigned marker=r.u8(),start=r.u8(),count=r.u8();
            if(!r.ok||count>20||r.size!=3+count*47||((type==216||type==217)&&(marker!=16||start!=1||count>3)))return {"payload_mismatch"};
            number(out,"marker",marker); number(out,"start",start); number(out,"count",count);
            auto *items=array(out,"schedules"); bool seen[40]{};
            for(unsigned i=0;i<count;++i) {
                unsigned index=r.u8(),enabled=r.u8(),value=r.u8(),low=r.u8(),high=r.u8();
                bool present=value!=255||low!=255||high!=255;
                if(index>=40||seen[index]||enabled>1||(present&&(low>value||value>high))||((type==216||type==217)&&(index<16||index>18)))return {"payload_mismatch"};
                seen[index]=true; auto *entry=object(items);
                number(entry,"index",index); boolean(entry,"parameter_present",present);
                number(entry,"parameter_raw",value);
                auto *week=array(entry,"week");
                for(unsigned day=0;day<7;++day) {
                    auto *bytes=week?cJSON_CreateArray():nullptr; if(week)cJSON_AddItemToArray(week,bytes);
                    for(unsigned byte=0;byte<6;++byte) { unsigned v=r.u8(); if(bytes)cJSON_AddItemToArray(bytes,cJSON_CreateNumber(v)); }
                }
                auto *meta=week?cJSON_CreateArray():nullptr; if(week)cJSON_AddItemToArray(week,meta);
                for(unsigned v:{enabled,present?value:0U,present?low:0U,present?high:0U})if(meta)cJSON_AddItemToArray(meta,cJSON_CreateNumber(v));
                ++fields;
            }
            break;
        }
        case 89: case 55: {
            if(r.size!=46||r.u8()!=1)return {"payload_mismatch"};
            unsigned index=r.u8(),enabled=r.u8(); if(index>=40||enabled>1||(type==89&&index!=16))return {"payload_mismatch"};
            number(out,"index",index); number(out,"enabled",enabled); number(out,"parameter",r.u8());
            r.take(42); fields=1; break;
        }
        case 189: {
            unsigned total=r.u8(),start=r.u8(),count=r.u8();
            if(!r.ok||count>10||start+count>total||r.size!=3+count*9)return {"payload_mismatch"};
            number(out,"total_count",total); number(out,"start",start); number(out,"count",count);
            auto *items=array(out,"alerts");
            for(unsigned i=0;i<count;++i) {
                auto *entry=object(items); unsigned code=r.u8(); uint32_t from=r.u32(),to=r.u32();
                number(entry,"index",start+i); number(entry,"code",code); boolean(entry,"active",to==UINT32_MAX);
                number(entry,"from_raw",from); alert_date(entry,"from",from);
                if(to==UINT32_MAX) { if(entry)cJSON_AddNullToObject(entry,"to"); }
                else { number(entry,"to_raw",to); alert_date(entry,"to",to); }
                ++fields;
            }
            break;
        }
        case 87: {
            unsigned count=r.u16(); if(!r.ok||count>256) return {"payload_mismatch"};
            auto *items=array(out,"parameters");
            for(unsigned i=0;i<count;++i) {
                unsigned t=r.u8(), id=r.u16(), bit=0; auto *item=object(items);
                number(item,"id",id); number(item,"wire_type",t);
                if(t!=4||!scalar(r,t,bit,item,"value")) return {"payload_mismatch"};
                ++fields;
            }
            break;
        }
        case 24: case 25: case 48: case 54: case 57: case 58: case 64: case 84: case 85: case 88:
        case 179: case 180: case 187: case 215: case 221: break;
        case 49: case 50: case 56: case 80: case 81: case 82: case 83: case 92: case 94: case 95:
            number(out, "count", r.u8()); number(out, "start", r.u8()); fields = 2; break;
        case 51: number(out, "index", r.u8()); number(out, "value", r.u8()); fields = 2; break;
        case 52: number(out, "mixer", r.u8()); number(out, "index", r.u8()); number(out, "value", r.u8()); fields = 3; break;
        case 59: number(out, "value", r.u8()); fields = 1; break;
        case 93: {
            if(r.size!=2&&r.size!=3) return {"payload_mismatch"};
            number(out,"index",r.u8()); number(out,"value",r.size==3?r.u16():r.u8()); fields=2; break;
        }
        case 61: number(out, "start", r.u8()); number(out, "count", r.u8()); fields = 2; break;
        case 86: {
            unsigned command=r.u8(); number(out,"command",command); fields=1;
            if(command==4) { number(out,"count",r.u8()); number(out,"start",r.u8()); fields=3; }
            else if(command==5) { unsigned count=r.u8(),index=r.u8(),value=r.u8(); if(count!=1||index>=168||r.size!=4) return {"payload_mismatch"}; number(out,"count",count); number(out,"index",index); number(out,"value",value); fields=4; }
            break;
        }
        default: supported = false; break;
    }
    if (!supported) return {name(type) ? "reference_only" : "not_documented"};
    return r.done() ? Outcome{"decoded", fields} : Outcome{"payload_mismatch"};
}
Schema context_copy() { Lock lock; return schema; }
}

void init() {
    mutex = xSemaphoreCreateMutex(); configASSERT(mutex);
    Reader r{protocol_fixtures::frame_213+8, sizeof(protocol_fixtures::frame_213)-10};
    configASSERT(schema_read(r, schema, nullptr));
    Reader values{protocol_fixtures::frame_8+8, sizeof(protocol_fixtures::frame_8)-10};
    values.take(4); configASSERT(versions(values, schema.version, schema.bound)); schema.recorded = true;
}
void receive(const uint8_t *raw, size_t length) {
    if (length < 10) return;
    auto context = context_copy(); auto result = decode(raw, length, context, nullptr);
    Lock lock; auto &entry = counts[raw[7]]; entry.state = result.state; entry.fields = result.fields;
    if (!strcmp(result.state, "decoded")) ++entry.decoded;
    if (!strcmp(result.state, "payload_mismatch")) ++entry.failed;
    if (raw[4] != 69) return;
    if (raw[7] == 8 || raw[7] == 53) {
        Reader r{raw+8, length-10}; if (raw[7] == 8) r.take(4);
        uint16_t version = 0; bool found = false;
        if (versions(r, version, found) && found) { latest_version = version; version_seen = true; }
    } else if (raw[7] == 213 && !strcmp(result.state, "decoded")) {
        Reader r{raw+8, length-10}; Schema next;
        if (schema_read(r, next, nullptr)) { next.version = latest_version; next.bound = version_seen; schema = next; }
    }
}
void add_stats(cJSON *item, unsigned type) {
    Counts entry; { Lock lock; entry = counts[type]; }
    if (auto *value = name(type)) cJSON_AddStringToObject(item, "name", value); else cJSON_AddNullToObject(item, "name");
    cJSON_AddStringToObject(item, "name_source", origin(type));
    cJSON_AddStringToObject(item, "decode_state", entry.state);
    cJSON_AddBoolToObject(item, "decoded", !strcmp(entry.state, "decoded"));
    number(item, "decoded_frames", entry.decoded); number(item, "decode_errors", entry.failed); number(item, "field_count", entry.fields);
}
cJSON *describe(const uint8_t *raw, size_t length) {
    auto *root = cJSON_CreateObject(); auto *data = cJSON_CreateObject(); if (!root || !data) { cJSON_Delete(root); cJSON_Delete(data); return nullptr; }
    auto context = context_copy(); auto result = decode(raw, length, context, data);
    cJSON_AddStringToObject(root, "state", result.state);
    number(root, "field_count", result.fields);
    if (!strcmp(result.state, "decoded")) cJSON_AddItemToObject(root, "data", data); else cJSON_Delete(data);
    return root;
}
bool self_test() {
    auto context = context_copy();
    auto menu_test=[&](const uint8_t *payload,size_t size,unsigned expected_fields,unsigned type=214) {
        uint8_t frame[64]{}; size_t length=size+10; frame[0]=0x68; frame[1]=length; frame[3]=86; frame[4]=69; frame[5]=48; frame[6]=5; frame[7]=type;
        memcpy(frame+8,payload,size); frame[length-1]=0x16; for(size_t i=0;i<length-2;++i) frame[length-2]^=frame[i];
        auto result=decode(frame,length,context,nullptr);
        if(strcmp(result.state,"decoded")||result.fields!=expected_fields) return false;
        for(size_t cut=0;cut<size;++cut) {
            uint8_t broken[64]{}; size_t n=cut+10; memcpy(broken,frame,cut+8); broken[1]=n; broken[n-1]=0x16;
            for(size_t i=0;i<n-2;++i) broken[n-2]^=broken[i];
            auto truncated=decode(broken,n,context,nullptr);
            // An empty 217 ACK is a legal alternative to its full schedule reply.
            if(type==217&&cut==0) { if(strcmp(truncated.state,"decoded")||truncated.fields!=0)return false; }
            else if(!strcmp(truncated.state,"decoded")) return false;
        }
        return true;
    };
    const uint8_t config[]={3,3,0xa1,1,100,31,0,1,1,1,0},values[]={4,3,0,109,60,140,1,0,1,45,35,65},versions_data[]={0,0,1,2,3,1,0,4,2,0},menu_ack[]={5,1,47};
    if(!menu_test(config,sizeof(config),3)||!menu_test(values,sizeof(values),3)||!menu_test(versions_data,sizeof(versions_data),2)||!menu_test(menu_ack,sizeof(menu_ack),1)) return false;
    const uint8_t point_value[]={4,1,47,46,35,65};
    if(!menu_test(point_value,sizeof(point_value),1)) return false;
    const uint8_t structure[]={9,6,0,7,1,0,0,7,1,1,47,3,1,6,5};
    if(!menu_test(structure,sizeof(structure),5)) return false;
    const uint8_t locked_structure[]={9,7,0,7,1,0,0,7,1,0x21,47,0x11,48,6,5,7,0};
    if(!menu_test(locked_structure,sizeof(locked_structure),6))return false;
    const uint8_t active_alert[]={1,0,1,8,1,0,0,0,255,255,255,255},empty_alerts[]={0,0,0},bad_alerts[]={0,0,1,8,1,0,0,0,255,255,255,255};
    if(!menu_test(active_alert,sizeof(active_alert),1,189)||!menu_test(empty_alerts,sizeof(empty_alerts),0,189)||menu_test(bad_alerts,sizeof(bad_alerts),1,189))return false;
    uint8_t schedule[50]={16,1,1,1,1,5,0,20}; schedule[8]=128;
    if(!menu_test(schedule,sizeof(schedule),1,182))return false;
    schedule[4]=2; if(menu_test(schedule,sizeof(schedule),1,182))return false; schedule[4]=1;
    schedule[5]=21; if(menu_test(schedule,sizeof(schedule),1,182))return false; schedule[5]=5;
    schedule[3]=40; if(menu_test(schedule,sizeof(schedule),1,182))return false;
    uint8_t schedule_write[46]={1,1,1,5}; schedule_write[4]=128;
    if(!menu_test(schedule_write,sizeof(schedule_write),1,55))return false;
    schedule_write[1]=16; schedule_write[3]=255;
    if(!menu_test(schedule_write,sizeof(schedule_write),1,89))return false;
    schedule[3]=16; schedule[4]=1; schedule[5]=schedule[6]=schedule[7]=255;
    if(!menu_test(schedule,sizeof(schedule),1,216)||!menu_test(schedule,sizeof(schedule),1,217))return false;
    schedule[3]=15; if(menu_test(schedule,sizeof(schedule),1,216))return false; schedule[3]=16;
    const uint8_t captured_menu_ack[]={0x68,0x0d,0x00,0x00,0x50,0xfc,0xb1,0xd6,0x05,0x01,0x2f,0x85,0x16};
    if(strcmp(decode(captured_menu_ack,sizeof(captured_menu_ack),context,nullptr).state,"decoded")) return false;
    struct Fixture { const uint8_t *data; size_t size; unsigned fields; };
    const Fixture fixtures[] = {{protocol_fixtures::frame_213, sizeof(protocol_fixtures::frame_213), 194},
        {protocol_fixtures::frame_8, sizeof(protocol_fixtures::frame_8), 194},
        {protocol_fixtures::frame_177, sizeof(protocol_fixtures::frame_177), 139},
        {protocol_fixtures::frame_178, sizeof(protocol_fixtures::frame_178), 56},
        {protocol_fixtures::frame_212, sizeof(protocol_fixtures::frame_212), 6}};
    for (const auto &fixture : fixtures) {
        auto result = decode(fixture.data, fixture.size, context, nullptr);
        if (strcmp(result.state, "decoded") || result.fields != fixture.fields) return false;
        for (size_t size = 0; size < fixture.size; ++size) if (!strcmp(decode(fixture.data, size, context, nullptr).state, "decoded")) return false;
        // Also exercise payload bounds with a valid header and recomputed checksum.
        uint8_t cut[1000];
        for (size_t payload = 0; payload < fixture.size-10; ++payload) {
            size_t size = payload+10; memcpy(cut, fixture.data, size-2);
            cut[1] = size & 255; cut[2] = size >> 8; cut[size-2] = 0; cut[size-1] = 0x16;
            for (size_t i = 0; i < size-2; ++i) cut[size-2] ^= cut[i];
            if (!strcmp(decode(cut, size, context, nullptr).state, "decoded")) return false;
        }
    }
    auto *root = describe(protocol_fixtures::frame_8, sizeof(protocol_fixtures::frame_8));
    auto *data = cJSON_GetObjectItemCaseSensitive(root, "data"); auto *items = cJSON_GetObjectItemCaseSensitive(data, "fields");
    bool ok = true;
    for (const auto &expected : protocol_fixtures::values) {
        cJSON *match = nullptr, *item = nullptr;
        cJSON_ArrayForEach(item, items) {
            auto *id = cJSON_GetObjectItemCaseSensitive(item, "id"); if (cJSON_IsNumber(id) && id->valueint == expected.id) match = item;
        }
        auto *value = cJSON_GetObjectItemCaseSensitive(match, "value");
        double actual = cJSON_IsBool(value) ? cJSON_IsTrue(value) : cJSON_IsNumber(value) ? value->valuedouble : NAN;
        if (!std::isfinite(actual) || std::fabs(actual-expected.value) > 0.0001) { ok = false; break; }
    }
    cJSON_Delete(root);
    auto mismatch = context; ++mismatch.version;
    if (strcmp(decode(protocol_fixtures::frame_8, sizeof(protocol_fixtures::frame_8), mismatch, nullptr).state, "waiting_schema")) ok = false;
    return ok && plum::thermostat_self_test();
}
}
