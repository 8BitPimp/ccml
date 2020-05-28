#pragma once
#include <cstring>
#include <string>
#include <array>
#include <map>

#include "nano.h"
#include "token.h"
#include "ast.h"
#include "program_builder.h"

#include "../lib_common/instructions.h"


namespace nano {

struct codegen_t {

  codegen_t(nano_t &c, program_t &prog);

  bool run(ast_program_t &program, error_t &error);

protected:
  nano_t &nano_;
  program_builder_t stream_;
};

} // namespace nano
