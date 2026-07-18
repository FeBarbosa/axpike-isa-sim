require_extension('D');
require_rv64;
require_fp;
OBSERVE_FRS1_EFFECTIVE_TYPE();
WRITE_RD(static_cast<freg_t>(FRS1).v[0]);
