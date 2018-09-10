#pragma once
#include "ccml.h"
#include "token.h"


namespace ccml {

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct lexer_t {

  lexer_t(ccml_t &c)
    : ccml_(c)
    , stream_(c)
    , line_no_(0) {}

  bool lex(const char *source);

  // reset any stored state
  void reset();

  ccml_t &ccml_;
  token_stream_t stream_;

  const std::string &get_line(int32_t no) const {
    static const std::string empty;
    if (no >= 0 && no < int32_t(lines_.size())) {
      return lines_.at(no);
    }
    return empty;
  }

protected:
  uint32_t line_no_;

  std::vector<std::string> lines_;

  void push(token_e);
  void push_ident(const char *start, const char *end);
  void push_val(const char *start, const char *end);
  void push_string(const char *start, const char *end);
};

} // namespace ccml
