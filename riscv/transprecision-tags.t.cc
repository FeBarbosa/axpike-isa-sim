#include "transprecision.h"

#include <cassert>
#include <cstdint>

static_assert(static_cast<uint8_t>(transprecision_type_t::E5M2)
              < static_cast<uint8_t>(transprecision_type_t::FP16));
static_assert(static_cast<uint8_t>(transprecision_type_t::FP16)
              < static_cast<uint8_t>(transprecision_type_t::FP32));
static_assert(static_cast<uint8_t>(transprecision_type_t::FP32)
              < static_cast<uint8_t>(transprecision_type_t::FP64));

int main()
{
  transprecision_tag_file_t<4> tags;

  for (size_t i = 0; i < 4; ++i)
    assert(tags.read(i) == transprecision_type_t::UNCLASSIFIED);

  tags.write(1, transprecision_type_t::E5M2);
  tags.write(3, transprecision_type_t::FP64);

  assert(tags.read(0) == transprecision_type_t::UNCLASSIFIED);
  assert(tags.read(1) == transprecision_type_t::E5M2);
  assert(tags.read(2) == transprecision_type_t::UNCLASSIFIED);
  assert(tags.read(3) == transprecision_type_t::FP64);

  tags.reset();
  for (size_t i = 0; i < 4; ++i)
    assert(tags.read(i) == transprecision_type_t::UNCLASSIFIED);

  return 0;
}
