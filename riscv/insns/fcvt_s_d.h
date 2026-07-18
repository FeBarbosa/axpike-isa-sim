require_either_extension('D', EXT_ZDINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_F_OPERATION_RESULT(f64_to_f32(FRS1_D), FRS1_EFFECTIVE_TYPE);
set_fp_exceptions;
