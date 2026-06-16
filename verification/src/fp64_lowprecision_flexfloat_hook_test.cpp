#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "adele/adf/LowPrecisionSimulation/typeConvertion.h"

enum class Format {
    BF16,
    E5M2,
    E4M3
};

static bool parse_u64_hex(const std::string& token, uint64_t& out)
{
    unsigned long long value = 0;
    std::stringstream ss(token);
    ss >> std::hex >> value;
    if (ss.fail() || !ss.eof()) {
        return false;
    }
    out = static_cast<uint64_t>(value);
    return true;
}

static double bits_to_host_double(uint64_t bits)
{
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t host_double_to_bits(double value)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

template<uint32_t E, uint32_t M>
static uint64_t direct_flexfloat_model(uint64_t fp64_bits)
{
    const double fp64_value = bits_to_host_double(fp64_bits);
    const flexfloat<E, M> lowprecision_value = fp64_value;
    return host_double_to_bits(static_cast<double>(lowprecision_value));
}

static uint64_t direct_model(Format format, uint64_t fp64_bits)
{
    switch (format) {
        case Format::BF16:
            return direct_flexfloat_model<8, 7>(fp64_bits);
        case Format::E5M2:
            return direct_flexfloat_model<5, 2>(fp64_bits);
        case Format::E4M3:
            return direct_flexfloat_model<4, 3>(fp64_bits);
    }
    return fp64_bits;
}

static uint64_t lowprecision_hook_model(Format format, uint64_t fp64_bits)
{
    switch (format) {
        case Format::BF16:
            return typeSimulationFF64(8, 7, fp64_bits);
        case Format::E5M2:
            return typeSimulationFF64(5, 2, fp64_bits);
        case Format::E4M3:
            return typeSimulationFF64(4, 3, fp64_bits);
    }
    return fp64_bits;
}

static std::string format_name(Format format)
{
    switch (format) {
        case Format::BF16:
            return "bf16";
        case Format::E5M2:
            return "e5m2";
        case Format::E4M3:
            return "e4m3";
    }
    return "unknown";
}

static bool parse_format(const std::string& text, Format& format)
{
    if (text == "bf16") {
        format = Format::BF16;
        return true;
    }
    if (text == "e5m2") {
        format = Format::E5M2;
        return true;
    }
    if (text == "e4m3") {
        format = Format::E4M3;
        return true;
    }
    return false;
}

static std::string hex64(uint64_t value)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return os.str();
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <bf16|e5m2|e4m3> <fp64-input-file>\n";
        return 1;
    }

    Format format;
    if (!parse_format(argv[1], format)) {
        std::cerr << "unknown format: " << argv[1] << "\n";
        return 1;
    }

    std::ifstream in(argv[2]);
    if (!in.is_open()) {
        std::cerr << "failed to open input file: " << argv[2] << "\n";
        return 1;
    }

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
        std::string token;
        if (!(ss >> token)) {
            continue;
        }

        uint64_t fp64_bits = 0;
        if (!parse_u64_hex(token, fp64_bits)) {
            std::cerr << "parse error on line " << line_no << "\n";
            return 1;
        }

        ++total;

        const uint64_t direct_bits = direct_model(format, fp64_bits);
        const uint64_t hook_bits = lowprecision_hook_model(format, fp64_bits);

        if (direct_bits != hook_bits) {
            ++mismatches;
            std::cout << "mismatch"
                      << " format=" << format_name(format)
                      << " in=" << hex64(fp64_bits)
                      << " direct=" << hex64(direct_bits)
                      << " hook=" << hex64(hook_bits)
                      << '\n';
        }
    }

    std::cout << "format=" << format_name(format)
              << " total=" << total
              << " mismatches=" << mismatches
              << '\n';

    return mismatches == 0 ? 0 : 2;
}
