#include "decode_macros.h"

#include <cassert>
#include <cstdint>

struct transprecision_macro_state_t
{
  transprecision_tag_file_t<NFPR> FPR_TAGS;
  transprecision_fp64_load_boxing_file_t<NFPR> FPR_FP64_LOAD_BOXING;
  transprecision_type_t last_transprecision_effective_type;
  transprecision_counters_t transprecision_counters;
};

struct transprecision_macro_control_t
{
  uint32_t cur_insn_id;
  reg_t pc;
  insn_t insn;
};

struct transprecision_macro_config_t
{
  transprecision_policy_config_t transprecision_policy;
};

struct transprecision_macro_processor_t
{
  transprecision_macro_control_t ax_control;
  transprecision_macro_config_t config;

  const transprecision_macro_config_t& get_cfg() const
  {
    return config;
  }

};

struct transprecision_macro_write_t
{
  uint64_t reg;
  uint64_t bits;
};

static transprecision_macro_state_t macro_state;
static transprecision_macro_write_t last_write;
static transprecision_macro_processor_t macro_processor;
static transprecision_macro_processor_t* p = &macro_processor;

static void reset_macro_context()
{
  macro_state.FPR_TAGS.reset();
  macro_state.FPR_FP64_LOAD_BOXING.reset();
  macro_state.last_transprecision_effective_type =
      transprecision_type_t::UNCLASSIFIED;
  macro_state.transprecision_counters.reset(4);
  macro_processor.ax_control.cur_insn_id = 0;
  macro_processor.ax_control.pc = 0;
  macro_processor.ax_control.insn = insn_t(0);
  macro_processor.config.transprecision_policy =
      transprecision_policy_config_t();
  last_write.reg = UINT64_MAX;
  last_write.bits = UINT64_MAX;
}

static void record_macro_write(uint64_t reg, float32_t value)
{
  macro_state.FPR_FP64_LOAD_BOXING.clear(reg);
  last_write.reg = reg;
  last_write.bits = value.v;
}

static void record_macro_write(uint64_t reg, float64_t value)
{
  macro_state.FPR_FP64_LOAD_BOXING.clear(reg);
  last_write.reg = reg;
  last_write.bits = value.v;
}

// Keep the test focused on the architectural transprecision write macros.
#undef STATE
#undef WRITE_FREG
#define STATE macro_state
#define WRITE_FREG(reg, value) record_macro_write((reg), value)

static insn_t insn_with_rvc_rs2s(uint64_t reg)
{
  assert(reg >= 8 && reg < 16);
  return insn_t((reg - 8) << 2);
}

static void check_fp32_macro(uint32_t bits, transprecision_type_t expected_tag)
{
  reset_macro_context();

  WRITE_FREG_F_ARCHITECTURAL(3, f32(bits));

  assert(last_write.reg == 3);
  assert(last_write.bits == bits);
  assert(macro_state.FPR_TAGS.read(3) == expected_tag);
}

static void check_rvc_fp32_macro(uint32_t bits,
    transprecision_type_t expected_tag)
{
  reset_macro_context();
  insn_t insn = insn_with_rvc_rs2s(12);

  WRITE_RVC_FRS2S_F_ARCHITECTURAL(f32(bits));

  assert(last_write.reg == 12);
  assert(last_write.bits == bits);
  assert(macro_state.FPR_TAGS.read(12) == expected_tag);
}

static void check_fp64_macro(uint64_t bits, transprecision_type_t expected_tag)
{
  reset_macro_context();

  WRITE_FREG_D_ARCHITECTURAL(5, f64(bits));

  assert(last_write.reg == 5);
  assert(last_write.bits == bits);
  assert(macro_state.FPR_TAGS.read(5) == expected_tag);
}

static void check_fp32_operation_macro(uint32_t bits,
    transprecision_type_t intended_type, uint32_t expected_bits,
    transprecision_type_t expected_tag)
{
  reset_macro_context();

  WRITE_FREG_F_OPERATION_RESULT(7, f32(bits), intended_type);

  assert(last_write.reg == 7);
  assert(last_write.bits == expected_bits);
  assert(macro_state.FPR_TAGS.read(7) == expected_tag);
  const transprecision_type_t effective_type =
      transprecision_effective_type_ceiling(
          intended_type, transprecision_type_t::FP32);
  if (transprecision_type_less_than(expected_tag, effective_type)) {
    assert(macro_state.transprecision_counters
        .result_tag_reduction_total_from_to[
            transprecision_type_bucket(effective_type)]
            [transprecision_type_bucket(expected_tag)] == 1);
    assert(macro_state.transprecision_counters
        .result_tag_reduction_changed_from_to[
            transprecision_type_bucket(effective_type)]
            [transprecision_type_bucket(expected_tag)] == 0);
  }
  assert(macro_state.transprecision_counters
      .invalid_result_promotion_total == 0);
}

static void check_fp32_masked_architectural_macro()
{
  reset_macro_context();
  macro_processor.config.transprecision_policy
      .fp32_protected_bits = 0;

  WRITE_FREG_F_ARCHITECTURAL(3, f32(UINT32_C(0x00000001)));

  assert(last_write.reg == 3);
  assert(last_write.bits == UINT32_C(0x00000000));
  assert(macro_state.FPR_TAGS.read(3) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters
      .external_write_masked_from_to[2][0] == 1);
  assert(macro_state.transprecision_counters
      .external_write_masked_to_zero_total == 1);
  assert(macro_state.transprecision_counters.external_write_class_total[0]
      == 1);
}

static void check_fp32_masked_operation_macro()
{
  reset_macro_context();
  macro_processor.config.transprecision_policy
      .fp32_protected_bits = 0;

  WRITE_FREG_F_OPERATION_RESULT(
      7, f32(UINT32_C(0x3f900001)), transprecision_type_t::FP32);

  assert(last_write.reg == 7);
  assert(last_write.bits == UINT32_C(0x3f800000));
  assert(macro_state.FPR_TAGS.read(7) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_total_from_to[2][0] == 1);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_changed_from_to[2][0] == 1);
}

static void check_fp64_operation_macro(uint64_t bits,
    transprecision_type_t intended_type, uint64_t expected_bits,
    transprecision_type_t expected_tag)
{
  reset_macro_context();

  WRITE_FREG_D_OPERATION_RESULT(9, f64(bits), intended_type);

  assert(last_write.reg == 9);
  assert(last_write.bits == expected_bits);
  assert(macro_state.FPR_TAGS.read(9) == expected_tag);
  assert(macro_state.transprecision_counters
      .invalid_result_promotion_total == 0);
}

static void check_fp64_masked_macros()
{
  reset_macro_context();
  macro_processor.config.transprecision_policy
      .fp64_protected_bits = 0;

  WRITE_FREG_D_ARCHITECTURAL(
      5, f64(UINT64_C(0x3ff0000020000001)));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT64_C(0x3ff0000000000000));
  assert(macro_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters
      .external_write_masked_from_to[3][0] == 1);

  reset_macro_context();
  macro_processor.config.transprecision_policy
      .fp64_protected_bits = 0;

  WRITE_FREG_D_OPERATION_RESULT(9, f64(UINT64_C(0x3ff0000020000001)),
      transprecision_type_t::FP64);

  assert(last_write.reg == 9);
  assert(last_write.bits == UINT64_C(0x3ff0000000000000));
  assert(macro_state.FPR_TAGS.read(9) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_changed_from_to[3][0] == 1);
}

static void assert_no_special_value_transitions()
{
  for (size_t source = 0; source < transprecision_type_bucket_count;
       ++source) {
    for (size_t destination = 0;
         destination < transprecision_type_bucket_count; ++destination) {
      assert(macro_state.transprecision_counters
          .result_tag_reduction_total_from_to[source][destination] == 0);
      assert(macro_state.transprecision_counters
          .result_tag_reduction_changed_from_to[source][destination] == 0);
      assert(macro_state.transprecision_counters
          .external_write_masked_from_to[source][destination] == 0);
    }
  }
  assert(macro_state.transprecision_counters
      .invalid_result_promotion_total == 0);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_to_zero_total == 0);
  assert(macro_state.transprecision_counters
      .external_write_masked_to_zero_total == 0);
}

static void check_operation_counter_stage_separation()
{
  reset_macro_context();
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x3f8ccccd)),
      transprecision_type_t::FP16);
  assert(macro_state.transprecision_counters
      .result_quantization_total_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_changed_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_total_from_to[1][0] == 0);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_changed_from_to[1][0] == 0);

  reset_macro_context();
  macro_processor.config.transprecision_policy.fp32_protected_bits = 0;
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x3f900001)),
      transprecision_type_t::FP32);
  assert(macro_state.transprecision_counters
      .result_quantization_total_from_to[2][2] == 0);
  assert(macro_state.transprecision_counters
      .result_quantization_changed_from_to[2][2] == 0);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_total_from_to[2][0] == 1);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_changed_from_to[2][0] == 1);

  transprecision_counters_t identity_state_counters;
  identity_state_counters.record_operation_result(
      0, transprecision_type_t::FP32, transprecision_type_t::FP32,
      transprecision_type_t::FP32, true, true, false, false, false, false,
      false);
  assert(identity_state_counters
      .result_quantization_total_from_to[2][2] == 0);
  assert(identity_state_counters
      .result_quantization_changed_from_to[2][2] == 0);

  reset_macro_context();
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x00000001)),
      transprecision_type_t::FP16);
  assert(macro_state.transprecision_counters
      .result_quantization_changed_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_to_zero_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_underflow_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_tag_reduction_to_zero_total == 0);

  reset_macro_context();
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x38000000)),
      transprecision_type_t::E5M2);
  assert(last_write.bits == UINT32_C(0x38000000));
  assert(macro_state.transprecision_counters
      .result_quantization_total_from_to[2][0] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_changed_from_to[2][0] == 0);
  assert(macro_state.transprecision_counters
      .result_quantization_underflow_from_to[2][0] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_to_zero_from_to[2][0] == 0);

  reset_macro_context();
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x7f7fffff)),
      transprecision_type_t::FP16);
  assert(macro_state.transprecision_counters
      .result_quantization_changed_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .result_quantization_overflow_from_to[2][1] == 1);
  assert(macro_state.transprecision_counters
      .invalid_result_promotion_total == 0);

  transprecision_counters_t invalid_state_counters;
  invalid_state_counters.record_operation_result(
      0, transprecision_type_t::FP32, transprecision_type_t::FP16,
      transprecision_type_t::FP32, true, false, false, false, false, false,
      false);
  assert(invalid_state_counters.invalid_result_promotion_total == 1);
}

static void check_special_value_context_and_counters()
{
  reset_macro_context();
  WRITE_FREG_F_ARCHITECTURAL(3, f32(UINT32_C(0xffc12345)));
  assert(last_write.bits == UINT32_C(0xffc12345));
  assert(macro_state.FPR_TAGS.read(3) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters.external_write_class_total[3]
      == 1);
  assert(macro_state.transprecision_counters.operation_result_class_total[3]
      == 0);
  assert(macro_state.transprecision_counters.write_tag_total[0] == 1);
  assert_no_special_value_transitions();

  reset_macro_context();
  WRITE_FREG_D_ARCHITECTURAL(
      5, f64(UINT64_C(0xfff0000000000000)));
  assert(last_write.bits == UINT64_C(0xfff0000000000000));
  assert(macro_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters.external_write_class_total[2]
      == 1);
  assert(macro_state.transprecision_counters.operation_result_class_total[2]
      == 0);
  assert(macro_state.transprecision_counters.write_tag_total[0] == 1);
  assert_no_special_value_transitions();

  reset_macro_context();
  WRITE_FREG_F_OPERATION_RESULT(7, f32(UINT32_C(0x7f800000)),
      transprecision_type_t::FP16);
  assert(last_write.bits == UINT32_C(0x7f800000));
  assert(macro_state.FPR_TAGS.read(7) == transprecision_type_t::FP16);
  assert(macro_state.transprecision_counters.operation_result_class_total[2]
      == 1);
  assert(macro_state.transprecision_counters.external_write_class_total[2]
      == 0);
  assert(macro_state.transprecision_counters.write_tag_total[1] == 1);
  assert_no_special_value_transitions();

  reset_macro_context();
  WRITE_FREG_D_OPERATION_RESULT(
      9, f64(UINT64_C(0xfff8000000001234)),
      transprecision_type_t::FP32);
  assert(last_write.bits == UINT64_C(0xfff8000000001234));
  assert(macro_state.FPR_TAGS.read(9) == transprecision_type_t::FP32);
  assert(macro_state.transprecision_counters.operation_result_class_total[3]
      == 1);
  assert(macro_state.transprecision_counters.external_write_class_total[3]
      == 0);
  assert(macro_state.transprecision_counters.write_tag_total[2] == 1);
  assert_no_special_value_transitions();
}

static insn_t insn_with_fp_regs(uint64_t rs1, uint64_t rs2, uint64_t rs3)
{
  return insn_t((rs1 << 15) | (rs2 << 20) | (rs3 << 27));
}

static void check_effective_type_macros()
{
  reset_macro_context();

  macro_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  macro_state.FPR_TAGS.write(2, transprecision_type_t::FP32);
  macro_state.FPR_TAGS.write(3, transprecision_type_t::FP16);
  macro_state.FPR_TAGS.write(4, transprecision_type_t::FP64);

  insn_t insn = insn_with_fp_regs(1, 2, 4);
  assert(FRS1_EFFECTIVE_TYPE == transprecision_type_t::E5M2);
  assert(FRS1_FRS2_EFFECTIVE_TYPE == transprecision_type_t::FP32);
  assert(FRS1_FRS2_FRS3_EFFECTIVE_TYPE == transprecision_type_t::FP64);

  insn = insn_with_fp_regs(1, 0, 3);
  assert(FRS1_FRS2_EFFECTIVE_TYPE == transprecision_type_t::UNCLASSIFIED);
  assert(FRS1_FRS2_FRS3_EFFECTIVE_TYPE
      == transprecision_type_t::UNCLASSIFIED);
}

static void check_observe_effective_type_macros()
{
  reset_macro_context();

  macro_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  macro_state.FPR_TAGS.write(2, transprecision_type_t::FP32);
  macro_state.FPR_TAGS.write(3, transprecision_type_t::FP64);
  macro_state.FPR_TAGS.write(14, transprecision_type_t::FP16);

  insn_t insn = insn_with_fp_regs(1, 2, 3);

  OBSERVE_FRS1_EFFECTIVE_TYPE();
  assert(macro_state.last_transprecision_effective_type
      == transprecision_type_t::E5M2);
  assert(macro_state.transprecision_counters
      .effective_type_by_instruction[0][0] == 1);

  macro_processor.ax_control.cur_insn_id = 1;
  OBSERVE_FRS1_FRS2_EFFECTIVE_TYPE();
  assert(macro_state.last_transprecision_effective_type
      == transprecision_type_t::FP32);
  assert(macro_state.transprecision_counters
      .operand_promotion_from_to[0][2] == 1);

  macro_processor.ax_control.cur_insn_id = 2;
  OBSERVE_FRS2_EFFECTIVE_TYPE();
  assert(macro_state.last_transprecision_effective_type
      == transprecision_type_t::FP32);

  insn = insn_with_rvc_rs2s(14);
  macro_processor.ax_control.cur_insn_id = 3;
  OBSERVE_RVC_FRS2S_EFFECTIVE_TYPE();
  assert(macro_state.last_transprecision_effective_type
      == transprecision_type_t::FP16);
  assert(macro_state.transprecision_counters
      .effective_type_by_instruction[3][1] == 1);
}

static void check_rvc_fp64_macro(uint64_t bits,
    transprecision_type_t expected_tag)
{
  reset_macro_context();
  insn_t insn = insn_with_rvc_rs2s(14);

  WRITE_RVC_FRS2S_D_ARCHITECTURAL(f64(bits));

  assert(last_write.reg == 14);
  assert(last_write.bits == bits);
  assert(macro_state.FPR_TAGS.read(14) == expected_tag);
}

static void check_fp64_load_context_macros()
{
  reset_macro_context();

  WRITE_FREG_D_LOAD_ARCHITECTURAL(
      5, f64(UINT64_C(0xffffffff437f0000)));
  assert(last_write.reg == 5);
  assert(last_write.bits == UINT64_C(0xffffffff437f0000));
  assert(macro_state.FPR_FP64_LOAD_BOXING.read(5)
      == transprecision_fp64_load_boxing_state_t::
          NAN_BOXED_FP32_CANDIDATE);
  assert(macro_state.transprecision_counters.external_write_class_total[3]
      == 1);
  assert(macro_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 0);

  WRITE_FREG_D_ARCHITECTURAL(
      5, f64(UINT64_C(0xffffffff437f0000)));
  assert(macro_state.FPR_FP64_LOAD_BOXING.read(5)
      == transprecision_fp64_load_boxing_state_t::NONE);

  WRITE_FREG_D_LOAD_ARCHITECTURAL(
      5, f64(UINT64_C(0x7ff8000000000000)));
  assert(macro_state.FPR_FP64_LOAD_BOXING.read(5)
      == transprecision_fp64_load_boxing_state_t::NONE);

  insn_t insn = insn_with_rvc_rs2s(14);
  WRITE_RVC_FRS2S_D_LOAD_ARCHITECTURAL(
      f64(UINT64_C(0xffffffff00000000)));
  assert(macro_state.FPR_FP64_LOAD_BOXING.read(14)
      == transprecision_fp64_load_boxing_state_t::
          NAN_BOXED_FP32_CANDIDATE);
}

int main()
{
  check_fp32_macro(UINT32_C(0x3f800000), transprecision_type_t::E5M2);
  check_fp32_macro(UINT32_C(0x3f900000), transprecision_type_t::FP16);
  check_fp32_macro(UINT32_C(0x3f800001), transprecision_type_t::FP32);
  check_fp32_macro(UINT32_C(0x00000000), transprecision_type_t::E5M2);
  check_fp32_macro(UINT32_C(0x80000000), transprecision_type_t::E5M2);
  check_fp32_macro(UINT32_C(0x7f800000), transprecision_type_t::E5M2);
  check_fp32_macro(UINT32_C(0x7fc00000), transprecision_type_t::E5M2);
  check_rvc_fp32_macro(UINT32_C(0x3f900000), transprecision_type_t::FP16);
  check_fp32_masked_architectural_macro();

  check_fp64_macro(UINT64_C(0x3ff0000000000000), transprecision_type_t::E5M2);
  check_fp64_macro(UINT64_C(0x3ff2000000000000), transprecision_type_t::FP16);
  check_fp64_macro(UINT64_C(0x3ff0000020000000), transprecision_type_t::FP32);
  check_fp64_macro(UINT64_C(0x3ff0000000000001), transprecision_type_t::FP64);
  check_fp64_macro(UINT64_C(0x0000000000000000), transprecision_type_t::E5M2);
  check_fp64_macro(UINT64_C(0x8000000000000000), transprecision_type_t::E5M2);
  check_fp64_macro(UINT64_C(0x7ff0000000000000), transprecision_type_t::E5M2);
  check_fp64_macro(UINT64_C(0x7ff8000000000000), transprecision_type_t::E5M2);
  check_rvc_fp64_macro(UINT64_C(0x3ff0000020000000),
      transprecision_type_t::FP32);
  check_fp64_load_context_macros();

  check_fp32_operation_macro(UINT32_C(0x3f800000),
      transprecision_type_t::FP32, UINT32_C(0x3f800000),
      transprecision_type_t::E5M2);
  check_fp32_operation_macro(UINT32_C(0x3f800001),
      transprecision_type_t::FP16, UINT32_C(0x3f800000),
      transprecision_type_t::E5M2);
  check_fp32_operation_macro(UINT32_C(0x3f8ccccd),
      transprecision_type_t::FP16, UINT32_C(0x3f8cc000),
      transprecision_type_t::FP16);
  check_fp32_operation_macro(UINT32_C(0x3f800001),
      transprecision_type_t::FP64, UINT32_C(0x3f800001),
      transprecision_type_t::FP32);
  check_fp32_operation_macro(UINT32_C(0x00000000),
      transprecision_type_t::FP64, UINT32_C(0x00000000),
      transprecision_type_t::E5M2);
  check_fp32_operation_macro(UINT32_C(0x7fc00000),
      transprecision_type_t::FP16, UINT32_C(0x7fc00000),
      transprecision_type_t::FP16);
  check_fp32_operation_macro(UINT32_C(0x3f800000),
      transprecision_type_t::UNCLASSIFIED, UINT32_C(0x3f800000),
      transprecision_type_t::UNCLASSIFIED);
  check_fp32_masked_operation_macro();

  check_fp64_operation_macro(UINT64_C(0x3ff0000000000000),
      transprecision_type_t::FP64, UINT64_C(0x3ff0000000000000),
      transprecision_type_t::E5M2);
  check_fp64_operation_macro(UINT64_C(0x3ff0000000000001),
      transprecision_type_t::FP32, UINT64_C(0x3ff0000000000000),
      transprecision_type_t::E5M2);
  check_fp64_operation_macro(UINT64_C(0x3ff199999999999a),
      transprecision_type_t::FP32, UINT64_C(0x3ff19999a0000000),
      transprecision_type_t::FP32);
  check_fp64_operation_macro(UINT64_C(0x3ff199999999999a),
      transprecision_type_t::FP16, UINT64_C(0x3ff1980000000000),
      transprecision_type_t::FP16);
  check_fp64_operation_macro(UINT64_C(0x7ff8000000000000),
      transprecision_type_t::FP32, UINT64_C(0x7ff8000000000000),
      transprecision_type_t::FP32);
  check_fp64_operation_macro(UINT64_C(0x3ff0000000000000),
      transprecision_type_t::UNCLASSIFIED,
      UINT64_C(0x3ff0000000000000),
      transprecision_type_t::UNCLASSIFIED);
  check_fp64_masked_macros();
  check_special_value_context_and_counters();
  check_operation_counter_stage_separation();

  check_effective_type_macros();
  check_observe_effective_type_macros();

  return 0;
}
