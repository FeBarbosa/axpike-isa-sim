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

uint32_t clear_fp32_fraction_lsb(uint32_t bits, uint8_t bit_count)
{
  if (bit_count > 23)
    throw std::invalid_argument("FP32 mantissa mask exceeds 23 bits");
  if (bit_count == 0)
    return bits;

  const uint32_t ignored_mask = (UINT32_C(1) << bit_count) - UINT32_C(1);
  return bits & ~ignored_mask;
}

uint64_t clear_fp64_fraction_lsb(uint64_t bits, uint8_t bit_count)
{
  if (bit_count > 52)
    throw std::invalid_argument("FP64 mantissa mask exceeds 52 bits");
  if (bit_count == 0)
    return bits;

  const uint64_t ignored_mask =
      (UINT64_C(1) << bit_count) - UINT64_C(1);
  return bits & ~ignored_mask;
}

void validate_fp32_policy(const transprecision_policy_config_t& policy)
{
  if (policy.fp32_to_e5m2_protected_bits > 21)
    throw std::invalid_argument(
        "FP32 to E5M2 protected bits must be between 0 and 21");
  if (policy.fp32_to_fp16_protected_bits > 13)
    throw std::invalid_argument(
        "FP32 to FP16 protected bits must be between 0 and 13");
}

void validate_fp64_policy(const transprecision_policy_config_t& policy)
{
  if (policy.fp64_to_e5m2_protected_bits > 50)
    throw std::invalid_argument(
        "FP64 to E5M2 protected bits must be between 0 and 50");
  if (policy.fp64_to_fp16_protected_bits > 42)
    throw std::invalid_argument(
        "FP64 to FP16 protected bits must be between 0 and 42");
  if (policy.fp64_to_fp32_protected_bits > 29)
    throw std::invalid_argument(
        "FP64 to FP32 protected bits must be between 0 and 29");
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
  validate_fp32_policy(policy);

  const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
  const uint32_t exponent = bits & UINT32_C(0x7f800000);
  const uint32_t fraction = bits & UINT32_C(0x007fffff);

  if (magnitude == 0)
    return zero_classification(bits);
  if (exponent == UINT32_C(0x7f800000))
    return special_classification(fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE, bits);

  const uint8_t e5m2_ignored_bits =
      21 - policy.fp32_to_e5m2_protected_bits;
  const uint32_t e5m2_candidate_bits =
      clear_fp32_fraction_lsb(bits, e5m2_ignored_bits);
  if (typeSimulationFF(5, 2, e5m2_candidate_bits)
      == e5m2_candidate_bits) {
    return finite_classification(
        transprecision_type_t::E5M2, e5m2_candidate_bits, bits,
        magnitude != 0
            && (e5m2_candidate_bits & UINT32_C(0x7fffffff)) == 0);
  }

  const uint8_t fp16_ignored_bits =
      13 - policy.fp32_to_fp16_protected_bits;
  const uint32_t fp16_candidate_bits =
      clear_fp32_fraction_lsb(bits, fp16_ignored_bits);
  if (typeSimulationFF(5, 10, fp16_candidate_bits) == fp16_candidate_bits) {
    return finite_classification(
        transprecision_type_t::FP16, fp16_candidate_bits, bits,
        magnitude != 0
            && (fp16_candidate_bits & UINT32_C(0x7fffffff)) == 0);
  }

  return finite_classification(transprecision_type_t::FP32, bits, bits);
}

transprecision_classification_t classify_transprecision_fp64(uint64_t bits,
    const transprecision_policy_config_t& policy)
{
  validate_fp64_policy(policy);

  const uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
  const uint64_t exponent = bits & UINT64_C(0x7ff0000000000000);
  const uint64_t fraction = bits & UINT64_C(0x000fffffffffffff);

  if (magnitude == 0)
    return zero_classification(bits);
  if (exponent == UINT64_C(0x7ff0000000000000))
    return special_classification(fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE, bits);

  const uint8_t e5m2_ignored_bits =
      50 - policy.fp64_to_e5m2_protected_bits;
  const uint64_t e5m2_candidate_bits =
      clear_fp64_fraction_lsb(bits, e5m2_ignored_bits);
  if (typeSimulationFF64(5, 2, e5m2_candidate_bits)
      == e5m2_candidate_bits) {
    return finite_classification(
        transprecision_type_t::E5M2, e5m2_candidate_bits, bits,
        magnitude != 0
            && (e5m2_candidate_bits
                & UINT64_C(0x7fffffffffffffff)) == 0);
  }

  const uint8_t fp16_ignored_bits =
      42 - policy.fp64_to_fp16_protected_bits;
  const uint64_t fp16_candidate_bits =
      clear_fp64_fraction_lsb(bits, fp16_ignored_bits);
  if (typeSimulationFF64(5, 10, fp16_candidate_bits)
      == fp16_candidate_bits) {
    return finite_classification(
        transprecision_type_t::FP16, fp16_candidate_bits, bits,
        magnitude != 0
            && (fp16_candidate_bits
                & UINT64_C(0x7fffffffffffffff)) == 0);
  }

  const uint8_t fp32_ignored_bits =
      29 - policy.fp64_to_fp32_protected_bits;
  const uint64_t fp32_candidate_bits =
      clear_fp64_fraction_lsb(bits, fp32_ignored_bits);
  if (typeSimulationFF64(8, 23, fp32_candidate_bits)
      == fp32_candidate_bits) {
    return finite_classification(
        transprecision_type_t::FP32, fp32_candidate_bits, bits,
        magnitude != 0
            && (fp32_candidate_bits
                & UINT64_C(0x7fffffffffffffff)) == 0);
  }

  return finite_classification(transprecision_type_t::FP64, bits, bits);
}

transprecision_classification_t classify_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  return apply_operation_context(
      classify_transprecision_fp32(bits, policy), intended_execution_type);
}

transprecision_classification_t classify_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type,
    const transprecision_policy_config_t& policy)
{
  return apply_operation_context(
      classify_transprecision_fp64(bits, policy), intended_execution_type);
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
