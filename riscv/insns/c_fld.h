require_extension(EXT_ZCD);
require_fp;
WRITE_RVC_FRS2S_D_ARCHITECTURAL(
    f64(MMU.load<uint64_t>(RVC_RS1S + insn.rvc_ld_imm())));
