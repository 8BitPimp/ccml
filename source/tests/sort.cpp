#include "stdint.h"

#include "../lib/ccml.h"
#include "../lib/vm.h"
#include "../lib/parser.h"
#include "../lib/codegen.h"
#include "../lib/errors.h"

static const char *sort_prog = R"(
function main()
  var data[64]
  var i = 0

  # fill array with random data
  while (i < 64)
    data[i] = random()
    i = i + 1
  end

  # do 64 sort iterations
  i = 0
  while (i < 64)
    var j = 0
    while (j < 63)
      # sort ascending
      if (data[j] > data[j + 1])
        # swap data element
        var t = data[j]
        data[j] = data[j + 1]
        data[j + 1] = t
      end
      j = j + 1
    end
    i = i + 1
  end

  # check in the sorted array
  i = 0
  while (i < 64)
    check(data[i])
    i = i + 1
  end
end
)";

struct sort_test_t {

  static bool test_passing;

  // return random number between [0 -> 255]
  static void sys_random(struct ccml::thread_t &thread) {
    static uint32_t x = 0x12345;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    thread.push_int(x & 0xff);
  }

  // check that the output is in an ascending order
  static void sys_check(struct ccml::thread_t &thread) {
    static int32_t last = -1;
    const ccml::value_t *current = thread.pop();
    if (current->v < last) {
      test_passing = false;
    }
    last = current->v;
    // push dummy return value
    thread.push_int(0);
  }

  static bool run() {
    using namespace ccml;
    ccml_t ccml;
    // register syscalls
    ccml.add_function("random", sys_random, 0);
    ccml.add_function("check", sys_check, 1);
    // compile the program
    ccml::error_t error;
    if (!ccml.build(sort_prog, error)) {
      return false;
    }
    // run it
    const function_t* func = ccml.find_function("main");

    thread_t thread{ccml};
    if (!thread.prepare(*func, 0, nullptr)) {
      return false;
    }
    if (!thread.resume(1024 * 1024, false)) {
      return false;
    }
    if (!thread.finished()) {
      return false;
    }

    // currently                  99338
    // with INS_JMP:              95114
    // with !CJMP:                86792
    const uint32_t cycle_count = thread.cycle_count();

    const value_t *res = thread.return_code();
    return test_passing && !thread.has_error();
  }
};

bool sort_test_t::test_passing = true;

bool test_sort_1() {
  return sort_test_t::run();
}
