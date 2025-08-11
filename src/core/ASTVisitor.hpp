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
    virtual void visit(ASTNode& node) {}
    
    // Parent node visit method
    virtual void visit(ASTNode_Parent& node) {}
    
    // Concrete node visit methods
    virtual void visit(ASTNode_Block& node) {}
    virtual void visit(ASTNode_Function& node) {}
    virtual void visit(ASTNode_FunctionCall& node) {}
    virtual void visit(ASTNode_If& node) {}
    virtual void visit(ASTNode_While& node) {}
    virtual void visit(ASTNode_Return& node) {}
    virtual void visit(ASTNode_Break& node) {}
    virtual void visit(ASTNode_Continue& node) {}
    virtual void visit(ASTNode_ToDouble& node) {}
    virtual void visit(ASTNode_ToInt& node) {}
    virtual void visit(ASTNode_ToString& node) {}
    virtual void visit(ASTNode_Math1& node) {}
    virtual void visit(ASTNode_Math2& node) {}
    virtual void visit(ASTNode_CharLit& node) {}
    virtual void visit(ASTNode_IntLit& node) {}
    virtual void visit(ASTNode_FloatLit& node) {}
    virtual void visit(ASTNode_StringLit& node) {}
    virtual void visit(ASTNode_Var& node) {}
    virtual void visit(ASTNode_Indexing& node) {}
    virtual void visit(ASTNode_Size& node) {}
};