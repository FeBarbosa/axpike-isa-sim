require_extension(EXT_ZCF);
require_fp;
OBSERVE_RVC_FRS2S_EFFECTIVE_TYPE();
MMU.store<uint32_t>(RVC_RS1S + insn.rvc_lw_imm(), static_cast<freg_t>(RVC_FRS2S).v[0]);
