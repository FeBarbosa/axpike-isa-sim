#include "transprecision_classification.h"
#include "cfg.h"
#include "softfloat.h"

#include <cassert>
#include <stdexcept>

static void check_fp32_effective_type_quantization()
{
  const auto e5m2 = quantize_transprecision_fp32_to_type(
      UINT32_C(0x3f8ccccd), transprecision_type_t::E5M2);
  assert(e5m2.effective_type == transprecision_type_t::E5M2);
  assert(e5m2.architectural_value_class
      == transprecision_value_class_t::FINITE);
  assert(e5m2.quantized_value_class
      == transprecision_value_class_t::FINITE);
  assert(e5m2.architectural_bits == UINT32_C(0x3f8ccccd));
  assert(e5m2.quantized_bits == UINT32_C(0x3f800000));
  assert(e5m2.value_changed);
  assert(!e5m2.changed_to_zero);
  assert(!e5m2.overflow);
  assert(!e5m2.underflow);

  const auto fp16 = quantize_transprecision_fp32_to_type(
      UINT32_C(0x3f8ccccd), transprecision_type_t::FP16);
  assert(fp16.quantized_bits == UINT32_C(0x3f8cc000));
  assert(fp16.value_changed);

  const auto fp16_exact_subnormal = quantize_transprecision_fp32_to_type(
      UINT32_C(0x33800000), transprecision_type_t::FP16);
  assert(fp16_exact_subnormal.quantized_bits == UINT32_C(0x33800000));
  assert(!fp16_exact_subnormal.value_changed);
  assert(fp16_exact_subnormal.underflow);

  const auto fp32 = quantize_transprecision_fp32_to_type(
      UINT32_C(0x3f8ccccd), transprecision_type_t::FP32);
  assert(fp32.quantized_bits == UINT32_C(0x3f8ccccd));
  assert(!fp32.value_changed);
}

static void check_fp64_effective_type_quantization()
{
  const auto e5m2 = quantize_transprecision_fp64_to_type(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::E5M2);
  assert(e5m2.effective_type == transprecision_type_t::E5M2);
  assert(e5m2.architectural_value_class
      == transprecision_value_class_t::FINITE);
  assert(e5m2.quantized_value_class
      == transprecision_value_class_t::FINITE);
  assert(e5m2.quantized_bits == UINT64_C(0x3ff0000000000000));
  assert(e5m2.value_changed);

  const auto fp16 = quantize_transprecision_fp64_to_type(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP16);
  assert(fp16.quantized_bits == UINT64_C(0x3ff1980000000000));
  assert(fp16.value_changed);

  const auto fp32 = quantize_transprecision_fp64_to_type(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP32);
  assert(fp32.quantized_bits == UINT64_C(0x3ff19999a0000000));
  assert(fp32.value_changed);

  const auto fp64 = quantize_transprecision_fp64_to_type(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP64);
  assert(fp64.quantized_bits == UINT64_C(0x3ff199999999999a));
  assert(!fp64.value_changed);
}

static void check_effective_type_quantization_boundaries()
{
  const auto fp16_min_normal = quantize_transprecision_fp32_to_type(
      UINT32_C(0x38800000), transprecision_type_t::FP16);
  assert(fp16_min_normal.quantized_bits == UINT32_C(0x38800000));
  assert(!fp16_min_normal.underflow);

  const auto below_fp16_min_normal = quantize_transprecision_fp32_to_type(
      UINT32_C(0x387fffff), transprecision_type_t::FP16);
  assert(below_fp16_min_normal.underflow);

  const auto positive_underflow = quantize_transprecision_fp32_to_type(
      UINT32_C(0x00000001), transprecision_type_t::FP16);
  assert(positive_underflow.architectural_value_class
      == transprecision_value_class_t::FINITE);
  assert(positive_underflow.quantized_value_class
      == transprecision_value_class_t::ZERO);
  assert(positive_underflow.quantized_bits == UINT32_C(0x00000000));
  assert(positive_underflow.value_changed);
  assert(positive_underflow.changed_to_zero);
  assert(!positive_underflow.overflow);
  assert(positive_underflow.underflow);

  const auto negative_underflow = quantize_transprecision_fp64_to_type(
      UINT64_C(0x8000000000000001), transprecision_type_t::E5M2);
  assert(negative_underflow.quantized_value_class
      == transprecision_value_class_t::ZERO);
  assert(negative_underflow.quantized_bits
      == UINT64_C(0x8000000000000000));
  assert(negative_underflow.changed_to_zero);
  assert(negative_underflow.underflow);

  const auto e5m2_subnormal = quantize_transprecision_fp32_to_type(
      UINT32_C(0x38000000), transprecision_type_t::E5M2);
  assert(e5m2_subnormal.quantized_value_class
      == transprecision_value_class_t::FINITE);
  assert(e5m2_subnormal.quantized_bits == UINT32_C(0x38000000));
  assert(!e5m2_subnormal.changed_to_zero);
  assert(e5m2_subnormal.underflow);

  const auto fp16_max_finite = quantize_transprecision_fp32_to_type(
      UINT32_C(0x477fe000), transprecision_type_t::FP16);
  assert(fp16_max_finite.quantized_bits == UINT32_C(0x477fe000));
  assert(!fp16_max_finite.overflow);

  const auto above_fp16_max_finite = quantize_transprecision_fp32_to_type(
      UINT32_C(0x477fe001), transprecision_type_t::FP16);
  assert(above_fp16_max_finite.quantized_bits == UINT32_C(0x7f800000));
  assert(above_fp16_max_finite.overflow);

  const auto overflow = quantize_transprecision_fp32_to_type(
      UINT32_C(0x7f7fffff), transprecision_type_t::FP16);
  assert(overflow.architectural_value_class
      == transprecision_value_class_t::FINITE);
  assert(overflow.quantized_value_class
      == transprecision_value_class_t::INFINITY);
  assert(overflow.quantized_bits == UINT32_C(0x7f800000));
  assert(overflow.value_changed);
  assert(!overflow.changed_to_zero);
  assert(overflow.overflow);
  assert(!overflow.underflow);

  const auto negative_overflow = quantize_transprecision_fp32_to_type(
      UINT32_C(0xff7fffff), transprecision_type_t::E5M2);
  assert(negative_overflow.quantized_bits == UINT32_C(0xff800000));
  assert(negative_overflow.overflow);

  const auto positive_zero = quantize_transprecision_fp32_to_type(
      UINT32_C(0x00000000), transprecision_type_t::E5M2);
  assert(positive_zero.quantized_value_class
      == transprecision_value_class_t::ZERO);
  assert(positive_zero.quantized_bits == UINT32_C(0x00000000));
  assert(!positive_zero.value_changed);
  assert(!positive_zero.changed_to_zero);

  const auto negative_zero = quantize_transprecision_fp64_to_type(
      UINT64_C(0x8000000000000000), transprecision_type_t::FP16);
  assert(negative_zero.quantized_value_class
      == transprecision_value_class_t::ZERO);
  assert(negative_zero.quantized_bits
      == UINT64_C(0x8000000000000000));
  assert(!negative_zero.value_changed);
  assert(!negative_zero.changed_to_zero);
}

static void check_effective_type_quantization_special_values()
{
  const uint32_t fp32_specials[] = {
    UINT32_C(0x7f800000),
    UINT32_C(0xff800000),
    UINT32_C(0x7fc12345),
    UINT32_C(0xff812345),
  };
  for (uint32_t bits : fp32_specials) {
    const auto result = quantize_transprecision_fp32_to_type(
        bits, transprecision_type_t::E5M2);
    assert(result.quantized_bits == bits);
    assert(!result.value_changed);
    assert(!result.changed_to_zero);
    assert(!result.overflow);
    assert(!result.underflow);
  }

  const uint64_t fp64_specials[] = {
    UINT64_C(0x7ff0000000000000),
    UINT64_C(0xfff0000000000000),
    UINT64_C(0x7ff8000000001234),
    UINT64_C(0xfff0000000005678),
  };
  for (uint64_t bits : fp64_specials) {
    const auto result = quantize_transprecision_fp64_to_type(
        bits, transprecision_type_t::FP16);
    assert(result.quantized_bits == bits);
    assert(!result.value_changed);
    assert(!result.changed_to_zero);
    assert(!result.overflow);
    assert(!result.underflow);
  }
}

static void check_effective_type_quantization_validation_and_flags()
{
  bool rejected = false;
  try {
    (void) quantize_transprecision_fp32_to_type(
        UINT32_C(0x3f800000), transprecision_type_t::FP64);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    (void) quantize_transprecision_fp64_to_type(
        UINT64_C(0x3ff0000000000000),
        transprecision_type_t::UNCLASSIFIED);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  softfloat_exceptionFlags = softfloat_flag_invalid | softfloat_flag_inexact;
  (void) quantize_transprecision_fp32_to_type(
      UINT32_C(0x3f8ccccd), transprecision_type_t::FP16);
  (void) quantize_transprecision_fp64_to_type(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP32);
  (void) quantize_transprecision_fp32_to_type(
      UINT32_C(0x7f7fffff), transprecision_type_t::FP16);
  (void) quantize_transprecision_fp32_to_type(
      UINT32_C(0x38000000), transprecision_type_t::E5M2);
  assert(softfloat_exceptionFlags
      == (softfloat_flag_invalid | softfloat_flag_inexact));
  softfloat_exceptionFlags = 0;
}

static void check_fp32(uint32_t bits, transprecision_type_t expected)
{
  const auto result = classify_transprecision_fp32(bits);
  assert(result.value_class == transprecision_value_class_t::FINITE);
  assert(result.type == expected);
  assert(result.selected_bits == bits);
  assert(!result.value_was_masked);
  assert(!result.masked_to_zero);
}

static void check_fp64(uint64_t bits, transprecision_type_t expected)
{
  const auto result = classify_transprecision_fp64(bits);
  assert(result.value_class == transprecision_value_class_t::FINITE);
  assert(result.type == expected);
  assert(result.selected_bits == bits);
  assert(!result.value_was_masked);
  assert(!result.masked_to_zero);
}

static void check_type_based_mask_widths()
{
  transprecision_policy_config_t policy;
  assert(transprecision_mantissa_width(transprecision_type_t::E5M2) == 2);
  assert(transprecision_mantissa_width(transprecision_type_t::FP16) == 10);
  assert(transprecision_mantissa_width(transprecision_type_t::FP32) == 23);
  assert(transprecision_mantissa_width(transprecision_type_t::FP64) == 52);
  assert(transprecision_effective_n(policy, transprecision_type_t::FP64)
      == 50);
  assert(transprecision_effective_n(policy, transprecision_type_t::FP32)
      == 21);
  assert(transprecision_effective_n(policy, transprecision_type_t::FP16)
      == 8);
  assert(transprecision_effective_n(policy, transprecision_type_t::E5M2)
      == 0);

  policy.fp32_protected_bits = 10;
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP32, transprecision_type_t::FP16) == 3);
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP32, transprecision_type_t::E5M2) == 11);

  policy.fp16_protected_bits = 0;
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP16, transprecision_type_t::E5M2) == 8);
  policy.fp16_protected_bits = 8;
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP16, transprecision_type_t::E5M2) == 0);

  policy.fp64_protected_bits = 20;
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP64, transprecision_type_t::FP32) == 9);
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP64, transprecision_type_t::FP16) == 22);
  assert(transprecision_candidate_mask_width(policy,
      transprecision_type_t::FP64, transprecision_type_t::E5M2) == 30);
}

static void check_fp32_effective_type_classification()
{
  transprecision_policy_config_t policy;
  policy.fp16_protected_bits = 0;
  const auto offset_sensitive =
      classify_transprecision_fp32_quantized_effective_type(
          UINT32_C(0x3f802000), transprecision_type_t::FP16, policy);
  assert(offset_sensitive.type == transprecision_type_t::E5M2);
  assert(offset_sensitive.selected_bits == UINT32_C(0x3f800000));
  assert(offset_sensitive.value_was_masked);

  policy = transprecision_policy_config_t();
  const auto retained_fp16 =
      classify_transprecision_fp32_quantized_effective_type(
          UINT32_C(0x3f802000), transprecision_type_t::FP16, policy);
  assert(retained_fp16.type == transprecision_type_t::FP16);
  assert(retained_fp16.selected_bits == UINT32_C(0x3f802000));
  assert(!retained_fp16.value_was_masked);

  const auto retained_e5m2 =
      classify_transprecision_fp32_quantized_effective_type(
          UINT32_C(0x3fa00000), transprecision_type_t::E5M2, policy);
  assert(retained_e5m2.type == transprecision_type_t::E5M2);
  assert(retained_e5m2.selected_bits == UINT32_C(0x3fa00000));
  assert(!retained_e5m2.value_was_masked);

  policy.fp32_protected_bits = 5;
  const auto independent_candidates =
      classify_transprecision_fp32_quantized_effective_type(
          UINT32_C(0x3f900401), transprecision_type_t::FP32, policy);
  assert(independent_candidates.type == transprecision_type_t::FP32);
  assert(independent_candidates.selected_bits == UINT32_C(0x3f900401));
  assert(!independent_candidates.value_was_masked);

  policy.fp32_protected_bits = 0;
  const auto masked_to_zero =
      classify_transprecision_fp32_quantized_effective_type(
          UINT32_C(0x00000001), transprecision_type_t::FP32, policy);
  assert(masked_to_zero.type == transprecision_type_t::E5M2);
  assert(masked_to_zero.selected_bits == UINT32_C(0x00000000));
  assert(masked_to_zero.value_was_masked);
  assert(masked_to_zero.masked_to_zero);
}

static void check_fp64_effective_type_classification()
{
  transprecision_policy_config_t policy;
  policy.fp32_protected_bits = 13;
  const auto offset_sensitive =
      classify_transprecision_fp64_quantized_effective_type(
          UINT64_C(0x3ff0000020000000),
          transprecision_type_t::FP32, policy);
  assert(offset_sensitive.type == transprecision_type_t::E5M2);
  assert(offset_sensitive.selected_bits
      == UINT64_C(0x3ff0000000000000));
  assert(offset_sensitive.value_was_masked);

  policy = transprecision_policy_config_t();
  const auto retained_fp32 =
      classify_transprecision_fp64_quantized_effective_type(
          UINT64_C(0x3ff0000020000000),
          transprecision_type_t::FP32, policy);
  assert(retained_fp32.type == transprecision_type_t::FP32);
  assert(retained_fp32.selected_bits
      == UINT64_C(0x3ff0000020000000));
  assert(!retained_fp32.value_was_masked);

  const auto special =
      classify_transprecision_fp64_quantized_effective_type(
          UINT64_C(0x7ff8000000001234),
          transprecision_type_t::FP16, policy);
  assert(special.type == transprecision_type_t::FP16);
  assert(special.value_class == transprecision_value_class_t::NAN_VALUE);
  assert(special.selected_bits == UINT64_C(0x7ff8000000001234));
}

static void check_operation_result_quantization_integration()
{
  const auto fp32_fp16 = classify_transprecision_fp32_operation_result(
      UINT32_C(0x3f8ccccd), transprecision_type_t::FP16);
  assert(fp32_fp16.type == transprecision_type_t::FP16);
  assert(fp32_fp16.selected_bits == UINT32_C(0x3f8cc000));
  assert(!fp32_fp16.value_was_masked);

  const auto fp32_e5m2 = classify_transprecision_fp32_operation_result(
      UINT32_C(0x3f8ccccd), transprecision_type_t::E5M2);
  assert(fp32_e5m2.type == transprecision_type_t::E5M2);
  assert(fp32_e5m2.selected_bits == UINT32_C(0x3f800000));
  assert(!fp32_e5m2.value_was_masked);

  const auto fp32_ceiling = classify_transprecision_fp32_operation_result(
      UINT32_C(0x3f800001), transprecision_type_t::FP64);
  assert(fp32_ceiling.type == transprecision_type_t::FP32);
  assert(fp32_ceiling.selected_bits == UINT32_C(0x3f800001));
  assert(!fp32_ceiling.value_was_masked);

  const auto fp64_fp32 = classify_transprecision_fp64_operation_result(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP32);
  assert(fp64_fp32.type == transprecision_type_t::FP32);
  assert(fp64_fp32.selected_bits == UINT64_C(0x3ff19999a0000000));
  assert(!fp64_fp32.value_was_masked);

  const auto fp64_fp16 = classify_transprecision_fp64_operation_result(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP16);
  assert(fp64_fp16.type == transprecision_type_t::FP16);
  assert(fp64_fp16.selected_bits == UINT64_C(0x3ff1980000000000));
  assert(!fp64_fp16.value_was_masked);

  assert(transprecision_effective_type_ceiling(
      transprecision_type_t::FP64, transprecision_type_t::FP32)
      == transprecision_type_t::FP32);
  assert(transprecision_effective_type_ceiling(
      transprecision_type_t::FP16, transprecision_type_t::FP32)
      == transprecision_type_t::FP16);
  assert(transprecision_effective_type_ceiling(
      transprecision_type_t::UNCLASSIFIED, transprecision_type_t::FP32)
      == transprecision_type_t::UNCLASSIFIED);

  softfloat_exceptionFlags = softfloat_flag_invalid | softfloat_flag_underflow;
  (void) classify_transprecision_fp32_operation_result(
      UINT32_C(0x3f8ccccd), transprecision_type_t::FP16);
  (void) classify_transprecision_fp64_operation_result(
      UINT64_C(0x3ff199999999999a), transprecision_type_t::FP32);
  assert(softfloat_exceptionFlags
      == (softfloat_flag_invalid | softfloat_flag_underflow));
  softfloat_exceptionFlags = 0;
}

static void expect_invalid_policy(
    const transprecision_policy_config_t& policy)
{
  bool rejected = false;
  try {
    (void) transprecision_effective_n(policy, transprecision_type_t::FP32);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

static void check_invalid_policy_bounds()
{
  transprecision_policy_config_t policy;
  policy.fp64_protected_bits = 51;
  expect_invalid_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp32_protected_bits = 22;
  expect_invalid_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp16_protected_bits = 9;
  expect_invalid_policy(policy);

  policy = transprecision_policy_config_t();
  policy.e5m2_protected_bits = 1;
  expect_invalid_policy(policy);
}

static void expect_invalid_policy_vector(const char* vector)
{
  bool rejected = false;
  try {
    (void) parse_transprecision_policy_config(vector);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

static void check_policy_vector_parsing()
{
  const transprecision_policy_config_t defaults;
  assert(defaults.fp64_protected_bits == 50);
  assert(defaults.fp32_protected_bits == 21);
  assert(defaults.fp16_protected_bits == 8);
  assert(defaults.e5m2_protected_bits == 0);

  const auto policy = parse_transprecision_policy_config(
      "fp16:3,e5m2:0,fp64:9,fp32:5");
  assert(policy.fp64_protected_bits == 9);
  assert(policy.fp32_protected_bits == 5);
  assert(policy.fp16_protected_bits == 3);
  assert(policy.e5m2_protected_bits == 0);

  expect_invalid_policy_vector("");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:8");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:8,fp16:0");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:8,unknown:0");
  expect_invalid_policy_vector("fp64:50,fp32:x,fp16:8,e5m2:0");
  expect_invalid_policy_vector("fp64:50,fp32:22,fp16:8,e5m2:0");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:9,e5m2:0");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:8,e5m2:1");
  expect_invalid_policy_vector("fp64:50,fp32:21,fp16:8,e5m2:0,");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-fp32:29");
}

static void check_contextual_fp32(uint32_t bits,
    transprecision_value_class_t expected_class)
{
  const auto operation = classify_transprecision_fp32_operation_result(
      bits, transprecision_type_t::FP16);
  assert(operation.value_class == expected_class);
  assert(operation.type == transprecision_type_t::FP16);
  assert(operation.selected_bits == bits);
  assert(!operation.value_was_masked);
  assert(!operation.masked_to_zero);

  const auto architectural = classify_transprecision_fp32_architectural_write(
      bits);
  assert(architectural.value_class == expected_class);
  assert(architectural.type == transprecision_type_t::E5M2);
  assert(architectural.selected_bits == bits);
  assert(!architectural.value_was_masked);
  assert(!architectural.masked_to_zero);
}

static void check_contextual_fp64(uint64_t bits,
    transprecision_value_class_t expected_class)
{
  const auto operation = classify_transprecision_fp64_operation_result(
      bits, transprecision_type_t::FP16);
  assert(operation.value_class == expected_class);
  assert(operation.type == transprecision_type_t::FP16);
  assert(operation.selected_bits == bits);
  assert(!operation.value_was_masked);
  assert(!operation.masked_to_zero);

  const auto architectural = classify_transprecision_fp64_architectural_write(
      bits);
  assert(architectural.value_class == expected_class);
  assert(architectural.type == transprecision_type_t::E5M2);
  assert(architectural.selected_bits == bits);
  assert(!architectural.value_was_masked);
  assert(!architectural.masked_to_zero);
}

static void check_contextual_zero()
{
  const auto fp32_operation = classify_transprecision_fp32_operation_result(
      UINT32_C(0x80000000), transprecision_type_t::FP16);
  assert(fp32_operation.value_class == transprecision_value_class_t::ZERO);
  assert(fp32_operation.type == transprecision_type_t::E5M2);
  assert(fp32_operation.selected_bits == UINT32_C(0x80000000));
  assert(!fp32_operation.value_was_masked);

  const auto fp32_architectural =
      classify_transprecision_fp32_architectural_write(UINT32_C(0x00000000));
  assert(fp32_architectural.value_class == transprecision_value_class_t::ZERO);
  assert(fp32_architectural.type == transprecision_type_t::E5M2);
  assert(fp32_architectural.selected_bits == UINT32_C(0x00000000));
  assert(!fp32_architectural.value_was_masked);

  const auto fp64_operation = classify_transprecision_fp64_operation_result(
      UINT64_C(0x8000000000000000), transprecision_type_t::FP16);
  assert(fp64_operation.value_class == transprecision_value_class_t::ZERO);
  assert(fp64_operation.type == transprecision_type_t::E5M2);
  assert(fp64_operation.selected_bits == UINT64_C(0x8000000000000000));
  assert(!fp64_operation.value_was_masked);

  const auto fp64_architectural =
      classify_transprecision_fp64_architectural_write(
          UINT64_C(0x0000000000000000));
  assert(fp64_architectural.value_class == transprecision_value_class_t::ZERO);
  assert(fp64_architectural.type == transprecision_type_t::E5M2);
  assert(fp64_architectural.selected_bits == UINT64_C(0x0000000000000000));
  assert(!fp64_architectural.value_was_masked);
}

static void check_fp64_load_context_classification()
{
  const auto finite_payload = classify_transprecision_fp64_load(
      UINT64_C(0xffffffff437f0000));
  assert(finite_payload.nan_boxed_fp32_candidate);
  assert(finite_payload.fp32_payload_value_class
      == transprecision_value_class_t::FINITE);

  const auto zero_payload = classify_transprecision_fp64_load(
      UINT64_C(0xffffffff00000000));
  assert(zero_payload.nan_boxed_fp32_candidate);
  assert(zero_payload.fp32_payload_value_class
      == transprecision_value_class_t::ZERO);

  const auto nan_payload = classify_transprecision_fp64_load(
      UINT64_C(0xffffffffffc00000));
  assert(nan_payload.nan_boxed_fp32_candidate);
  assert(nan_payload.fp32_payload_value_class
      == transprecision_value_class_t::NAN_VALUE);

  const auto canonical_fp64_nan = classify_transprecision_fp64_load(
      UINT64_C(0x7ff8000000000000));
  assert(!canonical_fp64_nan.nan_boxed_fp32_candidate);
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
  check_fp32_effective_type_quantization();
  check_fp64_effective_type_quantization();
  check_effective_type_quantization_boundaries();
  check_effective_type_quantization_special_values();
  check_effective_type_quantization_validation_and_flags();

  check_fp32(UINT32_C(0x3f800000), transprecision_type_t::E5M2);
  check_fp32(UINT32_C(0x3f900000), transprecision_type_t::FP16);
  check_fp32(UINT32_C(0x3f800001), transprecision_type_t::FP32);
  check_type_based_mask_widths();
  check_fp32_effective_type_classification();

  check_fp64(UINT64_C(0x3ff0000000000000), transprecision_type_t::E5M2);
  check_fp64(UINT64_C(0x3ff2000000000000), transprecision_type_t::FP16);
  check_fp64(UINT64_C(0x3ff0000020000000), transprecision_type_t::FP32);
  check_fp64(UINT64_C(0x3ff0000000000001), transprecision_type_t::FP64);
  check_fp64_effective_type_classification();
  check_operation_result_quantization_integration();
  check_invalid_policy_bounds();
  check_policy_vector_parsing();

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
  check_contextual_fp32(UINT32_C(0x7fc12345),
      transprecision_value_class_t::NAN_VALUE);
  check_contextual_fp32(UINT32_C(0xff812345),
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
  check_contextual_fp64(UINT64_C(0x7ff8000000001234),
      transprecision_value_class_t::NAN_VALUE);
  check_contextual_fp64(UINT64_C(0xfff0000000005678),
      transprecision_value_class_t::NAN_VALUE);
  check_contextual_zero();
  check_fp64_load_context_classification();
  check_unclassified_operation_context();

  return 0;
}
