#pragma once
#include <vector>

#include "ccml.h"


enum class thread_error_t {
  e_success = 0,
  e_bad_getv,
  e_bad_setv,
  e_bad_num_args,
  e_bad_syscall,
  e_bad_opcode,
  e_bad_set_global,
  e_bad_get_global,
  e_bad_pop,
  e_bad_divide_by_zero
};

using value_t = int32_t;

struct thread_t {

  thread_t(ccml_t &ccml)
      : ccml_(ccml), return_code_(0), finished_(true), cycles_(0) {}

  int32_t pop() {
    if (s_.empty()) {
      set_error(thread_error_t::e_bad_pop);
      return 0;
    }
    const int32_t v = s_.back();
    s_.pop_back();
    return v;
  }

  void push(int32_t v) {
    s_.push_back(v);
  }

  bool prepare(const function_t &func, int32_t argc, const value_t *argv);

  bool resume(uint32_t cycles, bool trace);

  bool finished() const {
    return finished_;
  }

  int32_t return_code() const {
    return return_code_;
  }

  uint32_t cycle_count() const {
    return cycles_;
  }

  bool has_error() const {
    return error_ != thread_error_t::e_success;
  }

protected:
  friend struct vm_t;

  ccml_t &ccml_;
  int32_t return_code_;
  bool finished_;
  thread_error_t error_;
  uint32_t cycles_;

  struct frame_t {
    int32_t sp_;
    int32_t pc_;
  };

  int32_t pc_;                    // program counter
  std::vector<value_t> s_;        // value stack
  std::vector<frame_t> f_;        // frames

  void set_error(thread_error_t error) {
      finished_ = true;
      error_ = error;
      return_code_ = -1;
  }

  value_t getv(int32_t offs);
  void setv(int32_t offs, value_t val);

  int32_t _read_operand();
  uint8_t _read_opcode();

  void _do_INS_ADD();
  void _do_INS_INC();
  void _do_INS_SUB();
  void _do_INS_MUL();
  void _do_INS_DIV();
  void _do_INS_MOD();
  void _do_INS_AND();
  void _do_INS_OR();
  void _do_INS_NOT();
  void _do_INS_LT();
  void _do_INS_GT();
  void _do_INS_LEQ();
  void _do_INS_GEQ();
  void _do_INS_EQ();
  void _do_INS_JMP();
  void _do_INS_CJMP();
  void _do_INS_CALL();
  void _do_INS_RET();
  void _do_INS_SCALL();
  void _do_INS_POP();
  void _do_INS_CONST();
  void _do_INS_LOCALS();
  void _do_INS_GETV();
  void _do_INS_SETV();
  void _do_INS_GETVI();
  void _do_INS_SETVI();
  void _do_INS_GETG();
  void _do_INS_SETG();
  void _do_INS_GETGI();
  void _do_INS_SETGI();
};

struct vm_t {

  vm_t(ccml_t &c)
    : ccml_(c) {
  }

  bool execute(const function_t &func, int32_t argc, const value_t *argv,
               int32_t *ret = nullptr, bool trace = false);

  // reset any stored state
  void reset();

protected:
  ccml_t &ccml_;
};
