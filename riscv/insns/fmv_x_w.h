require_extension('F');
require_fp;
(void) FRS1_F;
OBSERVE_FRS1_EFFECTIVE_TYPE();
WRITE_RD(sext32(static_cast<freg_t>(FRS1).v[0]));
