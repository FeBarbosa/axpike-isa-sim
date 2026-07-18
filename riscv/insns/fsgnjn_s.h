require_either_extension('F', EXT_ZFINX);
require_fp;
WRITE_FRD_F_OPERATION_RESULT(fsgnj32(freg(FRS1_F), freg(FRS2_F), true, false),
    FRS1_FRS2_EFFECTIVE_TYPE);
