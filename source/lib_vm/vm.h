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


namespace nano {

struct thread_t;

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

} // namespace nano
