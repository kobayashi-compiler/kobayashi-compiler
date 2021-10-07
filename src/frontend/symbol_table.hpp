#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "common/common.hpp"
#include "frontend/init_value.hpp"
#include "optimizer/ir.hpp"

struct Type {
  std::vector<MemSize> array_dims;
  bool is_const, omit_first_dim;

  Type();                      // int
  bool is_array() const;       // array_dims not empty || omit_first_dim
  Type deref_one_dim() const;  // throw when not array. return the type where
                               // the first dimension is removed
  size_t count_array_dims() const;
  MemSize count_elements() const;
  MemSize size() const;
  bool check_assign(const Type &rhs) const;  // don't check is_const
  bool check_index(const std::vector<MemSize> &index);
  MemSize get_index(
      const std::vector<MemSize> &index);  // call when check_index is true

  static const Type UnknownLengthArray;
};

struct StringType {};

struct FunctionInterface {
  bool return_value_non_void, variadic;
  std::vector<std::variant<Type, StringType>> args_type;

  FunctionInterface();
};

struct FunctionTableEntry {
  IR::Func *ir_func;
  FunctionInterface interface;
};

struct FunctionTable {
  std::map<std::string, std::unique_ptr<FunctionTableEntry>> mapping;

  FunctionTableEntry *resolve(const std::string &name);
  void register_func(const std::string &name, IR::Func *ir_func,
                     const FunctionInterface &interface);
};

struct VariableTableEntry {
  IR::MemObject *ir_obj;
  Type type;
  int arg_id;                       // -1 if not array parameter
  std::vector<int32_t> const_init;  // empty when !type.is_const
};

struct VariableTable {
  std::map<std::string, std::unique_ptr<VariableTableEntry>> mapping;
  VariableTable *parent;

  VariableTable(VariableTable *_parent);
  VariableTableEntry *resolve(const std::string &name);
  VariableTableEntry *recursively_resolve(const std::string &name);
  void register_var(const std::string &name, IR::MemObject *ir_obj,
                    const Type &type);
  void register_const(const std::string &name, IR::MemObject *ir_obj,
                      const Type &type, std::vector<int32_t> init);
};
