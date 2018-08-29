#include <cstring>

#include "assembler.h"
#include "lexer.h"
#include "ccml.h"
#include "errors.h"


namespace {

const char *gMnemonic[] = {
    "INS_ADD", "INS_SUB",   "INS_MUL",    "INS_DIV",  "INS_MOD",
    "INS_AND", "INS_OR",    "INS_NOT",    "INS_LT",   "INS_GT",
    "INS_LEQ", "INS_GEQ",   "INS_EQ",     "INS_JMP",  "INS_CALL",
    "INS_RET", "INS_POP",   "INS_CONST",  "INS_GETV", "INS_SETV",
    "INS_NOP", "INS_SCALL", "INS_LOCALS", "INS_GETG", "INS_SETG",
    "INS_GETI", "INS_SETI"};

// make sure this is kept up to date with the opcode table 'instruction_e'
static_assert(sizeof(gMnemonic) / sizeof(const char *) == __INS_COUNT__,
              "gMnemonic table should match instruction_e enum layout");
};

assembler_t::assembler_t(ccml_t &c)
  : ccml_(c)
  , head_(0)
{}

void assembler_t::write8(uint8_t v) {
  // check for code space overflow
  if (head_ + 1 >= code_.size()) {
    ccml_.errors().program_too_large();
  }
  // write int8_t to code stream
  code_[head_++] = v;
}

void assembler_t::write32(int32_t v) {
  // check for code space overflow
  if (head_ + 4 >= code_.size()) {
    ccml_.errors().program_too_large();
  }
  // write int32_t to code stream
  memcpy(code_.data() + head_, &v, sizeof(v));
  head_ += 4;
}

void assembler_t::emit(instruction_e ins, const token_t *t) {
  add_to_linetable(t);
  // encode this instruction
  switch (ins) {
  case INS_ADD:
  case INS_SUB:
  case INS_MUL:
  case INS_DIV:
  case INS_MOD:
  case INS_AND:
  case INS_OR:
  case INS_NOT:
  case INS_LT:
  case INS_GT:
  case INS_LEQ:
  case INS_GEQ:
  case INS_EQ:
  case INS_NOP:
    write8(ins);
    break;
  default:
    assert(!"unknown instruction");
  }
}

int32_t *assembler_t::emit(instruction_e ins, int32_t v, const token_t *t) {
  add_to_linetable(t);
  // encode this instruction
  switch (ins) {
  case INS_JMP:
  case INS_CALL:
  case INS_RET:
  case INS_POP:
  case INS_CONST:
  case INS_GETV:
  case INS_SETV:
  case INS_LOCALS:
  case INS_GETG:
  case INS_SETG:
  case INS_GETI:
  case INS_SETI:
  case INS_SCALL:
    write8(ins);
    write32(v);
    break;
  default:
    assert(!"unknown instruction");
  }
  // return the operand
  return (int32_t *)(code_.data() + (head_ - 4));
}

int32_t assembler_t::pos() const {
  return head_;
}

// return number of bytes disassembled or <= 0 on error
int32_t assembler_t::disasm(const uint8_t *ptr) const {

  // extract opcode
  uint32_t i = 0;
  const uint8_t op = ptr[i];
  i += 1;

  // instructions with no operand
  switch (op) {
  case INS_ADD:
  case INS_SUB:
  case INS_MUL:
  case INS_DIV:
  case INS_MOD:
  case INS_AND:
  case INS_OR:
  case INS_NOT:
  case INS_LT:
  case INS_GT:
  case INS_LEQ:
  case INS_GEQ:
  case INS_EQ:
  case INS_NOP:
    printf("%s\n", gMnemonic[op]);
    return i;
  }

  // syscall
  if (op == INS_SCALL) {
    void *call = 0;
    memcpy(&call, ptr + i, sizeof(void *));
    i += sizeof(void *);
    printf("%-12s %p\n", "INS_SCALL", call);
    return i;
  }

  // instruction with integer operand
  const int32_t val = *(int32_t *)(ptr + i);
  i += 4;

  switch (op) {
  case INS_JMP:
  case INS_CALL:
  case INS_RET:
  case INS_POP:
  case INS_CONST:
  case INS_GETV:
  case INS_SETV:
  case INS_LOCALS:
  case INS_GETG:
  case INS_SETG:
  case INS_GETI:
  case INS_SETI:
    printf("%-12s %d\n", gMnemonic[op], val);
    return i;
  }

  return 0;
}

int32_t assembler_t::disasm() {
  uint32_t count = 0;
  uint32_t line_no = -1;
  for (uint32_t i = 0; i < head_; ++count) {

    const uint8_t *ptr = code_.data() + i;
    bool print_line = false;

    // dump line table mapping
    auto itt = line_table_.find(ptr);
    if (itt != line_table_.end()) {
      if (line_no != itt->second) {
        print_line = true;
      }
      line_no = itt->second;
    }

    if (print_line) {
      const std::string &line = ccml_.lexer().get_line(line_no);
      printf("  %02d -- %s\n", line_no, line.c_str());
    }

    printf("%04d ", i);
    const int32_t nb = disasm(code_.data() + i);
    if (nb <= 0) {
      assert(!"unknown opcode");
    }
    i += nb;
  }
  return count;
}

void assembler_t::emit(token_e tok, const token_t *t) {
  switch (tok) {
  case TOK_ADD: emit(INS_ADD, t); break;
  case TOK_SUB: emit(INS_SUB, t); break;
  case TOK_MUL: emit(INS_MUL, t); break;
  case TOK_DIV: emit(INS_DIV, t); break;
  case TOK_MOD: emit(INS_MOD, t); break;
  case TOK_AND: emit(INS_AND, t); break;
  case TOK_OR:  emit(INS_OR , t); break;
  case TOK_NOT: emit(INS_NOT, t); break;
  case TOK_EQ:  emit(INS_EQ , t); break;
  case TOK_LT:  emit(INS_LT , t); break;
  case TOK_GT:  emit(INS_GT , t); break;
  case TOK_LEQ: emit(INS_LEQ, t); break;
  case TOK_GEQ: emit(INS_GEQ, t); break;
  default:
    assert(!"cant emit token type");
  }
}

int32_t &assembler_t::get_fixup() {
  // warning: if code_ can grow this will error
  assert(head_ >= sizeof(int32_t));
  return *reinterpret_cast<int32_t*>(code_.data() + (head_ - sizeof(int32_t)));
}

void assembler_t::add_to_linetable(const token_t *t) {
  // insert into the line table
  const uint8_t *ptr = code_.data() + head_;
  if (t) {
    line_table_[ptr] = t->line_no_;
  }
  else {
    line_table_[ptr] = ccml_.lexer().stream_.line_number();
  }
}

void assembler_t::reset() {
  code_.fill(0);
  head_ = 0;
  line_table_.clear();
}
