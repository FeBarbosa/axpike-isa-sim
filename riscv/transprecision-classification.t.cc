#include "transprecision_classification.h"
#include "cfg.h"

#include <cassert>
#include <stdexcept>

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

static void check_fp32_to_fp16_masking()
{
  transprecision_policy_config_t policy;
  policy.fp32_to_fp16_protected_bits = 0;

  const auto accepted =
      classify_transprecision_fp32(UINT32_C(0x3f900001), policy);
  assert(accepted.value_class == transprecision_value_class_t::FINITE);
  assert(accepted.type == transprecision_type_t::FP16);
  assert(accepted.selected_bits == UINT32_C(0x3f900000));
  assert(accepted.value_was_masked);
  assert(!accepted.masked_to_zero);

  const auto masked_to_zero =
      classify_transprecision_fp32(UINT32_C(0x00000001), policy);
  assert(masked_to_zero.value_class
      == transprecision_value_class_t::FINITE);
  assert(masked_to_zero.type == transprecision_type_t::FP16);
  assert(masked_to_zero.selected_bits == UINT32_C(0x00000000));
  assert(masked_to_zero.value_was_masked);
  assert(masked_to_zero.masked_to_zero);

  const auto fallback =
      classify_transprecision_fp32(UINT32_C(0x7f7fffff), policy);
  assert(fallback.value_class == transprecision_value_class_t::FINITE);
  assert(fallback.type == transprecision_type_t::FP32);
  assert(fallback.selected_bits == UINT32_C(0x7f7fffff));
  assert(!fallback.value_was_masked);
}

static void check_fp32_candidate_order_and_independence()
{
  transprecision_policy_config_t policy;
  policy.fp32_to_e5m2_protected_bits = 0;
  policy.fp32_to_fp16_protected_bits = 0;

  const auto smallest_candidate =
      classify_transprecision_fp32(UINT32_C(0x3f900001), policy);
  assert(smallest_candidate.type == transprecision_type_t::E5M2);
  assert(smallest_candidate.selected_bits == UINT32_C(0x3f800000));
  assert(smallest_candidate.value_was_masked);

  policy.fp32_to_e5m2_protected_bits = 16;
  policy.fp32_to_fp16_protected_bits = 13;
  const auto independent_candidates =
      classify_transprecision_fp32(UINT32_C(0x3f900001), policy);
  assert(independent_candidates.type == transprecision_type_t::FP32);
  assert(independent_candidates.selected_bits == UINT32_C(0x3f900001));
  assert(!independent_candidates.value_was_masked);
}

static void check_fp64_masking()
{
  transprecision_policy_config_t policy;
  policy.fp64_to_e5m2_protected_bits = 0;

  const auto e5m2 =
      classify_transprecision_fp64(UINT64_C(0x3ff2000000000001), policy);
  assert(e5m2.type == transprecision_type_t::E5M2);
  assert(e5m2.selected_bits == UINT64_C(0x3ff0000000000000));
  assert(e5m2.value_was_masked);

  policy = transprecision_policy_config_t();
  policy.fp64_to_fp16_protected_bits = 0;
  const auto fp16 =
      classify_transprecision_fp64(UINT64_C(0x3ff2000000000001), policy);
  assert(fp16.type == transprecision_type_t::FP16);
  assert(fp16.selected_bits == UINT64_C(0x3ff2000000000000));
  assert(fp16.value_was_masked);

  policy = transprecision_policy_config_t();
  policy.fp64_to_fp32_protected_bits = 0;
  const auto fp32 =
      classify_transprecision_fp64(UINT64_C(0x3ff0000020000001), policy);
  assert(fp32.type == transprecision_type_t::FP32);
  assert(fp32.selected_bits == UINT64_C(0x3ff0000020000000));
  assert(fp32.value_was_masked);

  policy.fp64_to_e5m2_protected_bits = 0;
  policy.fp64_to_fp16_protected_bits = 0;
  const auto candidate_order =
      classify_transprecision_fp64(UINT64_C(0x3ff2000000000001), policy);
  assert(candidate_order.type == transprecision_type_t::E5M2);
  assert(candidate_order.selected_bits == UINT64_C(0x3ff0000000000000));

  policy = transprecision_policy_config_t();
  policy.fp64_to_e5m2_protected_bits = 49;
  const auto independent_candidates =
      classify_transprecision_fp64(UINT64_C(0x3ff2000000000001), policy);
  assert(independent_candidates.type == transprecision_type_t::FP64);
  assert(independent_candidates.selected_bits
      == UINT64_C(0x3ff2000000000001));
  assert(!independent_candidates.value_was_masked);

  policy = transprecision_policy_config_t();
  policy.fp64_to_e5m2_protected_bits = 0;
  policy.fp64_to_fp16_protected_bits = 0;
  policy.fp64_to_fp32_protected_bits = 0;
  const auto fallback =
      classify_transprecision_fp64(UINT64_C(0x7fefffffffffffff), policy);
  assert(fallback.type == transprecision_type_t::FP64);
  assert(fallback.selected_bits == UINT64_C(0x7fefffffffffffff));
  assert(!fallback.value_was_masked);

  const auto masked_to_zero =
      classify_transprecision_fp64(UINT64_C(0x8000000000000001), policy);
  assert(masked_to_zero.type == transprecision_type_t::E5M2);
  assert(masked_to_zero.selected_bits == UINT64_C(0x8000000000000000));
  assert(masked_to_zero.value_was_masked);
  assert(masked_to_zero.masked_to_zero);
}

static void expect_invalid_fp32_policy(
    const transprecision_policy_config_t& policy)
{
  bool rejected = false;
  try {
    (void) classify_transprecision_fp32(UINT32_C(0x3f800000), policy);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

static void expect_invalid_fp64_policy(
    const transprecision_policy_config_t& policy)
{
  bool rejected = false;
  try {
    (void) classify_transprecision_fp64(
        UINT64_C(0x3ff0000000000000), policy);
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}

static void check_invalid_policy_bounds()
{
  transprecision_policy_config_t policy;
  policy.fp32_to_e5m2_protected_bits = 22;
  expect_invalid_fp32_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp32_to_fp16_protected_bits = 14;
  expect_invalid_fp32_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp64_to_e5m2_protected_bits = 51;
  expect_invalid_fp64_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp64_to_fp16_protected_bits = 43;
  expect_invalid_fp64_policy(policy);

  policy = transprecision_policy_config_t();
  policy.fp64_to_fp32_protected_bits = 30;
  expect_invalid_fp64_policy(policy);
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
  assert(defaults.fp32_to_e5m2_protected_bits == 21);
  assert(defaults.fp32_to_fp16_protected_bits == 13);
  assert(defaults.fp64_to_e5m2_protected_bits == 50);
  assert(defaults.fp64_to_fp16_protected_bits == 42);
  assert(defaults.fp64_to_fp32_protected_bits == 29);

  const auto policy = parse_transprecision_policy_config(
      "fp64-fp32:7,fp32-fp16:3,fp64-e5m2:9,"
      "fp32-e5m2:5,fp64-fp16:8");
  assert(policy.fp32_to_e5m2_protected_bits == 5);
  assert(policy.fp32_to_fp16_protected_bits == 3);
  assert(policy.fp64_to_e5m2_protected_bits == 9);
  assert(policy.fp64_to_fp16_protected_bits == 8);
  assert(policy.fp64_to_fp32_protected_bits == 7);

  expect_invalid_policy_vector("");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,fp64-fp16:42");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-fp16:29");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-unknown:29");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:x,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-fp32:29");
  expect_invalid_policy_vector(
      "fp32-e5m2:22,fp32-fp16:13,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-fp32:29");
  expect_invalid_policy_vector(
      "fp32-e5m2:21,fp32-fp16:13,fp64-e5m2:50,"
      "fp64-fp16:42,fp64-fp32:29,");
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
  check_fp32(UINT32_C(0x3f800000), transprecision_type_t::E5M2);
  check_fp32(UINT32_C(0x3f900000), transprecision_type_t::FP16);
  check_fp32(UINT32_C(0x3f800001), transprecision_type_t::FP32);
  check_fp32_to_fp16_masking();
  check_fp32_candidate_order_and_independence();

  check_fp64(UINT64_C(0x3ff0000000000000), transprecision_type_t::E5M2);
  check_fp64(UINT64_C(0x3ff2000000000000), transprecision_type_t::FP16);
  check_fp64(UINT64_C(0x3ff0000020000000), transprecision_type_t::FP32);
  check_fp64(UINT64_C(0x3ff0000000000001), transprecision_type_t::FP64);
  check_fp64_masking();
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
