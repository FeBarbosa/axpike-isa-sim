#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

static bool parse_u16_hex(const std::string& text, uint16_t& out)
{
    char* end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, 16);
    if (end == text.c_str() || *end != '\0' || value > 0xFFFFul) {
        return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
}

static void usage(const char* prog)
{
    std::cerr
        << "usage: " << prog
        << " <output-file> [--a-start HEX] [--a-end HEX] [--b-start HEX] [--b-end HEX] [--limit N]\n";
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string out_path = argv[1];
    uint16_t a_start = 0x0000;
    uint16_t a_end = 0xFFFF;
    uint16_t b_start = 0x0000;
    uint16_t b_end = 0xFFFF;
    std::uint64_t limit = 0;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--a-start" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], a_start)) {
                std::cerr << "invalid --a-start value\n";
                return 1;
            }
        } else if (arg == "--a-end" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], a_end)) {
                std::cerr << "invalid --a-end value\n";
                return 1;
            }
        } else if (arg == "--b-start" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], b_start)) {
                std::cerr << "invalid --b-start value\n";
                return 1;
            }
        } else if (arg == "--b-end" && i + 1 < argc) {
            if (!parse_u16_hex(argv[++i], b_end)) {
                std::cerr << "invalid --b-end value\n";
                return 1;
            }
        } else if (arg == "--limit" && i + 1 < argc) {
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0') {
                std::cerr << "invalid --limit value\n";
                return 1;
            }
            limit = static_cast<std::uint64_t>(parsed);
        } else {
            std::cerr << "unknown or incomplete option: " << arg << '\n';
            usage(argv[0]);
            return 1;
        }
    }

    if (a_start > a_end || b_start > b_end) {
        std::cerr << "invalid range: start must be <= end\n";
        return 1;
    }

    std::ofstream out(out_path);
    if (!out.is_open()) {
        std::cerr << "failed to open output file: " << out_path << '\n';
        return 1;
    }

    out << "# FP16 operand pairs, encoded as raw uint16_t hex values\n";
    out << "# format: <a_bits> <b_bits>\n";
    out << std::hex << std::setfill('0');

    std::uint64_t count = 0;
    for (std::uint32_t a = a_start; a <= a_end; ++a) {
        for (std::uint32_t b = b_start; b <= b_end; ++b) {
            out << std::setw(4) << a << ' ' << std::setw(4) << b << '\n';
            ++count;
            if (limit != 0 && count >= limit) {
                std::cerr << "stopped after limit=" << limit << " pairs\n";
                return 0;
            }
        }
        if (a == a_end) {
            break;
        }
    }

    std::cerr << "wrote " << count << " pairs to " << out_path << '\n';
    return 0;
}
