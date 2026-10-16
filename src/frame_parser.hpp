#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Only framing, not boiler decoding. Noise and every address remain visible in raw capture.
class FrameParser {
public:
    using Callback = void (*)(const uint8_t *, size_t, void *);
    uint64_t valid = 0, invalid = 0, noise = 0;
    void feed(const uint8_t *bytes, size_t length, uint64_t now_ms, Callback cb, void *ctx) {
        expire(now_ms);
        for (size_t i = 0; i < length; ++i) {
            if (!used_ && bytes[i] != 0x68) { ++noise; continue; }
            if (used_ == sizeof(buffer_)) { ++invalid; consume(1); }
            buffer_[used_++] = bytes[i];
            last_ms_ = now_ms;
            process(cb, ctx);
        }
    }
    void expire(uint64_t now_ms) {
        if (used_ && now_ms - last_ms_ > 200) { ++invalid; used_ = 0; }
    }
    void reset() { used_ = 0; }
    bool partial() const { return used_ != 0; }
private:
    uint8_t buffer_[1000]{};
    size_t used_ = 0;
    uint64_t last_ms_ = 0;
    void consume(size_t n) {
        used_ -= n;
        if (used_) std::memmove(buffer_, buffer_ + n, used_);
    }
    void process(Callback cb, void *ctx) {
        while (used_) {
            if (buffer_[0] != 0x68) { ++noise; consume(1); continue; }
            if (used_ < 3) return;
            size_t length = buffer_[1] | (static_cast<size_t>(buffer_[2]) << 8);
            if (length < 10 || length > sizeof(buffer_)) { ++invalid; consume(1); continue; }
            if (used_ < length) return;
            uint8_t bcc = 0;
            for (size_t i = 0; i < length - 2; ++i) bcc ^= buffer_[i];
            if (buffer_[length - 1] != 0x16 || bcc != buffer_[length - 2]) {
                ++invalid; consume(1); continue;
            }
            ++valid;
            if (cb) cb(buffer_, length, ctx);
            consume(length);
        }
    }
};

