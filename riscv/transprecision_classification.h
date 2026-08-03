#ifndef _AXPIKE_TRANSPRECISION_CLASSIFICATION_H_
#define _AXPIKE_TRANSPRECISION_CLASSIFICATION_H_

#include <cstdint>

#include "transprecision.h"

enum class transprecision_value_class_t : uint8_t
{
  FINITE,
  ZERO,
  INFINITY,
  NAN_VALUE,
};

static inline size_t transprecision_value_class_bucket(
    transprecision_value_class_t value_class)
{
  switch (value_class) {
    case transprecision_value_class_t::FINITE:
      return 0;
    case transprecision_value_class_t::ZERO:
      return 1;
    case transprecision_value_class_t::INFINITY:
      return 2;
    case transprecision_value_class_t::NAN_VALUE:
    default:
      return 3;
  }
}

static inline const char* transprecision_value_class_name(size_t bucket)
{
  switch (bucket) {
    case 0:
      return "FINITE";
    case 1:
      return "ZERO";
    case 2:
      return "INFINITY";
    case 3:
      return "NAN";
    default:
      return "UNKNOWN";
  }
}

struct transprecision_classification_t
{
  transprecision_type_t type;
  transprecision_value_class_t value_class;
  uint64_t selected_bits;
  bool value_was_masked;
  bool masked_to_zero;
};

struct transprecision_quantization_t
{
  transprecision_type_t effective_type;
  transprecision_value_class_t architectural_value_class;
  transprecision_value_class_t quantized_value_class;
  uint64_t architectural_bits;
  uint64_t quantized_bits;
  bool value_changed;
  bool changed_to_zero;
  bool overflow;
  bool underflow;
};

struct transprecision_operation_result_t
{
  transprecision_classification_t classification;
  transprecision_quantization_t quantization;
  bool quantization_performed;
};

struct transprecision_fp64_load_classification_t
{
  bool nan_boxed_fp32_candidate;
  transprecision_value_class_t fp32_payload_value_class;
};

transprecision_quantization_t quantize_transprecision_fp32_to_type(
    uint32_t bits, transprecision_type_t effective_type);
transprecision_quantization_t quantize_transprecision_fp64_to_type(
    uint64_t bits, transprecision_type_t effective_type);

uint8_t transprecision_mantissa_width(transprecision_type_t type);
uint8_t transprecision_effective_n(
    const transprecision_policy_config_t& policy,
    transprecision_type_t source_type);
uint8_t transprecision_candidate_mask_width(
    const transprecision_policy_config_t& policy,
    transprecision_type_t source_type,
    transprecision_type_t candidate_type);
transprecision_type_t transprecision_effective_type_ceiling(
    transprecision_type_t effective_type,
    transprecision_type_t architectural_carrier_type);

// The input bits must already be quantized to effective_type.
transprecision_classification_t
classify_transprecision_fp32_quantized_effective_type(
    uint32_t quantized_bits, transprecision_type_t effective_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
// The input bits must already be quantized to effective_type.
transprecision_classification_t
classify_transprecision_fp64_quantized_effective_type(
    uint64_t quantized_bits, transprecision_type_t effective_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());

transprecision_classification_t classify_transprecision_fp32(uint32_t bits,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_classification_t classify_transprecision_fp64(uint64_t bits,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_classification_t classify_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_classification_t classify_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_operation_result_t analyze_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_operation_result_t analyze_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_classification_t classify_transprecision_fp32_architectural_write(
    uint32_t bits, const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_classification_t classify_transprecision_fp64_architectural_write(
    uint64_t bits, const transprecision_policy_config_t& policy =
        transprecision_policy_config_t());
transprecision_fp64_load_classification_t
classify_transprecision_fp64_load(uint64_t bits);

void trace_transprecision_external_nan(uint8_t carrier_bits, uint64_t bits,
    size_t destination_register, uint32_t instruction_id, uint64_t pc,
    uint64_t raw_instruction);

#endif
