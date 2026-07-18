require_rv32;
require_extension('D');
require_extension(EXT_ZFA);
require_fp;
OBSERVE_FRS1_EFFECTIVE_TYPE();
ui64_f64 ui;
ui.f = FRS1_D;
WRITE_RD(sext32(ui.ui >> 32));
