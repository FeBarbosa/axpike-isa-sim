#include "axpike_stats_sections.h"

#include <cassert>

int main()
{
  uint64_t section_zero[] = {11, 12};
  uint64_t section_seven[] = {71, 72};
  std::unordered_map<uint64_t,
      std::unordered_map<uint8_t, uint64_t*>> counters;
  counters[0][0] = section_zero;
  counters[0][7] = section_seven;

  assert(AxPIKE::detail::sectionCounterValue(counters, 0, 0, 1) == 12);
  assert(AxPIKE::detail::sectionCounterValue(counters, 0, 6, 1) == 0);
  assert(AxPIKE::detail::sectionCounterValue(counters, 0, 7, 1) == 72);
  assert(AxPIKE::detail::sectionCounterValue(counters, 1, 7, 1) == 0);
  assert(AxPIKE::detail::findSectionCounters(counters, 0, 6) == nullptr);
  return 0;
}
