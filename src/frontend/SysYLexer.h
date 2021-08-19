
// Generated from SysY.g4 by ANTLR 4.8

#pragma once

#include "antlr4-runtime.h"

class SysYLexer : public antlr4::Lexer {
 public:
  enum {
    T__0 = 1,
    Int = 2,
    Void = 3,
    Const = 4,
    Return = 5,
    If = 6,
    Else = 7,
    For = 8,
    While = 9,
    Do = 10,
    Break = 11,
    Continue = 12,
    Lparen = 13,
    Rparen = 14,
    Lbrkt = 15,
    Rbrkt = 16,
    Lbrace = 17,
    Rbrace = 18,
    Comma = 19,
    Semicolon = 20,
    Question = 21,
    Colon = 22,
    Minus = 23,
    Exclamation = 24,
    Tilde = 25,
    Addition = 26,
    Multiplication = 27,
    Division = 28,
    Modulo = 29,
    LAND = 30,
    LOR = 31,
    EQ = 32,
    NEQ = 33,
    LT = 34,
    LE = 35,
    GT = 36,
    GE = 37,
    IntLiteral = 38,
    Identifier = 39,
    STRING = 40,
    WS = 41,
    LINE_COMMENT = 42,
    COMMENT = 43
  };

  SysYLexer(antlr4::CharStream* input);
  ~SysYLexer();

  virtual std::string getGrammarFileName() const override;
  virtual const std::vector<std::string>& getRuleNames() const override;

  virtual const std::vector<std::string>& getChannelNames() const override;
  virtual const std::vector<std::string>& getModeNames() const override;
  virtual const std::vector<std::string>& getTokenNames()
      const override;  // deprecated, use vocabulary instead
  virtual antlr4::dfa::Vocabulary& getVocabulary() const override;

  virtual const std::vector<uint16_t> getSerializedATN() const override;
  virtual const antlr4::atn::ATN& getATN() const override;

 private:
  static std::vector<antlr4::dfa::DFA> _decisionToDFA;
  static antlr4::atn::PredictionContextCache _sharedContextCache;
  static std::vector<std::string> _ruleNames;
  static std::vector<std::string> _tokenNames;
  static std::vector<std::string> _channelNames;
  static std::vector<std::string> _modeNames;

  static std::vector<std::string> _literalNames;
  static std::vector<std::string> _symbolicNames;
  static antlr4::dfa::Vocabulary _vocabulary;
  static antlr4::atn::ATN _atn;
  static std::vector<uint16_t> _serializedATN;

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

  struct Initializer {
    Initializer();
  };
  static Initializer _init;
};
