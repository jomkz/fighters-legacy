// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Typed, alignment-safe read/write helpers for the wire structs in GameProtocol.h.
//
// GameProtocol structs are naturally aligned (NOT packed) — fields ordered large→small with explicit
// reserved padding, so no implicit compiler padding is inserted. The only correct way to read them
// from a raw network buffer is still std::memcpy: an incoming buffer may not satisfy alignof(T)
// (e.g. an ENet receive pointer is unaligned on ARM64), which would SIGBUS on a direct cast.
// These helpers centralise that rule so server (WorldBroadcaster) and client (ClientNetEventHandler)
// parse/produce packets through one audited path.
//
// readMsg / readRecordAt : bounds-checked memcpy reads (the portable default).
// viewMsg                : zero-copy const T* when buffer size AND alignment permit, else nullptr —
//                          sound only because the wire layout is naturally aligned (size-multiple
//                          records); callers fall back to readMsg when it returns nullptr.
// appendMsg / writeMsgAt : serialisation helpers so packet builders stop hand-managing offsets.
// findExt / readExtValue : TLV extension block scanner/reader — scan for a known tag after the
//                          fixed struct section; skip unknown tags via their len field.
// appendExt / appendExtRaw : TLV extension block builders.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace fl {

// Read a fixed-size wire struct from the front of a buffer. Returns false (out untouched) if the
// buffer is shorter than sizeof(T).
template <class T> [[nodiscard]] bool readMsg(const void* data, std::size_t size, T& out) {
    static_assert(std::is_trivially_copyable_v<T>, "wire type must be trivially copyable");
    if (size < sizeof(T))
        return false;
    std::memcpy(&out, data, sizeof(T));
    return true;
}

// Read a fixed-size record at a byte offset within a buffer (for variable-length message tails:
// MsgEntityTypeDef[] after MsgConnectAck, MsgEntityEntry[] after MsgWorldSnapshotHeader).
// Returns false if the record would read past the end of the buffer.
template <class T> [[nodiscard]] bool readRecordAt(const void* data, std::size_t size, std::size_t offset, T& out) {
    static_assert(std::is_trivially_copyable_v<T>, "wire type must be trivially copyable");
    if (offset > size || size - offset < sizeof(T))
        return false;
    std::memcpy(&out, static_cast<const std::uint8_t*>(data) + offset, sizeof(T));
    return true;
}

// Zero-copy view into the buffer. Returns a const T* only when the buffer is large enough AND the
// pointer satisfies alignof(T); otherwise nullptr so the caller falls back to readMsg. Safe because
// the wire structs are naturally aligned and ENet/std::vector buffer bases are >= max_align_t.
template <class T> [[nodiscard]] const T* viewMsg(const void* data, std::size_t size) {
    static_assert(std::is_trivially_copyable_v<T>, "wire type must be trivially copyable");
    if (size < sizeof(T))
        return nullptr;
    if (reinterpret_cast<std::uintptr_t>(data) % alignof(T) != 0)
        return nullptr;
    return static_cast<const T*>(data);
}

// Append a fixed-size wire struct (header or record) to a byte buffer.
template <class T> void appendMsg(std::vector<std::uint8_t>& buf, const T& msg) {
    static_assert(std::is_trivially_copyable_v<T>, "wire type must be trivially copyable");
    const auto* p = reinterpret_cast<const std::uint8_t*>(&msg);
    buf.insert(buf.end(), p, p + sizeof(T));
}

// Overwrite a previously-reserved header slot at a byte offset (the snapshot/ack builders append a
// placeholder header, fill the records, then patch the final count back in). Caller guarantees the
// buffer already holds sizeof(T) bytes at offset.
template <class T> void writeMsgAt(std::vector<std::uint8_t>& buf, std::size_t offset, const T& msg) {
    static_assert(std::is_trivially_copyable_v<T>, "wire type must be trivially copyable");
    std::memcpy(buf.data() + offset, &msg, sizeof(T));
}

// ------- TLV extension block helpers -------
// Extension blocks are appended after the fixed struct section (or after the record array for array
// messages). Each extension entry: [tag: uint16_t LE][len: uint16_t LE][data: len bytes].
// Senders include any subset of entries. Receivers scan past unknown tags via their len field.
// Old receivers that call only readMsg<T> or iterate records stop at the right byte count and
// naturally ignore trailing extension bytes — no code change required to remain correct.

// Scan an extension block for a specific tag. Returns a pointer to the value data if found, sets
// valueLen to the data length; returns nullptr if the tag is absent or the block is malformed.
// extData: pointer to the start of the extension region (after the fixed struct or record array).
// extSize: number of bytes available in the extension region (packet size minus fixed section size).
inline const std::uint8_t* findExt(const void* extData, std::size_t extSize, std::uint16_t tag,
                                   std::uint16_t& valueLen) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(extData);
    const auto* end = p + extSize;
    while (p + 4 <= end) {
        std::uint16_t t{}, l{};
        std::memcpy(&t, p, 2);
        std::memcpy(&l, p + 2, 2);
        p += 4;
        if (p + l > end)
            break; // malformed: data would overflow the available window
        if (t == tag) {
            valueLen = l;
            return p;
        }
        p += l;
    }
    return nullptr;
}

// Read a trivially-copyable extension value by tag from an extension block. Returns false if the
// tag is absent, or if the stored length does not match sizeof(T).
template <class T>
[[nodiscard]] bool readExtValue(const void* extData, std::size_t extSize, std::uint16_t tag, T& out) {
    static_assert(std::is_trivially_copyable_v<T>, "extension value type must be trivially copyable");
    std::uint16_t valueLen{};
    const std::uint8_t* p = findExt(extData, extSize, tag, valueLen);
    if (!p || valueLen != sizeof(T))
        return false;
    std::memcpy(&out, p, sizeof(T));
    return true;
}

// Append a TLV extension entry [tag:2][sizeof(T):2][value:sizeof(T)] to a byte buffer.
template <class T> void appendExt(std::vector<std::uint8_t>& buf, std::uint16_t tag, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "extension value type must be trivially copyable");
    static_assert(sizeof(T) <= 65535u, "extension value too large for uint16_t length field");
    const auto len = static_cast<std::uint16_t>(sizeof(T));
    const auto sz = buf.size();
    buf.resize(sz + 4 + sizeof(T));
    std::memcpy(buf.data() + sz, &tag, 2);
    std::memcpy(buf.data() + sz + 2, &len, 2);
    std::memcpy(buf.data() + sz + 4, &value, sizeof(T));
}

// Append a raw TLV extension entry [tag:2][len:2][data:len] to a byte buffer.
inline void appendExtRaw(std::vector<std::uint8_t>& buf, std::uint16_t tag, const void* data, std::uint16_t len) {
    const auto sz = buf.size();
    buf.resize(sz + 4 + len);
    std::memcpy(buf.data() + sz, &tag, 2);
    std::memcpy(buf.data() + sz + 2, &len, 2);
    if (len > 0)
        std::memcpy(buf.data() + sz + 4, data, len);
}

} // namespace fl
