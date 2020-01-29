#include <cstdarg>

#include "ccml.h"

#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"
#include "disassembler.h"
#include "vm.h"
#include "phases.h"

using namespace ccml;

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
ccml_t::ccml_t()
  : program_()
  , lexer_(new lexer_t(*this))
  , parser_(new parser_t(*this))
  , ast_(new ast_t(*this))
  , codegen_(new codegen_t(*this, program_.stream()))
  , disassembler_(new disassembler_t(*this))
  , errors_(new error_manager_t(*this))
  , optimize(true)
{
  add_builtins_();
}

bool ccml_t::build(const char *source, error_t &error) {
  // clear the error
  error.clear();
  // lex into tokens
  try {
    if (!lexer_->lex(source)) {
      return false;
    }
    // parse into instructions
    if (!parser_->parse(error)) {
      return false;
    }

    // run semantic checker
    run_sema(*this);
    // run optmizer
    run_optimize(*this);
    // run pre-codegen passes
    run_pre_codegen(*this);

    // kick off the code generator
    if (!codegen_->run(ast_->program, error)) {
      return false;
    }

    // collect all garbage
    ast().gc();
  }
  catch (const error_t &e) {
    error = e;
    return false;
  }
  // success
  return true;
}

void ccml_t::reset() {

  functions_.clear();

  lexer_->reset();
  parser_->reset();
  ast_->reset();
  codegen_->reset();
  program_.reset();
}

function_t *ccml_t::find_function(const std::string &name) {
  for (auto &f : functions_) {
    if (f.name_ == name) {
      return &f;
    }
  }
  return nullptr;
}

const function_t *ccml_t::find_function(const std::string &name) const {
  for (const auto &f : functions_) {
    if (f.name_ == name) {
      return &f;
    }
  }
  return nullptr;
}

const function_t *ccml_t::find_function(const uint32_t pc) const {
  for (const auto &f : functions_) {
    if (pc >= f.code_start_ && pc < f.code_end_) {
      return &f;
    }
  }
  return nullptr;
}

void ccml_t::add_function(const std::string &name, ccml_syscall_t sys, int32_t num_args) {
  const function_t fn{name, sys, num_args};
  functions_.push_back(fn);
}
