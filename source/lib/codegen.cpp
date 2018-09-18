#include <cstring>
#include <set>
#include <map>

#include "codegen.h"
#include "disassembler.h"
#include "lexer.h"
#include "ccml.h"
#include "errors.h"
#include "instructions.h"
#include "parser.h"


using namespace ccml;

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
namespace {
static instruction_e tok_to_ins_(token_e op) {
  switch (op) {
  case TOK_ADD: return INS_ADD;
  case TOK_SUB: return INS_SUB;
  case TOK_MUL: return INS_MUL;
  case TOK_DIV: return INS_DIV;
  case TOK_MOD: return INS_MOD;
  case TOK_AND: return INS_AND;
  case TOK_OR:  return INS_OR ;
  case TOK_NOT: return INS_NOT;
  case TOK_NEG: return INS_NEG;
  case TOK_EQ:  return INS_EQ ;
  case TOK_LT:  return INS_LT ;
  case TOK_GT:  return INS_GT ;
  case TOK_LEQ: return INS_LEQ;
  case TOK_GEQ: return INS_GEQ;
  default:
    assert(!"unknown token type");
    return instruction_e(0);
  }
}
} // namespace {}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
void asm_stream_t::set_line(lexer_t &lexer, const token_t *t) {
  const uint32_t pc = pos();
  const uint32_t line = t ? t->line_no_ : lexer.stream_.line_number();
  store_.add_line(pc, line);
}

void asm_stream_t::add_ident(const identifier_t &ident) {
  for (auto &i : store_.identifiers_) {
    if (i.token == ident.token) {
      i.start = std::min(ident.start, i.start);
      i.end   = std::max(ident.end,   i.end);
      return;
    }
  }
  store_.identifiers_.push_back(ident);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct stack_pass_t: ast_visitor_t {

  stack_pass_t()
    : size_(0)
    , max_size_(0) {
  }

  void visit(ast_stmt_while_t *n) {
    scope_.emplace_back();
    const int32_t s = size_;
    ast_visitor_t::visit(n);
    size_ = s;
    scope_.pop_back();
  }

  void visit(ast_stmt_if_t *n) {
    scope_.emplace_back();
    const int32_t s = size_;
    ast_visitor_t::visit(n);
    size_ = s;
    scope_.pop_back();
  }

  void visit(ast_decl_func_t *n) {
    reset();
    scope_.emplace_back();
    // add arguments
    int32_t i = -int32_t(n->args.size());
    for (auto *a : n->args) {
      auto *var = a->cast<ast_decl_var_t>();
      assert(var);
      scope_.back().insert(var);
      args_.insert(var);
      offsets_[var] = i++;
    }
    // visit body
    ast_visitor_t::visit(n);
    scope_.pop_back();
  }

  void visit(ast_decl_var_t *n) {
    if (!scope_.empty()) {
      scope_.back().insert(n);
      if (find(n->name->str_)) {
        // duplicate identifier!
      }
      offsets_[n] = size_;
      size_ += n->count();
      max_size_ = std::max(size_, max_size_);
    }
  }

  void visit(ast_exp_array_t *i) {
    ast_node_t *node = find(i->name->str_);
    if (!node) {
      // identifier not found!
    }
    ast_decl_var_t *v = node->cast<ast_decl_var_t>();
    assert(v);

    decl_[i] = v;
  }

  void visit(ast_exp_ident_t *i) {
    ast_node_t *node = find(i->name->str_);
    if (!node) {
      // identifier not found!
    }
    auto *var = node->cast<ast_decl_var_t>();
    if (var->is_array()) {
      // array access requires subscript []
    }
    decl_[i] = var;
  }

  void visit(ast_program_t *p) {
    int32_t size = 0;
    scope_.emplace_back();
    for (ast_node_t *n : p->children) {
      // collect var decls
      if (ast_decl_var_t *v = n->cast<ast_decl_var_t>()) {
        globals_.insert(v);
        const std::string &name = v->name->str_;
        offsets_[v] = size;
        size += v->count();
      }
      // traverse functions
      if (ast_decl_func_t *f = n->cast<ast_decl_func_t>()) {
        dispatch(f);
      }
    }
    scope_.pop_back();
  }

  void reset() {
    decl_.clear();
    offsets_.clear();
    scope_.clear();
    size_ = 0;
    max_size_ = 0;
    args_.clear();
    // note: dont clear globals
  }

  // find the stack offset for a given decl
  int32_t find_offset(ast_decl_var_t *decl) {
    auto itt = offsets_.find(decl);
    if (itt == offsets_.end()) {
      assert(!"unknown decl");
    }
    return itt->second;
  }

  // given a use, find its matching decl
  ast_decl_var_t *find_decl(ast_node_t *use) const {
    auto itt = decl_.find(use);
    if (itt == decl_.end()) {
      return nullptr;
    }
    return itt->second;
  }

  int32_t get_locals_operand() const {
    return max_size_;
  }

  int32_t get_ret_operand() const {
    return max_size_ + args_.size();
  }

protected:
  // find a decl given its name
  ast_decl_var_t *find(const std::string &name) const {
    // find locals/args/globals
    for (auto i = scope_.rbegin(); i != scope_.rend(); ++i) {
      for (ast_decl_var_t *j : *i) {
        if (j->name->str_ == name)
          return j;
      }
    }
    return nullptr;
  }

  // identifier -> decl map
  std::map<ast_node_t*, ast_decl_var_t*> decl_;
  // decl -> offset map
  std::map<ast_decl_var_t*, int32_t> offsets_;
  // ast_decl_var_t
  std::vector<std::set<ast_decl_var_t*>> scope_;
  // global variables
  std::set<ast_decl_var_t*> globals_;
  // args
  std::set<ast_decl_var_t*> args_;
  // current stack size
  int32_t size_;
  // max stack size
  int32_t max_size_;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct codegen_pass_t : ast_visitor_t {

  codegen_pass_t(ccml_t &c, asm_stream_t &stream)
    : ccml_(c)
    , stream_(stream) {
  }

  void visit(ast_program_t* n) override {
    stack_pass_.visit(n);
    for (ast_node_t *c : n->children) {
      dispatch(c);
    }
  }

  void visit(ast_exp_ident_t* n) override {
    ast_decl_var_t *decl = stack_pass_.find_decl(n);
    // getv/getg
    emit(INS_GETV, 0, n->name);
  }

  virtual void visit(ast_exp_const_t* n) override {
    emit(INS_CONST, n->value->val_, n->value);
  }

  void visit(ast_exp_array_t* n) override {
    int32_t offset = 0;
    if (ast_decl_var_t *decl = stack_pass_.find_decl(n)) {
      offset = stack_pass_.find_offset(decl);
    }
    // getvi/getgi
    dispatch(n->index);
    emit(INS_GETVI, offset, nullptr);
  }

  void visit(ast_exp_call_t* n) override {
    for (ast_node_t *c : n->args) {
      dispatch(c);
    }
    emit(INS_CALL, 0, n->name);
    int32_t *addr = get_fixup();
    // insert addr into map
  }

  void visit(ast_stmt_call_t *n) override {
    dispatch(n->expr);
    // discard return value
    emit(INS_POP, 1, nullptr);
  }

  void visit(ast_exp_bin_op_t* n) override {
    dispatch(n->left);
    dispatch(n->right);
    emit(tok_to_ins_(n->op->type_), n->op);
  }

  void visit(ast_exp_unary_op_t* n) override {
    dispatch(n->child);
    emit(tok_to_ins_(n->op->type_), n->op);
  }

  void visit(ast_stmt_if_t* n) override {

    // expr
    dispatch(n->expr);
    // false jump to L0 (else)
    emit(INS_FJMP, 0, n->token); // ---> LO

    // then
    for (ast_node_t *c : n->then_block) {
      dispatch(c);
    }
    // unconditional jump to end
    emit(INS_JMP, 0); // ---> L1

    // else
    // L0 <---
    for (ast_node_t *c : n->else_block) {
      dispatch(c);
    }

    // L1 <---
  }

  void visit(ast_stmt_while_t* n) override {

    // initial jump to L1 --->
    emit(INS_JMP, 0, n->token);
    int32_t *fixup = get_fixup();

    // L0 <---
    // emit the while loop body
    for (ast_node_t *c : n->body) {
      dispatch(c);
    }

    // L1 <---
    // while loop condition
    dispatch(n->expr);
    // true jump to L0 --->
    emit(INS_TJMP, 0, n->token);
  }

  void visit(ast_stmt_return_t* n) override {
    dispatch(n->expr);
    const int32_t space = stack_pass_.get_ret_operand();
    emit(INS_RET, space);
  }

  void visit(ast_stmt_assign_var_t* n) override {
    if (n->expr) {
      dispatch(n->expr);
      emit(INS_SETV, 0, n->name);
    }
  }

  void visit(ast_stmt_assign_array_t* n) override {
  }

  void visit(ast_decl_func_t* n) override {
    // 
    const int32_t space = stack_pass_.get_locals_operand();
    if (space > 0) {
      emit(INS_LOCALS, space, n->name);
    }
    bool is_return = false;
    for (ast_node_t *c : n->body) {
      dispatch(c);
      is_return = c->is_a<ast_stmt_return_t>();
    }
    if (!is_return) {
      emit(INS_CONST, 0, nullptr);
      emit(INS_RET, stack_pass_.get_ret_operand(), nullptr);
    }
  }

  void visit(ast_decl_var_t* n) override {
    if (n->expr) {
      dispatch(n->expr);
      emit(INS_SETV, 0, n->name);
    }
  }

protected:

  // emit into the code stream
  void emit(instruction_e ins, const token_t *t = nullptr);
  void emit(instruction_e ins, int32_t v, const token_t *t = nullptr);

  // return the current output head
  int32_t pos() const {
    return stream_.pos();
  }

  // return a reference to the last instructions operand
  int32_t *codegen_pass_t::get_fixup() {
    return reinterpret_cast<int32_t *>(stream_.head(-4));
  }

  stack_pass_t stack_pass_;
  asm_stream_t &stream_;
  ccml_t &ccml_;
};

void codegen_pass_t::emit(instruction_e ins, const token_t *t) {
  stream_.set_line(ccml_.lexer(), t);
  // encode this instruction
  switch (ins) {
  case INS_ADD:
  case INS_SUB:
  case INS_MUL:
  case INS_DIV:
  case INS_MOD:
  case INS_AND:
  case INS_OR:
  case INS_NOT:
  case INS_LT:
  case INS_GT:
  case INS_LEQ:
  case INS_GEQ:
  case INS_EQ:
  case INS_NEG:
    stream_.write8(uint32_t(ins));
    break;
  default:
    assert(!"unknown instruction");
  }
}

void codegen_pass_t::emit(instruction_e ins, int32_t v, const token_t *t) {
  stream_.set_line(ccml_.lexer(), t);
  // encode this instruction
  switch (ins) {
  case INS_JMP:
  case INS_FJMP:
  case INS_TJMP:
  case INS_CALL:
  case INS_RET:
  case INS_SCALL:
  case INS_POP:
  case INS_CONST:
  case INS_LOCALS:
  case INS_ACCV:
  case INS_GETV:
  case INS_SETV:
  case INS_GETVI:
  case INS_SETVI:
  case INS_GETG:
  case INS_SETG:
  case INS_GETGI:
  case INS_SETGI:
    stream_.write8(uint8_t(ins));
    stream_.write32(v);
    break;
  default:
    assert(!"unknown instruction");
  }
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
codegen_t::codegen_t(ccml_t &c, asm_stream_t &s)
  : ccml_(c)
  , stream_(s)
{
}

bool codegen_t::run(ast_program_t &program) {

  codegen_pass_t cg{ccml_, stream_};
  cg.visit(&program);

  return true;
}

void codegen_t::reset() {
}
