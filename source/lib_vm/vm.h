#pragma once
#include <vector>
#include <array>
#include <bitset>
#include <string>
#include <set>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <map>

#include "../lib_common/common.h"
#include "../lib_common/types.h"

#include "vm_gc.h"


namespace nano {

struct thread_t;

struct handlers_t {

  handlers_t()
    : on_member_get(nullptr)
    , on_member_set(nullptr)
    , on_array_get(nullptr)
    , on_array_set(nullptr)
    , on_equals(nullptr)
    , on_add(nullptr)
    , on_sub(nullptr)
    , on_mul(nullptr)
    , on_div(nullptr)
  {}

  // on_collect?

  bool (*on_member_get)(thread_t &t, value_t *v, const std::string &member);

  bool (*on_member_set)(thread_t &t, value_t *obj, value_t *expr, const std::string &member);

  bool (*on_array_get)(thread_t &t, value_t *a, const value_t *index);

  bool (*on_array_set)(thread_t &t, value_t *a, const value_t *index, value_t *val);

  bool (*on_equals)(thread_t &t, const value_t *l, const value_t *r);

  bool (*on_add)(thread_t &t, const value_t *l, const value_t *r);

  bool (*on_sub)(thread_t &t, const value_t *l, const value_t *r);

  bool (*on_mul)(thread_t &t, const value_t *l, const value_t *r);

  bool (*on_div)(thread_t &t, const value_t *l, const value_t *r);
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

  // run a frame and schedule all threads
  bool run_frame();

  // the currently bound program
  program_t &program_;

  // garbage collector
  std::unique_ptr<value_gc_t> gc_;
  void gc_collect();

  // globals
  std::vector<value_t*> g_;

  // threads
  std::map<thread_id_t, thread_t *> threads_;
  thread_id_t next_thread_id_;

  // handlers
  handlers_t handlers;
};

} // namespace nano
