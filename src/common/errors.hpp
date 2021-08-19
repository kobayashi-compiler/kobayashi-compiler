#pragma once
#include <stdexcept>
#include <string>
#include <utility>

class SyntaxError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class CompileTimeValueEvalFail : SyntaxError {
 public:
  using SyntaxError::SyntaxError;
};

class MainFuncNotFound : SyntaxError {
 public:
  MainFuncNotFound() : SyntaxError("main function not found") {}
};

class NegativeArraySize : SyntaxError {
 public:
  NegativeArraySize() : SyntaxError("negative array size") {}
};

class VoidFuncReturnValueUsed : SyntaxError {
 public:
  VoidFuncReturnValueUsed()
      : SyntaxError("use the return value of void function") {}
};

class ArrayTypedValueUsed : SyntaxError {
 public:
  ArrayTypedValueUsed()
      : SyntaxError("use the value of an array-typed expression") {}
};

class InvalidInitList : SyntaxError {
 public:
  InvalidInitList() : SyntaxError("invalid initialization list") {}
};

class DuplicateLocalName : SyntaxError {
 public:
  DuplicateLocalName(std::string name)
      : SyntaxError("duplicate local name: " + std::move(name)) {}
};

class DuplicateGlobalName : SyntaxError {
 public:
  DuplicateGlobalName(std::string name)
      : SyntaxError("duplicate global name: " + std::move(name)) {}
};

class InvalidMainFuncInterface : SyntaxError {
 public:
  InvalidMainFuncInterface(std::string msg) : SyntaxError(std::move(msg)) {}
};

class AssignmentTypeError : SyntaxError {
 public:
  AssignmentTypeError(std::string msg) : SyntaxError(std::move(msg)) {}
};

class InvalidBreak : SyntaxError {
 public:
  InvalidBreak() : SyntaxError("break should be in loop") {}
};

class InvalidContinue : SyntaxError {
 public:
  InvalidContinue() : SyntaxError("continue should be in loop") {}
};

class InvalidReturn : SyntaxError {
 public:
  InvalidReturn(std::string msg) : SyntaxError(std::move(msg)) {}
};

class UnrecognizedVarName : SyntaxError {
 public:
  UnrecognizedVarName(std::string name)
      : SyntaxError("unrecognized variable name: " + std::move(name)) {}
};

class InvalidIndexOperator : SyntaxError {
 public:
  InvalidIndexOperator() : SyntaxError("operator[] on non-array") {}
};

class UnrecognizedFuncName : SyntaxError {
 public:
  UnrecognizedFuncName(std::string name)
      : SyntaxError("unrecognized function name: " + std::move(name)) {}
};

class InvalidFuncCallArg : SyntaxError {
 public:
  InvalidFuncCallArg(std::string msg) : SyntaxError(std::move(msg)) {}
};

class InvalidLiteral : SyntaxError {
 public:
  InvalidLiteral(std::string msg) : SyntaxError(std::move(msg)) {}
};

class RuntimeError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};