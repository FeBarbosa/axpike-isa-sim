require_extension(EXT_ZCF);
require_fp;
WRITE_RVC_FRS2S_F_ARCHITECTURAL(
    f32(MMU.load<uint32_t>(RVC_RS1S + insn.rvc_lw_imm())));
