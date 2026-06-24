#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "adele/adf/LowPrecisionSimulation/typeConvertion.h"

static bool parse_u32_hex(const std::string& token, uint32_t& out)
{
    unsigned long value = 0;
    std::stringstream ss(token);
    ss >> std::hex >> value;
    if (ss.fail() || !ss.eof() || value > 0xFFFFFFFFul) {
        return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
}

static float bits_to_host_float(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t host_float_to_bits(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t direct_flexfloat_model(uint32_t fp32_bits)
{
    const float fp32_value = bits_to_host_float(fp32_bits);
    const flexfloat<5, 10> lowprecision_value = fp32_value;
    return host_float_to_bits(static_cast<float>(lowprecision_value));
}

static uint32_t lowprecision_hook_model(uint32_t fp32_bits)
{
    return typeSimulationFF(5, 10, fp32_bits);
}

static std::string hex32(uint32_t value)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return os.str();
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <fp32-input-file>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "failed to open input file: " << argv[1] << "\n";
        return 1;
    }

    std::fesetround(FE_TONEAREST);
    std::feclearexcept(FE_ALL_EXCEPT);

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

        uint32_t fp32_bits = 0;
        if (!parse_u32_hex(token, fp32_bits)) {
            std::cerr << "parse error on line " << line_no << "\n";
            return 1;
        }

        ++total;

        const uint32_t direct_bits = direct_flexfloat_model(fp32_bits);
        const uint32_t hook_bits = lowprecision_hook_model(fp32_bits);

        if (direct_bits != hook_bits) {
            ++mismatches;
            std::cout << "mismatch"
                      << " in=" << hex32(fp32_bits)
                      << " direct=" << hex32(direct_bits)
                      << " hook=" << hex32(hook_bits)
                      << '\n';
        }
    }

    std::cout << "total=" << total
              << " mismatches=" << mismatches
              << '\n';

    return mismatches == 0 ? 0 : 2;
}
