#pragma once
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <map>

#include "common.h"
#include "program.h"
#include "instructions.h"
#include "types.h"
#include "source.h"


namespace ccml {

struct ccml_t {

  ccml_t(program_t &prog);

  // accessors
  error_manager_t &errors()       { return *errors_;  }
  lexer_t         &lexer()        { return *lexer_.back(); }
  parser_t        &parser()       { return *parser_;  }
  ast_t           &ast()          { return *ast_;     }
  codegen_t       &codegen()      { return *codegen_; }

  bool build(source_manager_t &sources, error_t &error);

  void reset();

  // XXX: this will need to be abstracted somehow
  void add_function(const std::string &name, ccml_syscall_t sys, int32_t num_args);

  // enable codegen optimizations
  bool optimize;

protected:
  friend struct lexer_t;
  friend struct parser_t;
  friend struct codegen_t;
  friend struct token_stream_t;
  friend struct error_manager_t;
  friend struct pregen_functions_t;
  friend struct codegen_pass_t;

  // the current program we are building
  program_t &program_;

  // the source manager we are working with
  source_manager_t *sources_;
  // the current file we are parsing
  const source_t *source_;

  // objects used while compiling
  std::unique_ptr<error_manager_t> errors_;

  // stack of lexers, one per file
  std::vector<std::unique_ptr<lexer_t>> lexer_;

  std::unique_ptr<parser_t>  parser_;
  std::unique_ptr<ast_t>     ast_;
  std::unique_ptr<codegen_t> codegen_;
};

} // namespace ccml
