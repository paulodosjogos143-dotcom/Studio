// ROXBOX - small linear arena allocator (used for per-frame mesh staging buffers)
// Keeps allocations off the heap during hot meshing: 60 FPS on weak hardware.
#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
#include <cstdint>

namespace rb {

class Arena {
public:
    Arena() { reset(1 << 20); } // default 1 MB
    explicit Arena(size_t initialCap) { reset(initialCap); }

    void reset(size_t cap) {
        m_buf.resize(cap);
        m_top = 0;
    }

    void* alloc(size_t size, size_t align = 4) {
        size_t off = (m_top + align - 1) & ~(align - 1);
        if (off + size > m_buf.size()) return nullptr;
        void* p = m_buf.data() + off;
        m_top = off + size;
        return p;
    }

    template <typename T> T* allocArray(size_t count, size_t align = 4) {
        return static_cast<T*>(alloc(count * sizeof(T), align));
    }

    void resetTo(size_t top) { m_top = top; }
    size_t top() const { return m_top; }
    size_t capacity() const { return m_buf.size(); }

private:
    std::vector<uint8_t> m_buf;
    size_t m_top = 0;
};

// simple bump allocator with reset for mesh staging (no free)
template <size_t Block>
class StackAlloc {
public:
    char* base() { return m_base; }
    size_t top() const { return m_top; }
    void reset() { m_top = 0; }
    void* alloc(size_t size, size_t align = 4) {
        size_t off = (m_top + align - 1) & ~(align - 1);
        if (off + size > Block) return nullptr;
        m_top = off + size;
        return m_base + off;
    }
private:
    alignas(16) char m_base[Block];
    size_t m_top = 0;
};

} // namespace rb
