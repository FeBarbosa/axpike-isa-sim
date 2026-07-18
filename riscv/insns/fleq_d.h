require_extension('D');
require_extension(EXT_ZFA);
require_fp;
OBSERVE_FRS1_FRS2_EFFECTIVE_TYPE();
WRITE_RD(f64_le_quiet(FRS1_D, FRS2_D));
set_fp_exceptions;
