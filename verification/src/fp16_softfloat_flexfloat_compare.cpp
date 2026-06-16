#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "softfloat/softfloat.h"
#include <flexfloat.hpp>

enum class Op {
    Add,
    Sub,
    Mul,
    Div
};

static uint32_t host_float_to_bits(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_to_host_float(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool parse_u16_hex(const std::string& token, uint16_t& out)
{
    unsigned long value = 0;
    std::stringstream ss(token);
    ss >> std::hex >> value;
    if (ss.fail() || !ss.eof() || value > 0xFFFFul) {
        return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
}

static std::string op_name(Op op)
{
    switch (op) {
        case Op::Add: return "add";
        case Op::Sub: return "sub";
        case Op::Mul: return "mul";
        case Op::Div: return "div";
    }
    return "unknown";
}

static float16_t softfloat_exec(Op op, float16_t a, float16_t b)
{
    switch (op) {
        case Op::Add: return f16_add(a, b);
        case Op::Sub: return f16_sub(a, b);
        case Op::Mul: return f16_mul(a, b);
        case Op::Div: return f16_div(a, b);
    }
    return a;
}

static flexfloat<5, 10> flexfloat_exec(Op op, const flexfloat<5, 10>& a, const flexfloat<5, 10>& b)
{
    switch (op) {
        case Op::Add: return a + b;
        case Op::Sub: return a - b;
        case Op::Mul: return a * b;
        case Op::Div: return a / b;
    }
    return a;
}

static flexfloat<5, 10> from_fp16_bits(uint16_t bits)
{
    float16_t h{bits};
    float32_t f32 = f16_to_f32(h);
    return flexfloat<5, 10>(bits_to_host_float(f32.v));
}

static uint16_t to_fp16_bits(const flexfloat<5, 10>& value)
{
    flexfloat_t raw = static_cast<flexfloat_t>(value);
    return static_cast<uint16_t>(flexfloat_get_bits(&raw) & 0xFFFFu);
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <add|sub|mul|div> <input-file>\n";
        return 1;
    }

    Op op;
    const std::string op_arg = argv[1];
    if (op_arg == "add") op = Op::Add;
    else if (op_arg == "sub") op = Op::Sub;
    else if (op_arg == "mul") op = Op::Mul;
    else if (op_arg == "div") op = Op::Div;
    else {
        std::cerr << "unknown operation: " << op_arg << "\n";
        return 1;
    }

    std::ifstream in(argv[2]);
    if (!in.is_open()) {
        std::cerr << "failed to open input file: " << argv[2] << "\n";
        return 1;
    }

    std::fesetround(FE_TONEAREST);
    std::feclearexcept(FE_ALL_EXCEPT);
    softfloat_roundingMode = softfloat_round_near_even;
    softfloat_detectTininess = softfloat_tininess_afterRounding;

    std::string line;
    std::size_t line_no = 0;
    std::size_t total = 0;
    std::size_t mismatches = 0;

    while (std::getline(in, line)) {
        ++line_no;
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }
        std::stringstream ss(line);
        std::string a_tok;
        std::string b_tok;
        if (!(ss >> a_tok >> b_tok)) {
            continue;
        }

        uint16_t a_bits = 0;
        uint16_t b_bits = 0;
        if (!parse_u16_hex(a_tok, a_bits) || !parse_u16_hex(b_tok, b_bits)) {
            std::cerr << "parse error on line " << line_no << "\n";
            return 1;
        }

        ++total;

        const float16_t a_soft{a_bits};
        const float16_t b_soft{b_bits};
        const float16_t soft_result = softfloat_exec(op, a_soft, b_soft);
        const uint16_t soft_bits = soft_result.v;
        const uint_fast8_t soft_flags = softfloat_exceptionFlags;
        softfloat_exceptionFlags = 0;

        const flexfloat<5, 10> a_ff = from_fp16_bits(a_bits);
        const flexfloat<5, 10> b_ff = from_fp16_bits(b_bits);
        const flexfloat<5, 10> ff_result = flexfloat_exec(op, a_ff, b_ff);
        const uint16_t ff_bits = to_fp16_bits(ff_result);

        if (soft_bits != ff_bits) {
            ++mismatches;
            std::cout << op_name(op)
                      << " a=0x" << std::hex << std::setw(4) << std::setfill('0') << a_bits
                      << " b=0x" << std::setw(4) << b_bits
                      << " soft=0x" << std::setw(4) << soft_bits
                      << " flex=0x" << std::setw(4) << ff_bits
                      << " flags=0x" << static_cast<unsigned>(soft_flags)
                      << std::dec
                      << '\n';
        }
    }

    std::cout << "operation=" << op_name(op)
              << " total=" << total
              << " mismatches=" << mismatches
              << '\n';

    return mismatches == 0 ? 0 : 2;
}
