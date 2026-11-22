// Fallback interpreter: guest code the ahead-of-time translation does not hold (a function reached only through a
// pointer no analysis saw, code past an analysed function's end). Instead of stopping, the runtime runs it here one
// instruction at a time until it reaches an address that has translated code (an import stub counts: system calls
// keep going through their wrappers), then hands that address back to the dispatcher.
//
// Every address run here is recorded (PSPRECOMP_LEARN_FILE, one hex address per line): tools/pspport passes the
// file to the next translation as extra seeds, so the code runs translated from then on.
//
// The semantics follow tools/codegen_main.cpp instruction for instruction. VFPU instructions are not interpreted:
// the runtime stops at one, as it did before for any untranslated code.
#include "psprecomp/common.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/decoder.hpp"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace psprecomp {

namespace {

constexpr std::uint64_t kSliceInstructions = 1'000'000u;   // then back to the dispatcher (scheduler, heartbeat)

std::uint32_t branch_target(std::uint32_t pc, const DecodedInstruction &d) {
    return pc + 4u + (static_cast<std::uint32_t>(static_cast<std::int32_t>(d.immediate)) << 2u);
}

std::uint32_t jump_target(std::uint32_t pc, const DecodedInstruction &d) {
    return ((pc + 4u) & 0xF0000000u) | (d.target << 2u);
}

bool fpu_compare(std::uint32_t condition, float fs, float ft) {
    const bool unordered = std::isnan(fs) || std::isnan(ft);
    switch (condition & 7u) {
    case 0u: return false;
    case 1u: return unordered;
    case 2u: return !unordered && fs == ft;
    case 3u: return unordered || fs == ft;
    case 4u: return fs < ft;
    case 5u: return unordered || fs < ft;
    case 6u: return fs <= ft;
    default: return unordered || fs <= ft;
    }
}

} // namespace

// Executes one non-control instruction. False: not interpretable (the caller stops the runtime).
bool Runtime::interpret_one(AllegrexContext &ctx, std::uint32_t pc, const DecodedInstruction &d) {
    auto r = [&](std::uint32_t i) { return ctx.gpr[i]; };
    auto set = [&](std::uint32_t i, std::uint32_t v) { if (i != 0u) ctx.gpr[i] = v; };
    const std::uint32_t uimm = static_cast<std::uint16_t>(d.immediate);
    const std::uint32_t simm = static_cast<std::uint32_t>(static_cast<std::int32_t>(d.immediate));
    const std::uint32_t ea = r(d.rs) + simm;
    auto s32 = [](std::uint32_t v) { return static_cast<std::int32_t>(v); };
    switch (d.kind) {
    case OpcodeKind::Nop: case OpcodeKind::Sync: case OpcodeKind::Cache: return true;
    case OpcodeKind::Addiu: set(d.rt, r(d.rs) + simm); return true;
    case OpcodeKind::Slti: set(d.rt, s32(r(d.rs)) < s32(simm) ? 1u : 0u); return true;
    case OpcodeKind::Sltiu: set(d.rt, r(d.rs) < simm ? 1u : 0u); return true;
    case OpcodeKind::Andi: set(d.rt, r(d.rs) & uimm); return true;
    case OpcodeKind::Ori: set(d.rt, r(d.rs) | uimm); return true;
    case OpcodeKind::Xori: set(d.rt, r(d.rs) ^ uimm); return true;
    case OpcodeKind::Lui: set(d.rt, uimm << 16u); return true;
    case OpcodeKind::Add: case OpcodeKind::Addu: set(d.rd, r(d.rs) + r(d.rt)); return true;
    case OpcodeKind::Sub: case OpcodeKind::Subu: set(d.rd, r(d.rs) - r(d.rt)); return true;
    case OpcodeKind::And: set(d.rd, r(d.rs) & r(d.rt)); return true;
    case OpcodeKind::Or: set(d.rd, r(d.rs) | r(d.rt)); return true;
    case OpcodeKind::Xor: set(d.rd, r(d.rs) ^ r(d.rt)); return true;
    case OpcodeKind::Nor: set(d.rd, ~(r(d.rs) | r(d.rt))); return true;
    case OpcodeKind::Slt: set(d.rd, s32(r(d.rs)) < s32(r(d.rt)) ? 1u : 0u); return true;
    case OpcodeKind::Sltu: set(d.rd, r(d.rs) < r(d.rt) ? 1u : 0u); return true;
    case OpcodeKind::Max: set(d.rd, s32(r(d.rs)) > s32(r(d.rt)) ? r(d.rs) : r(d.rt)); return true;
    case OpcodeKind::Min: set(d.rd, s32(r(d.rs)) < s32(r(d.rt)) ? r(d.rs) : r(d.rt)); return true;
    case OpcodeKind::Movz: if (r(d.rt) == 0u) set(d.rd, r(d.rs)); return true;
    case OpcodeKind::Movn: if (r(d.rt) != 0u) set(d.rd, r(d.rs)); return true;
    case OpcodeKind::Sll: set(d.rd, r(d.rt) << d.sa); return true;
    case OpcodeKind::Srl: set(d.rd, r(d.rt) >> d.sa); return true;
    case OpcodeKind::Sra: set(d.rd, static_cast<std::uint32_t>(s32(r(d.rt)) >> d.sa)); return true;
    case OpcodeKind::Rotr: set(d.rd, std::rotr(r(d.rt), static_cast<int>(d.sa))); return true;
    case OpcodeKind::Sllv: set(d.rd, r(d.rt) << (r(d.rs) & 31u)); return true;
    case OpcodeKind::Srlv: set(d.rd, r(d.rt) >> (r(d.rs) & 31u)); return true;
    case OpcodeKind::Srav: set(d.rd, static_cast<std::uint32_t>(s32(r(d.rt)) >> (r(d.rs) & 31u))); return true;
    case OpcodeKind::Rotrv: set(d.rd, std::rotr(r(d.rt), static_cast<int>(r(d.rs) & 31u))); return true;
    case OpcodeKind::Clz: set(d.rd, static_cast<std::uint32_t>(std::countl_zero(r(d.rs)))); return true;
    case OpcodeKind::Clo: set(d.rd, static_cast<std::uint32_t>(std::countl_one(r(d.rs)))); return true;
    case OpcodeKind::Ext: {
        const std::uint32_t size = d.rd + 1u;
        const std::uint32_t mask = size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u);
        set(d.rt, (r(d.rs) >> d.sa) & mask);
        return true;
    }
    case OpcodeKind::Ins: {
        const std::uint32_t size = d.rd >= d.sa ? d.rd - d.sa + 1u : 0u;
        const std::uint32_t mask = size >= 32u ? 0xFFFFFFFFu : (size == 0u ? 0u : ((1u << size) - 1u));
        set(d.rt, (r(d.rt) & ~(mask << d.sa)) | ((r(d.rs) & mask) << d.sa));
        return true;
    }
    case OpcodeKind::Seb: set(d.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(r(d.rt))))); return true;
    case OpcodeKind::Seh: set(d.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(r(d.rt))))); return true;
    case OpcodeKind::Bitrev: {
        std::uint32_t v = r(d.rt);
        v = ((v >> 1u) & 0x55555555u) | ((v & 0x55555555u) << 1u);
        v = ((v >> 2u) & 0x33333333u) | ((v & 0x33333333u) << 2u);
        v = ((v >> 4u) & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4u);
        v = ((v >> 8u) & 0x00FF00FFu) | ((v & 0x00FF00FFu) << 8u);
        set(d.rd, (v >> 16u) | (v << 16u));
        return true;
    }
    case OpcodeKind::Wsbh: set(d.rd, ((r(d.rt) & 0x00FF00FFu) << 8u) | ((r(d.rt) & 0xFF00FF00u) >> 8u)); return true;
    case OpcodeKind::Wsbw: {
        const std::uint32_t v = r(d.rt);
        set(d.rd, (v << 24u) | ((v & 0x0000FF00u) << 8u) | ((v & 0x00FF0000u) >> 8u) | (v >> 24u));
        return true;
    }
    case OpcodeKind::Lw: set(d.rt, memory_.load32(ea)); return true;
    case OpcodeKind::Lwl: set(d.rt, memory_.load_word_left(ea, r(d.rt))); return true;
    case OpcodeKind::Lwr: set(d.rt, memory_.load_word_right(ea, r(d.rt))); return true;
    case OpcodeKind::Lh: set(d.rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(memory_.load16(ea))))); return true;
    case OpcodeKind::Lhu: set(d.rt, memory_.load16(ea)); return true;
    case OpcodeKind::Lb: set(d.rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(memory_.load8(ea))))); return true;
    case OpcodeKind::Lbu: set(d.rt, memory_.load8(ea)); return true;
    case OpcodeKind::Sw: memory_.store32(ea, r(d.rt)); return true;
    case OpcodeKind::Swl: memory_.store_word_left(ea, r(d.rt)); return true;
    case OpcodeKind::Swr: memory_.store_word_right(ea, r(d.rt)); return true;
    case OpcodeKind::Sh: memory_.store16(ea, static_cast<std::uint16_t>(r(d.rt))); return true;
    case OpcodeKind::Sb: memory_.store8(ea, static_cast<std::uint8_t>(r(d.rt))); return true;
    case OpcodeKind::Lwc1: ctx.fpr[d.rt] = std::bit_cast<float>(memory_.load32(ea)); return true;
    case OpcodeKind::Swc1: memory_.store32(ea, std::bit_cast<std::uint32_t>(ctx.fpr[d.rt])); return true;
    case OpcodeKind::Mfhi: set(d.rd, ctx.hi); return true;
    case OpcodeKind::Mflo: set(d.rd, ctx.lo); return true;
    case OpcodeKind::Mthi: ctx.hi = r(d.rs); return true;
    case OpcodeKind::Mtlo: ctx.lo = r(d.rs); return true;
    case OpcodeKind::Mult: {
        const std::int64_t p = static_cast<std::int64_t>(s32(r(d.rs))) * static_cast<std::int64_t>(s32(r(d.rt)));
        ctx.lo = static_cast<std::uint32_t>(p);
        ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(p) >> 32u);
        return true;
    }
    case OpcodeKind::Multu: {
        const std::uint64_t p = static_cast<std::uint64_t>(r(d.rs)) * static_cast<std::uint64_t>(r(d.rt));
        ctx.lo = static_cast<std::uint32_t>(p);
        ctx.hi = static_cast<std::uint32_t>(p >> 32u);
        return true;
    }
    case OpcodeKind::Madd: case OpcodeKind::Maddu: case OpcodeKind::Msub: case OpcodeKind::Msubu: {
        std::uint64_t acc = (static_cast<std::uint64_t>(ctx.hi) << 32u) | ctx.lo;
        const bool sign = d.kind == OpcodeKind::Madd || d.kind == OpcodeKind::Msub;
        const std::uint64_t p = sign
            ? static_cast<std::uint64_t>(static_cast<std::int64_t>(s32(r(d.rs))) * static_cast<std::int64_t>(s32(r(d.rt))))
            : static_cast<std::uint64_t>(r(d.rs)) * static_cast<std::uint64_t>(r(d.rt));
        acc = (d.kind == OpcodeKind::Madd || d.kind == OpcodeKind::Maddu) ? acc + p : acc - p;
        ctx.lo = static_cast<std::uint32_t>(acc);
        ctx.hi = static_cast<std::uint32_t>(acc >> 32u);
        return true;
    }
    case OpcodeKind::Div: {
        const std::int32_t a = s32(r(d.rs)), b = s32(r(d.rt));
        if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = static_cast<std::uint32_t>(a); }
        else if (a == std::numeric_limits<std::int32_t>::min() && b == -1) { ctx.lo = 0x80000000u; ctx.hi = 0u; }
        else { ctx.lo = static_cast<std::uint32_t>(a / b); ctx.hi = static_cast<std::uint32_t>(a % b); }
        return true;
    }
    case OpcodeKind::Divu: {
        const std::uint32_t a = r(d.rs), b = r(d.rt);
        if (b == 0u) { ctx.lo = 0xFFFFFFFFu; ctx.hi = a; }
        else { ctx.lo = a / b; ctx.hi = a % b; }
        return true;
    }
    case OpcodeKind::Mfc1: set(d.rt, std::bit_cast<std::uint32_t>(ctx.fpr[d.rd])); return true;
    case OpcodeKind::Mtc1: ctx.fpr[d.rd] = std::bit_cast<float>(r(d.rt)); return true;
    case OpcodeKind::Cfc1: if (d.rd != 31u) return false; set(d.rt, ctx.fcr31); return true;
    case OpcodeKind::Ctc1: if (d.rd != 31u) return false; ctx.fcr31 = r(d.rt) & 0x0181FFFFu; return true;
    case OpcodeKind::AddS: ctx.fpr[d.sa] = ctx.fpr[d.rd] + ctx.fpr[d.rt]; return true;
    case OpcodeKind::SubS: ctx.fpr[d.sa] = ctx.fpr[d.rd] - ctx.fpr[d.rt]; return true;
    case OpcodeKind::MulS: {
        const float fs = ctx.fpr[d.rd], ft = ctx.fpr[d.rt];
        if ((std::isinf(fs) && ft == 0.0f) || (std::isinf(ft) && fs == 0.0f)) ctx.set_fpr_bits(d.sa, 0x7FC00000u);
        else ctx.fpr[d.sa] = fs * ft;
        return true;
    }
    case OpcodeKind::DivS: ctx.fpr[d.sa] = ctx.fpr[d.rd] / ctx.fpr[d.rt]; return true;
    case OpcodeKind::SqrtS: ctx.fpr[d.sa] = std::sqrt(ctx.fpr[d.rd]); return true;
    case OpcodeKind::AbsS: ctx.set_fpr_bits(d.sa, ctx.fpr_bits(d.rd) & 0x7FFFFFFFu); return true;
    case OpcodeKind::MovS: ctx.set_fpr_bits(d.sa, ctx.fpr_bits(d.rd)); return true;
    case OpcodeKind::NegS: ctx.set_fpr_bits(d.sa, ctx.fpr_bits(d.rd) ^ 0x80000000u); return true;
    case OpcodeKind::RoundWS: ctx.set_fpr_bits(d.sa, ctx.fpu_float_to_word(ctx.fpr[d.rd], 0u)); return true;
    case OpcodeKind::TruncWS: ctx.set_fpr_bits(d.sa, ctx.fpu_float_to_word(ctx.fpr[d.rd], 1u)); return true;
    case OpcodeKind::CeilWS: ctx.set_fpr_bits(d.sa, ctx.fpu_float_to_word(ctx.fpr[d.rd], 2u)); return true;
    case OpcodeKind::FloorWS: ctx.set_fpr_bits(d.sa, ctx.fpu_float_to_word(ctx.fpr[d.rd], 3u)); return true;
    case OpcodeKind::CvtWS: ctx.set_fpr_bits(d.sa, ctx.fpu_float_to_word(ctx.fpr[d.rd], 4u)); return true;
    case OpcodeKind::CvtSW: ctx.fpr[d.sa] = static_cast<float>(static_cast<std::int32_t>(ctx.fpr_bits(d.rd))); return true;
    case OpcodeKind::FpuCompare: ctx.set_fpu_condition(fpu_compare(d.word & 0xFu, ctx.fpr[d.rd], ctx.fpr[d.rt])); return true;
    default:
        (void)pc;
        return false;
    }
}

bool Runtime::interpret_missing(AllegrexContext &ctx) {
    const std::uint32_t entry = ctx.pc;
    if (interpreted_entries_.insert(entry).second) {
        if (const char *learn = std::getenv("PSPRECOMP_LEARN_FILE"); learn != nullptr && *learn != '\0') {
            if (FILE *f = std::fopen(learn, "a")) {
                std::fprintf(f, "0x%08X\n", entry);
                std::fclose(f);
            }
        }
        if (interpreted_entries_.size() <= 32u)
            std::cerr << "[interpreter] untranslated code at " << hex32(entry) << " ra=" << hex32(ctx.gpr[31])
                      << ": running it interpreted (recorded for the next translation)\n";
    }
    std::uint32_t pc = entry;
    for (std::uint64_t executed = 0; executed < kSliceInstructions; ++executed) {
        if (executed != 0u && lookup_function(pc) != nullptr) {
            ctx.pc = pc;
            return true;
        }
        const DecodedInstruction d = decode_allegrex(memory_.load32(pc));
        if (!d.is_control_flow()) {
            if (!interpret_one(ctx, pc, d)) {
                ctx.pc = pc;
                unsupported(pc, d.word, "not interpretable (" + d.mnemonic + ")");
                return false;
            }
            pc += 4u;
            continue;
        }
        // A branch or jump: decide, run the delay slot (unless a likely branch is not taken), then go on.
        bool taken = true, likely = false;
        std::uint32_t target = 0u;
        const auto rs = static_cast<std::int32_t>(ctx.gpr[d.rs]);
        switch (d.kind) {
        case OpcodeKind::Beq: taken = ctx.gpr[d.rs] == ctx.gpr[d.rt]; target = branch_target(pc, d); break;
        case OpcodeKind::Bne: taken = ctx.gpr[d.rs] != ctx.gpr[d.rt]; target = branch_target(pc, d); break;
        case OpcodeKind::Beql: likely = true; taken = ctx.gpr[d.rs] == ctx.gpr[d.rt]; target = branch_target(pc, d); break;
        case OpcodeKind::Bnel: likely = true; taken = ctx.gpr[d.rs] != ctx.gpr[d.rt]; target = branch_target(pc, d); break;
        case OpcodeKind::Blez: taken = rs <= 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bgtz: taken = rs > 0; target = branch_target(pc, d); break;
        case OpcodeKind::Blezl: likely = true; taken = rs <= 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bgtzl: likely = true; taken = rs > 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bltz: case OpcodeKind::Bltzal: taken = rs < 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bgez: case OpcodeKind::Bgezal: taken = rs >= 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bltzl: case OpcodeKind::Bltzall: likely = true; taken = rs < 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bgezl: case OpcodeKind::Bgezall: likely = true; taken = rs >= 0; target = branch_target(pc, d); break;
        case OpcodeKind::Bc1f: taken = !ctx.fpu_condition(); target = branch_target(pc, d); break;
        case OpcodeKind::Bc1t: taken = ctx.fpu_condition(); target = branch_target(pc, d); break;
        case OpcodeKind::Bc1fl: likely = true; taken = !ctx.fpu_condition(); target = branch_target(pc, d); break;
        case OpcodeKind::Bc1tl: likely = true; taken = ctx.fpu_condition(); target = branch_target(pc, d); break;
        case OpcodeKind::J: case OpcodeKind::Jal: target = jump_target(pc, d); break;
        case OpcodeKind::Jr: case OpcodeKind::Jalr: target = ctx.gpr[d.rs]; break;
        default:
            ctx.pc = pc;
            unsupported(pc, d.word, "not interpretable (" + d.mnemonic + ")");
            return false;
        }
        const bool links = d.kind == OpcodeKind::Jal || d.kind == OpcodeKind::Bltzal || d.kind == OpcodeKind::Bgezal ||
                           d.kind == OpcodeKind::Bltzall || d.kind == OpcodeKind::Bgezall;
        if (links) ctx.gpr[31] = pc + 8u;
        if (d.kind == OpcodeKind::Jalr && d.rd != 0u) ctx.gpr[d.rd] = pc + 8u;
        if (!(likely && !taken)) {
            const DecodedInstruction slot = decode_allegrex(memory_.load32(pc + 4u));
            if (slot.is_control_flow() || !interpret_one(ctx, pc + 4u, slot)) {
                ctx.pc = pc + 4u;
                unsupported(pc + 4u, slot.word, "not interpretable in a delay slot (" + slot.mnemonic + ")");
                return false;
            }
        }
        pc = taken ? target : pc + 8u;
    }
    ctx.pc = pc;   // a long stretch: let the dispatcher run the scheduler, then come back here
    return true;
}

} // namespace psprecomp
