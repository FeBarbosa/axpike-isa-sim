require_extension('F');
require_fp;
OBSERVE_FRS2_EFFECTIVE_TYPE();
MMU.store<uint32_t>(RS1 + insn.s_imm(),static_cast<freg_t>(FRS2).v[0]);
