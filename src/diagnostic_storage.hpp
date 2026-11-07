#pragma once
#include <cstddef>
#include <cstdint>

namespace diagnostic_storage {
enum class Pool { external, internal };
struct Record { uint64_t seq, ms; uint16_t length; bool validated; uint8_t data[1000]; };
struct Log { uint64_t seq, ms; char text[160]; };
struct Traffic { uint64_t count, ms; };
struct Counters { Traffic types[256], devices[256]; };

// Allocated once before the receiver starts. Diagnostic history is expendable:
// allocation failure must not stop delivery of frames to the boiler decoder.
struct Storage {
    static constexpr size_t history_capacity = 2048, fallback_capacity = 48, log_capacity = 48;
    Record emergency_record{};
    Log emergency_log{};
    Record *records = &emergency_record;
    size_t capacity = 1;
    Log *entries = &emergency_log;
    size_t entry_capacity = 1;
    Counters *counters = nullptr;
    bool initialized = false;

    Storage() = default;
    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    template<class Calloc> void init(Calloc allocate) {
        if (initialized) return;
        initialized = true;
        if (auto *p = allocate(history_capacity, sizeof(Record), Pool::external)) {
            records = static_cast<Record *>(p); capacity = history_capacity;
        } else if (auto *p = allocate(fallback_capacity, sizeof(Record), Pool::internal)) {
            records = static_cast<Record *>(p); capacity = fallback_capacity;
        }
        auto *logs = allocate(log_capacity, sizeof(Log), Pool::external);
        if (!logs) logs = allocate(log_capacity, sizeof(Log), Pool::internal);
        if (logs) { entries = static_cast<Log *>(logs); entry_capacity = log_capacity; }
        auto *stats = allocate(1, sizeof(Counters), Pool::external);
        if (!stats) stats = allocate(1, sizeof(Counters), Pool::internal);
        counters = static_cast<Counters *>(stats);
    }
};
}
