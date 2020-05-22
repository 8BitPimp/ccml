#pragma once
#include <vector>
#include <array>
#include <bitset>
#include <string>
#include <set>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <set>

#include "common.h"
#include "types.h"

#include "vm_gc.h"


namespace ccml {

struct vm_t;

struct frame_t {
  // stack pointer
  int32_t sp_;

  // return address
  int32_t return_;

  // ?
  int32_t callee_;

  // ?
  bool terminal_;
};

struct thread_t {

  thread_t(vm_t &vm);
  ~thread_t();

  // prepare to execute a function
  bool prepare(const function_t &func, int32_t argc, const value_t *argv);

  // run for a number of clock cycles
  bool resume(int32_t cycles);

  // step a single instruction
  bool step_inst();

  // step a source line
  bool step_line();

  bool finished() const {
    return finished_;
  }

  // return the current error code
  value_t* get_return_code() const {
    return finished_ ? return_code_ : nullptr;
  }

  // return the total cycle count
  uint32_t get_cycle_count() const {
    return cycles_;
  }

  bool has_error() const {
    return error_ != thread_error_t::e_success;
  }

  thread_error_t get_error() const {
    return error_;
  }

  // return the current source line number
  line_t get_source_line() const;

  // raise a thread error
  void raise_error(thread_error_t e) {
    error_ = e;
    finished_ = true;
  }

  // return garbage collector
  value_gc_t &gc() {
    return gc_;
  }

  // return the value stack
  value_stack_t &get_stack() {
    return stack_;
  }

  // halt this thread
  void halt() {
    halted_ = true;
  }

  void reset();

  // unwind the stack
  void unwind();

  // return the program counter
  int32_t get_pc() const {
    return pc_;
  }

  const std::vector<frame_t> &frames() const {
    return f_;
  }

  void breakpoint_add(line_t line);
  void breakpoint_remove(line_t line);
  void breakpoint_clear();

protected:

  friend struct vm_t;

  // when we resume a thread we must keep track of the previous source line.
  // we need to do this so we can resume from a breakpoint without hitting it again.
  line_t last_line_;

  // the return value of the thread function
  value_t* return_code_;

  // set when a thread raises and error
  thread_error_t error_;

  // total elapsed cycle count for thread execution
  uint32_t cycles_;

  // true when a thread has finished executing
  bool finished_;

  // syscalls can set to true to halt execution
  bool halted_;

  // program counter
  int32_t pc_;

  // parent virtual machine
  vm_t &vm_;

  // garbage collector from parent vm
  value_gc_t &gc_;

  // frame stack
  std::vector<frame_t> f_;

  friend struct value_stack_t;
  value_stack_t stack_;

  std::set<line_t> breakpoints_;

  // execute thread init function which will initalize any globals
  bool call_init_();

  // tick the garbage collector
  void tick_gc_(int32_t cycles);

  // step a single instruction (internal)
  void step_imp_();

  // frame control
  void enter_(uint32_t sp, uint32_t ret, uint32_t callee);
  uint32_t leave_();

  // syscall helper
  void do_syscall_(int32_t index, int32_t num_args);

  // current stack frame
  const frame_t &frame_() const {
    assert(!f_.empty());
    return f_.back();
  }

  // current stack frame
  frame_t &frame_() {
    assert(!f_.empty());
    return f_.back();
  }

  // raise an error
  void set_error_(thread_error_t error) {
    finished_ = true;
    error_ = error;
    return_code_ = nullptr;
  }

  value_t* getv_(int32_t offs);
  void setv_(int32_t offs, value_t* val);

  int32_t read_operand_();
  uint8_t read_opcode_();
  uint8_t peek_opcode_();

  void do_INS_ADD_();
  void do_INS_SUB_();
  void do_INS_MUL_();
  void do_INS_DIV_();
  void do_INS_MOD_();
  void do_INS_AND_();
  void do_INS_OR_();
  void do_INS_NOT_();
  void do_INS_NEG_();
  void do_INS_LT_();
  void do_INS_GT_();
  void do_INS_LEQ_();
  void do_INS_GEQ_();
  void do_INS_EQ_();
  void do_INS_JMP_();
  void do_INS_TJMP_();
  void do_INS_FJMP_();
  void do_INS_CALL_();
  void do_INS_RET_();
  void do_INS_SCALL_();
  void do_INS_ICALL_();
  void do_INS_POP_();
  void do_INS_NEW_STR_();
  void do_INS_NEW_ARY_();
  void do_INS_NEW_NONE_();
  void do_INS_NEW_INT_();
  void do_INS_NEW_FLT_();
  void do_INS_NEW_FUNC_();
  void do_INS_NEW_SCALL_();
  void do_INS_LOCALS_();
  void do_INS_GLOBALS_();
  void do_INS_GETV_();
  void do_INS_SETV_();
  void do_INS_GETG_();
  void do_INS_SETG_();
  void do_INS_GETA_();
  void do_INS_SETA_();
};

struct vm_t {

  vm_t(program_t &program);

  void reset();

  // call the init function
  bool call_init();

  // execute a single function
  bool call_once(const function_t &func,
                 int32_t argc,
                 const value_t *argv,
                 value_t* &return_code,
                 thread_error_t &error);

  // the currently bound program
  program_t &program_;

  // garbage collector
  std::unique_ptr<value_gc_t> gc_;
  void gc_collect();

  // globals
  std::vector<value_t*> g_;

  // threads
  std::set<thread_t *> threads_;
};

} // namespace ccml
