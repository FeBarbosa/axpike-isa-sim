// See LICENSE for license details.

#ifndef _RISCV_TRANSPRECISION_H
#define _RISCV_TRANSPRECISION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class transprecision_type_t : uint8_t
{
  E5M2 = 0,
  FP16 = 1,
  FP32 = 2,
  FP64 = 3,
  UNCLASSIFIED = UINT8_MAX,
};

static const size_t transprecision_type_bucket_count = 5;
static const size_t transprecision_value_class_bucket_count = 4;

template <size_t N>
class transprecision_tag_file_t
{
public:
  transprecision_tag_file_t()
  {
    reset();
  }

  void reset()
  {
    tags.fill(transprecision_type_t::UNCLASSIFIED);
  }

  transprecision_type_t read(size_t index) const
  {
    return tags[index];
  }

  void write(size_t index, transprecision_type_t type)
  {
    tags[index] = type;
  }

private:
  std::array<transprecision_type_t, N> tags;
};

static inline bool transprecision_is_supported_type(transprecision_type_t type)
{
  return type == transprecision_type_t::E5M2
      || type == transprecision_type_t::FP16
      || type == transprecision_type_t::FP32
      || type == transprecision_type_t::FP64;
}

static inline size_t transprecision_type_bucket(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
      return 0;
    case transprecision_type_t::FP16:
      return 1;
    case transprecision_type_t::FP32:
      return 2;
    case transprecision_type_t::FP64:
      return 3;
    case transprecision_type_t::UNCLASSIFIED:
    default:
      return 4;
  }
}

static inline const char* transprecision_type_name(transprecision_type_t type)
{
  switch (type) {
    case transprecision_type_t::E5M2:
      return "E5M2";
    case transprecision_type_t::FP16:
      return "FP16";
    case transprecision_type_t::FP32:
      return "FP32";
    case transprecision_type_t::FP64:
      return "FP64";
    case transprecision_type_t::UNCLASSIFIED:
      return "UNCLASSIFIED";
    default:
      return "UNKNOWN";
  }
}

static inline const char* transprecision_type_bucket_name(size_t bucket)
{
  switch (bucket) {
    case 0:
      return "E5M2";
    case 1:
      return "FP16";
    case 2:
      return "FP32";
    case 3:
      return "FP64";
    case 4:
      return "UNCLASSIFIED";
    default:
      return "UNKNOWN";
  }
}

static inline transprecision_type_t transprecision_max_type(
    transprecision_type_t lhs, transprecision_type_t rhs)
{
  if (!transprecision_is_supported_type(lhs)
      || !transprecision_is_supported_type(rhs))
    return transprecision_type_t::UNCLASSIFIED;

  return static_cast<uint8_t>(lhs) >= static_cast<uint8_t>(rhs) ? lhs : rhs;
}

static inline bool transprecision_type_less_than(
    transprecision_type_t lhs, transprecision_type_t rhs)
{
  return transprecision_is_supported_type(lhs)
      && transprecision_is_supported_type(rhs)
      && static_cast<uint8_t>(lhs) < static_cast<uint8_t>(rhs);
}

struct transprecision_counters_t
{
  std::array<uint64_t, transprecision_type_bucket_count> effective_type_total;
  std::vector<std::array<uint64_t, transprecision_type_bucket_count> >
      effective_type_by_instruction;
  std::array<std::array<uint64_t, transprecision_type_bucket_count>,
      transprecision_type_bucket_count> promotion_from_to;
  std::array<std::array<uint64_t, transprecision_type_bucket_count>,
      transprecision_type_bucket_count> result_narrow_from_to;
  std::array<uint64_t, transprecision_value_class_bucket_count>
      operation_result_class_total;
  std::array<uint64_t, transprecision_type_bucket_count> write_tag_total;
  uint64_t operand_unclassified_total;

  transprecision_counters_t()
  {
    reset(0);
  }

  void reset(size_t instruction_count)
  {
    effective_type_total.fill(0);
    effective_type_by_instruction.assign(
        instruction_count,
        std::array<uint64_t, transprecision_type_bucket_count>());
    for (auto& row : effective_type_by_instruction)
      row.fill(0);
    for (auto& row : promotion_from_to)
      row.fill(0);
    for (auto& row : result_narrow_from_to)
      row.fill(0);
    operation_result_class_total.fill(0);
    write_tag_total.fill(0);
    operand_unclassified_total = 0;
  }

  void record_effective_type(uint32_t instruction_id,
      transprecision_type_t effective_type,
      const transprecision_type_t* operands, size_t operand_count)
  {
    const size_t effective_bucket =
        transprecision_type_bucket(effective_type);
    effective_type_total[effective_bucket]++;
    if (instruction_id < effective_type_by_instruction.size())
      effective_type_by_instruction[instruction_id][effective_bucket]++;

    for (size_t i = 0; i < operand_count; i++) {
      const transprecision_type_t operand_type = operands[i];
      if (operand_type == transprecision_type_t::UNCLASSIFIED)
        operand_unclassified_total++;
      if (transprecision_type_less_than(operand_type, effective_type)) {
        promotion_from_to[transprecision_type_bucket(operand_type)]
            [effective_bucket]++;
      }
    }
  }

  void record_write_tag(transprecision_type_t tag)
  {
    write_tag_total[transprecision_type_bucket(tag)]++;
  }

  void record_operation_result(uint8_t value_class_bucket,
      transprecision_type_t effective_type, transprecision_type_t result_tag)
  {
    if (value_class_bucket < operation_result_class_total.size())
      operation_result_class_total[value_class_bucket]++;
    if (transprecision_type_less_than(result_tag, effective_type)) {
      result_narrow_from_to[transprecision_type_bucket(effective_type)]
          [transprecision_type_bucket(result_tag)]++;
    }
  }
};

static inline transprecision_type_t transprecision_effective_type(
    transprecision_type_t lhs, transprecision_type_t rhs)
{
  return transprecision_max_type(lhs, rhs);
}

static inline transprecision_type_t transprecision_effective_type(
    transprecision_type_t lhs, transprecision_type_t rhs,
    transprecision_type_t third)
{
  const transprecision_type_t pair_type =
      transprecision_effective_type(lhs, rhs);
  return transprecision_effective_type(pair_type, third);
}

template <size_t N>
static inline transprecision_type_t transprecision_effective_type_from_regs(
    const transprecision_tag_file_t<N>& tags, size_t lhs, size_t rhs)
{
  return transprecision_effective_type(tags.read(lhs), tags.read(rhs));
}

template <size_t N>
static inline transprecision_type_t transprecision_effective_type_from_regs(
    const transprecision_tag_file_t<N>& tags, size_t lhs, size_t rhs,
    size_t third)
{
  return transprecision_effective_type(tags.read(lhs), tags.read(rhs),
      tags.read(third));
}

#endif
