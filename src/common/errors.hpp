#pragma once
#include <stdexcept>
#include <string>
#include <utility>

class SyntaxError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class CompileTimeValueEvalFail : public SyntaxError {
 public:
  using SyntaxError::SyntaxError;
};

class MainFuncNotFound : public SyntaxError {
 public:
  MainFuncNotFound() : SyntaxError("main function not found") {}
};

class NegativeArraySize : public SyntaxError {
 public:
  NegativeArraySize() : SyntaxError("negative array size") {}
};

class VoidFuncReturnValueUsed : public SyntaxError {
 public:
  VoidFuncReturnValueUsed()
      : SyntaxError("use the return value of void function") {}
};

class ArrayTypedValueUsed : public SyntaxError {
 public:
  ArrayTypedValueUsed()
      : SyntaxError("use the value of an array-typed expression") {}
};

class InvalidInitList : public SyntaxError {
 public:
  InvalidInitList() : SyntaxError("invalid initialization list") {}
};

class DuplicateLocalName : public SyntaxError {
 public:
  DuplicateLocalName(std::string name)
      : SyntaxError("duplicate local name: " + std::move(name)) {}
};

class DuplicateGlobalName : public SyntaxError {
 public:
  DuplicateGlobalName(std::string name)
      : SyntaxError("duplicate global name: " + std::move(name)) {}
};

class InvalidMainFuncInterface : public SyntaxError {
 public:
  InvalidMainFuncInterface(std::string msg) : SyntaxError(std::move(msg)) {}
};

class AssignmentTypeError : public SyntaxError {
 public:
  AssignmentTypeError(std::string msg) : SyntaxError(std::move(msg)) {}
};

class InvalidBreak : public SyntaxError {
 public:
  InvalidBreak() : SyntaxError("break should be in loop") {}
};

class InvalidContinue : public SyntaxError {
 public:
  InvalidContinue() : SyntaxError("continue should be in loop") {}
};

class InvalidReturn : public SyntaxError {
 public:
  InvalidReturn(std::string msg) : SyntaxError(std::move(msg)) {}
};

class UnrecognizedVarName : public SyntaxError {
 public:
  UnrecognizedVarName(std::string name)
      : SyntaxError("unrecognized variable name: " + std::move(name)) {}
};

class InvalidIndexOperator : public SyntaxError {
 public:
  InvalidIndexOperator() : SyntaxError("operator[] on non-array") {}
};

class UnrecognizedFuncName : public SyntaxError {
 public:
  UnrecognizedFuncName(std::string name)
      : SyntaxError("unrecognized function name: " + std::move(name)) {}
};

class InvalidFuncCallArg : public SyntaxError {
 public:
  InvalidFuncCallArg(std::string msg) : SyntaxError(std::move(msg)) {}
};

class InvalidLiteral : public SyntaxError {
 public:
  InvalidLiteral(std::string msg) : SyntaxError(std::move(msg)) {}
};

class RuntimeError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};