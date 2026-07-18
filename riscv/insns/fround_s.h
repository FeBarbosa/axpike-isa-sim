require_extension('F');
require_extension(EXT_ZFA);
require_fp;
WRITE_FRD_F_OPERATION_RESULT(f32_roundToInt(FRS1_F, RM, false),
    FRS1_EFFECTIVE_TYPE);
set_fp_exceptions;
