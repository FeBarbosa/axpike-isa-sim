#ifndef _AXPIKE_STATS_SECTIONS_H_
#define _AXPIKE_STATS_SECTIONS_H_

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace AxPIKE::detail {

template <typename Counter>
const Counter* findSectionCounters(
    const std::unordered_map<uint64_t,
        std::unordered_map<uint8_t, Counter*>>& counters,
    uint64_t approximation, uint8_t section)
{
  const auto approximation_entry = counters.find(approximation);
  if (approximation_entry == counters.end())
    return nullptr;
  const auto section_entry = approximation_entry->second.find(section);
  if (section_entry == approximation_entry->second.end())
    return nullptr;
  return section_entry->second;
}

template <typename Counter>
Counter sectionCounterValue(
    const std::unordered_map<uint64_t,
        std::unordered_map<uint8_t, Counter*>>& counters,
    uint64_t approximation, uint8_t section, size_t instruction)
{
  const Counter* section_counters =
      findSectionCounters(counters, approximation, section);
  return section_counters == nullptr ? Counter{}
                                     : section_counters[instruction];
}

} // namespace AxPIKE::detail

#endif
