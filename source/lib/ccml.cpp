#include <cstdarg>

#include "ccml.h"

#include "lexer.h"
#include "parser.h"
#include "assembler.h"
#include "vm.h"
#include "errors.h"


using namespace ccml;

ccml_t::ccml_t()
    : lexer_(new lexer_t(*this))
    , parser_(new parser_t(*this))
    , assembler_(new assembler_t(*this))
    , vm_(new vm_t(*this))
    , errors_(new error_manager_t(*this))
{
}

ccml_t::~ccml_t() {
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
  }
  catch (const error_t &e) {
    error = e;
    return false;
  }
  // success
  return true;
}

void ccml_t::reset() {
  lexer_->reset();
  parser_->reset();
  assembler_->reset();
  vm_->reset();
}
