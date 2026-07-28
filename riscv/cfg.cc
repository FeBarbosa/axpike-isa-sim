// See LICENSE for license details.

#include "config.h"
#include "cfg.h"
#include "mmu.h"
#include "decode.h"
#include "encoding.h"
#include "platform.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct transprecision_policy_field_t
{
  const char* name;
  uint8_t transprecision_policy_config_t::*member;
  uint8_t maximum;
};

const std::array<transprecision_policy_field_t, 5>
    transprecision_policy_fields = {{
      {"fp32-e5m2",
          &transprecision_policy_config_t::fp32_to_e5m2_protected_bits, 21},
      {"fp32-fp16",
          &transprecision_policy_config_t::fp32_to_fp16_protected_bits, 13},
      {"fp64-e5m2",
          &transprecision_policy_config_t::fp64_to_e5m2_protected_bits, 50},
      {"fp64-fp16",
          &transprecision_policy_config_t::fp64_to_fp16_protected_bits, 42},
      {"fp64-fp32",
          &transprecision_policy_config_t::fp64_to_fp32_protected_bits, 29},
    }};

} // namespace

transprecision_policy_config_t parse_transprecision_policy_config(
    const char* argument)
{
  if (argument == nullptr || argument[0] == '\0')
    throw std::invalid_argument("the protected-bit vector is empty");

  const std::string vector_text(argument);
  if (vector_text.back() == ',')
    throw std::invalid_argument("the vector contains an empty entry");

  transprecision_policy_config_t policy;
  std::array<bool, 5> seen = {};
  std::stringstream entries(vector_text);
  std::string entry;
  size_t entry_count = 0;

  while (std::getline(entries, entry, ',')) {
    if (entry.empty())
      throw std::invalid_argument("the vector contains an empty entry");

    const size_t separator = entry.find(':');
    if (separator == std::string::npos
        || separator == 0
        || separator + 1 == entry.size()
        || entry.find(':', separator + 1) != std::string::npos) {
      throw std::invalid_argument(
          "each entry must use the form transition:value");
    }

    const std::string name = entry.substr(0, separator);
    const std::string value_text = entry.substr(separator + 1);
    size_t field_index = transprecision_policy_fields.size();
    for (size_t i = 0; i < transprecision_policy_fields.size(); ++i) {
      if (name == transprecision_policy_fields[i].name) {
        field_index = i;
        break;
      }
    }
    if (field_index == transprecision_policy_fields.size())
      throw std::invalid_argument("unknown transition '" + name + "'");
    if (seen[field_index])
      throw std::invalid_argument("duplicate transition '" + name + "'");

    for (char character : value_text) {
      if (!std::isdigit(static_cast<unsigned char>(character))) {
        throw std::invalid_argument(
            "protected width for '" + name + "' must be a decimal integer");
      }
    }

    char* end = nullptr;
    const unsigned long value =
        std::strtoul(value_text.c_str(), &end, 10);
    const auto& field = transprecision_policy_fields[field_index];
    if (end == nullptr || *end != '\0' || value > field.maximum) {
      throw std::invalid_argument(
          "protected width for '" + name + "' must be between 0 and "
          + std::to_string(field.maximum));
    }

    policy.*(field.member) = static_cast<uint8_t>(value);
    seen[field_index] = true;
    entry_count++;
  }

  if (entry_count != transprecision_policy_fields.size()) {
    throw std::invalid_argument(
        "the vector must define all five transitions exactly once");
  }

  return policy;
}

mem_cfg_t::mem_cfg_t(reg_t base, reg_t size) : base(base), size(size)
{
  assert(mem_cfg_t::check_if_supported(base, size));
}

bool mem_cfg_t::check_if_supported(reg_t base, reg_t size)
{
  // The truth of these conditions should be ensured by whatever is creating
  // the regions in the first place, but we have them here to make sure that
  // we can't end up describing memory regions that don't make sense. They
  // ask that the page size is a multiple of the minimum page size, that the
  // page is aligned to the minimum page size, that the page is non-empty,
  // that the size doesn't overflow size_t, and that the top address is still
  // representable in a reg_t.
  //
  // Note: (base + size == 0) part of the assertion is to handle cases like
  //   { base = 0xffff_ffff_ffff_f000, size: 0x1000 }
  return (size % PGSIZE == 0) &&
         (base % PGSIZE == 0) &&
         (size_t(size) == size) &&
         (size > 0) &&
         ((base + size > base) || (base + size == 0));
}

cfg_t::cfg_t()
{
  // The default system configuration
  initrd_bounds    = std::make_pair((reg_t)0, (reg_t)0);
  bootargs         = nullptr;
  isa              = DEFAULT_ISA;
  priv             = DEFAULT_PRIV;
  misaligned       = false;
  endianness       = endianness_little;
  pmpregions       = 16;
  pmpgranularity   = (1 << PMP_SHIFT);
  mem_layout       = std::vector<mem_cfg_t>({mem_cfg_t(reg_t(DRAM_BASE), (size_t)2048 << 20)});
  hartids          = std::vector<size_t>({0});
  explicit_hartids = false;
  real_time_clint  = false;
  trigger_count    = 4;
}
