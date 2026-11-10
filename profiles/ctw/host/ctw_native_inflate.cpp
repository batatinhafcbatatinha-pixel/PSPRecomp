// Native port of the hand-written inflate_fast in Chinatown Wars (guest 0x08AE8B88, zlib 1.2.3
// structure layout).
//
// The game inflates streamed map sections on its main thread. The guest routine is MIPS assembly
// built from trapping add/sub, which the recompiler has to translate into overflow-checked calls;
// a section took 150-400 ms and showed up as a stutter with the area missing on screen. This is a
// literal transcription of that routine (same refill points, same window arithmetic, same exit
// state), not of stock zlib: it has no input bound, no distance check and treats every
// "op & 0x40" as end of block, and those behaviours are kept.
//
// PSPRECOMP_NO_NATIVE_INFLATE=1 turns it off (the generated code then runs as before).
#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace psprecomp {
bool (*g_native_hook_08AE8B88)(Runtime &, AllegrexContext &) = nullptr;
} // namespace psprecomp

namespace vcs {
namespace {
constexpr std::uint32_t kLow = 0x08000000u; // whole user RAM: inputs can sit in volatile memory (0x084xxxxx-0x087xxxxx)
constexpr std::uint32_t kSize = 0x02000000u;

struct Ram {
    std::uint8_t *base;
    [[nodiscard]] static std::uint32_t offset(std::uint32_t address) noexcept {
        return (address & 0x0FFFFFFFu) - kLow;
    }
    [[nodiscard]] bool ok(std::uint32_t address, std::uint32_t length) const noexcept {
        const std::uint32_t o = offset(address);
        return o < kSize && kSize - o >= length;
    }
    [[nodiscard]] std::uint8_t r8(std::uint32_t address) const noexcept {
        const std::uint32_t o = offset(address);
        return o < kSize ? base[o] : std::uint8_t{0};
    }
    void w8(std::uint32_t address, std::uint8_t value) const noexcept {
        const std::uint32_t o = offset(address);
        if (o < kSize) base[o] = value;
    }
    [[nodiscard]] std::uint32_t r32(std::uint32_t address) const noexcept {
        const std::uint32_t o = offset(address);
        if (o >= kSize - 3u) return 0u;
        std::uint32_t value;
        std::memcpy(&value, base + o, 4u);
        return value;
    }
    void w32(std::uint32_t address, std::uint32_t value) const noexcept {
        const std::uint32_t o = offset(address);
        if (o < kSize - 3u) std::memcpy(base + o, &value, 4u);
    }
};

[[nodiscard]] constexpr std::uint32_t low_mask(std::uint32_t bits) noexcept {
    return ~(0xFFFFFFFFu << (bits & 31u)); // sllv uses the low five bits of the count
}

[[nodiscard]] constexpr bool less(std::uint32_t a, std::uint32_t b) noexcept {
    return static_cast<std::int32_t>(a) < static_cast<std::int32_t>(b); // slt
}

bool native_inflate_core(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx) {
    std::uint8_t *base = runtime.memory().raw_pointer(kLow, kSize);
    static const bool announced = [&] {
        std::fputs(base != nullptr ? "[inflate] native inflate_fast active" : "[inflate] native inflate_fast unavailable (RAM window)", stderr);
        std::fputc(10, stderr);
        return true;
    }();
    (void)announced;
    if (base == nullptr) return false;
    const Ram ram{base};
    const std::uint32_t strm = ctx.gpr[4];
    const std::uint32_t start = ctx.gpr[5];
    if (!ram.ok(strm, 0x20u)) return false;
    const std::uint32_t state = ram.r32(strm + 0x1Cu);
    if (!ram.ok(state, 0x5Cu)) return false;

    std::uint32_t out = ram.r32(strm + 0x0Cu);
    const std::uint32_t avail_out = ram.r32(strm + 0x10u);
    const std::uint32_t end = out + avail_out - 0x101u;
    const std::uint32_t beg = out - (start - avail_out);
    std::uint32_t in = ram.r32(strm + 0x00u);
    std::uint32_t bits = ram.r32(state + 0x3Cu);
    std::uint32_t hold = ram.r32(state + 0x38u);
    const std::uint32_t lencode = ram.r32(state + 0x4Cu);
    const std::uint32_t distcode = ram.r32(state + 0x50u);
    const std::uint32_t lmask = low_mask(ram.r32(state + 0x54u));
    const std::uint32_t dmask = low_mask(ram.r32(state + 0x58u));
    const std::uint32_t wsize = ram.r32(state + 0x28u);
    const std::uint32_t write = ram.r32(state + 0x30u);
    const std::uint32_t window = ram.r32(state + 0x34u);

    const auto refill16 = [&] {
        const std::uint32_t pair = ram.r8(in) | (std::uint32_t{ram.r8(in + 1u)} << 8u);
        in += 2u;
        hold += pair << (bits & 31u);
        bits += 16u;
    };
    const auto copy = [&](std::uint32_t &from, std::uint32_t count) {
        for (; count != 0u; --count) ram.w8(out++, ram.r8(from++));
    };

    bool end_of_block = false;
    while (static_cast<std::int32_t>(end - out) > 0) {
        if (less(bits, 15u)) refill16();
        std::uint32_t here = ram.r32(lencode + ((hold & lmask) << 2u));
        for (;;) { // dolen
            const std::uint32_t code_bits = (here >> 8u) & 0xFFu;
            hold >>= (code_bits & 31u);
            bits -= code_bits;
            const std::uint32_t op = here & 0xFFu;
            if (op == 0u) {
                ram.w8(out++, static_cast<std::uint8_t>(here >> 16u));
                break;
            }
            if ((op & 0x10u) == 0u) {
                if ((op & 0x40u) != 0u) {
                    end_of_block = true;
                    break;
                }
                here = ram.r32(lencode + (((here >> 16u) + (hold & low_mask(op))) << 2u));
                continue;
            }
            std::uint32_t length = here >> 16u;
            std::uint32_t extra = here & 0xFu;
            if (less(bits, extra)) {
                hold |= std::uint32_t{ram.r8(in++)} << (bits & 31u);
                bits += 8u;
            }
            length += hold & low_mask(extra);
            hold >>= extra;
            bits -= extra;
            if (less(bits, 15u)) refill16();
            here = ram.r32(distcode + ((hold & dmask) << 2u));
            std::uint32_t distance;
            for (;;) { // dodist
                const std::uint32_t dist_bits = (here >> 8u) & 0xFFu;
                hold >>= (dist_bits & 31u);
                bits -= dist_bits;
                const std::uint32_t dist_op = here & 0xFFu;
                distance = here >> 16u;
                if ((dist_op & 0x10u) != 0u) {
                    extra = here & 0xFu;
                    break;
                }
                here = ram.r32(distcode + ((distance + (hold & low_mask(dist_op))) << 2u));
            }
            if (less(bits, extra)) refill16();
            distance += hold & low_mask(extra);
            hold >>= extra;
            bits -= extra;

            std::uint32_t from;
            const std::uint32_t beyond = distance - (out - beg);
            if (static_cast<std::int32_t>(beyond) <= 0) {
                from = out - distance;
            } else if (write == 0u || less(write, beyond)) {
                // Guest 0x08AE8D54, transcribed as written there.
                from = window + (write == 0u ? wsize : write) - beyond;
                if (less(beyond, length)) {
                    length -= beyond;
                    copy(from, beyond);
                    from = out - distance;
                }
            } else {
                // Guest 0x08AE8D90.
                from = window + write - beyond + wsize;
                if (less(beyond, length)) {
                    length -= beyond;
                    copy(from, beyond);
                    from = window;
                    if (less(write, length)) {
                        length -= write;
                        copy(from, write);
                        from = out - distance;
                    }
                }
            }
            if (static_cast<std::int32_t>(length) > 0) copy(from, length);
            break;
        }
        if (end_of_block) break;
    }
    if (end_of_block) ram.w32(state + 0x00u, 11u); // TYPE

    const std::uint32_t unused = bits >> 3u;
    in -= unused;
    bits -= unused << 3u;
    hold &= low_mask(bits);
    ram.w32(state + 0x3Cu, bits);
    ram.w32(state + 0x38u, hold);
    ram.w32(strm + 0x04u, ram.r32(strm + 0x00u) - in + ram.r32(strm + 0x04u));
    ram.w32(strm + 0x10u, ram.r32(strm + 0x0Cu) - out + ram.r32(strm + 0x10u));
    ram.w32(strm + 0x00u, in);
    ram.w32(strm + 0x0Cu, out);
    ctx.set_gpr(4, strm);
    // PSPRECOMP_INFLATE_DIAG=1: running totals, printed every 256 calls.
    static const bool diag = std::getenv("PSPRECOMP_INFLATE_DIAG") != nullptr;
    if (diag) {
        static std::uint64_t calls = 0u, bytes = 0u;
        bytes += out - (beg + (start - avail_out));
        if ((++calls & 255u) == 0u)
            std::fprintf(stderr, "[inflate] calls=%llu out_bytes=%llu\n",
                         static_cast<unsigned long long>(calls), static_cast<unsigned long long>(bytes));
    }
    return true;
}
// PSPRECOMP_VALIDATE_NATIVE_INFLATE=1: run the native routine, undo it, run the generated code for the
// same call and compare stream, state and output. The generated result is the one kept; the first
// eight divergences are logged.
bool native_inflate_fast(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx) {
    static const bool disabled = std::getenv("PSPRECOMP_NO_NATIVE_INFLATE") != nullptr;
    static const bool validate = std::getenv("PSPRECOMP_VALIDATE_NATIVE_INFLATE") != nullptr;
    static bool in_reference = false;
    if (disabled || in_reference) return false;
    if (!validate) return native_inflate_core(runtime, ctx);

    std::uint8_t *base = runtime.memory().raw_pointer(kLow, kSize);
    if (base == nullptr) return false;
    const Ram ram{base};
    const std::uint32_t strm = ctx.gpr[4];
    if (!ram.ok(strm, 0x20u)) return false;
    const std::uint32_t state = ram.r32(strm + 0x1Cu);
    const std::uint32_t out = ram.r32(strm + 0x0Cu);
    const std::uint32_t avail_out = ram.r32(strm + 0x10u);
    if (!ram.ok(state, 0x5Cu) || avail_out > 0x00400000u || !ram.ok(out, avail_out)) return false;
    std::uint8_t *strm_p = base + Ram::offset(strm), *state_p = base + Ram::offset(state),
                 *out_p = base + Ram::offset(out);
    static std::vector<std::uint8_t> saved, native;
    const std::size_t total = 0x20u + 0x5Cu + avail_out;
    saved.resize(total);
    native.resize(total);
    const auto snapshot = [&](std::vector<std::uint8_t> &to) {
        std::memcpy(to.data(), strm_p, 0x20u);
        std::memcpy(to.data() + 0x20u, state_p, 0x5Cu);
        std::memcpy(to.data() + 0x7Cu, out_p, avail_out);
    };
    snapshot(saved);
    psprecomp::AllegrexContext native_ctx = ctx;
    if (!native_inflate_core(runtime, native_ctx)) return false;
    snapshot(native);
    std::memcpy(strm_p, saved.data(), 0x20u);
    std::memcpy(state_p, saved.data() + 0x20u, 0x5Cu);
    std::memcpy(out_p, saved.data() + 0x7Cu, avail_out);

    psprecomp::AllegrexContext reference = ctx;
    reference.set_gpr(31, 0x00000004u);
    in_reference = true;
    // No preemption inside the reference run: a time slice ending in its loop would return half way.
    const std::uint64_t saved_interval = psprecomp::g_runtime_starvation_interval_fast;
    psprecomp::g_runtime_starvation_interval_fast = 0u;
    const bool ran = runtime.invoke_isolated_aot(0x08AE8B88u, reference);
    psprecomp::g_runtime_starvation_interval_fast = saved_interval;
    in_reference = false;
    if (!ran) return false;

    static std::uint64_t calls = 0u, mismatches = 0u;
    ++calls;
    const bool same = std::memcmp(native.data(), strm_p, 0x20u) == 0 &&
                      std::memcmp(native.data() + 0x20u, state_p, 0x5Cu) == 0 &&
                      std::memcmp(native.data() + 0x7Cu, out_p, avail_out) == 0;
    if (!same && mismatches++ < 8u) {
        std::size_t first = 0u;
        while (first < avail_out && native[0x7Cu + first] == out_p[first]) ++first;
        std::uint32_t n_in, n_out, n_mode, n_bits;
        std::memcpy(&n_in, native.data() + 0x00u, 4u);
        std::memcpy(&n_out, native.data() + 0x0Cu, 4u);
        std::memcpy(&n_mode, native.data() + 0x20u, 4u);
        std::memcpy(&n_bits, native.data() + 0x20u + 0x3Cu, 4u);
        std::fprintf(stderr,
                     "[inflate-validate] MISMATCH call=%llu avail_out=%u first_out_diff=%zu native(in=%08X out=%08X mode=%u bits=%u) "
                     "aot(in=%08X out=%08X mode=%u bits=%u) write=%u wsize=%u\n",
                     static_cast<unsigned long long>(calls), avail_out, first, n_in, n_out, n_mode, n_bits,
                     ram.r32(strm), ram.r32(strm + 0x0Cu), ram.r32(state), ram.r32(state + 0x3Cu),
                     ram.r32(state + 0x30u), ram.r32(state + 0x28u));
    } else if (same && (calls & 1023u) == 0u) {
        std::fprintf(stderr, "[inflate-validate] %llu calls checked, %llu mismatches\n",
                     static_cast<unsigned long long>(calls), static_cast<unsigned long long>(mismatches));
    }
    ctx.set_gpr(4, strm);
    return true;
}
} // namespace

void install_ctw_native_inflate() { psprecomp::g_native_hook_08AE8B88 = &native_inflate_fast; }
} // namespace vcs
