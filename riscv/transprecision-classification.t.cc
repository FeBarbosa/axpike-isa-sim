#include "transprecision_classification.h"

#include <cassert>

static void check_fp32(uint32_t bits, transprecision_type_t expected)
{
  const auto result = classify_transprecision_fp32(bits);
  assert(result.value_class == transprecision_value_class_t::FINITE);
  assert(result.type == expected);
}

static void check_fp64(uint64_t bits, transprecision_type_t expected)
{
  const auto result = classify_transprecision_fp64(bits);
  assert(result.value_class == transprecision_value_class_t::FINITE);
  assert(result.type == expected);
}

static void check_contextual_fp32(uint32_t bits,
    transprecision_value_class_t expected_class)
{
  const auto operation = classify_transprecision_fp32_operation_result(
      bits, transprecision_type_t::FP16);
  assert(operation.value_class == expected_class);
  assert(operation.type == transprecision_type_t::FP16);

  const auto architectural = classify_transprecision_fp32_architectural_write(
      bits);
  assert(architectural.value_class == expected_class);
  assert(architectural.type == transprecision_type_t::FP32);
}

static void check_contextual_fp64(uint64_t bits,
    transprecision_value_class_t expected_class)
{
  const auto operation = classify_transprecision_fp64_operation_result(
      bits, transprecision_type_t::FP16);
  assert(operation.value_class == expected_class);
  assert(operation.type == transprecision_type_t::FP16);

  const auto architectural = classify_transprecision_fp64_architectural_write(
      bits);
  assert(architectural.value_class == expected_class);
  assert(architectural.type == transprecision_type_t::FP64);
}

static void check_contextual_zero()
{
  const auto fp32_operation = classify_transprecision_fp32_operation_result(
      UINT32_C(0x80000000), transprecision_type_t::FP16);
  assert(fp32_operation.value_class == transprecision_value_class_t::ZERO);
  assert(fp32_operation.type == transprecision_type_t::E5M2);

  const auto fp32_architectural =
      classify_transprecision_fp32_architectural_write(UINT32_C(0x00000000));
  assert(fp32_architectural.value_class == transprecision_value_class_t::ZERO);
  assert(fp32_architectural.type == transprecision_type_t::E5M2);

  const auto fp64_operation = classify_transprecision_fp64_operation_result(
      UINT64_C(0x8000000000000000), transprecision_type_t::FP16);
  assert(fp64_operation.value_class == transprecision_value_class_t::ZERO);
  assert(fp64_operation.type == transprecision_type_t::E5M2);

  const auto fp64_architectural =
      classify_transprecision_fp64_architectural_write(
          UINT64_C(0x0000000000000000));
  assert(fp64_architectural.value_class == transprecision_value_class_t::ZERO);
  assert(fp64_architectural.type == transprecision_type_t::E5M2);
}

static void check_unclassified_operation_context()
{
  const auto fp32_finite = classify_transprecision_fp32_operation_result(
      UINT32_C(0x3f800000), transprecision_type_t::UNCLASSIFIED);
  assert(fp32_finite.value_class == transprecision_value_class_t::FINITE);
  assert(fp32_finite.type == transprecision_type_t::UNCLASSIFIED);

  const auto fp32_zero = classify_transprecision_fp32_operation_result(
      UINT32_C(0x00000000), transprecision_type_t::UNCLASSIFIED);
  assert(fp32_zero.value_class == transprecision_value_class_t::ZERO);
  assert(fp32_zero.type == transprecision_type_t::UNCLASSIFIED);

  const auto fp32_nan = classify_transprecision_fp32_operation_result(
      UINT32_C(0x7fc00000), transprecision_type_t::UNCLASSIFIED);
  assert(fp32_nan.value_class == transprecision_value_class_t::NAN_VALUE);
  assert(fp32_nan.type == transprecision_type_t::UNCLASSIFIED);

  const auto fp64_finite = classify_transprecision_fp64_operation_result(
      UINT64_C(0x3ff0000000000000), transprecision_type_t::UNCLASSIFIED);
  assert(fp64_finite.value_class == transprecision_value_class_t::FINITE);
  assert(fp64_finite.type == transprecision_type_t::UNCLASSIFIED);
}

int main()
{
  check_fp32(UINT32_C(0x3f800000), transprecision_type_t::E5M2);
  check_fp32(UINT32_C(0x3f900000), transprecision_type_t::FP16);
  check_fp32(UINT32_C(0x3f800001), transprecision_type_t::FP32);

  check_fp64(UINT64_C(0x3ff0000000000000), transprecision_type_t::E5M2);
  check_fp64(UINT64_C(0x3ff2000000000000), transprecision_type_t::FP16);
  check_fp64(UINT64_C(0x3ff0000020000000), transprecision_type_t::FP32);
  check_fp64(UINT64_C(0x3ff0000000000001), transprecision_type_t::FP64);

  const uint32_t fp32_specials[] = {
    UINT32_C(0x00000000), UINT32_C(0x80000000),
    UINT32_C(0x7f800000), UINT32_C(0xff800000), UINT32_C(0x7fc00000),
  };
  const transprecision_value_class_t fp32_classes[] = {
    transprecision_value_class_t::ZERO,
    transprecision_value_class_t::ZERO,
    transprecision_value_class_t::INFINITY,
    transprecision_value_class_t::INFINITY,
    transprecision_value_class_t::NAN_VALUE,
  };
  for (size_t i = 0; i < 5; ++i) {
    const auto result = classify_transprecision_fp32(fp32_specials[i]);
    assert(result.type == (fp32_classes[i] == transprecision_value_class_t::ZERO
        ? transprecision_type_t::E5M2
        : transprecision_type_t::UNCLASSIFIED));
    assert(result.value_class == fp32_classes[i]);
  }
  check_contextual_fp32(UINT32_C(0x7f800000),
      transprecision_value_class_t::INFINITY);
  check_contextual_fp32(UINT32_C(0xff800000),
      transprecision_value_class_t::INFINITY);
  check_contextual_fp32(UINT32_C(0x7fc00000),
      transprecision_value_class_t::NAN_VALUE);

  const uint64_t fp64_specials[] = {
    UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
    UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
    UINT64_C(0x7ff8000000000000),
  };
  const transprecision_value_class_t fp64_classes[] = {
    transprecision_value_class_t::ZERO,
    transprecision_value_class_t::ZERO,
    transprecision_value_class_t::INFINITY,
    transprecision_value_class_t::INFINITY,
    transprecision_value_class_t::NAN_VALUE,
  };
  for (size_t i = 0; i < 5; ++i) {
    const auto result = classify_transprecision_fp64(fp64_specials[i]);
    assert(result.type == (fp64_classes[i] == transprecision_value_class_t::ZERO
        ? transprecision_type_t::E5M2
        : transprecision_type_t::UNCLASSIFIED));
    assert(result.value_class == fp64_classes[i]);
  }
  check_contextual_fp64(UINT64_C(0x7ff0000000000000),
      transprecision_value_class_t::INFINITY);
  check_contextual_fp64(UINT64_C(0xfff0000000000000),
      transprecision_value_class_t::INFINITY);
  check_contextual_fp64(UINT64_C(0x7ff8000000000000),
      transprecision_value_class_t::NAN_VALUE);
  check_contextual_zero();
  check_unclassified_operation_context();

  return 0;
}
