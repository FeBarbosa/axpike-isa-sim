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
};

transprecision_classification_t classify_transprecision_fp32(uint32_t bits);
transprecision_classification_t classify_transprecision_fp64(uint64_t bits);
transprecision_classification_t classify_transprecision_fp32_operation_result(
    uint32_t bits, transprecision_type_t intended_execution_type);
transprecision_classification_t classify_transprecision_fp64_operation_result(
    uint64_t bits, transprecision_type_t intended_execution_type);
transprecision_classification_t classify_transprecision_fp32_architectural_write(
    uint32_t bits);
transprecision_classification_t classify_transprecision_fp64_architectural_write(
    uint64_t bits);

#endif
