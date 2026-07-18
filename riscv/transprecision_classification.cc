#include "transprecision_classification.h"

#include "adele/adf/LowPrecisionSimulation/typeConvertion.h"

namespace {

transprecision_classification_t special_classification(
    transprecision_value_class_t value_class)
{
  return {transprecision_type_t::UNCLASSIFIED, value_class};
}

transprecision_classification_t zero_classification()
{
  return {transprecision_type_t::E5M2, transprecision_value_class_t::ZERO};
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

transprecision_classification_t classify_transprecision_fp32(uint32_t bits)
{
  const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
  const uint32_t exponent = bits & UINT32_C(0x7f800000);
  const uint32_t fraction = bits & UINT32_C(0x007fffff);

  if (magnitude == 0)
    return zero_classification();
  if (exponent == UINT32_C(0x7f800000))
    return special_classification(fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE);

  if (typeSimulationFF(5, 2, bits) == bits)
    return {transprecision_type_t::E5M2, transprecision_value_class_t::FINITE};
  if (typeSimulationFF(5, 10, bits) == bits)
    return {transprecision_type_t::FP16, transprecision_value_class_t::FINITE};
  return {transprecision_type_t::FP32, transprecision_value_class_t::FINITE};
}

transprecision_classification_t classify_transprecision_fp64(uint64_t bits)
{
  const uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
  const uint64_t exponent = bits & UINT64_C(0x7ff0000000000000);
  const uint64_t fraction = bits & UINT64_C(0x000fffffffffffff);

  if (magnitude == 0)
    return zero_classification();
  if (exponent == UINT64_C(0x7ff0000000000000))
    return special_classification(fraction == 0
        ? transprecision_value_class_t::INFINITY
        : transprecision_value_class_t::NAN_VALUE);

  if (typeSimulationFF64(5, 2, bits) == bits)
    return {transprecision_type_t::E5M2, transprecision_value_class_t::FINITE};
  if (typeSimulationFF64(5, 10, bits) == bits)
    return {transprecision_type_t::FP16, transprecision_value_class_t::FINITE};
  if (typeSimulationFF64(8, 23, bits) == bits)
    return {transprecision_type_t::FP32, transprecision_value_class_t::FINITE};
  return {transprecision_type_t::FP64, transprecision_value_class_t::FINITE};
}

transprecision_classification_t classify_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type)
{
  return apply_operation_context(
      classify_transprecision_fp32(bits), intended_execution_type);
}

transprecision_classification_t classify_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type)
{
  return apply_operation_context(
      classify_transprecision_fp64(bits), intended_execution_type);
}

transprecision_classification_t classify_transprecision_fp32_architectural_write(
    uint32_t bits)
{
  return apply_nan_infinity_context(
      classify_transprecision_fp32(bits), transprecision_type_t::FP32);
}

transprecision_classification_t classify_transprecision_fp64_architectural_write(
    uint64_t bits)
{
  return apply_nan_infinity_context(
      classify_transprecision_fp64(bits), transprecision_type_t::FP64);
}
