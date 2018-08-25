#include "token.h"
#include "assembler.h"

token_stream_t::token_stream_t(ccml_t &ccml)
  : ccml_(ccml)
  , index_(0)
  , line_no_(0) {
}

token_e token_stream_t::type() {
  return stream_[index_].type_;
}

const token_t *token_stream_t::found(token_e type) {
  if (stream_[index_].type_ == type)
    return pop();
  return nullptr;
}

const token_t *token_stream_t::pop(token_e type) {
  if (const token_t *t = found(type))
    return t;
  const auto &tok = stream_[index_];
  static const char *expected = token_t::token_name(type);
  ccml_.on_error_(
    tok.line_no_,
    "unexpected token '%s' expecting '%s'",
                  tok.str_.c_str(), expected);
  return nullptr;
}

const token_t *token_stream_t::pop() {
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

const char *token_t::token_name(token_e e) {
  switch (e) {
  case TOK_FUNC:   return "function";
  case TOK_END:    return "end";
  case TOK_IF:     return "if";
  case TOK_ELSE:   return "else";
  case TOK_WHILE:  return "while";
  case TOK_VAR:    return "var";
  case TOK_VAL:    return "value";
  case TOK_IDENT:  return "identifier";
  case TOK_LPAREN: return "(";
  case TOK_RPAREN: return ")";
  case TOK_COMMA:  return ",";
  case TOK_EOL:    return "new line";
  case TOK_ADD:    return "+";
  case TOK_SUB:    return "-";
  case TOK_MUL:    return "*";
  case TOK_DIV:    return "/";
  case TOK_MOD:    return "%";
  case TOK_AND:    return "and";
  case TOK_OR:     return "or";
  case TOK_NOT:    return "not";
  case TOK_ASSIGN: return "=";
  case TOK_EQ:     return "==";
  case TOK_LT:     return "<";
  case TOK_GT:     return ">";
  case TOK_LEQ:    return "<=";
  case TOK_GEQ:    return ">=";
  case TOK_RETURN: return "return";
  case TOK_EOF:    return "end of file";
  default:
    assert(!"unhandled token type");
    return "";
  }
}
