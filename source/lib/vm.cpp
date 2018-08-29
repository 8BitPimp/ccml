#include "vm.h"
#include "assembler.h"
#include "parser.h"

#include <cstring>


bool thread_t::prepare(const function_t &func, int32_t argc, const int32_t *argv) {

  finished_ = true;
  cycles_ = 0;

  // push globals
  globals_.clear();
  for (const auto &g : ccml_.parser().globals()) {
    globals_.push_back(g.value_);
  }

  // save the target pc (entry point)
  pc_ = func.pos_;
  if (func.num_args_ != argc) {
    return_code_ = -1;
    error_ = "incorrect number of arguments";
    return false;
  }

  // push any arguments
  for (int32_t i = 0; i < argc; ++i) {
    push(argv[i]);
  }

  // push the initial frame
  new_frame(pc_);

  // good to go
  error_.clear();
  finished_ = false;
  return true;
}

bool thread_t::resume(uint32_t cycles, bool trace) {

  if (finished_) {
    return false;
  }

  // get the instruction stream
  const uint8_t *c = ccml_.assembler().get_code();

  // while we haven't returned from frame 0
  while (--cycles && f_.size()) {

    // increment the cycle count
    ++cycles_;

    if (trace) {
      // print an instruction trace
      printf(" > ");
      ccml_.assembler().disasm(c + pc_);
    }

    // get opcode
    const uint8_t op = c[pc_];
    pc_ += 1;

#define OPERATOR(OP)                                                           \
  {                                                                            \
    const int32_t r = pop(), l = pop();                                        \
    push(l OP r);                                                              \
  }
    switch (op) {
    case (INS_ADD): OPERATOR(+ ); continue;
    case (INS_SUB): OPERATOR(- ); continue;
    case (INS_MUL): OPERATOR(* ); continue;
    case (INS_DIV): OPERATOR(/ ); continue;
    case (INS_MOD): OPERATOR(% ); continue;
    case (INS_AND): OPERATOR(&&); continue;
    case (INS_OR):  OPERATOR(||); continue;
    case (INS_LT):  OPERATOR(< ); continue;
    case (INS_GT):  OPERATOR(> ); continue;
    case (INS_LEQ): OPERATOR(<=); continue;
    case (INS_GEQ): OPERATOR(>=); continue;
    case (INS_EQ):  OPERATOR(==); continue;
    case (INS_NOT): push(!pop()); continue;
    case (INS_NOP):
      continue;
    }
#undef OPERATOR

    // get operand
    const int32_t val = *(int32_t *)(c + pc_);
    pc_ += 4;

    switch (op) {
    case INS_SCALL: {
      const function_t *func = ccml_.parser().find_function(val);
      if (!func) {
        finished_ = true;
        error_ = "Unknown system call ID";
        return true;
        return_code_ = 0;
      }
      func->sys_(*this);
    } continue;
    case INS_JMP:
      pc_ = val;
      continue;
    case INS_CJMP:
      if (pop()) {
        pc_ = val;
      }
      continue;
    case INS_CALL:
      new_frame(pc_);
      pc_ = val;
      continue;
    case INS_RET:
      pc_ = ret(val);
      continue;
    case INS_POP:
      for (int i = 0; i < val; ++i) {
        pop();
      };
      continue;
    case INS_CONST:
      push(val);
      continue;
    case INS_GETV:
      push(getv(val));
      continue;
    case INS_SETV:
      setv(val, pop());
      continue;

    case INS_GETI:
      push(getv(val + pop()));
      continue;
    case INS_SETI: {
      const int32_t value = pop();
      setv(val + pop(), value);
    }
      continue;

    case INS_LOCALS:
      // reserve this many values on the stack
      if (val) {
        s_.resize(s_.size() + val, 0);
      }
      continue;
    case INS_GETG:
      push(globals_[val]);
      continue;
    case INS_SETG:
      globals_[val] = pop();
      continue;
    }
    // an exception has occurred
    {
      finished_ = true;
      error_ = "unknown instruction opcode";
      return_code_ = 0;
      return true;
    }
  } // while

  if (!finished_ && f_.empty()) {
    assert(s_.size() > 0);
    return_code_ = s_.back();
    finished_ = true;
  }

  return true;
}

bool vm_t::execute(const function_t &func, int32_t argc, const int32_t *argv, int32_t *ret, bool trace) {
  thread_t t{ccml_};
  if (!t.prepare(func, argc, argv)) {
    return false;
  }
  if (!t.resume(-1, trace)) {
    return false;
  }
  if (!t.finished()) {
    return false;
  }
  if (ret) {
    *ret = t.return_code();
  }
  return true;
}

void vm_t::reset() {
  // nothing to do
}

int32_t thread_t::getv(int32_t offs) {
  const int32_t index = f_.back().sp_ + offs;
  if (index < 0 || index >= int32_t(s_.size())) {
    ccml_.assembler().disasm();
    __debugbreak();
  }
  return s_[index];
}

void thread_t::setv(int32_t offs, int32_t val) {
  const int32_t index = f_.back().sp_ + offs;
  if (index < 0 || index >= int32_t(s_.size())) {
    ccml_.assembler().disasm();
    __debugbreak();
  }
  s_[index] = val;
}
