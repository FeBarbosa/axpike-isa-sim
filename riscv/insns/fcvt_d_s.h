require_either_extension('D', EXT_ZDINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_D_OPERATION_RESULT(f32_to_f64(FRS1_F), FRS1_EFFECTIVE_TYPE);
set_fp_exceptions;
