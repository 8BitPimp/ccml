#pragma once
#include <cstring>
#include <string>
#include <array>
#include <map>

#include "ccml.h"
#include "token.h"
#include "instructions.h"
#include "ast.h"


namespace ccml {

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// the asm stream bridges the assembler and the code store
struct asm_stream_t {

  asm_stream_t(code_store_t &store)
    : store_(store)
    , end(store.end())
    , start(store.data())
    , ptr(store.data())
  {
  }

  bool write8(const uint8_t data) {
    if (end - ptr > 1) {
      *ptr = data;
      ptr += 1;
      return true;
    }
    return false;
  }

  bool write32(const uint32_t data) {
    if (end - ptr > 4) {
      memcpy(ptr, &data, 4);
      ptr += 4;
      return true;
    }
    return false;
  }

  const uint8_t *tail() const {
    return end;
  }

  size_t written() const {
    return ptr - start;
  }

  const uint8_t *data() const {
    return start;
  }

  size_t size() const {
    return end - start;
  }

  uint32_t pos() const {
    return ptr - start;
  }

  uint8_t *head(int32_t adjust) const {
    uint8_t *p = ptr + adjust;
    assert(p < end && p >= start);
    return p;
  }

  // set the line number for the current pc
  // if line is nullptr then current lexer line is used
  void set_line(lexer_t &lexer, const token_t *line);

protected:
  code_store_t &store_;
  std::map<const uint8_t *, uint32_t> line_table_;

  const uint8_t *end;
  uint8_t *start;
  uint8_t *ptr;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct instruction_t {
  instruction_e opcode;
  int32_t operand;
  const token_t *token;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct codegen_t {

  codegen_t(ccml_t &c, asm_stream_t &stream);

  bool run(ast_program_t &program, error_t &error);

  void reset();

protected:
  ccml_t &ccml_;
  asm_stream_t &stream_;
};

} // namespace ccml
