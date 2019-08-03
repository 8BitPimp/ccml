#include <set>

#include "ast.h"
#include "errors.h"
#include "sema.h"

namespace ccml {

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// check global var expressions
//
struct sema_global_var_t : public ast_visitor_t {

  sema_global_var_t(ccml_t &ccml)
    : errs_(ccml.errors())
    , decl_(nullptr)
    , ast_(ccml.ast()) {
  }

  void visit(ast_array_init_t *n) override {
    for (auto *t : n->item) {
      switch (t->type_) {
      case TOK_INT:
      case TOK_NONE:
      case TOK_STRING:
      case TOK_FLOAT:
        break;
      default:
        errs_.bad_array_init_value(*t);
      }
    }
  }

  void visit(ast_decl_var_t *n) override {
    assert(n->expr);
    switch (n->expr->type) {
    case ast_exp_none_e:
      n->expr = nullptr;
      break;
    case ast_array_init_e:
      break;
    case ast_exp_bin_op_e:
    case ast_exp_unary_op_e: {
      value_.clear();
      decl_ = n;
      ast_visitor_t::visit(n);
      assert(value_.size() == 1);
      const int32_t val = value_.back();
      n->expr = ast_.alloc<ast_exp_lit_var_t>(val);
    } break;
    case ast_exp_lit_str_e:
    case ast_exp_lit_var_e:
      break;
    default:
      errs_.global_var_const_expr(*n->name);
    }
  }

  void visit(ast_exp_unary_op_t *n) override {
    decend_(n);
    const int32_t v = value_.back();
    int32_t r = 0;
    value_.pop_back();
    switch (n->op->type_) {
    case TOK_NEG:
      r = -v;
      break;
    default:
      assert(!"unknown operator");
    }
    value_.push_back(r);
  }

  void visit(ast_exp_bin_op_t *n) override {
    decend_(n);
    const int32_t r = eval_(*(n->token));
    value_.push_back(r);
  }

  void visit(ast_exp_lit_var_t *n) override {
    value_.push_back(n->val);
  }

  void visit(ast_program_t *p) override {
    for (auto *n : p->children) {
      if (auto *d = n->cast<ast_decl_var_t>()) {
        if (d->expr) {
          dispatch(d);
        }
      }
    }
  }

protected:
  void decend_(ast_node_t *n) {
    switch (n->type) {
    case ast_exp_lit_var_e:
      ast_visitor_t::visit(n->cast<ast_exp_lit_var_t>());
      break;
    case ast_exp_bin_op_e:
      ast_visitor_t::visit(n->cast<ast_exp_bin_op_t>());
      break;
    case ast_exp_unary_op_e:
      ast_visitor_t::visit(n->cast<ast_exp_unary_op_t>());
      break;
    default:
      errs_.global_var_const_expr(*decl_->name);
    }
  }

  int32_t eval_(const token_t &tok) {
    assert(value_.size() >= 2);
    const int32_t b = value_.back();
    value_.pop_back();
    const int32_t a = value_.back();
    value_.pop_back();
    // check for constant division
    if (b == 0) {
      if (tok.type_ == TOK_DIV || tok.type_ == TOK_MOD) {
        errs_.constant_divie_by_zero(tok);
      }
    }
    // evaluate operator
    switch (tok.type_) {
    case TOK_ADD:     return a + b;
    case TOK_SUB:     return a - b;
    case TOK_MUL:     return a * b;
    case TOK_AND:     return a && b;
    case TOK_OR:      return a || b;
    case TOK_LEQ:     return a <= b;
    case TOK_GEQ:     return a >= b;
    case TOK_LT:      return a < b;
    case TOK_GT:      return a > b;
    case TOK_EQ:      return a == b;
    case TOK_DIV:     return a / b;
    case TOK_MOD:     return a % b;
    default:
      assert(!"unknown operator");
    }
    return 0;
  }

  ast_t &ast_;
  ast_decl_var_t *decl_;
  std::vector<int32_t> value_;
  error_manager_t &errs_;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// annotate nodes with a data type where possible to check validity
//
struct sema_type_annotation_t : public ast_visitor_t {

  sema_type_annotation_t(ccml_t &ccml)
    : errs_(ccml.errors()) {
  }

  std::map<const ast_node_t *, value_type_t> types_;

  value_type_t type_of(const ast_node_t *n) const {
    assert(n);
    auto itt = types_.find(n);
    if (itt != types_.end()) {
      return itt->second;
    } else {
      return val_type_unknown;
    }
  }

  void visit(ast_exp_lit_var_t *node) override { types_[node] = val_type_int; }

  void visit(ast_exp_lit_str_t *node) override {
    types_[node] = val_type_string;
  }

  void visit(ast_exp_none_t *node) override {
    types_[node] = val_type_none;
  }

  void visit(ast_exp_ident_t *n) override {
    ast_visitor_t::visit(n);
    if (n->decl) {
      auto itt = types_.find(n->decl);
      if (itt != types_.end()) {
        types_[n] = itt->second;
      }
    }
  }

  void visit(ast_exp_array_t *n) override {
    ast_visitor_t::visit(n);
  }

  void visit(ast_exp_call_t *n) override {
    ast_visitor_t::visit(n);
  }

  void visit(ast_exp_bin_op_t *n) override {
    ast_visitor_t::visit(n);
    assert(n->left && n->right);
    auto tl = type_of(n->left);
    auto tr = type_of(n->right);

    if (tl == val_type_int && tr == val_type_int) {
      types_[n] = val_type_int;
      return;
    }

    if (n->op == TOK_ADD) {
      if (tl == val_type_string && tr == val_type_string) {
        types_[n] = val_type_string;
        return;
      }
      if (tl == val_type_string && tr == val_type_int) {
        types_[n] = val_type_string;
        return;
      }
      if (tl == val_type_int && tr == val_type_string) {
        types_[n] = val_type_string;
        return;
      }
    }
  }

  void visit(ast_exp_unary_op_t *n) override {
    ast_visitor_t::visit(n);

    if (type_of(n->child) == val_type_int) {
      types_[n] = val_type_int;
    } else {
      // we cant unary a string
    }
  }

  void visit(ast_decl_var_t *n) override {
    ast_visitor_t::visit(n);
    if (n->expr) {
      switch (type_of(n->expr)) {
      case val_type_int:
        break;
      default:
        break;
      }
    }
  }

  void visit(ast_program_t *p) override { ast_visitor_t::visit(p); }

  error_manager_t &errs_;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// annotate nodes with their associated decl node
// set function `is_syscall` member
//
struct sema_decl_annotate_t : public ast_visitor_t {

  sema_decl_annotate_t(ccml_t &ccml)
    : errs_(ccml.errors())
    , ccml_(ccml) {
  }

  void visit(ast_stmt_for_t *p) override {
    ast_node_t *n = find_decl(p->name->str_);
    if (n) {
      if (ast_decl_var_t *d = n->cast<ast_decl_var_t>()) {
        p->decl = d;
        ast_visitor_t::visit(p);
        return;
      }
    }
    errs_.unknown_variable(*p->name);
  }

  void visit(ast_program_t *p) override {
    // collect global decls
    scope_.emplace_back();
    for (auto &f : p->children) {
      switch (f->type) {
      case ast_decl_func_e:
      case ast_decl_var_e:
        scope_[0].insert(f);
        break;
      default:
        break;
      }
    }
    prog_ = p;
    ast_visitor_t::visit(p);
  }

  void visit(ast_stmt_assign_var_t *n) override {
    ast_visitor_t::visit(n);
    ast_node_t *found = find_decl(n->name->str_);
    if (!found) {
      errs_.unknown_variable(*n->name);
    }
    n->decl = found->cast<ast_decl_var_t>();
    if (!n->decl) {
      errs_.unknown_variable(*n->name);
    }
    if (n->decl->is_array()) {
      errs_.ident_is_array_not_var(*n->name);
    }
    if (n->decl->is_arg()) {
      // can you assign to an argument?
    }
  }

  void visit(ast_stmt_assign_array_t *n) override {
    ast_visitor_t::visit(n);
    ast_node_t *found = find_decl(n->name->str_);
    if (!found) {
      errs_.unknown_array(*n->name);
    }
    n->decl = found->cast<ast_decl_var_t>();
    if (!n->decl) {
      errs_.unknown_array(*n->name);
    }
  }

  void visit(ast_exp_array_t *n) override {
    ast_visitor_t::visit(n);
    ast_node_t *found = find_decl(n->name->str_);
    if (!found) {
      errs_.unknown_array(*n->name);
    }
    n->decl = found->cast<ast_decl_var_t>();
    if (n->decl) {
      if (!n->decl->is_array()) {
        // runtime error as strings are also acceptable
        // errs_.variable_is_not_array(*n->name);
      }
      return;
    } else {
      if (found->cast<ast_decl_func_t>()) {
        errs_.expected_func_call(*n->name);
      } else {
        errs_.unexpected_token((*n->name));
      }
    }
    //    assert(false);
  }

  void visit(ast_exp_ident_t *n) override {
    ast_visitor_t::visit(n);
    ast_node_t *found = find_decl(n->name->str_);
    if (!found) {
      errs_.unknown_identifier(*n->name);
    }
    n->decl = found->cast<ast_decl_var_t>();
    if (!n->decl) {
      errs_.unknown_identifier(*n->name);
    }
    if (const auto *v = n->decl->cast<ast_decl_var_t>()) {
      if (v->is_array()) {
        errs_.array_requires_subscript(*n->name);
      }
    } else {
      if (n->decl->cast<ast_decl_func_t>()) {
        errs_.expected_func_call(*n->name);
      } else {
        errs_.unexpected_token((*n->name));
      }
    }
  }

  void visit(ast_exp_call_t *n) override {
    ast_visitor_t::visit(n);
    ast_node_t *f = find_decl(n->name->str_);
    if (f) {
      n->decl = f->cast<ast_decl_func_t>();
      if (n->decl) {
        n->is_syscall = false;
        return;
      }
    } else {
      // check for extern function
      for (const auto &func : ccml_.functions()) {
        if (func.name_ == n->name->str_) {
          n->is_syscall = true;
          return;
        }
      }
    }
    errs_.unknown_function(*n->name);
  }

  void visit(ast_decl_func_t *n) override {
    scope_.emplace_back();
    ast_visitor_t::visit(n);
    scope_.pop_back();
  }

  void visit(ast_stmt_if_t *n) override {
    scope_.emplace_back();
    ast_visitor_t::visit(n);
    scope_.pop_back();
  }

  void visit(ast_stmt_while_t *n) override {
    scope_.emplace_back();
    ast_visitor_t::visit(n);
    scope_.pop_back();
  }

  void visit(ast_decl_var_t *n) override {
    ast_visitor_t::visit(n);
    scope_.back().insert(n);
  }

  ast_node_t *find_decl(const std::string &name) {
    ast_node_t *out = nullptr;
    // move from inner to outer scope
    for (auto s = scope_.rbegin(); s != scope_.rend(); ++s) {
      for (auto d : *s) {
        if (auto *m = d->cast<ast_decl_func_t>()) {
          if (m->name.c_str() == name)
            return m;
          else
            continue;
        }
        if (auto *m = d->cast<ast_decl_var_t>()) {
          if (m->name->str_ == name)
            return m;
          else
            continue;
        }
      }
    }
    return out;
  }

  std::vector<std::set<ast_node_t *>> scope_;

  ast_program_t *prog_;
  ccml_t &ccml_;
  error_manager_t &errs_;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// check for multiple declarations with the same name
//
struct sema_multi_decls_t : public ast_visitor_t {

  sema_multi_decls_t(ccml_t &ccml)
    : errs_(ccml.errors()) {
  }

  void visit(ast_decl_var_t *var) override {
    const auto &name = var->name->str_;
    if (is_def_(name)) {
      errs_.var_already_exists(*var->name);
    }
    add_(name);
  }

  void visit(ast_stmt_if_t *stmt) override {
    scope_.emplace_back();
    ast_visitor_t::visit(stmt);
    scope_.pop_back();
  }

  void visit(ast_stmt_while_t *stmt) override {
    scope_.emplace_back();
    ast_visitor_t::visit(stmt);
    scope_.pop_back();
  }

  void visit(ast_decl_func_t *func) override {
    if (is_def_(func->name.c_str())) {
      errs_.function_already_exists(*func->token);
    }
    add_(func->name.c_str());
    scope_.emplace_back();
    for (const auto &a : func->args) {
      dispatch(a);
    }
    dispatch(func->body);
    scope_.pop_back();
  }

  void visit(ast_program_t *prog) override {
    scope_.emplace_back();
    ast_visitor_t::visit(prog);
    scope_.pop_back();
  }

protected:
  error_manager_t &errs_;
  std::vector<std::set<std::string>> scope_;

  // add a name to scope
  void add_(const std::string &name) {
    scope_.back().insert(name);
  }

  // check if a name is already present
  bool is_def_(const std::string &name) const {
    for (const auto &s : scope_) {
      for (const auto &n : s) {
        if (n == name) {
          return true;
        }
      }
    }
    return false;
  }
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// check for valid/compatable type usage for certain operations
//
struct sema_type_uses_t : public ast_visitor_t {

  sema_type_uses_t(ccml_t &ccml)
    : errs_(ccml.errors()) {
  }

  void visit(ast_decl_var_t *var) override {
    add_(var);
  }

  void visit(ast_stmt_assign_array_t *a) override {
    const ast_decl_var_t *d = get_decl_(a->name->str_);
    if (d && !d->is_array()) {
      errs_.variable_is_not_array(*d->name);
    }
  }

  void visit(ast_stmt_assign_var_t *v) override {
    const ast_decl_var_t *d = get_decl_(v->name->str_);
    if (d && d->is_array()) {
      errs_.ident_is_array_not_var(*d->name);
    }
  }

  void visit(ast_exp_ident_t *i) override {
    const ast_decl_var_t *d = get_decl_(i->name->str_);
    if (d && d->is_array()) {
      errs_.ident_is_array_not_var(*d->name);
    }
  }

  void visit(ast_stmt_if_t *stmt) override {
    scope_.emplace_back();
    ast_visitor_t::visit(stmt);
    scope_.pop_back();
  }

  void visit(ast_stmt_while_t *stmt) override {
    scope_.emplace_back();
    ast_visitor_t::visit(stmt);
    scope_.pop_back();
  }

  void visit(ast_decl_func_t *func) override {
    scope_.emplace_back();
    ast_visitor_t::visit(func);
    scope_.pop_back();
  }

  void visit(ast_program_t *prog) override {
    scope_.emplace_back();
    ast_visitor_t::visit(prog);
    scope_.pop_back();
  }

protected:
  error_manager_t &errs_;
  std::vector<std::set<const ast_decl_var_t *>> scope_;

  void add_(const ast_decl_var_t *v) {
    scope_.back().insert(v);
  }

  const ast_decl_var_t *get_decl_(const std::string &name) const {
    for (const auto &s : scope_) {
      for (const auto &n : s) {
        if (n->name->str_ == name) {
          return n;
        }
      }
    }
    return nullptr;
  }
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// check call sites use the correct number of arguments
//
struct sema_num_args_t : public ast_visitor_t {

  sema_num_args_t(ccml_t &ccml)
    : errs_(ccml.errors()) {
  }

  error_manager_t &errs_;
  std::map<std::string, const ast_decl_func_t *> funcs_;

  void visit(ast_exp_call_t *call) override {
    const auto &name = call->name->str_;
    auto itt = funcs_.find(name);
    if (itt == funcs_.end()) {
      // its likely a builtin or syscall
      return;
    }
    const ast_decl_func_t *func = itt->second;
    if (call->args.size() > func->args.size()) {
      errs_.too_many_args(*call->name);
    }
    if (call->args.size() < func->args.size()) {
      errs_.not_enought_args(*call->name);
    }
  }

  void visit(ast_program_t *prog) override {
    // gather all functions
    for (ast_node_t *n : prog->children) {
      if (auto *f = n->cast<ast_decl_func_t>()) {
        const std::string &name = f->name.c_str();
        funcs_.emplace(name, f);
      }
    }
    // dispatch
    ast_visitor_t::visit(prog);
  }
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// check for a valid array size
//
struct sema_array_size_t : public ast_visitor_t {

  error_manager_t &errs_;

  sema_array_size_t(ccml_t &ccml)
    : errs_(ccml.errors()) {
  }

  void visit(ast_decl_var_t *d) override {
    if (d->is_array()) {
      if (d->size->val_ <= 1) {
        errs_.array_size_must_be_greater_than(*d->name);
      }
    }
  }

  void visit(ast_program_t *p) override {
    ast_visitor_t::visit(p);
  }
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
// generate an init function
//
struct sema_init_t : public ast_visitor_t {

  ccml_t &ccml_;
  ast_t &ast_;
  ast_decl_func_t *init_;

  sema_init_t(ccml_t &ccml)
    : ccml_(ccml)
    , ast_(ccml.ast())
    , init_(nullptr) {
  }

  void on_global(ast_decl_var_t *v) {
    if (!v->expr) {
      return;
    }
    if (auto *n = v->expr->cast<ast_exp_lit_var_t>()) {
      auto *a = ast_.alloc<ast_stmt_assign_var_t>(v->name);
      a->decl = v;
      a->expr = v->expr;
      init_->body->add(a);
      return;
    }
    if (auto *n = v->expr->cast<ast_exp_lit_str_t>()) {
      auto *a = ast_.alloc<ast_stmt_assign_var_t>(v->name);
      a->decl = v;
      a->expr = v->expr;
      init_->body->add(a);
      return;
    }
    if (auto *n = v->expr->cast<ast_array_init_t>()) {
      int i = 0;
      for (auto *t : n->item) {
        auto *a = ast_.alloc<ast_stmt_assign_array_t>(v->name);
        a->decl = v;
        a->index = ast_.alloc<ast_exp_lit_var_t>(i);
        switch (t->type_) {
        case TOK_INT:
          a->expr = ast_.alloc<ast_exp_lit_var_t>(t->get_int());
          break;
        case TOK_FLOAT:
          a->expr = ast_.alloc<ast_exp_lit_float_t>(t->get_float());
          break;
        case TOK_STRING:
          a->expr = ast_.alloc<ast_exp_lit_str_t>(t->string());
          break;
        case TOK_NONE:
          a->expr = ast_.alloc<ast_exp_none_t>();
          break;
        }
        init_->body->add(a);
        ++i;
      }
      return;
    }
  }

  void visit(ast_program_t *p) override {
    init_ = ccml_.ast().alloc<ast_decl_func_t>("@init");
    init_->body = ccml_.ast().alloc<ast_block_t>();

    for (auto *n : p->children) {
      if (auto *d = n->cast<ast_decl_var_t>()) {
        on_global(d);
      }
    }

    p->children.push_back(init_);
  }
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
void run_sema(ccml_t &ccml) {
  auto *prog = &(ccml.ast().program);
  sema_global_var_t(ccml).visit(prog);
  sema_decl_annotate_t(ccml).visit(prog);
  sema_type_annotation_t(ccml).visit(prog);
  sema_multi_decls_t(ccml).visit(prog);
  sema_num_args_t(ccml).visit(prog);
  sema_type_uses_t(ccml).visit(prog);
  sema_array_size_t(ccml).visit(prog);
  sema_init_t(ccml).visit(prog);
}

} // namespace ccml
