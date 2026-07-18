#include "isa_parser.h"
#include "decode_macros.h"
#include "softfloat.h"

#include <cassert>
#include <cstdint>

struct transprecision_execution_state_t
{
  transprecision_tag_file_t<NFPR> FPR_TAGS;
  transprecision_type_t last_transprecision_effective_type;
  uint64_t transprecision_effective_type_observations;
  transprecision_counters_t transprecision_counters;
  float32_t f32_regs[NFPR];
  float64_t f64_regs[NFPR];
};

struct transprecision_execution_write_t
{
  uint64_t reg;
  uint64_t bits;
};

struct transprecision_execution_processor_t
{
  struct {
    uint32_t cur_insn_id;
  } ax_control;

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

static void reset_execution_context()
{
  execution_state.FPR_TAGS.reset();
  execution_state.last_transprecision_effective_type =
      transprecision_type_t::UNCLASSIFIED;
  execution_state.transprecision_effective_type_observations = 0;
  execution_state.transprecision_counters.reset(8);
  for (size_t i = 0; i < NFPR; ++i) {
    execution_state.f32_regs[i] = f32(UINT32_C(0));
    execution_state.f64_regs[i] = f64(UINT64_C(0));
  }
  last_write.reg = UINT64_MAX;
  last_write.bits = UINT64_MAX;
  last_xwrite.reg = UINT64_MAX;
  last_xwrite.bits = UINT64_MAX;
  softfloat_exceptionFlags = 0;
}

static void record_execution_write(uint64_t reg, float32_t value)
{
  last_write.reg = reg;
  last_write.bits = value.v;
  execution_state.f32_regs[reg] = value;
}

static void record_execution_write(uint64_t reg, float64_t value)
{
  last_write.reg = reg;
  last_write.bits = value.v;
  execution_state.f64_regs[reg] = value;
}

static void record_execution_xwrite(uint64_t reg, uint64_t value)
{
  last_xwrite.reg = reg;
  last_xwrite.bits = value;
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
#define READ_FREG_F(reg) execution_state.f32_regs[(reg)]
#define READ_FREG_D(reg) execution_state.f64_regs[(reg)]
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
#define set_fp_exceptions softfloat_exceptionFlags = 0
#define RM softfloat_round_near_even

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
  assert(execution_state.transprecision_counters.promotion_from_to[0][2] == 1);
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
  assert(execution_state.transprecision_counters.result_narrow_from_to[2][0]
      == 1);
  assert(execution_state.transprecision_counters.operation_result_class_total[0]
      == 1);
}

static void check_fadd_s_unclassified_operand_is_conservative()
{
  reset_execution_context();

  execution_state.f32_regs[1] = f32(UINT32_C(0x3f800000));
  execution_state.f32_regs[2] = f32(UINT32_C(0x3f800000));
  execution_state.FPR_TAGS.write(1, transprecision_type_t::UNCLASSIFIED);
  execution_state.FPR_TAGS.write(2, transprecision_type_t::FP32);

  execute_fadd_s(insn_with_fp_regs(5, 1, 2));

  assert(last_write.reg == 5);
  assert(last_write.bits == UINT32_C(0x40000000));
  assert(execution_state.FPR_TAGS.read(5)
      == transprecision_type_t::UNCLASSIFIED);
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
  assert(execution_state.transprecision_effective_type_observations == 1);
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
  assert(execution_state.transprecision_effective_type_observations == 1);
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
  assert(execution_state.transprecision_effective_type_observations == 1);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters.effective_type_total[0] == 1);
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
  assert(execution_state.transprecision_effective_type_observations == 1);
  assert(execution_state.FPR_TAGS.read(5) == transprecision_type_t::E5M2);
  assert(execution_state.transprecision_counters.effective_type_total[0] == 1);
  assert(execution_state.transprecision_counters
      .effective_type_by_instruction[6][0] == 1);
  assert(execution_state.transprecision_counters.write_tag_total[0] == 1);
  assert(execution_state.transprecision_counters.operation_result_class_total[0]
      == 1);
}

int main()
{
  check_fadd_s_observable_promotion();
  check_fadd_s_finite_result_reclassification();
  check_fadd_s_unclassified_operand_is_conservative();
  check_fclass_s_observes_unary_effective_type();
  check_flt_s_observes_binary_effective_type();
  check_fmadd_s_ternary_effective_type();
  check_fadd_d_observable_promotion();
  check_fcvt_d_s_uses_source_effective_type();
  check_fcvt_s_d_uses_source_effective_type();

  return 0;
}
