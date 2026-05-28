#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

#include <flexfloat.h>

int main(int argc, char** argv)
{
    const float a_host = (argc > 1) ? std::strtof(argv[1], nullptr) : 1.0f;
    const float b_host = (argc > 2) ? std::strtof(argv[2], nullptr) : 2.0f;

    std::fesetround(FE_TONEAREST);
    std::feclearexcept(FE_ALL_EXCEPT);

    flexfloat_desc_t fp16_desc = {5, 10};
    flexfloat_t a;
    flexfloat_t b;
    flexfloat_t sum;

    ff_init_double(&a, static_cast<double>(a_host), fp16_desc);
    ff_init_double(&b, static_cast<double>(b_host), fp16_desc);
    ff_init(&sum, fp16_desc);
    ff_add(&sum, &a, &b);

    const uint32_t a_bits = static_cast<uint32_t>(flexfloat_get_bits(&a)) & 0xFFFFu;
    const uint32_t b_bits = static_cast<uint32_t>(flexfloat_get_bits(&b)) & 0xFFFFu;
    const uint32_t sum_bits = static_cast<uint32_t>(flexfloat_get_bits(&sum)) & 0xFFFFu;

    std::cout << std::hex << std::setfill('0');
    std::cout << "a16=0x" << std::setw(4) << a_bits
              << " b16=0x" << std::setw(4) << b_bits
              << " sum16=0x" << std::setw(4) << sum_bits << '\n';

    std::cout << std::dec << std::setprecision(8);
    std::cout << "a=" << a_host
              << " b=" << b_host
              << " sum=" << ff_get_double(&sum) << '\n';

    if (std::fetestexcept(FE_ALL_EXCEPT) != 0) {
        std::cout << "flags=0x" << std::hex << std::fetestexcept(FE_ALL_EXCEPT)
                  << std::dec << '\n';
    }

    return 0;
}
