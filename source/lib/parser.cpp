#include "parser.h"
#include "assembler.h"
#include "lexer.h"
#include "token.h"
#include "errors.h"


bool parser_t::parse() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //    var <TOK_IDENT> [ = <TOK_VAL> ]
  //    function <TOK_IDENT> ( [ <TOK_IDENT> [ , <TOK_IDENT> ]+ ] )

  while (!stream_.found(TOK_EOF)) {
    if (stream_.found(TOK_EOL)) {
      // consume any blank lines
      continue;
    }
    if (stream_.found(TOK_VAR)) {
      parse_global();
      continue;
    }
    if (stream_.found(TOK_FUNC)) {
      parse_function();
      continue;
    }

    const token_t *tok = stream_.pop();
    assert(tok);
    ccml_.errors().unexpected_token(*tok);
  }
  return true;
}

const function_t *parser_t::find_function(const token_t *name, bool can_fail) {
  token_stream_t &stream_ = ccml_.lexer().stream_;

  for (const function_t &func : funcs_) {
    if (func.name_ == name->str_) {
      return &func;
    }
  }

  if (!can_fail) {
    ccml_.errors().unknown_function(*name);
  }
  return nullptr;
}

const function_t *parser_t::find_function(const std::string &name) {
  token_stream_t &stream_ = ccml_.lexer().stream_;
  for (const function_t &func : funcs_) {
    if (func.name_ == name) {
      return &func;
    }
  }
  return nullptr;
}

int32_t parser_t::op_type(token_e type) {
  // return precedence
  // note: higher number will be evaluated earlier in an expression
  switch (type) {
  case (TOK_AND):
  case (TOK_OR):
    return 1;
  case (TOK_LT):
  case (TOK_GT):
  case (TOK_LEQ):
  case (TOK_GEQ):
  case (TOK_EQ):
    return 2;
  case (TOK_ADD):
  case (TOK_SUB):
    return 3;
  case (TOK_MUL):
  case (TOK_DIV):
  case (TOK_MOD):
    return 4;
  default:
    // this is not an operator
    return 0;
  }
}

bool parser_t::is_operator() {
  token_stream_t &stream_ = ccml_.lexer().stream_;
  return op_type(stream_.type()) > 0;
}

void parser_t::load_ident(const token_t &t) {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // try to load from local variable
  if (const identifier_t *ident = scope_.find_ident(t)) {
    asm_.emit(INS_GETV, ident->offset, &t);
    return;
  }
  // try to load from global variable
  for (uint32_t i = 0; i < global_.size(); ++i) {
    if (global_[i].token_->str_ == t.str_) {
      asm_.emit(INS_GETG, i, &t);
      return;
    }
  }
  // unable to find identifier
  ccml_.errors().unknown_identifier(t);
}

void parser_t::parse_lhs() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //    ( <expr> )
  //    <TOK_IDENT>
  //    <TOK_IDENT> ( ... )
  //    <TOK_VAL>

  if (stream_.found(TOK_LPAREN)) {
    parse_expr();
    stream_.found(TOK_RPAREN);
  } else if (const token_t *t = stream_.found(TOK_IDENT)) {
    if (stream_.found(TOK_LPAREN)) {
      // call function
      parse_call(t);
    } else {
      // load a local/global
      load_ident(*t);
    }
  } else if (const token_t *t = stream_.found(TOK_VAL)) {
    asm_.emit(INS_CONST, t->val_, t);
  } else {
    ccml_.errors().expecting_lit_or_ident(*stream_.pop());
  }
}

void parser_t::parse_expr_ex(uint32_t tide) {
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //    <lhs>
  //    <lhs> <op> <expr_ex>

  parse_lhs();
  if (is_operator()) {
    const token_t *op = stream_.pop();
    op_push(op->type_, tide);
    parse_expr_ex(tide);
  }
}

void parser_t::parse_expr() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //    not <expr_ex>
  //    <expr_ex>

  const token_t *not = stream_.found(TOK_NOT);

  uint32_t tide = op_stack_.size();
  parse_expr_ex(tide);
  op_popall(tide);

  if (not) {
    asm_.emit(INS_NOT, not);
  }
}

void parser_t::parse_decl() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //        V
  //    var   <TOK_IDENT> [ = <expr> ]

  const token_t *name = stream_.pop(TOK_IDENT);

  // check for duplicate function name
  if (scope_.find_ident(*name)) {
    ccml_.errors().var_already_exists(*name);
  }

  // parse assignment expression
  const token_t *assign = stream_.found(TOK_ASSIGN);
  if (assign) {
    parse_expr();
  }
  else {
    // implicitly set to zero
    // TODO: add tests for this
    asm_.emit(INS_CONST, 0, name);
  }

  // add name to identifier table
  // note: must happen after parse_expr() to avoid 'var x = x'
  scope_.var_add(name);

  // generate assignment
  const identifier_t *ident = scope_.find_ident(*name);
  assert(ident && "identifier should always be known here");
  asm_.emit(INS_SETV, ident->offset, assign);
}

void parser_t::parse_assign(const token_t *name) {
  assembler_t &asm_ = ccml_.assembler();

  // format:
  //                  V
  //    <TOK_IDENT> =   <expr>

  // parse assignment expression
  parse_expr();
  // assign to local variable
  if (const identifier_t *ident = scope_.find_ident(*name)) {
    asm_.emit(INS_SETV, ident->offset, name);
    return;
  }
  // assign to global variable
  for (uint32_t i = 0; i < global_.size(); ++i) {
    if (global_[i].token_->str_ == name->str_) {
      asm_.emit(INS_SETG, i, name);
      return;
    }
  }
  // cant locate variable
  ccml_.errors().cant_assign_unknown_var(*name);
}

void parser_t::parse_call(const token_t *name) {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //                  V
  //    <TOK_IDENT> (   <expr> [ , <expr> ]* )

  if (!stream_.found(TOK_RPAREN)) {
    do {
      parse_expr();
    } while (stream_.found(TOK_COMMA));
    stream_.pop(TOK_RPAREN);
  }
  const function_t *func = find_function(name, true);
  if (!func) {
    ccml_.errors().unknown_function(*name);
  }
  if (func->sys_) {
    asm_.emit(func->sys_, name);
  } else {
    asm_.emit(INS_CALL, func->pos_, name);
  }
}

void parser_t::parse_if() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //       V
  //    if   ( <expr> ) '\n'
  //      <statements>
  //  [ else '\n'
  //      <statements> ]
  //    end '\n'

  bool has_else = false;
  // IF condition
  stream_.pop(TOK_LPAREN);
  parse_expr();
  stream_.pop(TOK_RPAREN);
  stream_.pop(TOK_EOL);

  // this jump skips the body of the if, hence not
  asm_.emit(INS_NOT);
  int32_t *l1 = asm_.emit(INS_JMP, 0);

  // IF body
  {
    scope_.enter();
    while (!stream_.found(TOK_END)) {
      if (stream_.found(TOK_ELSE)) {
        stream_.pop(TOK_EOL);
        has_else = true;
        break;
      }
      parse_stmt();
    }
    scope_.leave();
  }

  int32_t *l2 = nullptr;
  if (has_else) {
    // skip over ELSE body
    asm_.emit(INS_CONST, 1);
    l2 = asm_.emit(INS_JMP, 0);
  }
  *l1 = asm_.pos();
  if (has_else) {
    // ELSE body
    {
      scope_.enter();
      while (!stream_.found(TOK_END)) {
        parse_stmt();
      }
      scope_.leave();
    }
    // END
    *l2 = asm_.pos();
  }

  // note: no need to pop newline as parse_stmt() handles that
}

void parser_t::parse_while() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //          V
  //    while   ( <expr> ) '\n'
  //      <statments>
  //    end '\n'

  // top of loop
  int32_t l1 = asm_.pos();
  // WHILE condition
  stream_.pop(TOK_LPAREN);
  parse_expr();
  stream_.pop(TOK_RPAREN);
  stream_.pop(TOK_EOL);

  // GOTO end if false
  asm_.emit(INS_NOT);
  int32_t *l2 = asm_.emit(INS_JMP, 0);
  // WHILE body
  {
    scope_.enter();
    while (!stream_.found(TOK_END)) {
      parse_stmt();
    }
    scope_.leave();
  }
  // note: no need to pop newline as parse_stmt() handles that

  // unconditional jump to top
  asm_.emit(INS_CONST, 1);
  asm_.emit(INS_JMP, l1);
  // WHILE end
  *l2 = asm_.pos();
}

void parser_t::parse_return() {
  assembler_t &asm_ = ccml_.assembler();

  // return <expr>
  parse_expr();
  asm_.emit(INS_RET, scope_.arg_count() + scope_.var_count());
}

void parser_t::parse_stmt() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //    [ '\n' ]+
  //    var <TOK_IDENT> [ = <expr> ] '\n'
  //    <TOK_IDENT> ( <expression list> ) '\n'
  //    if ( <expr> ) '\n'
  //    while ( <expr> ) '\n'
  //    return <expr> '\n'

  while (stream_.found(TOK_EOL)) {
    // consume any blank lines
  }

  if (stream_.found(TOK_VAR)) {
    // var ...
    parse_decl();
  } else if (const token_t *var = stream_.found(TOK_IDENT)) {
    if (stream_.found(TOK_ASSIGN)) {
      // x = ...
      parse_assign(var);
    } else if (stream_.found(TOK_LPAREN)) {
      // x(
      parse_call(var);
      // note: we throw away the return value since its not being used
      asm_.emit(INS_POP, 1);
    } else {
      ccml_.errors().assign_or_call_expected_after(*var);
    }
  } else if (stream_.found(TOK_IF)) {
    parse_if();
  } else if (stream_.found(TOK_WHILE)) {
    parse_while();
  } else if (stream_.found(TOK_RETURN)) {
    parse_return();
  } else {
    ccml_.errors().statement_expected();
  }

  // all statements should be on their own line
  stream_.pop(TOK_EOL);
}

void parser_t::parse_function() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //             V
  //    function   <TOK_IDENT> ( [ <TOK_IDENT> [ , <TOK_IDENT> ]+ ] )
  //      <statements>
  //    end

  // parse function decl.
  const token_t *name = stream_.pop(TOK_IDENT);
  assert(name);

  if (find_function(name, true)) {
    ccml_.errors().function_already_exists(*name);
  }

  // new function container
  funcs_.emplace_back(name->str_, asm_.pos());

  // reset the scope
  scope_.reset();

  // argument list
  stream_.pop(TOK_LPAREN);
  if (!stream_.found(TOK_RPAREN)) {
    do {
      const token_t *arg = stream_.pop(TOK_IDENT);
      scope_.arg_add(arg);
    } while (stream_.found(TOK_COMMA));
    stream_.pop(TOK_RPAREN);
    scope_.arg_calc_offsets();
  }
  stream_.pop(TOK_EOL);

  // emit dummy prologue
  asm_.emit(INS_LOCALS, 0, name);
  int32_t &locals = asm_.get_fixup();

  // note: at this point we parse all of the statements inside
  //       the function

  // function body
  {
    scope_.enter();
    while (!stream_.found(TOK_END)) {
      parse_stmt();
    }
    scope_.leave();
  }

  // emit dummy epilogue (may be unreachable)
  asm_.emit(INS_CONST, 0);
  asm_.emit(INS_RET, scope_.arg_count() + scope_.var_count());

  // fixup the number of locals we are reserving with INS_LOCALS
  const uint32_t num_locals = scope_.max_depth();
  if (num_locals) {
    locals = num_locals;
  }
}

void parser_t::parse_global() {
  assembler_t &asm_ = ccml_.assembler();
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // format:
  //        V
  //    var   <TOK_IDENT> = <TOK_VAL>

  const token_t *name = stream_.pop(TOK_IDENT);
  global_t global = {name, 0};

  // TODO: add find identifier and check for duplicates

  if (stream_.found(TOK_ASSIGN)) {
    const token_t *value = stream_.pop(TOK_VAL);
    global.value_ = value->val_;
  }

  global_.push_back(global);
}

void parser_t::op_push(token_e op, uint32_t tide) {
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // walk the operator stack for the current expression
  while (op_stack_.size() > tide) {
    // get the top operator on the stack
    const token_e top = op_stack_.back();
    // if this operator is higher precedence then top of stack
    if (op_type(op) > op_type(top)) {
      break;
    }
    // if lower or = precedence, pop from the stack and emit it
    ccml_.assembler().emit(top);
    op_stack_.pop_back();
  }
  // push this token on the top of the stack
  op_stack_.push_back(op);
}

void parser_t::op_popall(uint32_t tide) {
  token_stream_t &stream_ = ccml_.lexer().stream_;

  // walk the operator stack for the current expression
  while (op_stack_.size() > tide) {
    // emit this operator
    const token_e op = op_stack_.back();
    ccml_.assembler().emit(op);
    op_stack_.pop_back();
  }
}

void parser_t::add_function(const std::string &name, ccml_syscall_t func) {
  funcs_.emplace_back(name, func);
}

void parser_t::reset() {
  funcs_.clear();
  global_.clear();
  op_stack_.clear();
}
