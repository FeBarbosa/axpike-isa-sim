#include "transprecision_classification.h"

#include "adele/adf/LowPrecisionSimulation/typeConvertion.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace {

struct external_nan_trace_config_t
{
  bool initialized = false;
  bool enabled = false;
  uint64_t limit = 123;
  uint64_t count = 0;
  FILE* file = stderr;
  std::mutex mutex;
};

external_nan_trace_config_t external_nan_trace_config;

bool external_nan_trace_enabled(const char* value)
{
  return value != nullptr && value[0] != '\0'
      && std::strcmp(value, "0") != 0;
}

uint64_t external_nan_trace_limit(const char* value)
{
  if (value == nullptr || value[0] == '\0')
    return 123;

  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(value, &end, 10);
  return end != value && *end == '\0' ? parsed : 123;
}

void initialize_external_nan_trace()
{
  auto& config = external_nan_trace_config;
  if (config.initialized)
    return;

  config.initialized = true;
  config.enabled = external_nan_trace_enabled(
      std::getenv("AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN"));
  config.limit = external_nan_trace_limit(
      std::getenv("AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN_LIMIT"));
  if (!config.enabled)
    return;

  const char* path =
      std::getenv("AXPIKE_TRACE_TRANSPRECISION_EXTERNAL_NAN_FILE");
  if (path != nullptr && path[0] != '\0') {
    FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
      std::fprintf(stderr,
          "AxPIKE: unable to open external-NaN trace file '%s'; "
          "using stderr\n", path);
    }
    else {
      config.file = file;
    }
  }

  std::fprintf(config.file,
      "event\tcarrier_bits\tpc\tinstruction_id\traw_instruction\t"
      "destination_register\tbits\tupper_fp32_bits\tlower_fp32_bits\t"
      "lower_fp32_class\tnan_boxed_fp32\n");
  std::fflush(config.file);
}

transprecision_value_class_t classify_raw_fp32_value(uint32_t bits)
{
  const uint32_t exponent = (bits >> 23) & UINT32_C(0xff);
  const uint32_t fraction = bits & UINT32_C(0x007fffff);
  if (exponent == 0)
    return fraction == 0
        ? transprecision_value_class_t::ZERO
        : transprecision_value_class_t::FINITE;
  if (exponent == UINT32_C(0xff))
    return fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE;
  return transprecision_value_class_t::FINITE;
}

transprecision_value_class_t classify_raw_fp64_value(uint64_t bits)
{
  const uint64_t exponent = (bits >> 52) & UINT64_C(0x7ff);
  const uint64_t fraction = bits & UINT64_C(0x000fffffffffffff);
  if (exponent == 0)
    return fraction == 0
        ? transprecision_value_class_t::ZERO
        : transprecision_value_class_t::FINITE;
  if (exponent == UINT64_C(0x7ff))
    return fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE;
  return transprecision_value_class_t::FINITE;
}

transprecision_quantization_t make_quantization(
    transprecision_type_t effective_type,
    transprecision_value_class_t architectural_value_class,
    transprecision_value_class_t quantized_value_class,
    uint64_t architectural_bits, uint64_t quantized_bits,
    bool overflow = false, bool underflow = false)
{
  return {
    effective_type,
    architectural_value_class,
    quantized_value_class,
    architectural_bits,
    quantized_bits,
    architectural_bits != quantized_bits,
    architectural_value_class == transprecision_value_class_t::FINITE
        && quantized_value_class == transprecision_value_class_t::ZERO,
    overflow,
    underflow,
  };
}

uint32_t fp32_max_finite_for_type(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
      return UINT32_C(0x47600000);
    case transprecision_type_t::FP16:
      return UINT32_C(0x477fe000);
    case transprecision_type_t::FP32:
      return UINT32_C(0x7f7fffff);
    case transprecision_type_t::FP64:
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("effective type exceeds FP32 carrier");
  }
}

uint32_t fp32_min_normal_for_type(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
    case transprecision_type_t::FP16:
      return UINT32_C(0x38800000);
    case transprecision_type_t::FP32:
      return UINT32_C(0x00800000);
    case transprecision_type_t::FP64:
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("effective type exceeds FP32 carrier");
  }
}

uint64_t fp64_max_finite_for_type(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
      return UINT64_C(0x40ec000000000000);
    case transprecision_type_t::FP16:
      return UINT64_C(0x40effc0000000000);
    case transprecision_type_t::FP32:
      return UINT64_C(0x47efffffe0000000);
    case transprecision_type_t::FP64:
      return UINT64_C(0x7fefffffffffffff);
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("unsupported FP64 effective type");
  }
}

uint64_t fp64_min_normal_for_type(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
    case transprecision_type_t::FP16:
      return UINT64_C(0x3f10000000000000);
    case transprecision_type_t::FP32:
      return UINT64_C(0x3810000000000000);
    case transprecision_type_t::FP64:
      return UINT64_C(0x0010000000000000);
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("unsupported FP64 effective type");
  }
}

transprecision_classification_t special_classification(
    transprecision_value_class_t value_class, uint64_t bits)
{
  return {
    transprecision_type_t::UNCLASSIFIED, value_class, bits, false, false
  };
}

transprecision_classification_t zero_classification(uint64_t bits)
{
  return {
    transprecision_type_t::E5M2, transprecision_value_class_t::ZERO, bits,
    false, false
  };
}

transprecision_classification_t finite_classification(
    transprecision_type_t type, uint64_t selected_bits, uint64_t original_bits,
    bool masked_to_zero = false)
{
  return {
    type, transprecision_value_class_t::FINITE, selected_bits,
    selected_bits != original_bits, masked_to_zero
  };
}

uint32_t clear_fp32_fraction_region(
    uint32_t bits, uint8_t offset, uint8_t bit_count)
{
  if (offset > 23 || bit_count > 23 - offset)
    throw std::invalid_argument("FP32 mantissa mask exceeds carrier fraction");
  if (bit_count == 0)
    return bits;

  const uint32_t ignored_mask =
      ((UINT32_C(1) << bit_count) - UINT32_C(1)) << offset;
  return bits & ~ignored_mask;
}

uint64_t clear_fp64_fraction_region(
    uint64_t bits, uint8_t offset, uint8_t bit_count)
{
  if (offset > 52 || bit_count > 52 - offset)
    throw std::invalid_argument("FP64 mantissa mask exceeds carrier fraction");
  if (bit_count == 0)
    return bits;

  const uint64_t ignored_mask =
      ((UINT64_C(1) << bit_count) - UINT64_C(1)) << offset;
  return bits & ~ignored_mask;
}

void validate_policy(const transprecision_policy_config_t& policy)
{
  if (policy.fp64_protected_bits > 50)
    throw std::invalid_argument("FP64 protected bits must be between 0 and 50");
  if (policy.fp32_protected_bits > 21)
    throw std::invalid_argument("FP32 protected bits must be between 0 and 21");
  if (policy.fp16_protected_bits > 8)
    throw std::invalid_argument("FP16 protected bits must be between 0 and 8");
  if (policy.e5m2_protected_bits != 0)
    throw std::invalid_argument("E5M2 protected bits must be zero");
}

transprecision_classification_t apply_nan_infinity_context(
    transprecision_classification_t classification,
    transprecision_type_t context_type)
{
  if (classification.value_class == transprecision_value_class_t::INFINITY ||
      classification.value_class == transprecision_value_class_t::NAN_VALUE)
    classification.type = context_type;
  return classification;
}

transprecision_classification_t apply_operation_context(
    transprecision_classification_t classification,
    transprecision_type_t intended_execution_type)
{
  if (!transprecision_is_supported_type(intended_execution_type)) {
    classification.type = transprecision_type_t::UNCLASSIFIED;
    return classification;
  }

  return apply_nan_infinity_context(classification, intended_execution_type);
}

} // namespace

transprecision_quantization_t quantize_transprecision_fp32_to_type(
    uint32_t bits, transprecision_type_t effective_type)
{
  const transprecision_value_class_t architectural_value_class =
      classify_raw_fp32_value(bits);
  if (architectural_value_class == transprecision_value_class_t::INFINITY
      || architectural_value_class
          == transprecision_value_class_t::NAN_VALUE) {
    if (effective_type != transprecision_type_t::E5M2
        && effective_type != transprecision_type_t::FP16
        && effective_type != transprecision_type_t::FP32)
      throw std::invalid_argument("effective type exceeds FP32 carrier");
    return make_quantization(effective_type, architectural_value_class,
        architectural_value_class, bits, bits);
  }

  uint32_t quantized_bits = bits;
  const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
  const bool overflow =
      architectural_value_class == transprecision_value_class_t::FINITE
      && magnitude > fp32_max_finite_for_type(effective_type);
  const bool underflow =
      architectural_value_class == transprecision_value_class_t::FINITE
      && magnitude != 0
      && magnitude < fp32_min_normal_for_type(effective_type);
  if (overflow) {
    quantized_bits =
        (bits & UINT32_C(0x80000000)) | UINT32_C(0x7f800000);
  }
  else {
    switch (effective_type) {
      case transprecision_type_t::E5M2:
        quantized_bits = typeSimulationFF(5, 2, bits);
        break;
      case transprecision_type_t::FP16:
        quantized_bits = typeSimulationFF(5, 10, bits);
        break;
      case transprecision_type_t::FP32:
        break;
      case transprecision_type_t::FP64:
      case transprecision_type_t::UNCLASSIFIED:
      default:
        throw std::invalid_argument("effective type exceeds FP32 carrier");
    }
  }

  return make_quantization(effective_type, architectural_value_class,
      classify_raw_fp32_value(quantized_bits), bits, quantized_bits,
      overflow, underflow);
}

transprecision_quantization_t quantize_transprecision_fp64_to_type(
    uint64_t bits, transprecision_type_t effective_type)
{
  const transprecision_value_class_t architectural_value_class =
      classify_raw_fp64_value(bits);
  if (architectural_value_class == transprecision_value_class_t::INFINITY
      || architectural_value_class
          == transprecision_value_class_t::NAN_VALUE) {
    if (!transprecision_is_supported_type(effective_type))
      throw std::invalid_argument("unsupported FP64 effective type");
    return make_quantization(effective_type, architectural_value_class,
        architectural_value_class, bits, bits);
  }

  uint64_t quantized_bits = bits;
  const uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
  const bool overflow =
      architectural_value_class == transprecision_value_class_t::FINITE
      && magnitude > fp64_max_finite_for_type(effective_type);
  const bool underflow =
      architectural_value_class == transprecision_value_class_t::FINITE
      && magnitude != 0
      && magnitude < fp64_min_normal_for_type(effective_type);
  if (overflow) {
    quantized_bits =
        (bits & UINT64_C(0x8000000000000000))
        | UINT64_C(0x7ff0000000000000);
  }
  else {
    switch (effective_type) {
      case transprecision_type_t::E5M2:
        quantized_bits = typeSimulationFF64(5, 2, bits);
        break;
      case transprecision_type_t::FP16:
        quantized_bits = typeSimulationFF64(5, 10, bits);
        break;
      case transprecision_type_t::FP32:
        quantized_bits = typeSimulationFF64(8, 23, bits);
        break;
      case transprecision_type_t::FP64:
        break;
      case transprecision_type_t::UNCLASSIFIED:
      default:
        throw std::invalid_argument("unsupported FP64 effective type");
    }
  }

  return make_quantization(effective_type, architectural_value_class,
      classify_raw_fp64_value(quantized_bits), bits, quantized_bits,
      overflow, underflow);
}

uint8_t transprecision_mantissa_width(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
      return 2;
    case transprecision_type_t::FP16:
      return 10;
    case transprecision_type_t::FP32:
      return 23;
    case transprecision_type_t::FP64:
      return 52;
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("unsupported transprecision type");
  }
}

uint8_t transprecision_effective_n(
    const transprecision_policy_config_t& policy,
    transprecision_type_t source_type)
{
  validate_policy(policy);
  switch (source_type) {
    case transprecision_type_t::E5M2:
      return policy.e5m2_protected_bits;
    case transprecision_type_t::FP16:
      return policy.fp16_protected_bits;
    case transprecision_type_t::FP32:
      return policy.fp32_protected_bits;
    case transprecision_type_t::FP64:
      return policy.fp64_protected_bits;
    case transprecision_type_t::UNCLASSIFIED:
    default:
      throw std::invalid_argument("unsupported source type");
  }
}

uint8_t transprecision_candidate_mask_width(
    const transprecision_policy_config_t& policy,
    transprecision_type_t source_type,
    transprecision_type_t candidate_type)
{
  if (!transprecision_type_less_than(candidate_type, source_type))
    throw std::invalid_argument("candidate type must be lower than source type");

  const uint8_t active_difference =
      transprecision_mantissa_width(source_type)
      - transprecision_mantissa_width(candidate_type);
  const uint8_t protected_bits = transprecision_effective_n(
      policy, source_type);
  return protected_bits < active_difference
      ? active_difference - protected_bits
      : 0;
}

transprecision_type_t transprecision_effective_type_ceiling(
    transprecision_type_t effective_type,
    transprecision_type_t architectural_carrier_type)
{
  if (!transprecision_is_supported_type(effective_type)
      || !transprecision_is_supported_type(architectural_carrier_type))
    return transprecision_type_t::UNCLASSIFIED;
  return transprecision_type_less_than(
      architectural_carrier_type, effective_type)
      ? architectural_carrier_type
      : effective_type;
}

transprecision_classification_t
classify_transprecision_fp32_quantized_effective_type(
    uint32_t quantized_bits, transprecision_type_t effective_type,
    const transprecision_policy_config_t& policy)
{
  validate_policy(policy);
  if (effective_type == transprecision_type_t::FP64
      || !transprecision_is_supported_type(effective_type))
    throw std::invalid_argument("effective type exceeds FP32 carrier");

  const auto input = quantize_transprecision_fp32_to_type(
      quantized_bits, effective_type);
  if (input.quantized_bits != quantized_bits)
    throw std::invalid_argument("FP32 carrier input is not quantized to T");

  const auto value_class = classify_raw_fp32_value(quantized_bits);
  if (value_class == transprecision_value_class_t::ZERO)
    return zero_classification(quantized_bits);
  if (value_class == transprecision_value_class_t::INFINITY
      || value_class == transprecision_value_class_t::NAN_VALUE) {
    auto result = special_classification(value_class, quantized_bits);
    result.type = effective_type;
    return result;
  }

  const uint8_t carrier_mantissa = 23;
  const uint8_t source_mantissa =
      transprecision_mantissa_width(effective_type);
  const uint8_t carrier_offset = carrier_mantissa - source_mantissa;
  const transprecision_type_t candidates[] = {
    transprecision_type_t::E5M2,
    transprecision_type_t::FP16,
  };
  for (transprecision_type_t candidate : candidates) {
    if (!transprecision_type_less_than(candidate, effective_type))
      continue;

    const uint8_t mask_width = transprecision_candidate_mask_width(
        policy, effective_type, candidate);
    const uint32_t candidate_bits = clear_fp32_fraction_region(
        quantized_bits, carrier_offset, mask_width);
    const uint8_t candidate_exponent = 5;
    const uint8_t candidate_mantissa =
        transprecision_mantissa_width(candidate);
    if (typeSimulationFF(
            candidate_exponent, candidate_mantissa, candidate_bits)
        == candidate_bits) {
      return finite_classification(candidate, candidate_bits, quantized_bits,
          (candidate_bits & UINT32_C(0x7fffffff)) == 0);
    }
  }

  return finite_classification(
      effective_type, quantized_bits, quantized_bits);
}

transprecision_classification_t
classify_transprecision_fp64_quantized_effective_type(
    uint64_t quantized_bits, transprecision_type_t effective_type,
    const transprecision_policy_config_t& policy)
{
  validate_policy(policy);
  if (!transprecision_is_supported_type(effective_type))
    throw std::invalid_argument("unsupported FP64 effective type");

  const auto input = quantize_transprecision_fp64_to_type(
      quantized_bits, effective_type);
  if (input.quantized_bits != quantized_bits)
    throw std::invalid_argument("FP64 carrier input is not quantized to T");

  const auto value_class = classify_raw_fp64_value(quantized_bits);
  if (value_class == transprecision_value_class_t::ZERO)
    return zero_classification(quantized_bits);
  if (value_class == transprecision_value_class_t::INFINITY
      || value_class == transprecision_value_class_t::NAN_VALUE) {
    auto result = special_classification(value_class, quantized_bits);
    result.type = effective_type;
    return result;
  }

  const uint8_t carrier_mantissa = 52;
  const uint8_t source_mantissa =
      transprecision_mantissa_width(effective_type);
  const uint8_t carrier_offset = carrier_mantissa - source_mantissa;
  const transprecision_type_t candidates[] = {
    transprecision_type_t::E5M2,
    transprecision_type_t::FP16,
    transprecision_type_t::FP32,
  };
  for (transprecision_type_t candidate : candidates) {
    if (!transprecision_type_less_than(candidate, effective_type))
      continue;

    const uint8_t mask_width = transprecision_candidate_mask_width(
        policy, effective_type, candidate);
    const uint64_t candidate_bits = clear_fp64_fraction_region(
        quantized_bits, carrier_offset, mask_width);
    const uint8_t candidate_exponent =
        candidate == transprecision_type_t::FP32 ? 8 : 5;
    const uint8_t candidate_mantissa =
        transprecision_mantissa_width(candidate);
    if (typeSimulationFF64(
            candidate_exponent, candidate_mantissa, candidate_bits)
        == candidate_bits) {
      return finite_classification(candidate, candidate_bits, quantized_bits,
          (candidate_bits & UINT64_C(0x7fffffffffffffff)) == 0);
    }
  }

  return finite_classification(
      effective_type, quantized_bits, quantized_bits);
}

void trace_transprecision_external_nan(uint8_t carrier_bits, uint64_t bits,
    size_t destination_register, uint32_t instruction_id, uint64_t pc,
    uint64_t raw_instruction)
{
  auto& config = external_nan_trace_config;
  std::lock_guard<std::mutex> lock(config.mutex);
  initialize_external_nan_trace();
  if (!config.enabled || config.count >= config.limit)
    return;

  const uint32_t upper_fp32_bits = static_cast<uint32_t>(bits >> 32);
  const uint32_t lower_fp32_bits = static_cast<uint32_t>(bits);
  const auto load_classification =
      classify_transprecision_fp64_load(bits);
  const bool nan_boxed_fp32 = carrier_bits == 64
      && load_classification.nan_boxed_fp32_candidate;

  std::fprintf(config.file,
      "%llu\t%u\t0x%016llx\t%u\t0x%016llx\t%zu\t"
      "0x%016llx\t0x%08x\t0x%08x\t%s\t%s\n",
      static_cast<unsigned long long>(config.count),
      static_cast<unsigned>(carrier_bits),
      static_cast<unsigned long long>(pc),
      instruction_id,
      static_cast<unsigned long long>(raw_instruction),
      destination_register,
      static_cast<unsigned long long>(bits),
      upper_fp32_bits,
      lower_fp32_bits,
      transprecision_value_class_name(transprecision_value_class_bucket(
          load_classification.fp32_payload_value_class)),
      nan_boxed_fp32 ? "true" : "false");
  config.count++;
  std::fflush(config.file);
}

transprecision_fp64_load_classification_t
classify_transprecision_fp64_load(uint64_t bits)
{
  const uint32_t upper_fp32_bits = static_cast<uint32_t>(bits >> 32);
  const uint32_t lower_fp32_bits = static_cast<uint32_t>(bits);
  return {
    upper_fp32_bits == UINT32_MAX,
    classify_raw_fp32_value(lower_fp32_bits),
  };
}

transprecision_classification_t classify_transprecision_fp32(uint32_t bits,
    const transprecision_policy_config_t& policy)
{
  auto result = classify_transprecision_fp32_quantized_effective_type(
      bits, transprecision_type_t::FP32, policy);
  if (result.value_class == transprecision_value_class_t::INFINITY
      || result.value_class == transprecision_value_class_t::NAN_VALUE)
    result.type = transprecision_type_t::UNCLASSIFIED;
  return result;
}

transprecision_classification_t classify_transprecision_fp64(uint64_t bits,
    const transprecision_policy_config_t& policy)
{
  auto result = classify_transprecision_fp64_quantized_effective_type(
      bits, transprecision_type_t::FP64, policy);
  if (result.value_class == transprecision_value_class_t::INFINITY
      || result.value_class == transprecision_value_class_t::NAN_VALUE)
    result.type = transprecision_type_t::UNCLASSIFIED;
  return result;
}

transprecision_operation_result_t analyze_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  const transprecision_type_t effective_type =
      transprecision_effective_type_ceiling(
          intended_execution_type, transprecision_type_t::FP32);
  if (!transprecision_is_supported_type(effective_type)) {
    const auto value_class = classify_raw_fp32_value(bits);
    return {
      special_classification(value_class, bits),
      make_quantization(effective_type, value_class, value_class, bits, bits),
      false,
    };
  }

  const auto quantization =
      quantize_transprecision_fp32_to_type(bits, effective_type);
  return {
    classify_transprecision_fp32_quantized_effective_type(
        static_cast<uint32_t>(quantization.quantized_bits),
        effective_type, policy),
    quantization,
    transprecision_type_less_than(
        effective_type, transprecision_type_t::FP32),
  };
}

transprecision_operation_result_t analyze_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  const transprecision_type_t effective_type =
      transprecision_effective_type_ceiling(
          intended_execution_type, transprecision_type_t::FP64);
  if (!transprecision_is_supported_type(effective_type)) {
    const auto value_class = classify_raw_fp64_value(bits);
    return {
      special_classification(value_class, bits),
      make_quantization(effective_type, value_class, value_class, bits, bits),
      false,
    };
  }

  const auto quantization =
      quantize_transprecision_fp64_to_type(bits, effective_type);
  return {
    classify_transprecision_fp64_quantized_effective_type(
        quantization.quantized_bits, effective_type, policy),
    quantization,
    transprecision_type_less_than(
        effective_type, transprecision_type_t::FP64),
  };
}

transprecision_classification_t classify_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  return analyze_transprecision_fp32_operation_result(
      bits, intended_execution_type, policy).classification;
}

transprecision_classification_t classify_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  return analyze_transprecision_fp64_operation_result(
      bits, intended_execution_type, policy).classification;
}

transprecision_classification_t classify_transprecision_fp32_architectural_write(
    uint32_t bits, const transprecision_policy_config_t& policy)
{
  return apply_nan_infinity_context(
      classify_transprecision_fp32(bits, policy), transprecision_type_t::E5M2);
}

transprecision_classification_t classify_transprecision_fp64_architectural_write(
    uint64_t bits, const transprecision_policy_config_t& policy)
{
  return apply_nan_infinity_context(
      classify_transprecision_fp64(bits, policy), transprecision_type_t::E5M2);
}
