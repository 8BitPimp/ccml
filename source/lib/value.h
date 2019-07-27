#pragma once
#include <stdint.h>

namespace ccml {

enum value_type_t {
  val_type_unknown,
  val_type_none,
  val_type_int,
  val_type_string,
  val_type_array,
};

struct value_t {

  value_t()
    : type_(val_type_none)
  {}

  value_t(int32_t int_val)
    : type_(val_type_int)
    , v(int_val)
  {}

  bool is_int() const {
    return type() == val_type_int;
  }

  bool is_string() const {
    return type() == val_type_string;
  }

  bool is_array() const {
    return type() == val_type_array;
  }

  bool is_none() const {
    return type() == val_type_none;
  }

  void from_int(int32_t val) {
    type_ = val_type_int;
    v = val;
  }

  const std::string &str() const {
    assert(s);
    return *s;
  }

  const value_type_t type() const {
    return this == nullptr ?
      value_type_t::val_type_none :
      type_;
  }

  value_type_t type_;

  union {
    // int type
    int32_t v;
    // string type
    std::string *s;
    // array type
    struct {
      value_t** array_;
      int32_t array_size_;
    };
  };
};

} // namespace ccml
