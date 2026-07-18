require_either_extension('D', EXT_ZDINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_D_OPERATION_RESULT(
    f64_mulAdd(f64(FRS1_D.v ^ F64_SIGN), FRS2_D, FRS3_D),
    FRS1_FRS2_FRS3_EFFECTIVE_TYPE);
set_fp_exceptions;
