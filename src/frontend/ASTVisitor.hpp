#pragma once

// Forward declarations of all AST node types
class ASTNode;
class ASTNode_Parent;
class ASTNode_Block;
class ASTNode_Function;
class ASTNode_FunctionCall;
class ASTNode_If;
class ASTNode_While;
class ASTNode_Return;
class ASTNode_Break;
class ASTNode_Continue;
class ASTNode_ToDouble;
class ASTNode_ToInt;
class ASTNode_ToString;
class ASTNode_Math1;
class ASTNode_Math2;
class ASTNode_CharLit;
class ASTNode_IntLit;
class ASTNode_FloatLit;
class ASTNode_StringLit;
class ASTNode_Var;
class ASTNode_Indexing;
class ASTNode_Size;

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  // Base node visit method
  virtual void visit(ASTNode &) {}

  // Parent node visit method
  virtual void visit(ASTNode_Parent &) {}

  // Concrete node visit methods
  virtual void visit(ASTNode_Block &) {}
  virtual void visit(ASTNode_Function &) {}
  virtual void visit(ASTNode_FunctionCall &) {}
  virtual void visit(ASTNode_If &) {}
  virtual void visit(ASTNode_While &) {}
  virtual void visit(ASTNode_Return &) {}
  virtual void visit(ASTNode_Break &) {}
  virtual void visit(ASTNode_Continue &) {}
  virtual void visit(ASTNode_ToDouble &) {}
  virtual void visit(ASTNode_ToInt &) {}
  virtual void visit(ASTNode_ToString &) {}
  virtual void visit(ASTNode_Math1 &) {}
  virtual void visit(ASTNode_Math2 &) {}
  virtual void visit(ASTNode_CharLit &) {}
  virtual void visit(ASTNode_IntLit &) {}
  virtual void visit(ASTNode_FloatLit &) {}
  virtual void visit(ASTNode_StringLit &) {}
  virtual void visit(ASTNode_Var &) {}
  virtual void visit(ASTNode_Indexing &) {}
  virtual void visit(ASTNode_Size &) {}
};
