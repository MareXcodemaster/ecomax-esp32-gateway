#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <type_traits>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

// Optional scratch data must not consume the network/RTOS internal reserve
// when PSRAM is exhausted. Callers propagate failure as a diagnostic 503.
template<class T> class DiagnosticBuffer {
    static_assert(std::is_trivially_destructible<T>::value,"Diagnostic buffers hold plain data only");
    T *p_=nullptr;size_t capacity_=0,used_=0;
public:
    DiagnosticBuffer()=default;
    explicit DiagnosticBuffer(size_t count){if(reserve(count))used_=count;}
    ~DiagnosticBuffer(){std::free(p_);}
    DiagnosticBuffer(const DiagnosticBuffer&)=delete;
    DiagnosticBuffer& operator=(const DiagnosticBuffer&)=delete;
    bool reserve(size_t count){
        if(count>SIZE_MAX/sizeof(T))return false;
        if(capacity_>=count)return true;
#ifdef ESP_PLATFORM
        auto caps=MALLOC_CAP_8BIT|(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)?MALLOC_CAP_SPIRAM:MALLOC_CAP_INTERNAL);
        auto *next=static_cast<T*>(heap_caps_calloc(count,sizeof(T),caps));
#else
        auto *next=static_cast<T*>(std::calloc(count,sizeof(T)));
#endif
        if(!next)return false;
        for(size_t i=0;i<count;++i)new(next+i)T{};
        for(size_t i=0;i<used_;++i)next[i]=p_[i];
        std::free(p_);p_=next;capacity_=count;return true;
    }
    explicit operator bool()const{return p_!=nullptr;}
    void push_back(const T &value){if(used_<capacity_)p_[used_++]=value;}
    T *data(){return p_;}const T *data()const{return p_;}
    T &operator[](size_t i){return p_[i];}const T &operator[](size_t i)const{return p_[i];}
    T &back(){return p_[used_-1];}bool empty()const{return !used_;}size_t size()const{return used_;}
    T *begin(){return p_;}T *end(){return used_?p_+used_:p_;}
};
