require_either_extension('F', EXT_ZFINX);
require_fp;
(void) FRS1_F;
OBSERVE_FRS1_EFFECTIVE_TYPE();
WRITE_RD(f32_classify(FRS1_F));
