#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdint>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "../lib/ccml.h"
#include "../lib/vm.h"
#include "../lib/errors.h"
#include "../lib/parser.h"


std::array<char, 1024 * 64> program;


extern void print_history();


static uint32_t _seed = 12345;
uint32_t random(uint32_t max) {
    uint32_t &x = _seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return ((x * 0x4F6CDD1D) >> 8) % (max ? max : 1);
}


int main() {

  using namespace ccml;

  uint32_t tests = 0;
  std::vector<std::string> fails;

  for (int32_t i = 0, e = 0; i < 512; ++i) {

    std::string fname = "tests/" + std::string("test") + std::to_string(i) + ".txt";
    FILE *fd = fopen(fname.c_str(), "r");
    if (!fd) {
      if (++e > 2) {
        break;
      }
      continue;
    }
    e = 0;

    program.fill('\0');
    int read = fread(program.data(), 1, program.size(), fd);
    fclose(fd);
    if (read == 0) {
      continue;
    }
    program[read] = '\0';
    program[program.size() - 1] = '\0';

    // insert an error
#if 0
    while (random(2) == 1) {
      program[random(read)] = random(256);
    }
#endif

    ++tests;

    ccml_t ccml;
    // compile the program
    ccml::error_t error;
    if (!ccml.build(program.data(), error)) {
      fails.push_back(fname);
      fprintf(stderr, "%s\n", error.error.c_str());
      fprintf(stderr, "%d:  %s\n", error.line, ccml.lexer().get_line(error.line).c_str());
      continue;
    }

    const auto &funcs = ccml.functions();
    for (const auto &func : funcs) {

      thread_t thread{ccml};

      std::array<value_t, 16> args;
      assert(func.num_args_ < args.size());
      for (uint32_t j = 0; j < func.num_args_; ++j) {
        args[j].from_int(rand() & 0xff);
      }

      if (!thread.prepare(func, func.num_args_, args.data())) {
        continue;
      }
      if (!thread.resume(1024 * 8, false)) {
        continue;
      }
      if (!thread.finished()) {
        continue;
      }
    }
  }

  int32_t i = 0;
  for (const auto &name : fails) {
    printf("  ! %02d/%02d  %s\n", i, int32_t(fails.size()), name.c_str());
    ++i;
  }

  printf("Ran %d tests\n", tests);
  printf("%d failed\n", int32_t(fails.size()));

  if (!fails.empty()) {
    getchar();
  }

#if 0
  print_history();
#endif

#ifdef _MSC_VER
  // _CrtDumpMemoryLeaks();
#endif

  return fails.empty() ? 0 : 1;
}
