require_either_extension('F', EXT_ZFINX);
require_fp;
(void) FRS1_F;
(void) FRS2_F;
OBSERVE_FRS1_FRS2_EFFECTIVE_TYPE();
WRITE_RD(f32_lt(FRS1_F, FRS2_F));
set_fp_exceptions;
