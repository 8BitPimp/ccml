#include "instructions.h"

namespace nano {

bool ins_has_operand(const instruction_e ins) {
  switch (ins) {
  case INS_JMP:
  case INS_FJMP:
  case INS_TJMP:
  case INS_CALL:
  case INS_RET:
  case INS_SCALL:
  case INS_POP:
  case INS_NEW_INT:
  case INS_NEW_STR:
  case INS_NEW_FLT:
  case INS_LOCALS:
  case INS_GLOBALS:
  case INS_GETV:
  case INS_SETV:
  case INS_GETG:
  case INS_SETG:
  case INS_GETM:
  case INS_SETM:
  case INS_ARY_INIT:
    return true;
  default:
    return false;
  }
}

bool ins_will_branch(const instruction_e ins) {
  switch (ins) {
  case INS_JMP:
  case INS_FJMP:
  case INS_TJMP:
  case INS_CALL:
  case INS_RET:
    return true;
  default:
    return false;
  }
}

bool ins_is_binary_op(const instruction_e ins) {
  switch (ins) {
  case INS_ADD:
  case INS_SUB:
  case INS_MUL:
  case INS_DIV:
  case INS_MOD:
  case INS_AND:
  case INS_OR:
  case INS_EQ:
  case INS_LT:
  case INS_GT:
  case INS_LEQ:
  case INS_GEQ:
    return true;
  default:
    return false;
  }
}

bool ins_is_unary_op(const instruction_e ins) {
  switch (ins) {
  case INS_NOT:
  case INS_NEG:
    return true;
  default:
    return false;
  }
}

} // namespace nano
