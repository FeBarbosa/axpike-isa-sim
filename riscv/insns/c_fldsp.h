require_extension(EXT_ZCD);
require_fp;
WRITE_FRD_D_ARCHITECTURAL(
    f64(MMU.load<uint64_t>(RVC_SP + insn.rvc_ldsp_imm())));
