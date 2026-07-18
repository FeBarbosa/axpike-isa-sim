require_either_extension('D', EXT_ZDINX);
require_fp;
OBSERVE_FRS1_FRS2_EFFECTIVE_TYPE();
WRITE_RD(f64_le(FRS1_D, FRS2_D));
set_fp_exceptions;
