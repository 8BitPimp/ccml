#include <cstdarg>

#include "ccml.h"

#include "lexer.h"
#include "parser.h"
#include "assembler.h"
#include "disassembler.h"
#include "vm.h"
#include "errors.h"


using namespace ccml;

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
ccml_t::ccml_t()
  : store_()
  , lexer_(new lexer_t(*this))
  , parser_(new parser_t(*this))
  , assembler_(new assembler_t(*this, store_.stream()))
  , disassembler_(new disassembler_t(*this))
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
  store_.reset();
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
code_store_t::code_store_t()
  : stream_(new asm_stream_t{*this}) {
}

void code_store_t::reset() {
  identifiers_.clear();
  line_table_.clear();
  stream_.reset(new asm_stream_t(*this));
}

bool code_store_t::active_vars(const uint32_t pc,
                               std::vector<const identifier_t *> &out) const {
  // collect all active identifiers
  for (const auto &ident : identifiers_) {
    if (pc >= ident.start && pc < ident.end) {
      out.push_back(&ident);
    }
  }
  return true;
}
