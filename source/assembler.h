#pragma once
#include "ccml.h"
#include "token.h"
#include <cstring>
#include <string>

enum instruction_e {
  INS_ADD,
  INS_SUB,
  INS_MUL,
  INS_DIV,
  INS_MOD,
  INS_AND,
  INS_OR,
  INS_NOT,
  INS_LT,
  INS_GT,
  INS_LEQ,
  INS_GEQ,
  INS_EQ,
  INS_JMP,
  INS_CALL,     // call a function
  INS_RET,      // return to previous frame {popping args}
  INS_POP,      // pop constant from stack
  INS_CONST,    // push constant
  INS_GETV,     // get variable
  INS_SETV,     // set variable
  INS_NOP,      // no operation
  INS_SCALL,    // system call
  INS_LOCALS,   // number of locals to reserve on the stack
};

struct assembler_t {

  assembler_t(ccml_t &c);

  // cjmp (dst unknown)
  int32_t *cjmp();

  // cjmp (dst known)
  void cjmp(int32_t pos);

  void emit(token_e);

  void emit(instruction_e ins);
  void emit(instruction_e ins, int32_t v);
  void emit(ccml_syscall_t sys);

  int32_t pos() const;

  int32_t disasm(const uint8_t *ptr) const;
  int32_t disasm();

  // return a reference to the last instructions operand
  int32_t &get_fixup();

  const uint8_t *get_code() const { return code_; }

protected:
  void write8(uint8_t v);
  void write32(int32_t v);

  ccml_t &ccml_;
  uint32_t head_;
  uint8_t code_[1024];
};
