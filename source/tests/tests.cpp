#include <cassert>

#include "../lib/assembler.h"
#include "../lib/ccml.h"
#include "../lib/lexer.h"
#include "../lib/parser.h"
#include "../lib/vm.h"

#if _MSC_VER
extern "C" {
  int __stdcall IsDebuggerPresent(void);
}
#endif

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool return_value() {
  static const char *prog = R"(
function main()
  return 123
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 123;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool return_var() {
  static const char *prog = R"(
function func_name()
  var x = 1234
  return x
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("func_name")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 1234;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool return_arg() {
  static const char *prog = R"(
function test_arg_return(x)
  return x
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("test_arg_return")->pos_;
  int32_t input = 7654;
  const int32_t res = ccml.vm().execute(func, 1, &input, false);
  return res == input;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_arg_passing() {
  static const char *prog = R"(
function called(x, y, z)
  var dummy = 12345
  return y + x * z
end

function main()
  return called(2, 3, 4)
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 11;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_precedence_1() {
  static const char *prog = R"(
function main()
  return 2 + 3 * 4 + 5 * (6 + 3)
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 59;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_precedence_2() {
  static const char *prog = R"(
function main()
  return 2 * 3 > 4
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_precedence_3() {
  static const char *prog = R"(
function main()
  return 1 + 1 * 2
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 3;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_precedence_4() {
  static const char *prog = R"(
function main()
  return 1 + 2 > 2 and 2 * 5 == 10
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_precedence_5() {
  static const char *prog = R"(
function main()
  return not (1 + 2 > 2 and 2 * 5 == 10)
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 0;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_global_1() {
  static const char *prog = R"(
var global = 1234

function func_b()
  return global
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("func_b")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 1234;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_global_2() {
  static const char *prog = R"(
var global = 1234

function func_a()
  global = 987
end

function func_b()
  func_a()
  return global
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("func_b")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 987;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_global_3() {
  static const char *prog = R"(
var global = 0

function recurse( count )
  if (not count == 0)
    global = global + 1
    return recurse(count-1)
  else
    return global
  end
end

function driver()
  return recurse(15)
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("driver")->pos_;
  const int32_t res = ccml.vm().execute(func, 0, nullptr, false);
  return res == 15;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
static bool test_scope() {

  // XXX: this behavior is really weird and perhaps we should ban it
  //      or scope variables properly

  static const char *prog = R"(
function scope(flag)
  if (flag)
    var x = 1234
  end
  return x
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    // XXX: we should really error on this case
    return false;
  }
  const int32_t inputs[] = {0};
  const int32_t func = ccml.parser().find_function("scope")->pos_;
  const int32_t res = ccml.vm().execute(func, 1, inputs, false);
  return res == 1234;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// integer square route
static bool test_sqrt() {
  static const char *prog = R"(
function next(n, i)
  return (n + i / n) / 2
end

function abs(i)
  if (i >= 0)
    return i
  else
    return 0 - i
  end
end

function sqrt(number)
  var n = 1
  var n1 = next(n, number)
  while (abs(n1 - n) > 1)
    n  = n1
    n1 = next(n, number)
  end
  while (n1 * n1 > number)
    n1 = n1 - 1
  end
  return n1
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("sqrt")->pos_;
  int32_t input[] = {1234};
  const int32_t res = ccml.vm().execute(func, 1, input+0, false);
  return res == 35;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// prime number test
static bool test_is_prime() {
  static const char *prog = R"(
function is_prime(x)
  var i = 2
  while (i < (x / 2))
    if ((x % i) == 0)
      return 0
    end
    i = i + 1
  end
  return 1
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("is_prime")->pos_;
  const int32_t inputs[] = {
    9973,   // prime
    9977};  // nonprime
  const int32_t res1 = ccml.vm().execute(func, 1, inputs+0, false);
  const int32_t res2 = ccml.vm().execute(func, 1, inputs+1, false);
  return res1 == 1 && res2 == 0;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// check for coprime
static bool test_hcf() {
  static const char *prog = R"(
function hcf(a, b)
  var min = a
  var max = b
  if (a > b)
    min = b
    max = a
  end
  if ((max % min) == 0)
    return min
  else
    return hcf(max % min, min)
  end
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("hcf")->pos_;
  const int32_t inputs[] = {
    12, 25,   // coprime
    55, 42,   // coprime
    56, 42};  // non coprime
  const int32_t res1 = ccml.vm().execute(func, 2, inputs + 0, false);
  const int32_t res2 = ccml.vm().execute(func, 2, inputs + 2, false);
  const int32_t res3 = ccml.vm().execute(func, 2, inputs + 4, false);
  return res1 == 1 && res2 == 1 && res3 != 1;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// fibbonacci generator
static bool test_fib() {
  static const char *prog = R"(
function fib(count)
  var a = 0
  var b = 1
  while (count >= 2)
    var c = a + b
    a = b
    b = c
    count = count - 1
  end
  return b
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("fib")->pos_;
  const int32_t inputs[] = {9};
  const int32_t res = ccml.vm().execute(func, 1, inputs, false);
  return res == 34;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// greatest common divisor
static bool test_gcd() {
  static const char *prog = R"(
function main(a, b)
  while (not a == b)
    if (a > b)
      a = a - b
    else
      b = b - a
    end
  end
  return a
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t inputs[] = {81, 153};
  const int32_t res = ccml.vm().execute(func, 2, inputs, false);
  return res == 9;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// very silly triangle number test
static bool test_triangle() {
  static const char *prog = R"(
function main(a)
  var x = a
  var y
  var z = 0
  while (not x == 0)
    y = x
    while (not y == 0)
      z = z + 1
      y = y - 1
    end
    x = x - 1
  end
  return z
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("main")->pos_;
  const int32_t inputs[] = {3};
  const int32_t res = ccml.vm().execute(func, 1, inputs, false);
  return res == 6;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// calculate the weekday given a day, month, year
static bool test_weekday() {
  static const char *prog = R"(
# - 0 "Sunday"
# - 1 "Monday"
# - 2 "Tuesday"
# - 3 "Wednesday"
# - 4 "Thursday"
# - 5 "Friday"
# - 6 "Saturday"
function weekday(day, month, year)
  var a = 14-month
  a = a / 12
  var y = year-a
  var m = month+(12*a)-2
  var d = (day+y+(y/4)-(y/100)+(y/400)+((31*m)/12)) % 7
  return d
end
)";
  ccml_t ccml;
  if (!ccml.build(prog)) {
    return false;
  }
  const int32_t func = ccml.parser().find_function("weekday")->pos_;
  const int32_t inputs[] = {
    23,     // day
    8,      // month
    2018};  // year
  const int32_t res = ccml.vm().execute(func, 3, inputs, false);
  return res == 4; // thursday
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
typedef bool (*test_t)();
struct test_pair_t {
  const char *name;
  test_t func;
};

bool test_xfails();

#define TEST(X) (#X), X
static const test_pair_t tests[] = {
  TEST(return_value),
  TEST(return_var),
  TEST(return_arg),
  TEST(test_arg_passing),
  TEST(test_precedence_1),
  TEST(test_precedence_2),
  TEST(test_precedence_3),
  TEST(test_precedence_4),
  TEST(test_precedence_5),
  TEST(test_global_1),
  TEST(test_global_2),
  TEST(test_global_3),
  TEST(test_scope),
  TEST(test_sqrt),
  TEST(test_is_prime),
  TEST(test_hcf),
  TEST(test_fib),
  TEST(test_gcd),
  TEST(test_triangle),
  TEST(test_weekday),
  TEST(test_xfails),
  // sentinel
  nullptr, nullptr
};

static void pause() {
#if _MSC_VER
    if (IsDebuggerPresent()) {
      getchar();
    }
#endif
}

int main(const int argc, const char **args) {

  std::vector<std::string> fails;

  int32_t count = 0;

  printf("test log\n");
  printf("--------------------------------\n");

  const test_pair_t *pair = tests;
  for (; pair->name; ++pair) {

    assert(pair->name && pair->func);

    bool result = false;
    try {
      result = pair->func();
    }
    catch (const ccml_error_t &error) {
      (void)error;
      result = false;
    }

    if (!result) {
      fails.push_back(pair->name);
    }
    ++count;

    printf("%c %s\n", result ? '.' : 'F',  pair->name);
  }

  printf("--------------------------------\n");
  printf("%d tests, %d failures\n", count, int(fails.size()));
  for (const auto &name : fails) {
    printf("  ! %s\n", name.c_str());
  }

  if (!fails.empty()) {
    pause();
  }

  return int32_t(fails.size());
}
