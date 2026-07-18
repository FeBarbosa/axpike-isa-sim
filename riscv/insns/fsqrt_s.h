require_either_extension('F', EXT_ZFINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_F_OPERATION_RESULT(f32_sqrt(FRS1_F), FRS1_EFFECTIVE_TYPE);
set_fp_exceptions;
