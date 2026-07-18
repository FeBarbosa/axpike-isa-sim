require_extension('F');
require_extension(EXT_ZFA);
require_fp;
OBSERVE_FRS1_FRS2_EFFECTIVE_TYPE();
WRITE_RD(f32_lt_quiet(FRS1_F, FRS2_F));
set_fp_exceptions;
