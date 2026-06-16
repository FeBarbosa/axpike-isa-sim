#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "softfloat/softfloat.h"
#include "adele/adf/LowPrecisionSimulation/typeConvertionSoftFloat.h"

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

static uint64_t softfloat_hook_model(uint64_t fp64_bits)
{
    const float64_t in{fp64_bits};
    const float16_t half = f64_to_f16(in);
    const float64_t out = f16_to_f64(half);
    return out.v;
}

static uint64_t lowprecision_hook_model(uint64_t fp64_bits)
{
    return typeSimulationSoftFloatFP16(fp64_bits);
}

static std::string hex64(uint64_t value)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return os.str();
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <fp64-input-file>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "failed to open input file: " << argv[1] << "\n";
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

        softfloat_exceptionFlags = 0;
        const uint64_t soft_bits = softfloat_hook_model(fp64_bits);
        const uint_fast8_t soft_flags = softfloat_exceptionFlags;

        softfloat_exceptionFlags = 0;
        const uint64_t hook_bits = lowprecision_hook_model(fp64_bits);
        const uint_fast8_t hook_flags = softfloat_exceptionFlags;

        if (soft_bits != hook_bits || soft_flags != hook_flags) {
            ++mismatches;
            std::cout << "mismatch"
                      << " in=" << hex64(fp64_bits)
                      << " soft=" << hex64(soft_bits)
                      << " hook=" << hex64(hook_bits)
                      << " soft_flags=0x" << std::hex << static_cast<unsigned>(soft_flags)
                      << " hook_flags=0x" << static_cast<unsigned>(hook_flags)
                      << std::dec
                      << '\n';
        }
    }

    std::cout << "total=" << total
              << " mismatches=" << mismatches
              << '\n';

    return mismatches == 0 ? 0 : 2;
}
