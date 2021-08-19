
// Generated from SysY.g4 by ANTLR 4.8

#pragma once

#include "SysYVisitor.h"
#include "antlr4-runtime.h"

/**
 * This class provides an empty implementation of SysYVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the
 * available methods.
 */
class SysYBaseVisitor : public SysYVisitor {
 public:
  virtual antlrcpp::Any visitCompUnit(
      SysYParser::CompUnitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitDecl(SysYParser::DeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitConstDecl(
      SysYParser::ConstDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBType(SysYParser::BTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitConstDef(
      SysYParser::ConstDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitScalarConstInitVal(
      SysYParser::ScalarConstInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitListConstInitVal(
      SysYParser::ListConstInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitVarDecl(SysYParser::VarDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitUninitVarDef(
      SysYParser::UninitVarDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitInitVarDef(
      SysYParser::InitVarDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitScalarInitVal(
      SysYParser::ScalarInitValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitListInitval(
      SysYParser::ListInitvalContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFuncDef(SysYParser::FuncDefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFuncType(
      SysYParser::FuncTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFuncFParams(
      SysYParser::FuncFParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFuncFParam(
      SysYParser::FuncFParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBlock(SysYParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBlockItem(
      SysYParser::BlockItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitAssignment(
      SysYParser::AssignmentContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitExpStmt(SysYParser::ExpStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBlockStmt(
      SysYParser::BlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitIfStmt1(SysYParser::IfStmt1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitIfStmt2(SysYParser::IfStmt2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitWhileStmt(
      SysYParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitBreakStmt(
      SysYParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitContinueStmt(
      SysYParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitReturnStmt(
      SysYParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitExp(SysYParser::ExpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitCond(SysYParser::CondContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLVal(SysYParser::LValContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitPrimaryExp1(
      SysYParser::PrimaryExp1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitPrimaryExp2(
      SysYParser::PrimaryExp2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitPrimaryExp3(
      SysYParser::PrimaryExp3Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitNumber(SysYParser::NumberContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitUnary1(SysYParser::Unary1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitUnary2(SysYParser::Unary2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitUnary3(SysYParser::Unary3Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitUnaryOp(SysYParser::UnaryOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitFuncRParams(
      SysYParser::FuncRParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitExpAsRParam(
      SysYParser::ExpAsRParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitStringAsRParam(
      SysYParser::StringAsRParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitMul2(SysYParser::Mul2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitMul1(SysYParser::Mul1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitAdd2(SysYParser::Add2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitAdd1(SysYParser::Add1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitRel2(SysYParser::Rel2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitRel1(SysYParser::Rel1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitEq1(SysYParser::Eq1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitEq2(SysYParser::Eq2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLAnd2(SysYParser::LAnd2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLAnd1(SysYParser::LAnd1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLOr1(SysYParser::LOr1Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitLOr2(SysYParser::LOr2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual antlrcpp::Any visitConstExp(
      SysYParser::ConstExpContext *ctx) override {
    return visitChildren(ctx);
  }
};
