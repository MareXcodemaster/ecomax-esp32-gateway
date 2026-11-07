#include "diagnostic_storage.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace diagnostic_storage;

void diagnostic_storage_tests() {
    static_assert(sizeof(Storage) < 2048, "Do not permanently reserve large fallback buffers in internal RAM");
    static_assert(sizeof(Record) == 1024);
    // Fail each external/internal allocation independently, including complete
    // exhaustion. The fallback must remain writable and zero-initialized.
    for (unsigned failures = 0; failures < 64; ++failures) {
        struct Block { unsigned char *p; size_t bytes; };
        std::vector<Block> blocks;
        size_t internal_bytes = 0;
        auto allocate = [&](size_t count, size_t size, Pool pool) -> void * {
            unsigned kind = size == sizeof(Record) ? 0 : size == sizeof(Log) ? 1 : 2;
            unsigned bit = kind * 2 + (pool == Pool::internal ? 1 : 0);
            if (failures & (1U << bit)) return nullptr;
            size_t bytes = count * size;
            auto *p = static_cast<unsigned char *>(std::calloc(1, bytes + 16)); assert(p);
            std::memset(p + bytes, 0xA5, 16); blocks.push_back({p, bytes});
            if (pool == Pool::internal) internal_bytes += bytes;
            return p;
        };
        Storage s;
        s.init(allocate);
        size_t expected_capacity = !(failures & 1) ? 2048 : !(failures & 2) ? 48 : 1;
        assert(s.capacity == expected_capacity);
        assert(s.entry_capacity == ((failures & 12) == 12 ? 1 : 48));
        assert((s.counters != nullptr) == ((failures & 48) != 48));
        if (!(failures & 21)) assert(internal_bytes == 0);
        for (size_t i = 0; i < s.capacity; ++i) {
            assert(s.records[i].seq == 0 && s.records[i].length == 0);
            s.records[i].seq = i + 1; s.records[i].data[999] = 0x68;
        }
        for (size_t i = 0; i < s.entry_capacity; ++i) {
            assert(s.entries[i].seq == 0 && s.entries[i].text[0] == 0);
            s.entries[i].seq = i + 1; s.entries[i].text[159] = 'X';
        }
        if (s.counters) {
            assert(s.counters->types[255].count == 0 && s.counters->devices[255].ms == 0);
            s.counters->types[255].count = 7; s.counters->devices[255].ms = 11;
        }
        auto *records = s.records; auto before = blocks.size();
        s.init(allocate); // Accidental second init must not leak or reset history.
        assert(blocks.size() == before && s.records == records && s.records[0].seq == 1);
        for (auto block : blocks) {
            for (size_t i = 0; i < 16; ++i) assert(block.p[block.bytes + i] == 0xA5);
            std::free(block.p);
        }
    }
    puts("PASS diagnostic storage: 64 allocation-failure cases, no internal buffers with PSRAM, bounded emergency storage");
}
