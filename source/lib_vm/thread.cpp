#include <climits>
#include <cstring>

#include "thread.h"
#include "program.h"
#include "instructions.h"

#include "vm.h"

/*
 *   s_     STACK LAYOUT
 *        .  .  .  .  .  .
 *        |              |
 *   n    |var 0         |  <-- Frame 0 Pointer
 *   n-1  |arg 0         |
 *   n-2  |var 1         |
 *   n-3  |var 0         |  <-- Frame 1 Pointer
 *   n-4  |arg 1         |
 *   n-5  |arg 0         |
 *   n-6  |var 2         |
 *   n-7  |var 1         |
 *   n-8  |var 0         |  <-- Frame 2 Pointer
 *        |              |
 *        |....          |  ...
 *        |              |
 *   1    |global 1      |
 *   0    |global 0      |
 *        '--------------'
 */

namespace nano {

namespace {

bool to_string(char *buf, size_t size, const value_t *val) {
  switch (val->type()) {
  case val_type_float:
    snprintf(buf, size, "%f", val->f);
    return true;
  case val_type_int:
    snprintf(buf, size, "%i", val->integer());
    return true;
  case val_type_string:
    snprintf(buf, size, "%s", val->string());
    return true;
  case val_type_none:
    snprintf(buf, size, "none");
    return true;
  case val_type_func:
    snprintf(buf, size, "function@%d", val->v);
    return true;
  default:
    assert(false);
    return false;
  }
}

} // namespace {}

thread_t::thread_t(vm_t &vm)
  : waits(0)
  , cycles_(0)
  , finished_(true)
  , halted_(false)
  , pc_(0)
  , vm_(vm)
  , gc_(*(vm.gc_))
  , stack_(*this, *(vm.gc_))
  , user_data(nullptr)
{
  f_.reserve(16);
  reset();
}

int32_t thread_t::read_operand_() {
  const uint8_t *c = vm_.program_.data();
  assert(sizeof(pc_) < vm_.program_.size());
  const int32_t val = *(int32_t *)(c + pc_);
  pc_ += sizeof(int32_t);
  return val;
}

uint8_t thread_t::peek_opcode_() {
  const uint8_t *c = vm_.program_.data();
  assert(sizeof(pc_) < vm_.program_.size());
  const int8_t val = *(uint8_t *)(c + pc_);
  return val;
}

uint8_t thread_t::read_opcode_() {
  const uint8_t *c = vm_.program_.data();
  assert(sizeof(pc_) < vm_.program_.size());
  const int8_t val = *(uint8_t *)(c + pc_);
  pc_ += sizeof(uint8_t);
  return val;
}

void thread_t::do_INS_ADD_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // int like
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v + r->v);
    return;
  }
  // float like
  if (l->is_number() && r->is_number()) {
    stack_.push_float(l->as_float() + r->as_float());
    return;
  }
  if (l->is_a<val_type_string>()) {
    char rbuf[16] = {0};
    to_string(rbuf, sizeof(rbuf), r);
    int32_t rsize = int32_t(strlen(rbuf));
    const int32_t len = l->strlen() + rsize;
    value_t *s = gc_.new_string(len);
    char *dst = s->string();
    memcpy(dst, l->string(), l->strlen());
    memcpy(dst + l->strlen(), rbuf, rsize);
    dst[len] = '\0';
    stack_.push(s);
    return;
  }
  if (r->is_a<val_type_string>()) {
    char lbuf[16] = {0};
    to_string(lbuf, sizeof(lbuf), l);
    int32_t lsize = int32_t(strlen(lbuf));
    const int32_t len = lsize + r->strlen();
    value_t *s = gc_.new_string(len);
    char *dst = s->string();
    memcpy(dst, lbuf, lsize);
    memcpy(dst + lsize, r->string(), r->strlen());
    dst[len] = '\0';
    stack_.push(s);
    return;
  }
  // try a user handler
  if (vm_.handlers.on_add) {
    if (vm_.handlers.on_add(*this, l, r)) {
      return;
    }
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_SUB_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // int like
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v - r->v);
    return;
  }
  // float like
  if (l->is_number() && r->is_number()) {
    stack_.push_float(l->as_float() - r->as_float());
    return;
  }
  // try a user handler
  if (vm_.handlers.on_sub) {
    if (vm_.handlers.on_sub(*this, l, r)) {
      return;
    }
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_MUL_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // int like
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v * r->v);
    return;
  }
  // float like
  if (l->is_number() && r->is_number()) {
    stack_.push_float(l->as_float() * r->as_float());
    return;
  }

  // try a user handler
  if (vm_.handlers.on_mul) {
    if (vm_.handlers.on_mul(*this, l, r)) {
      return;
    }
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_DIV_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // int like
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    if (r->v == 0) {
      set_error_(thread_error_t::e_bad_divide_by_zero);
    } else {
      const int32_t o = l->v / r->v;
      stack_.push_int(o);
    }
    return;
  }
  // float like divide
  if (l->is_number() && r->is_number()) {
    stack_.push_float(l->as_float() / r->as_float());
    return;
  }
  // try a user handler
  if (vm_.handlers.on_div) {
    if (vm_.handlers.on_div(*this, l, r)) {
      return;
    }
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_MOD_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // int like
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    if (r->v == 0) {
      set_error_(thread_error_t::e_bad_divide_by_zero);
    } else {
      const int32_t o = l->v % r->v;
      stack_.push_int(o);
    }
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_AND_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  stack_.push_int(l->as_bool() && r->as_bool());
}

void thread_t::do_INS_OR_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  stack_.push_int(l->as_bool() || r->as_bool());
}

void thread_t::do_INS_NOT_() {
  const value_t *l = stack_.pop();
  stack_.push_int(!l->as_bool());
}

void thread_t::do_INS_NEG_() {
  const value_t *o = stack_.pop();
  if (o->is_a<val_type_int>()) {
    stack_.push_int(-o->v);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_LT_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // integer only comparison
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v < r->v ? 1 : 0);
    return;
  }
  // float like comparison
  if (l->is_number() && r->is_number()) {
    stack_.push_int(l->as_float() < r->as_float() ? 1 : 0);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_GT_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // integer only comparison
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v > r->v ? 1 : 0);
    return;
  }
  // float like comparison
  if (l->is_number() && r->is_number()) {
    stack_.push_int(l->as_float() > r->as_float() ? 1 : 0);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_LEQ_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // integer only comparison
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v <= r->v ? 1 : 0);
    return;
  }
  // float like comparison
  if (l->is_number() && r->is_number()) {
    stack_.push_int(l->as_float() <= r->as_float() ? 1 : 0);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_GEQ_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // integer only comparison
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v >= r->v ? 1 : 0);
    return;
  }
  if (l->is_number() && r->is_number()) {
    stack_.push_int(l->as_float() >= r->as_float() ? 1 : 0);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_EQ_() {
  const value_t *r = stack_.pop();
  const value_t *l = stack_.pop();
  // integer only comparison
  if (l->is_a<val_type_int>() && r->is_a<val_type_int>()) {
    stack_.push_int(l->v == r->v ? 1 : 0);
    return;
  }
  if (l->is_a<val_type_string>() &&
      r->is_a<val_type_string>()) {
    int32_t res = 0;
    if (l->strlen() == r->strlen()) {
      if (strcmp(l->string(), r->string()) == 0) {
        res = 1;
      }
    }
    stack_.push_int(res);
    return;
  }
  if (l->is_a<val_type_func>() &&
      r->is_a<val_type_func>()) {
    stack_.push_int((l->v == r->v) ? 1 : 0);
    return;
  }
  // only none == none
  if (l->is_a<val_type_none>() ||
      r->is_a<val_type_none>()) {
    stack_.push_int(l->is_a<val_type_none>() &&
                    r->is_a<val_type_none>() ? 1 : 0);
    return;
  }
  // float like comparison
  if (l->is_number() && r->is_number()) {
    // XXX: use epsilon here?
    stack_.push_int(
      l->as_float() == r->as_float() ? 1 : 0);
    return;
  }
  // check with user handlers
  if (vm_.handlers.on_equals) {
    if (vm_.handlers.on_equals(*this, l, r)) {
      return;
    }
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_JMP_() {
  pc_ = read_operand_();
}

void thread_t::do_INS_TJMP_() {
  const int32_t operand = read_operand_();
  const value_t *o = stack_.pop();
  if (o->as_bool()) {
    pc_ = operand;
  }
}

void thread_t::do_INS_FJMP_() {
  const int32_t operand = read_operand_();
  const value_t *o = stack_.pop();
  if (!o->as_bool()) {
    pc_ = operand;
  }
}

void thread_t::do_INS_CALL_() {
  const int32_t num_args = read_operand_();
  (void)num_args;
  const int32_t callee = read_operand_();
  // new frame
  enter_(stack_.head(), pc_, callee);
}

void thread_t::do_INS_RET_() {
  const int32_t operand = read_operand_();
  // pop return value
  value_t *sval = stack_.pop();
  // remove arguments and local vars
  if (int32_t(stack_.head()) < operand) {
    set_error_(thread_error_t::e_stack_underflow);
  } else {
    stack_.discard(operand);
    // push return value
    stack_.push(sval);
    // return to previous frame
    pc_ = leave_();
  }
}

void thread_t::do_syscall_(int32_t operand, int32_t num_args) {
  const auto &calls = vm_.program_.syscalls();
  assert(operand >= 0 && operand < int32_t(calls.size()));
  nano_syscall_t sys = calls[operand].call_;
  assert(sys);
  sys(*this, num_args);
}

void thread_t::do_INS_SCALL_() {
  const int32_t num_args = read_operand_();
  const int32_t operand = read_operand_();
  do_syscall_(operand, num_args);
}

void thread_t::do_INS_ICALL_() {
  const int32_t num_args = read_operand_();
  value_t *callee = stack_.pop();
  assert(callee);
  if (callee->is_a<val_type_syscall>()) {
    const int32_t operand = callee->v;
    do_syscall_(operand, num_args);
    return;
  }
  if (callee->is_a<val_type_func>()) {
    const int32_t addr = callee->v;
    // check number of arguments given to a function
    const function_t *func = vm_.program_.function_find(addr);
    assert(func);
    if (int32_t(func->num_args()) != num_args) {
      set_error_(thread_error_t::e_bad_num_args);
    }
    // new frame
    enter_(stack_.head(), pc_, addr);
    return;
  }
  set_error_(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_POP_() {
  const int32_t operand = read_operand_();
  for (int32_t i = 0; i < operand; ++i) {
    stack_.pop();
  };
}

void thread_t::do_INS_NEW_INT_() {
  value_t *op = gc_.new_int(read_operand_());
  stack_.push(op);
}

void thread_t::do_INS_NEW_STR_() {
  const int32_t index = read_operand_();
  const auto &str_tab = vm_.program_.strings();
  assert(index < (int32_t)str_tab.size());
  const std::string &s = str_tab[index];
  stack_.push_string(s);
}

void thread_t::do_INS_NEW_ARY_() {
  const int32_t index = read_operand_();
  assert(index > 0);
  stack_.push(gc_.new_array(index));
}

void thread_t::do_INS_NEW_NONE_() {
  stack_.push_none();
}

void thread_t::do_INS_NEW_FLT_() {
  uint32_t bits = read_operand_();
  float val = *(const float*)(&bits);
  value_t *op = gc_.new_float(val);
  stack_.push(op);
}

void thread_t::do_INS_NEW_FUNC_() {
  const int32_t index = read_operand_();
  assert(index >= 0);
  stack_.push_func(index);
}

void thread_t::do_INS_NEW_SCALL_() {
  const int32_t index = read_operand_();
  assert(index >= 0);
  stack_.push_syscall(index);
}

void thread_t::do_INS_GLOBALS_() {
  const int32_t operand = read_operand_();
  if (operand) {
    vm_.g_.resize(operand);
    memset(vm_.g_.data(), 0, sizeof(value_t*) * operand);
  }
}

void thread_t::do_INS_LOCALS_() {
  const int32_t operand = read_operand_();
  if (operand) {
    stack_.reserve(operand);
  }
}

void thread_t::do_INS_GETV_() {
  const int32_t operand = read_operand_();
  stack_.push(getv_(operand));
}

void thread_t::do_INS_SETV_() {
  const int32_t operand = read_operand_();
  setv_(operand, stack_.pop());
}

void thread_t::do_INS_GETG_() {
  const int32_t operand = read_operand_();
  if (operand < 0 || operand >= int32_t(vm_.g_.size())) {
    set_error_(thread_error_t::e_bad_get_global);
  } else {
    stack_.push(vm_.g_[operand]);
  }
}

void thread_t::do_INS_SETG_() {
  const int32_t operand = read_operand_();
  if (operand < 0 || operand >= int32_t(vm_.g_.size())) {
    set_error_(thread_error_t::e_bad_set_global);
  } else {
    vm_.g_[operand] = stack_.pop();
  }
}

void thread_t::do_INS_DEREF_() {
  value_t *a = stack_.pop();
  value_t *i = stack_.pop();

  // check user handlers for this action
  // XXX: can we try it at the end
  if (vm_.handlers.on_array_get) {
    if (vm_.handlers.on_array_get(*this, a, i)) {
      return;
    }
  }

  if (!a) {
    raise_error(thread_error_t::e_bad_array_object);
    return;
  }
  if (i->type() != val_type_int) {
    raise_error(thread_error_t::e_bad_array_index);
    return;
  }
  const int32_t index = i->v;
  if (a->type() == val_type_array) {
    if (index < 0 || index >= a->array_size()) {
      raise_error(thread_error_t::e_bad_array_bounds);
      return;
    }
    value_t *out = a->array()[index];
    stack_.push(out ? out : gc_.new_none());
    return;
  }
  if (a->type() == val_type_string) {
    if (index < 0 || index >= (int32_t)a->strlen()) {
      raise_error(thread_error_t::e_bad_array_bounds);
      return;
    }
    const uint32_t ch = (uint8_t)(a->string()[index]);
    stack_.push_int(ch);
    return;
  }
  raise_error(thread_error_t::e_bad_type_operation);
}

void thread_t::do_INS_SETA_() {
  value_t *a = stack_.pop();
  value_t *i = stack_.pop();
  value_t *v = stack_.pop();

  // check user handlers for this action
  // XXX: try this at the end
  if (vm_.handlers.on_array_set) {
    if (vm_.handlers.on_array_set(*this, a, i, v)) {
      return;
    }
  }

  if (a->type() != val_type_array) {
    raise_error(thread_error_t::e_bad_array_object);
    return;
  }
  if (i->type() != val_type_int) {
    raise_error(thread_error_t::e_bad_array_index);
    return;
  }
  const int32_t index = i->v;
  if (index < 0 || index >= a->array_size()) {
    raise_error(thread_error_t::e_bad_array_bounds);
    return;
  }
  if (v->is_a<val_type_array>()) {
    // we dont want to copy in this case since arrays are 'reference' type
    // objects. perhaps this can be copied into gc_.copy() soon.
    a->array()[index] = v;
  }
  else {
    a->array()[index] = gc_.copy(*v);
  }
}

void thread_t::do_INS_SETM_() {

  // pop value and operands
  value_t *obj = stack_.pop();
  value_t *expr = stack_.pop();
  const int32_t operand = read_operand_();

  // get the member string
  const auto &strtab = vm_.program_.strings();
  assert(strtab.size() > operand);
  const std::string &member = vm_.program_.strings()[operand];

  // try the user handler
  if (vm_.handlers.on_member_set) {
    if (vm_.handlers.on_member_set(*this, obj, expr, member)) {
      return;
    }
  }

  raise_error(thread_error_t::e_bad_member_access);
}

void thread_t::do_INS_GETM_() {

  // pop value and operands
  value_t *obj = stack_.pop();
  const int32_t operand = read_operand_();

  // get the member string
  const auto &strtab = vm_.program_.strings();
  assert(strtab.size() > operand);
  const std::string &member = vm_.program_.strings()[operand];

  if (obj->is_a<val_type_array>()) {
    if (member == "length") {
      const int32_t size = obj->array_size();
      stack_.push_int(size);
      return;
    }
  }

  // try a user handler if one is provided
  if (vm_.handlers.on_member_get) {
    if (vm_.handlers.on_member_get(*this, obj, member)) {
      // user says it was handled
      return;
    }
  }

  raise_error(thread_error_t::e_bad_member_access);
}

void thread_t::do_INS_ARY_INIT_() {
  const int32_t operand = read_operand_();
  assert(operand > 0);
  value_t *array = gc_.new_array(operand);
  value_t **data = array->array();
  for (int i = operand - 1; i >= 0; --i) {
    value_t *obj = stack_.pop();
    data[i] = obj;
  }
  stack_.push(array);
}

void thread_t::reset() {
  error_ = thread_error_t::e_success;
  stack_.clear();
  f_.clear();
  halted_ = false;
  finished_ = false;
}

bool thread_t::prepare(const function_t &func, int32_t argc,
                       const value_t **argv) {

  error_ = thread_error_t::e_success;
  finished_ = true;
  cycles_ = 0;
  halted_ = false;

  stack_.clear();

  // load the target pc (entry point)
  pc_ = func.code_start_;

  // verify num arguments
  if (int32_t(func.num_args()) != argc) {
    error_ = thread_error_t::e_bad_num_args;
    return false;
  }

  // push any arguments
  for (int i = 0; i < argc; ++i) {
    const value_t *v = argv[i];
    stack_.push(gc_.copy(*v));
  }

  // push the initial frame
  enter_(stack_.head(), pc_, func.code_start_);
  frame_().terminal_ = true;

  // catch any misc errors
  if (error_ != thread_error_t::e_success) {
    return false;
  }

  // good to go
  finished_ = false;
  return true;
}

void thread_t::step_imp_() {
  // fetch opcode
  const uint8_t opcode = read_opcode_();
  // dispatch
  switch (opcode) {
  case INS_ADD:      do_INS_ADD_();        break;
  case INS_SUB:      do_INS_SUB_();        break;
  case INS_MUL:      do_INS_MUL_();        break;
  case INS_DIV:      do_INS_DIV_();        break;
  case INS_MOD:      do_INS_MOD_();        break;
  case INS_AND:      do_INS_AND_();        break;
  case INS_OR:       do_INS_OR_();         break;
  case INS_NOT:      do_INS_NOT_();        break;
  case INS_NEG:      do_INS_NEG_();        break;
  case INS_LT:       do_INS_LT_();         break;
  case INS_GT:       do_INS_GT_();         break;
  case INS_LEQ:      do_INS_LEQ_();        break;
  case INS_GEQ:      do_INS_GEQ_();        break;
  case INS_EQ:       do_INS_EQ_();         break;
  case INS_JMP:      do_INS_JMP_();        break;
  case INS_TJMP:     do_INS_TJMP_();       break;
  case INS_FJMP:     do_INS_FJMP_();       break;
  case INS_CALL:     do_INS_CALL_();       break;
  case INS_RET:      do_INS_RET_();        break;
  case INS_SCALL:    do_INS_SCALL_();      break;
  case INS_ICALL:    do_INS_ICALL_();      break;
  case INS_POP:      do_INS_POP_();        break;
  case INS_NEW_ARY:  do_INS_NEW_ARY_();    break;
  case INS_NEW_INT:  do_INS_NEW_INT_();    break;
  case INS_NEW_STR:  do_INS_NEW_STR_();    break;
  case INS_NEW_NONE: do_INS_NEW_NONE_();   break;
  case INS_NEW_FLT:  do_INS_NEW_FLT_();    break;
  case INS_NEW_FUNC: do_INS_NEW_FUNC_();   break;
  case INS_NEW_SCALL: do_INS_NEW_SCALL_(); break;
  case INS_LOCALS:   do_INS_LOCALS_();     break;
  case INS_GLOBALS:  do_INS_GLOBALS_();    break;
  case INS_GETV:     do_INS_GETV_();       break;
  case INS_SETV:     do_INS_SETV_();       break;
  case INS_GETG:     do_INS_GETG_();       break;
  case INS_SETG:     do_INS_SETG_();       break;
  case INS_DEREF:    do_INS_DEREF_();      break;
  case INS_SETA:     do_INS_SETA_();       break;
  case INS_GETM:     do_INS_GETM_();       break;
  case INS_SETM:     do_INS_SETM_();       break;
  case INS_ARY_INIT: do_INS_ARY_INIT_();   break;
  default:
    set_error_(thread_error_t::e_bad_opcode);
  }
  // check for any breakpoints
  if (!breakpoints_.empty()) {
    // check if the source line is in our list
    const line_t l = get_source_line();
    if (breakpoints_.count(l)) {
      if (l != last_line_) {
        halted_ = true;
      }
    }
    // keep track of the last source line we were on
    last_line_ = l;
  }
  // increment cycle count
  ++cycles_;
}

void thread_t::enter_(uint32_t sp, uint32_t pc, uint32_t callee) {
  // create a new stack frame
  f_.emplace_back();
  frame_().sp_ = sp;
  frame_().return_ = pc;
  frame_().terminal_ = false;
  frame_().callee_ = callee;
  // jump to the new function
  pc_ = callee;
}

// return old PC as return value
uint32_t thread_t::leave_() {
  if (f_.empty()) {
    set_error_(thread_error_t::e_stack_underflow);
    return 0;
  } else {
    const uint32_t ret_pc = frame_().return_;
    // we have finished if this frame was terminal
    finished_ = frame_().terminal_;
    f_.pop_back();
    // if we have no more frames then we have reached the end anyway
    finished_ |= f_.empty();
    if (finished_) {
      assert(stack_.head() > 0);
    }
    return ret_pc;
  }
}

bool thread_t::step_inst() {
  if (finished_) {
    return false;
  }
  vm_.gc_collect();
  step_imp_();
  return !has_error();
}

bool thread_t::step_line() {
  if (finished_) {
    return false;
  }
  // get the current source line
  const line_t line = get_source_line();
  // step until the source line changes
  do {
    step_imp_();
    if (finished_) {
      break;
    }
    assert(!f_.empty());
    assert(!has_error());
  } while (line == get_source_line());
  return !has_error();
}

line_t thread_t::get_source_line() const {
  return vm_.program_.get_line(pc_);
}

void thread_t::tick_gc_(int32_t cycles) {
  (void)cycles;
  if (gc().should_collect()) {
    vm_.gc_collect();
  }
}

bool thread_t::resume(int32_t cycles) {
  if (finished_) {
    return false;
  }
  halted_ = false;
  // while we should keep processing instructions
  for (; cycles; --cycles) {
    tick_gc_(cycles);
    step_imp_();
    if (finished_) {
      if (has_error()) {
        if (vm_.handlers.on_thread_error) {
          vm_.handlers.on_thread_error(*this);
        }
      }
      if (vm_.handlers.on_thread_finish) {
        vm_.handlers.on_thread_finish(*this);
      }
      break;
    }
    if (halted_) {
      break;
    }
  }
  // cycles timeout
  return !has_error();
}

value_t *thread_t::getv_(int32_t offs) {
  const int32_t index = frame_().sp_ + offs;
  return stack_.get(index);
}

void thread_t::setv_(int32_t offs, value_t *val) {
  const int32_t index = frame_().sp_ + offs;
  stack_.set(index, val);
}

void thread_t::breakpoint_add(line_t line) {
  breakpoints_.insert(line);
}

void thread_t::breakpoint_remove(line_t line) {
  auto itt = breakpoints_.find(line);
  if (itt != breakpoints_.end()) {
    breakpoints_.erase(itt);
  }
}

void thread_t::breakpoint_clear() {
  breakpoints_.clear();
}

} // namespace nano
