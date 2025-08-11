#pragma once

#include "../Control.hpp"
#include "ASTNode.hpp"
#include "ASTVisitor.hpp"

class WATGenerator : public ASTVisitor {
private:
  Control &control;

public:
  WATGenerator(Control &control) : control(control) {}

  // Override the visit methods to generate WAT code
  void visit(ASTNode &node) override {
    // For base nodes, we just call ToWAT
    node.ToWAT(control);
  }

  void visit(ASTNode_Parent &node) override {
    // For parent nodes, we just call ToWAT
    node.ToWAT(control);
  }

  // Override specific node types if needed
  void visit(ASTNode_Block &node) override { node.ToWAT(control); }

  void visit(ASTNode_Function &node) override { node.ToWAT(control); }

  void visit(ASTNode_FunctionCall &node) override { node.ToWAT(control); }

  void visit(ASTNode_If &node) override { node.ToWAT(control); }

  void visit(ASTNode_While &node) override { node.ToWAT(control); }

  void visit(ASTNode_Return &node) override { node.ToWAT(control); }

  void visit(ASTNode_Break &node) override { node.ToWAT(control); }

  void visit(ASTNode_Continue &node) override { node.ToWAT(control); }

  void visit(ASTNode_ToDouble &node) override { node.ToWAT(control); }

  void visit(ASTNode_ToInt &node) override { node.ToWAT(control); }

  void visit(ASTNode_ToString &node) override { node.ToWAT(control); }

  void visit(ASTNode_Math1 &node) override { node.ToWAT(control); }

  void visit(ASTNode_Math2 &node) override { node.ToWAT(control); }

  void visit(ASTNode_CharLit &node) override { node.ToWAT(control); }

  void visit(ASTNode_IntLit &node) override { node.ToWAT(control); }

  void visit(ASTNode_FloatLit &node) override { node.ToWAT(control); }

  void visit(ASTNode_StringLit &node) override { node.ToWAT(control); }

  void visit(ASTNode_Var &node) override { node.ToWAT(control); }

  void visit(ASTNode_Indexing &node) override { node.ToWAT(control); }

  void visit(ASTNode_Size &node) override { node.ToWAT(control); }
};
