require_either_extension('F', EXT_ZFINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_F_OPERATION_RESULT(
    f32_mulAdd(f32(FRS1_F.v ^ F32_SIGN), FRS2_F, FRS3_F),
    FRS1_FRS2_FRS3_EFFECTIVE_TYPE);
set_fp_exceptions;
