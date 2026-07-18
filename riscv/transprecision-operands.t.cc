#include "transprecision.h"

#include <cassert>

static void check_supported_type_filter()
{
  assert(transprecision_is_supported_type(transprecision_type_t::E5M2));
  assert(transprecision_is_supported_type(transprecision_type_t::FP16));
  assert(transprecision_is_supported_type(transprecision_type_t::FP32));
  assert(transprecision_is_supported_type(transprecision_type_t::FP64));
  assert(!transprecision_is_supported_type(
      transprecision_type_t::UNCLASSIFIED));
}

static void check_binary_effective_type()
{
  assert(transprecision_effective_type(transprecision_type_t::E5M2,
      transprecision_type_t::E5M2) == transprecision_type_t::E5M2);
  assert(transprecision_effective_type(transprecision_type_t::FP16,
      transprecision_type_t::FP16) == transprecision_type_t::FP16);
  assert(transprecision_effective_type(transprecision_type_t::FP32,
      transprecision_type_t::FP32) == transprecision_type_t::FP32);
  assert(transprecision_effective_type(transprecision_type_t::FP64,
      transprecision_type_t::FP64) == transprecision_type_t::FP64);

  assert(transprecision_effective_type(transprecision_type_t::E5M2,
      transprecision_type_t::FP16) == transprecision_type_t::FP16);
  assert(transprecision_effective_type(transprecision_type_t::FP16,
      transprecision_type_t::FP32) == transprecision_type_t::FP32);
  assert(transprecision_effective_type(transprecision_type_t::FP32,
      transprecision_type_t::FP64) == transprecision_type_t::FP64);
  assert(transprecision_effective_type(transprecision_type_t::FP64,
      transprecision_type_t::E5M2) == transprecision_type_t::FP64);
}

static void check_ternary_effective_type()
{
  assert(transprecision_effective_type(transprecision_type_t::E5M2,
      transprecision_type_t::FP16, transprecision_type_t::FP32)
      == transprecision_type_t::FP32);
  assert(transprecision_effective_type(transprecision_type_t::FP64,
      transprecision_type_t::FP16, transprecision_type_t::FP32)
      == transprecision_type_t::FP64);
  assert(transprecision_effective_type(transprecision_type_t::E5M2,
      transprecision_type_t::E5M2, transprecision_type_t::E5M2)
      == transprecision_type_t::E5M2);
}

static void check_unclassified_is_conservative()
{
  assert(transprecision_effective_type(transprecision_type_t::UNCLASSIFIED,
      transprecision_type_t::E5M2)
      == transprecision_type_t::UNCLASSIFIED);
  assert(transprecision_effective_type(transprecision_type_t::FP64,
      transprecision_type_t::UNCLASSIFIED)
      == transprecision_type_t::UNCLASSIFIED);
  assert(transprecision_effective_type(transprecision_type_t::FP16,
      transprecision_type_t::UNCLASSIFIED, transprecision_type_t::FP32)
      == transprecision_type_t::UNCLASSIFIED);
}

static void check_register_tag_reads()
{
  transprecision_tag_file_t<8> tags;

  tags.write(1, transprecision_type_t::E5M2);
  tags.write(2, transprecision_type_t::FP32);
  tags.write(3, transprecision_type_t::FP16);
  tags.write(4, transprecision_type_t::FP64);

  assert(transprecision_effective_type_from_regs(tags, 1, 2)
      == transprecision_type_t::FP32);
  assert(transprecision_effective_type_from_regs(tags, 1, 3, 4)
      == transprecision_type_t::FP64);
  assert(transprecision_effective_type_from_regs(tags, 1, 0)
      == transprecision_type_t::UNCLASSIFIED);
}

int main()
{
  check_supported_type_filter();
  check_binary_effective_type();
  check_ternary_effective_type();
  check_unclassified_is_conservative();
  check_register_tag_reads();

  return 0;
}
