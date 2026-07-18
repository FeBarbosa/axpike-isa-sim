require_either_extension('D', EXT_ZDINX);
require_fp;
WRITE_FRD_D_OPERATION_RESULT(fsgnj64(freg(FRS1_D), freg(FRS2_D), false, true),
    FRS1_FRS2_EFFECTIVE_TYPE);
