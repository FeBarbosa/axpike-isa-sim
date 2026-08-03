#include "isa_parser.h"
#include "decode_macros.h"
#include "softfloat.h"

#include <cassert>
#include <cstdint>

struct transprecision_execution_state_t
{
  transprecision_tag_file_t<NFPR> FPR_TAGS;
  transprecision_fp64_load_boxing_file_t<NFPR> FPR_FP64_LOAD_BOXING;
  transprecision_type_t last_transprecision_effective_type;
  transprecision_counters_t transprecision_counters;
  float32_t f32_regs[NFPR];
  float64_t f64_regs[NFPR];
};

struct transprecision_execution_write_t
{
  uint64_t reg;
  uint64_t bits;
};

struct transprecision_execution_config_t
{
  transprecision_policy_config_t transprecision_policy;
};

static transprecision_policy_config_t execution_policy;

struct transprecision_execution_processor_t
{
  struct {
    uint32_t cur_insn_id;
    reg_t pc;
    insn_t insn;
  } ax_control;
  transprecision_execution_config_t config;

  transprecision_execution_processor_t()
  {
    config.transprecision_policy = execution_policy;
  }

  const transprecision_execution_config_t& get_cfg() const
  {
    return config;
  }

  bool extension_enabled(unsigned char ext) const
  {
    return ext == 'F' || ext == 'D';
  }

  bool extension_enabled(isa_extension_t ext) const
  {
    return false;
  }
};

static transprecision_execution_state_t execution_state;
static transprecision_execution_write_t last_write;
static transprecision_execution_write_t last_xwrite;
static uint_fast8_t last_fp_exceptions;
static uint_fast8_t execution_rounding_mode;

static void reset_execution_context()
{
  execution_state.FPR_TAGS.reset();
  execution_state.FPR_FP64_LOAD_BOXING.reset();
  execution_state.last_transprecision_effective_type =
      transprecision_type_t::UNCLASSIFIED;
  execution_state.transprecision_counters.reset(8);
  execution_policy = transprecision_policy_config_t();
  for (size_t i = 0; i < NFPR; ++i) {
    execution_state.f32_regs[i] = f32(UINT32_C(0));
    execution_state.f64_regs[i] = f64(UINT64_C(0));
  }
  last_write.reg = UINT64_MAX;
  last_write.bits = UINT64_MAX;
  last_xwrite.reg = UINT64_MAX;
  last_xwrite.bits = UINT64_MAX;
  last_fp_exceptions = 0;
  execution_rounding_mode = softfloat_round_near_even;
  softfloat_exceptionFlags = 0;
}

static void record_execution_write(uint64_t reg, float32_t value)
{
  execution_state.FPR_FP64_LOAD_BOXING.clear(reg);
  last_write.reg = reg;
  last_write.bits = value.v;
  execution_state.f32_regs[reg] = value;
}

static void record_execution_write(uint64_t reg, float64_t value)
{
  execution_state.FPR_FP64_LOAD_BOXING.clear(reg);
  last_write.reg = reg;
  last_write.bits = value.v;
  execution_state.f64_regs[reg] = value;
}

static void record_execution_xwrite(uint64_t reg, uint64_t value)
{
  last_xwrite.reg = reg;
  last_xwrite.bits = value;
}

static void record_fp_exceptions()
{
  last_fp_exceptions = softfloat_exceptionFlags;
  softfloat_exceptionFlags = 0;
}

// Keep this test focused on the instruction body and transprecision macros.
#undef STATE
#undef WRITE_REG
#undef WRITE_RD
#undef READ_FREG_F
#undef READ_FREG_D
#undef FRS1_F
#undef FRS1_D
#undef FRS2_F
#undef FRS2_D
#undef FRS3_F
#undef FRS3_D
#undef FREG_TAG
#undef FRS1_TAG
#undef FRS2_TAG
#undef FRS3_TAG
#undef FRS1_EFFECTIVE_TYPE
#undef FRS1_FRS2_EFFECTIVE_TYPE
#undef FRS1_FRS2_FRS3_EFFECTIVE_TYPE
#undef WRITE_FREG
#undef WRITE_FRD_F
#undef WRITE_FRD_D
#undef require_either_extension
#undef require_fp
#undef set_fp_exceptions
#undef RM
#define STATE execution_state
#define WRITE_REG(reg, value) record_execution_xwrite((reg), (value))
#define WRITE_RD(value) record_execution_xwrite(insn.rd(), (value))
#define READ_FREG_F(reg) ({ \
  const size_t __tp_test_reg = (reg); \
  const auto __tp_test_value = execution_state.f32_regs[__tp_test_reg]; \
  if (execution_state.FPR_FP64_LOAD_BOXING \
      .confirm_fp32_read(__tp_test_reg)) \
    execution_state.transprecision_counters \
        .record_fp64_load_nan_boxed_fp32_effective(); \
  RECOVER_TRANSPRECISION_FREG_TAG(__tp_test_reg, __tp_test_value.v, \
      transprecision_type_t::FP32, classify_transprecision_fp32); \
  __tp_test_value; \
})
#define READ_FREG_D(reg) ({ \
  const size_t __tp_test_reg = (reg); \
  const auto __tp_test_value = execution_state.f64_regs[__tp_test_reg]; \
  RECOVER_TRANSPRECISION_FREG_TAG(__tp_test_reg, __tp_test_value.v, \
      transprecision_type_t::FP64, classify_transprecision_fp64); \
  __tp_test_value; \
})
#define FRS1_F READ_FREG_F(insn.rs1())
#define FRS1_D READ_FREG_D(insn.rs1())
#define FRS2_F READ_FREG_F(insn.rs2())
#define FRS2_D READ_FREG_D(insn.rs2())
#define FRS3_F READ_FREG_F(insn.rs3())
#define FRS3_D READ_FREG_D(insn.rs3())
#define FREG_TAG(reg) STATE.FPR_TAGS.read(reg)
#define FRS1_TAG FREG_TAG(insn.rs1())
#define FRS2_TAG FREG_TAG(insn.rs2())
#define FRS3_TAG FREG_TAG(insn.rs3())
#define FRS1_EFFECTIVE_TYPE \
  TRANSPRECISION_EFFECTIVE_TYPE_WITH_OPERANDS(FRS1_TAG, FRS1_TAG)
#define FRS1_FRS2_EFFECTIVE_TYPE \
  TRANSPRECISION_EFFECTIVE_TYPE_WITH_OPERANDS( \
      transprecision_effective_type(FRS1_TAG, FRS2_TAG), FRS1_TAG, FRS2_TAG)
#define FRS1_FRS2_FRS3_EFFECTIVE_TYPE \
  TRANSPRECISION_EFFECTIVE_TYPE_WITH_OPERANDS( \
      transprecision_effective_type(FRS1_TAG, FRS2_TAG, FRS3_TAG), \
      FRS1_TAG, FRS2_TAG, FRS3_TAG)
#define WRITE_FREG(reg, value) record_execution_write((reg), value)
#define WRITE_FRD_F(value) record_execution_write(insn.rd(), value)
#define WRITE_FRD_D(value) record_execution_write(insn.rd(), value)
#define require_either_extension(A, B) ((void) 0)
#define require_fp ((void) 0)
#define set_fp_exceptions record_fp_exceptions()
#define RM execution_rounding_mode

static insn_t insn_with_fp_regs(uint64_t rd, uint64_t rs1, uint64_t rs2,
    uint64_t rs3 = 0)
{
  return insn_t((rd << 7) | (rs1 << 15) | (rs2 << 20) | (rs3 << 27));
}

static void execute_fadd_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 0;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fadd_s.h"
}

static void execute_fclass_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 1;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fclass_s.h"
}

static void execute_fclass_d(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 1;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fclass_d.h"
}

static void execute_flt_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 2;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/flt_s.h"
}

static void execute_fmadd_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 3;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fmadd_s.h"
}

static void execute_fadd_d(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 4;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fadd_d.h"
}

static void execute_fcvt_d_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 5;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fcvt_d_s.h"
}

static void execute_fcvt_s_d(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 6;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fcvt_s_d.h"
}

static void execute_fsgnj_s(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 7;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fsgnj_s.h"
}

static void execute_fsgnj_d(insn_t insn)
{
  transprecision_execution_processor_t processor;
  processor.ax_control.cur_insn_id = 8;
  transprecision_execution_processor_t* p = &processor;
  int xlen = 64;
  (void) p;
  (void) xlen;

#include "insns/fsgnj_d.h"
}

static void check_fadd_s_observable_promotion()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x7f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0xff800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_fadd_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert((last_write.bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000));
  assert((last_write.bits & UINT32_C(0x007fffff)) != 0);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP32);
  assert(execution_state.transprecision_counters
      .operand_promotion_from_to[0][2] == 1);
  assert(execution_state.transprecision_counters.operation_result_class_total[3]
      == 1);
}

static void check_fadd_s_finite_result_reclassification()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_fadd_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x40000000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .result_tag_reduction_total_from_to[2][0] == 1);
  assert(execution_state.transprecision_counters
      .result_tag_reduction_changed_from_to[2][0] == 0);
  assert(execution_state.transprecision_counters.operation_result_class_total[0]
      == 1);
}

static void check_fadd_s_recovers_unclassified_operand()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::UNCLASSIFIED);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_fadd_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x40000000));
  assert(execution_state.FPR_TAGS.read(1) == transprecision_type_t::E5M2);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 1);
  assert(execution_state.transprecision_counters
      .unclassified_fallback_total == 0);
  assert(execution_state.transprecision_counters
      .operand_unclassified_total == 0);
}

static void check_lazy_reclassification_uses_exact_policy()
{
  reset_execution_context();
  execution_policy.fp32_protected_bits = 0;

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f900001));
  softfloat_exceptionFlags = softfloat_flag_inexact;
  execute_fclass_s(insn_with_fp_regs(5, 1, 0));

  assert(execution_state.f32_regs[1].v == UINT32_C(0x3f900001));
  assert(execution_state.FPR_TAGS.read(1) == transprecision_type_t::FP32);
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP32);
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 1);
  assert(execution_state.transprecision_counters
      .unclassified_fallback_total == 0);
  assert(softfloat_exceptionFlags == softfloat_flag_inexact);

  execute_fclass_s(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 1);
}

static void check_lazy_reclassification_special_value_fallback()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x7fc12345));
  execute_fclass_s(insn_with_fp_regs(5, 1, 0));

  assert(execution_state.f32_regs[1].v == UINT32_C(0x7fc12345));
  assert(execution_state.FPR_TAGS.read(1) == transprecision_type_t::FP32);
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP32);
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 1);
  assert(execution_state.transprecision_counters
      .unclassified_fallback_total == 1);
  assert(execution_state.transprecision_counters
      .operand_unclassified_total == 0);
}

static void check_lazy_reclassification_fp64_carrier()
{
  reset_execution_context();

  execution_state.f64_regs[1] =
      f64(UINT64_C(0x3ff2000000000000));
  execute_fclass_d(insn_with_fp_regs(5, 1, 0));

  assert(execution_state.f64_regs[1].v
      == UINT64_C(0x3ff2000000000000));
  assert(execution_state.FPR_TAGS.read(1) == transprecision_type_t::FP16);
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP16);
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 1);
  assert(execution_state.transprecision_counters
      .unclassified_fallback_total == 0);
}

static void check_fclass_s_observes_unary_effective_type()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);

  execute_fclass_s(insn_with_fp_regs(5, 1, 0));

  assert(last_xwrite.reg == 5);
  assert(last_xwrite.bits == UINT64_C(0x40));
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::E5M2);
}

static void check_flt_s_observes_binary_effective_type()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x40000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_flt_s(insn_with_fp_regs(5, 1, 2));

  assert(last_xwrite.reg == 5);
  assert(last_xwrite.bits == 1);
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP32);
}

static void check_flt_s_recovers_before_observation()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f900000));

  execute_flt_s(insn_with_fp_regs(5, 1, 2));

  assert(last_xwrite.reg == 5);
  assert(last_xwrite.bits == 1);
  assert(execution_state.FPR_TAGS.read(1) == transprecision_type_t::E5M2);
  assert(execution_state.FPR_TAGS.read(2) == transprecision_type_t::FP16);
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP16);
  assert(execution_state.transprecision_counters
      .lazy_reclassification_total == 2);
  assert(execution_state.transprecision_counters
      .operand_unclassified_total == 0);
}

static void check_fmadd_s_ternary_effective_type()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x7f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[3] = f32(UINT32_C(0xff800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP16);
  execution_state.FPR_TAGS.write(3, transprecision_type_t::FP32);

  execute_fmadd_s(insn_with_fp_regs(5, 1, 2, 3));

  assert(last_write.reg == 5);
  assert((last_write.bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000));
  assert((last_write.bits & UINT32_C(0x007fffff)) != 0);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP32);
  assert((last_fp_exceptions & softfloat_flag_invalid) != 0);
}

static void check_fmadd_s_quantized_result_persists()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f8cc000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f8cc000));
  execution_state.f32_regs[3] = f32(UINT32_C(0x00000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP16);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP16);
  execution_state.FPR_TAGS.write(3, transprecision_type_t::E5M2);

  execute_fmadd_s(insn_with_fp_regs(5, 1, 2, 3));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x3f9ac000));
  assert(execution_state.f32_regs[5].v == UINT32_C(0x3f9ac000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP16);
  assert(execution_state.transprecision_counters
      .result_quantization_total_from_to[2][1] == 1);
  assert(execution_state.transprecision_counters
      .result_quantization_changed_from_to[2][1] == 1);
  assert(execution_state.transprecision_counters
      .invalid_result_promotion_total == 0);

  execution_state.f32_regs[4] = f32(UINT32_C(0x00000000));
  execution_state.FPR_TAGS.write(4, transprecision_type_t::E5M2);
  execute_fadd_s(insn_with_fp_regs(6, 5, 4));

  assert(last_write.reg == 6);
  assert(last_write.bits == UINT32_C(0x3f9ac000));
  assert(execution_state.FPR_TAGS.read(6) == transprecision_type_t::FP16);
}

static void check_fadd_s_effective_overflow_propagates_without_flags()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x477fe000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x477fe000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP16);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP16);

  execute_fadd_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x7f800000));
  assert(execution_state.f32_regs[5].v == UINT32_C(0x7f800000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP16);
  assert(last_fp_exceptions == 0);
  assert(execution_state.transprecision_counters
      .result_quantization_total_from_to[2][1] == 1);
  assert(execution_state.transprecision_counters
      .result_quantization_changed_from_to[2][1] == 1);
  assert(execution_state.transprecision_counters
      .result_quantization_overflow_from_to[2][1] == 1);
  assert(execution_state.transprecision_counters
      .result_quantization_underflow_from_to[2][1] == 0);
}

static void check_fadd_d_observable_promotion()
{
  reset_execution_context();

  execution_state.f64_regs[1] = f64(UINT64_C(0x7ff0000000000000));
  execution_state.f64_regs[2] = f64(UINT64_C(0xfff0000000000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP32);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP64);

  execute_fadd_d(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert((last_write.bits & UINT64_C(0x7ff0000000000000))
      == UINT64_C(0x7ff0000000000000));
  assert((last_write.bits & UINT64_C(0x000fffffffffffff)) != 0);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP64);
}

static void check_fcvt_d_s_uses_source_effective_type()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);

  execute_fcvt_d_s(insn_with_fp_regs(5, 1, 0));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT64_C(0x3ff0000000000000));
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::E5M2);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .effective_type_by_instruction[5][0] == 1);
  assert(execution_state.transprecision_counters.write_tag_total[0] == 1);
  assert(execution_state.transprecision_counters.operation_result_class_total[0]
      == 1);
}

static void check_fcvt_s_d_uses_source_effective_type()
{
  reset_execution_context();

  execution_state.f64_regs[1] = f64(UINT64_C(0x3ff0000000000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);

  execute_fcvt_s_d(insn_with_fp_regs(5, 1, 0));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x3f800000));
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::E5M2);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .effective_type_by_instruction[6][0] == 1);
  assert(execution_state.transprecision_counters.write_tag_total[0] == 1);
  assert(execution_state.transprecision_counters.operation_result_class_total[0]
      == 1);
}

static void check_fcvt_s_d_limits_result_type_to_fp32()
{
  reset_execution_context();

  execution_state.f64_regs[1] =
      f64(UINT64_C(0x3ff199999999999a));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP64);

  execute_fcvt_s_d(insn_with_fp_regs(5, 1, 0));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x3f8ccccd));
  assert(execution_state.last_transprecision_effective_type
      == transprecision_type_t::FP64);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::FP32);
  assert(execution_state.transprecision_counters
      .invalid_result_promotion_total == 0);
}

static void check_fcvt_s_d_rounding_and_fflags_boundaries()
{
  reset_execution_context();

  execution_state.f64_regs[1] =
      f64(UINT64_C(0x3ff0000010000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP64);
  execution_rounding_mode = softfloat_round_near_even;

  execute_fcvt_s_d(insn_with_fp_regs(5, 1, 0));

  assert(last_write.bits == UINT32_C(0x3f800000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert((last_fp_exceptions & softfloat_flag_inexact) != 0);

  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP64);
  execution_rounding_mode = softfloat_round_max;
  execute_fcvt_s_d(insn_with_fp_regs(6, 1, 0));

  assert(last_write.bits == UINT32_C(0x3f800001));
  assert(execution_state.FPR_TAGS.read(6) == transprecision_type_t::FP32);
  assert((last_fp_exceptions & softfloat_flag_inexact) != 0);
}

static void check_masked_value_persists_to_next_instruction()
{
  reset_execution_context();
  execution_policy.fp32_protected_bits = 0;

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f900001));
  execution_state.f32_regs[2] = f32(UINT32_C(0x00000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP32);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_fsgnj_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x3f800000));
  assert(execution_state.f32_regs[5].v == UINT32_C(0x3f800000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .result_tag_reduction_changed_from_to[2][0] == 1);

  execution_state.f32_regs[3] = f32(UINT32_C(0x00000000));
  execution_state.FPR_TAGS.write(3, transprecision_type_t::E5M2);
  execute_fadd_s(insn_with_fp_regs(6, 5, 3));

  assert(last_write.reg == 6);
  assert(last_write.bits == UINT32_C(0x3f800000));
  assert(execution_state.FPR_TAGS.read(6) == transprecision_type_t::E5M2);
}

static void check_fp64_masked_value_persists_to_next_instruction()
{
  reset_execution_context();
  execution_policy.fp64_protected_bits = 0;

  execution_state.f64_regs[1] =
      f64(UINT64_C(0x3ff0000020000001));
  execution_state.f64_regs[2] = f64(UINT64_C(0x0000000000000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::FP64);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP64);

  execute_fsgnj_d(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT64_C(0x3ff0000000000000));
  assert(execution_state.f64_regs[5].v
      == UINT64_C(0x3ff0000000000000));
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters
      .result_tag_reduction_changed_from_to[3][0] == 1);

  execution_state.f64_regs[3] = f64(UINT64_C(0x0000000000000000));
  execution_state.FPR_TAGS.write(3, transprecision_type_t::E5M2);
  execute_fadd_d(insn_with_fp_regs(6, 5, 3));

  assert(last_write.reg == 6);
  assert(last_write.bits == UINT64_C(0x3ff0000000000000));
  assert(execution_state.FPR_TAGS.read(6) == transprecision_type_t::E5M2);
}

static void check_fp64_load_nan_boxed_fp32_effective_counter()
{
  reset_execution_context();

  execution_state.f64_regs[1] =
      f64(UINT64_C(0xffffffff437f0000));
  execution_state.f32_regs[1] = f32(UINT32_C(0x437f0000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_FP64_LOAD_BOXING.mark_candidate(1, true);

  execute_fclass_d(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 0);
  assert(execution_state.FPR_FP64_LOAD_BOXING.read(1)
      == transprecision_fp64_load_boxing_state_t::
          NAN_BOXED_FP32_CANDIDATE);

  execute_fclass_s(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 1);
  assert(execution_state.FPR_FP64_LOAD_BOXING.read(1)
      == transprecision_fp64_load_boxing_state_t::
          NAN_BOXED_FP32_CONFIRMED);

  execute_fclass_s(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 1);
}

static void check_fp64_nan_and_overwrite_do_not_confirm_boxing()
{
  reset_execution_context();

  const auto canonical_nan = classify_transprecision_fp64_load(
      UINT64_C(0x7ff8000000000000));
  execution_state.f64_regs[1] =
      f64(UINT64_C(0x7ff8000000000000));
  execution_state.f32_regs[1] = f32(UINT32_C(0x00000000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::E5M2);
  execution_state.FPR_FP64_LOAD_BOXING.mark_candidate(
      1, canonical_nan.nan_boxed_fp32_candidate);
  execute_fclass_s(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 0);

  execution_state.FPR_FP64_LOAD_BOXING.mark_candidate(1, true);
  record_execution_write(1, f32(UINT32_C(0x3f800000)));
  assert(execution_state.FPR_FP64_LOAD_BOXING.read(1)
      == transprecision_fp64_load_boxing_state_t::NONE);
  execute_fclass_s(insn_with_fp_regs(5, 1, 0));
  assert(execution_state.transprecision_counters
      .fp64_load_nan_boxed_fp32_effective_total == 0);
}

static void check_transprecision_counters_partition_by_section()
{
  transprecision_counters_t counters;
  counters.reset(8);
  const transprecision_type_t operands[] = {
      transprecision_type_t::FP16,
      transprecision_type_t::FP32,
  };

  counters.set_section(1);
  counters.record_effective_type(
      3, transprecision_type_t::FP32, operands, 2);
  counters.set_section(2);
  counters.record_effective_type(
      3, transprecision_type_t::FP32, operands, 2);

  const size_t fp16 = transprecision_type_bucket(
      transprecision_type_t::FP16);
  const size_t fp32 = transprecision_type_bucket(
      transprecision_type_t::FP32);
  assert(counters.section_counters.size() == 3);
  assert(counters.effective_type_by_instruction[3][fp32] == 2);
  assert(counters.section_counters[0]
      .effective_type_by_instruction[3][fp32] == 0);
  assert(counters.section_counters[1]
      .effective_type_by_instruction[3][fp32] == 1);
  assert(counters.section_counters[2]
      .effective_type_by_instruction[3][fp32] == 1);
  assert(counters.operand_promotion_from_to[fp16][fp32] == 2);
  assert(counters.section_counters[1]
      .operand_promotion_from_to[fp16][fp32] == 1);
  assert(counters.section_counters[2]
      .operand_promotion_from_to[fp16][fp32] == 1);

  for (size_t repetition = 0; repetition < 1000; repetition++) {
    for (size_t section = 0; section < 8; section++) {
      counters.set_section(section);
      counters.record_effective_type(
          static_cast<uint32_t>(section), transprecision_type_t::FP32,
          operands, 2);
    }
  }
  for (size_t section = 0; section < 8; section++) {
    assert(counters.section_counters[section]
        .effective_type_by_instruction[section][fp32] == 1000);
  }
}

int main()
{
  check_fadd_s_observable_promotion();
  check_fadd_s_finite_result_reclassification();
  check_fadd_s_recovers_unclassified_operand();
  check_lazy_reclassification_uses_exact_policy();
  check_lazy_reclassification_special_value_fallback();
  check_lazy_reclassification_fp64_carrier();
  check_fclass_s_observes_unary_effective_type();
  check_flt_s_observes_binary_effective_type();
  check_flt_s_recovers_before_observation();
  check_fmadd_s_ternary_effective_type();
  check_fmadd_s_quantized_result_persists();
  check_fadd_s_effective_overflow_propagates_without_flags();
  check_fadd_d_observable_promotion();
  check_fcvt_d_s_uses_source_effective_type();
  check_fcvt_s_d_uses_source_effective_type();
  check_fcvt_s_d_limits_result_type_to_fp32();
  check_fcvt_s_d_rounding_and_fflags_boundaries();
  check_masked_value_persists_to_next_instruction();
  check_fp64_masked_value_persists_to_next_instruction();
  check_fp64_load_nan_boxed_fp32_effective_counter();
  check_fp64_nan_and_overwrite_do_not_confirm_boxing();
  check_transprecision_counters_partition_by_section();

  return 0;
}
