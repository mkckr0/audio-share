#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Lock-free single producer / single consumer queue of records, so the capture callback never blocks.
class spsc_byte_queue {
public:
    explicit spsc_byte_queue(size_t capacity_pow2)
        : _buf(capacity_pow2)
        , _mask(capacity_pow2 - 1)
    {
    }

    bool push(const uint8_t* data, size_t size)
    {
        const size_t tail = _tail.load(std::memory_order_relaxed);
        const size_t head = _head.load(std::memory_order_acquire);
        const size_t need = sizeof(uint32_t) + size;
        if (_buf.size() - (tail - head) < need) {
            return false;
        }
        const uint32_t len = (uint32_t)size;
        write(tail, (const uint8_t*)&len, sizeof(len));
        write(tail + sizeof(len), data, size);
        _tail.store(tail + need, std::memory_order_release);
        return true;
    }

    bool pop(std::vector<uint8_t>& out)
    {
        const size_t head = _head.load(std::memory_order_relaxed);
        if (head == _tail.load(std::memory_order_acquire)) {
            return false;
        }
        uint32_t len = 0;
        read(head, (uint8_t*)&len, sizeof(len));
        out.resize(len);
        read(head + sizeof(len), out.data(), len);
        _head.store(head + sizeof(len) + len, std::memory_order_release);
        return true;
    }

    // not safe while a producer or consumer is running
    void clear()
    {
        _head.store(0);
        _tail.store(0);
    }

private:
    void write(size_t pos, const uint8_t* src, size_t n)
    {
        const size_t i = pos & _mask;
        const size_t first = std::min(n, _buf.size() - i);
        std::memcpy(&_buf[i], src, first);
        std::memcpy(&_buf[0], src + first, n - first);
    }

    void read(size_t pos, uint8_t* dst, size_t n) const
    {
        const size_t i = pos & _mask;
        const size_t first = std::min(n, _buf.size() - i);
        std::memcpy(dst, &_buf[i], first);
        std::memcpy(dst + first, &_buf[0], n - first);
    }

    std::vector<uint8_t> _buf;
    size_t _mask;
    std::atomic<size_t> _head { 0 };
    std::atomic<size_t> _tail { 0 };
};

#endif // !SPSC_QUEUE_HPP
