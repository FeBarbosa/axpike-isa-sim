#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "softfloat/softfloat.h"

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

int main(int argc, char** argv)
{
    const float a_host = (argc > 1) ? std::strtof(argv[1], nullptr) : 1.0f;
    const float b_host = (argc > 2) ? std::strtof(argv[2], nullptr) : 2.0f;

    softfloat_roundingMode = softfloat_round_near_even;
    softfloat_detectTininess = softfloat_tininess_afterRounding; // how to indentify underflow
    softfloat_exceptionFlags = 0;

    const float32_t a32 = { host_float_to_bits(a_host) };
    const float32_t b32 = { host_float_to_bits(b_host) };

    const float16_t a16 = f32_to_f16(a32);
    const float16_t b16 = f32_to_f16(b32);
    const float16_t sum16 = f16_add(a16, b16);
    const float32_t sum32 = f16_to_f32(sum16);

    std::cout << std::hex << std::setfill('0');
    std::cout << "a16=0x" << std::setw(4) << a16.v
              << " b16=0x" << std::setw(4) << b16.v
              << " sum16=0x" << std::setw(4) << sum16.v << '\n';

    std::cout << std::dec << std::setprecision(8);
    std::cout << "a=" << a_host
              << " b=" << b_host
              << " sum=" << bits_to_host_float(sum32.v) << '\n';

    if (softfloat_exceptionFlags != 0) {
        std::cout << "flags=0x" << std::hex << static_cast<unsigned>(softfloat_exceptionFlags)
                  << std::dec << '\n';
    }

    return 0;
}
