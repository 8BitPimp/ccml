#pragma once
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <map>

#include "value.h"
#include "instructions.h"
#include "thread_error.h"


namespace ccml {

enum instruction_e;
struct instruction_t;

struct error_manager_t;
struct error_t;

struct lexer_t;
struct token_t;
struct token_stream_t;

struct parser_t;
struct function_t;
struct identifier_t;

struct ast_t;
struct ast_block_t;
struct ast_node_t;
struct ast_program_t;
struct ast_exp_ident_t;
struct ast_exp_array_t;
struct ast_exp_call_t;
struct ast_exp_bin_op_t;
struct ast_exp_unary_op_t;
struct ast_stmt_if_t;
struct ast_stmt_while_t;
struct ast_stmt_for_t;
struct ast_stmt_return_t;
struct ast_stmt_assign_var_t;
struct ast_stmt_assign_array_t;
struct ast_decl_func_t;
struct ast_decl_var_t;

struct asm_stream_t;
struct codegen_t;

struct disassembler_t;

struct thread_t;

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
typedef void(*ccml_syscall_t)(struct thread_t &thread);

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

struct identifier_t {
  std::string name_;
  int32_t offset_;
};

// But we need a PC for each function
struct function_t {
  std::string name_;
  ccml_syscall_t sys_;

  // XXX: REMOVE ME
  int32_t pos_;
  uint32_t num_args_;

  uint32_t code_start_;
  uint32_t code_end_;

  std::vector<identifier_t> locals_;
  std::vector<identifier_t> args_;

  bool is_syscall() const {
    return sys_ != nullptr;
  }

  function_t()
    : sys_(nullptr)
    , pos_(0)
    , num_args_(0)
    , code_start_(0)
    , code_end_(0)
  {
  }

  function_t(const std::string &name, ccml_syscall_t sys, int32_t num_args)
    : name_(name), pos_(-1), sys_(sys), num_args_(num_args) {}

  function_t(const std::string &name, int32_t pos, int32_t num_args)
    : name_(name), pos_(pos), sys_(nullptr), num_args_(num_args) {}
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct code_store_t {

  code_store_t();

  const uint8_t *data() const {
    return code_.data();
  }

  uint8_t *data() {
    return code_.data();
  }

  const size_t size() const {
    return code_.size();
  }

  const uint8_t *end() const {
    return code_.data() + code_.size();
  }

  asm_stream_t &stream() const {
    return *stream_;
  }

  const std::map<uint32_t, uint32_t> &line_table() const {
    return line_table_;
  }

  int32_t get_line(uint32_t pc) const {
    auto itt = line_table_.find(pc);
    if (itt != line_table_.end()) {
      return itt->second;
    }
    return -1;
  }

  void reset();

  const std::vector<std::string> &strings() const {
    return strings_;
  }

  std::vector<std::string> &strings() {
    return strings_;
  }

protected:
  friend struct asm_stream_t;

  void add_line(uint32_t pc, uint32_t line) {
    line_table_[pc] = line;
  }

  struct pc_range_t {
    uint32_t pc_start_;
    uint32_t pc_end_;
    ast_decl_func_t *f_;
  };

  // pc to function mapping
  std::vector<pc_range_t> pc_range_;

  // bytecode array
  std::array<uint8_t, 1024 * 8> code_;

  // assembly streamer
  std::unique_ptr<asm_stream_t> stream_;

  // line table [PC -> Line]
  std::map<uint32_t, uint32_t> line_table_;

  // string table
  std::vector<std::string> strings_;

  // function to pc mapping
  // std::map<const ast_decl_func_t, uint32_t> funcs_;
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
struct ccml_t {

  ccml_t();

  // accessors
  error_manager_t &errors()       { return *errors_; }
  code_store_t    &store()        { return store_; }
  lexer_t         &lexer()        { return *lexer_; }
  parser_t        &parser()       { return *parser_; }
  ast_t           &ast()          { return *ast_; }
  codegen_t       &codegen()      { return *codegen_; }
  disassembler_t  &disassembler() { return *disassembler_; }

  bool build(const char *source, error_t &error);

  void reset();

  const uint8_t *code() const {
    return store_.data();
  }

  const function_t *find_function(const std::string &name) const;
  const function_t *find_function(const uint32_t pc) const;
  function_t *find_function(const std::string &name);

  void add_function(const std::string &name, ccml_syscall_t sys, int32_t num_args);

  const std::vector<function_t> &functions() const {
    return functions_;
  }

  const std::vector<std::string> &strings() const {
    return store_.strings();
  }

  // enable codegen optimizations
  bool optimize;

protected:
  friend struct lexer_t;
  friend struct parser_t;
  friend struct codegen_t;
  friend struct disassembler_t;
  friend struct token_stream_t;
  friend struct error_manager_t;
  friend struct pregen_functions_t;
  friend struct codegen_pass_t;

  void add_builtins_();

  void add_(const function_t &func) {
    functions_.push_back(func);
  }

  void add_(const std::string &string) {
    store_.strings().push_back(string);
  }

  // the code store
  code_store_t store_;
  std::vector<function_t> functions_;

  std::unique_ptr<error_manager_t> errors_;
  std::unique_ptr<lexer_t>         lexer_;
  std::unique_ptr<parser_t>        parser_;
  std::unique_ptr<ast_t>           ast_;
  std::unique_ptr<codegen_t>       codegen_;
  std::unique_ptr<disassembler_t>  disassembler_;
};

} // namespace ccml
