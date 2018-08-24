#include "token.h"
#include "assembler.h"

token_t *token_stream_t::pop(token_e type) {
  if (token_t *t = found(type))
    return t;
  const auto &tok = stream_[index_];
  static const char *expected = ccml_.assembler().token_name(type);
  ccml_.on_error_(
    tok.line_no_,
    "unexpected token '%s' expecting '%s'",
                  tok.str_.c_str(), expected);
  return nullptr;
}

token_t *token_stream_t::pop() {
  line_no_ = stream_[index_].line_no_;
  return &stream_[index_++];
}

void token_stream_t::push(const token_t &tok) {
  stream_.push_back(tok);
}

void token_stream_t::reset() {
  index_ = 0;
  stream_.clear();
}
