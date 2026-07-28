require_either_extension('D', EXT_ZDINX);
require_fp;
(void) FRS1_D;
OBSERVE_FRS1_EFFECTIVE_TYPE();
WRITE_RD(f64_classify(FRS1_D));
