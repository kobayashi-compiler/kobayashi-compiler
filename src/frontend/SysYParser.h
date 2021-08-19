
// Generated from SysY.g4 by ANTLR 4.8

#pragma once

#include "antlr4-runtime.h"

class SysYParser : public antlr4::Parser {
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

  enum {
    RuleCompUnit = 0,
    RuleDecl = 1,
    RuleConstDecl = 2,
    RuleBType = 3,
    RuleConstDef = 4,
    RuleConstInitVal = 5,
    RuleVarDecl = 6,
    RuleVarDef = 7,
    RuleInitVal = 8,
    RuleFuncDef = 9,
    RuleFuncType = 10,
    RuleFuncFParams = 11,
    RuleFuncFParam = 12,
    RuleBlock = 13,
    RuleBlockItem = 14,
    RuleStmt = 15,
    RuleExp = 16,
    RuleCond = 17,
    RuleLVal = 18,
    RulePrimaryExp = 19,
    RuleNumber = 20,
    RuleUnaryExp = 21,
    RuleUnaryOp = 22,
    RuleFuncRParams = 23,
    RuleFuncRParam = 24,
    RuleMulExp = 25,
    RuleAddExp = 26,
    RuleRelExp = 27,
    RuleEqExp = 28,
    RuleLAndExp = 29,
    RuleLOrExp = 30,
    RuleConstExp = 31
  };

  SysYParser(antlr4::TokenStream *input);
  ~SysYParser();

  virtual std::string getGrammarFileName() const override;
  virtual const antlr4::atn::ATN &getATN() const override { return _atn; };
  virtual const std::vector<std::string> &getTokenNames() const override {
    return _tokenNames;
  };  // deprecated: use vocabulary instead.
  virtual const std::vector<std::string> &getRuleNames() const override;
  virtual antlr4::dfa::Vocabulary &getVocabulary() const override;

  class CompUnitContext;
  class DeclContext;
  class ConstDeclContext;
  class BTypeContext;
  class ConstDefContext;
  class ConstInitValContext;
  class VarDeclContext;
  class VarDefContext;
  class InitValContext;
  class FuncDefContext;
  class FuncTypeContext;
  class FuncFParamsContext;
  class FuncFParamContext;
  class BlockContext;
  class BlockItemContext;
  class StmtContext;
  class ExpContext;
  class CondContext;
  class LValContext;
  class PrimaryExpContext;
  class NumberContext;
  class UnaryExpContext;
  class UnaryOpContext;
  class FuncRParamsContext;
  class FuncRParamContext;
  class MulExpContext;
  class AddExpContext;
  class RelExpContext;
  class EqExpContext;
  class LAndExpContext;
  class LOrExpContext;
  class ConstExpContext;

  class CompUnitContext : public antlr4::ParserRuleContext {
   public:
    CompUnitContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EOF();
    std::vector<DeclContext *> decl();
    DeclContext *decl(size_t i);
    std::vector<FuncDefContext *> funcDef();
    FuncDefContext *funcDef(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  CompUnitContext *compUnit();

  class DeclContext : public antlr4::ParserRuleContext {
   public:
    DeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ConstDeclContext *constDecl();
    VarDeclContext *varDecl();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  DeclContext *decl();

  class ConstDeclContext : public antlr4::ParserRuleContext {
   public:
    ConstDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Const();
    BTypeContext *bType();
    std::vector<ConstDefContext *> constDef();
    ConstDefContext *constDef(size_t i);
    antlr4::tree::TerminalNode *Semicolon();
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ConstDeclContext *constDecl();

  class BTypeContext : public antlr4::ParserRuleContext {
   public:
    BTypeContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Int();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  BTypeContext *bType();

  class ConstDefContext : public antlr4::ParserRuleContext {
   public:
    ConstDefContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Identifier();
    ConstInitValContext *constInitVal();
    std::vector<antlr4::tree::TerminalNode *> Lbrkt();
    antlr4::tree::TerminalNode *Lbrkt(size_t i);
    std::vector<ConstExpContext *> constExp();
    ConstExpContext *constExp(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Rbrkt();
    antlr4::tree::TerminalNode *Rbrkt(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ConstDefContext *constDef();

  class ConstInitValContext : public antlr4::ParserRuleContext {
   public:
    ConstInitValContext(antlr4::ParserRuleContext *parent,
                        size_t invokingState);

    ConstInitValContext() = default;
    void copyFrom(ConstInitValContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class ListConstInitValContext : public ConstInitValContext {
   public:
    ListConstInitValContext(ConstInitValContext *ctx);

    antlr4::tree::TerminalNode *Lbrace();
    antlr4::tree::TerminalNode *Rbrace();
    std::vector<ConstInitValContext *> constInitVal();
    ConstInitValContext *constInitVal(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ScalarConstInitValContext : public ConstInitValContext {
   public:
    ScalarConstInitValContext(ConstInitValContext *ctx);

    ConstExpContext *constExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ConstInitValContext *constInitVal();

  class VarDeclContext : public antlr4::ParserRuleContext {
   public:
    VarDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    BTypeContext *bType();
    std::vector<VarDefContext *> varDef();
    VarDefContext *varDef(size_t i);
    antlr4::tree::TerminalNode *Semicolon();
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  VarDeclContext *varDecl();

  class VarDefContext : public antlr4::ParserRuleContext {
   public:
    VarDefContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    VarDefContext() = default;
    void copyFrom(VarDefContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class UninitVarDefContext : public VarDefContext {
   public:
    UninitVarDefContext(VarDefContext *ctx);

    antlr4::tree::TerminalNode *Identifier();
    std::vector<antlr4::tree::TerminalNode *> Lbrkt();
    antlr4::tree::TerminalNode *Lbrkt(size_t i);
    std::vector<ConstExpContext *> constExp();
    ConstExpContext *constExp(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Rbrkt();
    antlr4::tree::TerminalNode *Rbrkt(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class InitVarDefContext : public VarDefContext {
   public:
    InitVarDefContext(VarDefContext *ctx);

    antlr4::tree::TerminalNode *Identifier();
    InitValContext *initVal();
    std::vector<antlr4::tree::TerminalNode *> Lbrkt();
    antlr4::tree::TerminalNode *Lbrkt(size_t i);
    std::vector<ConstExpContext *> constExp();
    ConstExpContext *constExp(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Rbrkt();
    antlr4::tree::TerminalNode *Rbrkt(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  VarDefContext *varDef();

  class InitValContext : public antlr4::ParserRuleContext {
   public:
    InitValContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    InitValContext() = default;
    void copyFrom(InitValContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class ScalarInitValContext : public InitValContext {
   public:
    ScalarInitValContext(InitValContext *ctx);

    ExpContext *exp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ListInitvalContext : public InitValContext {
   public:
    ListInitvalContext(InitValContext *ctx);

    antlr4::tree::TerminalNode *Lbrace();
    antlr4::tree::TerminalNode *Rbrace();
    std::vector<InitValContext *> initVal();
    InitValContext *initVal(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  InitValContext *initVal();

  class FuncDefContext : public antlr4::ParserRuleContext {
   public:
    FuncDefContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    FuncTypeContext *funcType();
    antlr4::tree::TerminalNode *Identifier();
    antlr4::tree::TerminalNode *Lparen();
    antlr4::tree::TerminalNode *Rparen();
    BlockContext *block();
    FuncFParamsContext *funcFParams();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncDefContext *funcDef();

  class FuncTypeContext : public antlr4::ParserRuleContext {
   public:
    FuncTypeContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Void();
    antlr4::tree::TerminalNode *Int();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncTypeContext *funcType();

  class FuncFParamsContext : public antlr4::ParserRuleContext {
   public:
    FuncFParamsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<FuncFParamContext *> funcFParam();
    FuncFParamContext *funcFParam(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncFParamsContext *funcFParams();

  class FuncFParamContext : public antlr4::ParserRuleContext {
   public:
    FuncFParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    BTypeContext *bType();
    antlr4::tree::TerminalNode *Identifier();
    std::vector<antlr4::tree::TerminalNode *> Lbrkt();
    antlr4::tree::TerminalNode *Lbrkt(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Rbrkt();
    antlr4::tree::TerminalNode *Rbrkt(size_t i);
    std::vector<ConstExpContext *> constExp();
    ConstExpContext *constExp(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncFParamContext *funcFParam();

  class BlockContext : public antlr4::ParserRuleContext {
   public:
    BlockContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Lbrace();
    antlr4::tree::TerminalNode *Rbrace();
    std::vector<BlockItemContext *> blockItem();
    BlockItemContext *blockItem(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  BlockContext *block();

  class BlockItemContext : public antlr4::ParserRuleContext {
   public:
    BlockItemContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    DeclContext *decl();
    StmtContext *stmt();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  BlockItemContext *blockItem();

  class StmtContext : public antlr4::ParserRuleContext {
   public:
    StmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    StmtContext() = default;
    void copyFrom(StmtContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class WhileStmtContext : public StmtContext {
   public:
    WhileStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *While();
    antlr4::tree::TerminalNode *Lparen();
    CondContext *cond();
    antlr4::tree::TerminalNode *Rparen();
    StmtContext *stmt();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class BlockStmtContext : public StmtContext {
   public:
    BlockStmtContext(StmtContext *ctx);

    BlockContext *block();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class AssignmentContext : public StmtContext {
   public:
    AssignmentContext(StmtContext *ctx);

    LValContext *lVal();
    ExpContext *exp();
    antlr4::tree::TerminalNode *Semicolon();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class IfStmt1Context : public StmtContext {
   public:
    IfStmt1Context(StmtContext *ctx);

    antlr4::tree::TerminalNode *If();
    antlr4::tree::TerminalNode *Lparen();
    CondContext *cond();
    antlr4::tree::TerminalNode *Rparen();
    StmtContext *stmt();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class BreakStmtContext : public StmtContext {
   public:
    BreakStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *Break();
    antlr4::tree::TerminalNode *Semicolon();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ExpStmtContext : public StmtContext {
   public:
    ExpStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *Semicolon();
    ExpContext *exp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class IfStmt2Context : public StmtContext {
   public:
    IfStmt2Context(StmtContext *ctx);

    antlr4::tree::TerminalNode *If();
    antlr4::tree::TerminalNode *Lparen();
    CondContext *cond();
    antlr4::tree::TerminalNode *Rparen();
    std::vector<StmtContext *> stmt();
    StmtContext *stmt(size_t i);
    antlr4::tree::TerminalNode *Else();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ReturnStmtContext : public StmtContext {
   public:
    ReturnStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *Return();
    antlr4::tree::TerminalNode *Semicolon();
    ExpContext *exp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ContinueStmtContext : public StmtContext {
   public:
    ContinueStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *Continue();
    antlr4::tree::TerminalNode *Semicolon();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  StmtContext *stmt();

  class ExpContext : public antlr4::ParserRuleContext {
   public:
    ExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    AddExpContext *addExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ExpContext *exp();

  class CondContext : public antlr4::ParserRuleContext {
   public:
    CondContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    LOrExpContext *lOrExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  CondContext *cond();

  class LValContext : public antlr4::ParserRuleContext {
   public:
    LValContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Identifier();
    std::vector<antlr4::tree::TerminalNode *> Lbrkt();
    antlr4::tree::TerminalNode *Lbrkt(size_t i);
    std::vector<ExpContext *> exp();
    ExpContext *exp(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Rbrkt();
    antlr4::tree::TerminalNode *Rbrkt(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  LValContext *lVal();

  class PrimaryExpContext : public antlr4::ParserRuleContext {
   public:
    PrimaryExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    PrimaryExpContext() = default;
    void copyFrom(PrimaryExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class PrimaryExp2Context : public PrimaryExpContext {
   public:
    PrimaryExp2Context(PrimaryExpContext *ctx);

    LValContext *lVal();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class PrimaryExp1Context : public PrimaryExpContext {
   public:
    PrimaryExp1Context(PrimaryExpContext *ctx);

    antlr4::tree::TerminalNode *Lparen();
    ExpContext *exp();
    antlr4::tree::TerminalNode *Rparen();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class PrimaryExp3Context : public PrimaryExpContext {
   public:
    PrimaryExp3Context(PrimaryExpContext *ctx);

    NumberContext *number();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  PrimaryExpContext *primaryExp();

  class NumberContext : public antlr4::ParserRuleContext {
   public:
    NumberContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IntLiteral();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  NumberContext *number();

  class UnaryExpContext : public antlr4::ParserRuleContext {
   public:
    UnaryExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    UnaryExpContext() = default;
    void copyFrom(UnaryExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class Unary1Context : public UnaryExpContext {
   public:
    Unary1Context(UnaryExpContext *ctx);

    PrimaryExpContext *primaryExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Unary2Context : public UnaryExpContext {
   public:
    Unary2Context(UnaryExpContext *ctx);

    antlr4::tree::TerminalNode *Identifier();
    antlr4::tree::TerminalNode *Lparen();
    antlr4::tree::TerminalNode *Rparen();
    FuncRParamsContext *funcRParams();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Unary3Context : public UnaryExpContext {
   public:
    Unary3Context(UnaryExpContext *ctx);

    UnaryOpContext *unaryOp();
    UnaryExpContext *unaryExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  UnaryExpContext *unaryExp();

  class UnaryOpContext : public antlr4::ParserRuleContext {
   public:
    UnaryOpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *Addition();
    antlr4::tree::TerminalNode *Minus();
    antlr4::tree::TerminalNode *Exclamation();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  UnaryOpContext *unaryOp();

  class FuncRParamsContext : public antlr4::ParserRuleContext {
   public:
    FuncRParamsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<FuncRParamContext *> funcRParam();
    FuncRParamContext *funcRParam(size_t i);
    std::vector<antlr4::tree::TerminalNode *> Comma();
    antlr4::tree::TerminalNode *Comma(size_t i);

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncRParamsContext *funcRParams();

  class FuncRParamContext : public antlr4::ParserRuleContext {
   public:
    FuncRParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    FuncRParamContext() = default;
    void copyFrom(FuncRParamContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class StringAsRParamContext : public FuncRParamContext {
   public:
    StringAsRParamContext(FuncRParamContext *ctx);

    antlr4::tree::TerminalNode *STRING();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class ExpAsRParamContext : public FuncRParamContext {
   public:
    ExpAsRParamContext(FuncRParamContext *ctx);

    ExpContext *exp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  FuncRParamContext *funcRParam();

  class MulExpContext : public antlr4::ParserRuleContext {
   public:
    MulExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    MulExpContext() = default;
    void copyFrom(MulExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class Mul2Context : public MulExpContext {
   public:
    Mul2Context(MulExpContext *ctx);

    MulExpContext *mulExp();
    UnaryExpContext *unaryExp();
    antlr4::tree::TerminalNode *Multiplication();
    antlr4::tree::TerminalNode *Division();
    antlr4::tree::TerminalNode *Modulo();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Mul1Context : public MulExpContext {
   public:
    Mul1Context(MulExpContext *ctx);

    UnaryExpContext *unaryExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  MulExpContext *mulExp();
  MulExpContext *mulExp(int precedence);
  class AddExpContext : public antlr4::ParserRuleContext {
   public:
    AddExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    AddExpContext() = default;
    void copyFrom(AddExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class Add2Context : public AddExpContext {
   public:
    Add2Context(AddExpContext *ctx);

    AddExpContext *addExp();
    MulExpContext *mulExp();
    antlr4::tree::TerminalNode *Addition();
    antlr4::tree::TerminalNode *Minus();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Add1Context : public AddExpContext {
   public:
    Add1Context(AddExpContext *ctx);

    MulExpContext *mulExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  AddExpContext *addExp();
  AddExpContext *addExp(int precedence);
  class RelExpContext : public antlr4::ParserRuleContext {
   public:
    RelExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    RelExpContext() = default;
    void copyFrom(RelExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class Rel2Context : public RelExpContext {
   public:
    Rel2Context(RelExpContext *ctx);

    RelExpContext *relExp();
    AddExpContext *addExp();
    antlr4::tree::TerminalNode *LT();
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LE();
    antlr4::tree::TerminalNode *GE();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Rel1Context : public RelExpContext {
   public:
    Rel1Context(RelExpContext *ctx);

    AddExpContext *addExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  RelExpContext *relExp();
  RelExpContext *relExp(int precedence);
  class EqExpContext : public antlr4::ParserRuleContext {
   public:
    EqExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    EqExpContext() = default;
    void copyFrom(EqExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class Eq1Context : public EqExpContext {
   public:
    Eq1Context(EqExpContext *ctx);

    RelExpContext *relExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class Eq2Context : public EqExpContext {
   public:
    Eq2Context(EqExpContext *ctx);

    EqExpContext *eqExp();
    RelExpContext *relExp();
    antlr4::tree::TerminalNode *EQ();
    antlr4::tree::TerminalNode *NEQ();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  EqExpContext *eqExp();
  EqExpContext *eqExp(int precedence);
  class LAndExpContext : public antlr4::ParserRuleContext {
   public:
    LAndExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    LAndExpContext() = default;
    void copyFrom(LAndExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class LAnd2Context : public LAndExpContext {
   public:
    LAnd2Context(LAndExpContext *ctx);

    LAndExpContext *lAndExp();
    antlr4::tree::TerminalNode *LAND();
    EqExpContext *eqExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class LAnd1Context : public LAndExpContext {
   public:
    LAnd1Context(LAndExpContext *ctx);

    EqExpContext *eqExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  LAndExpContext *lAndExp();
  LAndExpContext *lAndExp(int precedence);
  class LOrExpContext : public antlr4::ParserRuleContext {
   public:
    LOrExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);

    LOrExpContext() = default;
    void copyFrom(LOrExpContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;
  };

  class LOr1Context : public LOrExpContext {
   public:
    LOr1Context(LOrExpContext *ctx);

    LAndExpContext *lAndExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class LOr2Context : public LOrExpContext {
   public:
    LOr2Context(LOrExpContext *ctx);

    LOrExpContext *lOrExp();
    antlr4::tree::TerminalNode *LOR();
    LAndExpContext *lAndExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  LOrExpContext *lOrExp();
  LOrExpContext *lOrExp(int precedence);
  class ConstExpContext : public antlr4::ParserRuleContext {
   public:
    ConstExpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    AddExpContext *addExp();

    virtual antlrcpp::Any accept(
        antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ConstExpContext *constExp();

  virtual bool sempred(antlr4::RuleContext *_localctx, size_t ruleIndex,
                       size_t predicateIndex) override;
  bool mulExpSempred(MulExpContext *_localctx, size_t predicateIndex);
  bool addExpSempred(AddExpContext *_localctx, size_t predicateIndex);
  bool relExpSempred(RelExpContext *_localctx, size_t predicateIndex);
  bool eqExpSempred(EqExpContext *_localctx, size_t predicateIndex);
  bool lAndExpSempred(LAndExpContext *_localctx, size_t predicateIndex);
  bool lOrExpSempred(LOrExpContext *_localctx, size_t predicateIndex);

 private:
  static std::vector<antlr4::dfa::DFA> _decisionToDFA;
  static antlr4::atn::PredictionContextCache _sharedContextCache;
  static std::vector<std::string> _ruleNames;
  static std::vector<std::string> _tokenNames;

  static std::vector<std::string> _literalNames;
  static std::vector<std::string> _symbolicNames;
  static antlr4::dfa::Vocabulary _vocabulary;
  static antlr4::atn::ATN _atn;
  static std::vector<uint16_t> _serializedATN;

  struct Initializer {
    Initializer();
  };
  static Initializer _init;
};
