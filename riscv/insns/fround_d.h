require_extension('D');
require_extension(EXT_ZFA);
require_fp;
WRITE_FRD_D_OPERATION_RESULT(f64_roundToInt(FRS1_D, RM, false),
    FRS1_EFFECTIVE_TYPE);
set_fp_exceptions;
