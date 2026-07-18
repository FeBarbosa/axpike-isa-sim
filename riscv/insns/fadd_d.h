require_either_extension('D', EXT_ZDINX);
require_fp;
softfloat_roundingMode = RM;
WRITE_FRD_D_OPERATION_RESULT(f64_add(FRS1_D, FRS2_D),
    FRS1_FRS2_EFFECTIVE_TYPE);
set_fp_exceptions;
