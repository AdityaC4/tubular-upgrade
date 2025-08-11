#pragma once

#include "ASTVisitor.hpp"
#include <iostream>

class NodeCounter : public ASTVisitor {
private:
    int count = 0;

public:
    int getCount() const { return count; }
    
    void visit(ASTNode& node) override {
        count++;
        // For base nodes, we don't need to do anything else
    }
    
    void visit(ASTNode_Parent& node) override {
        count++;
        // For parent nodes, we need to visit all children
        for (size_t i = 0; i < node.NumChildren(); i++) {
            node.GetChild(i).Accept(*this);
        }
    }
    
    // Override specific node types if needed
    void visit(ASTNode_Block& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Function& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_FunctionCall& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_If& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_While& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Return& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Break& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_Continue& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_ToDouble& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_ToInt& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_ToString& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Math1& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Math2& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_CharLit& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_IntLit& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_FloatLit& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_StringLit& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_Var& node) override {
        visit(static_cast<ASTNode&>(node));
    }
    
    void visit(ASTNode_Indexing& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
    
    void visit(ASTNode_Size& node) override {
        visit(static_cast<ASTNode_Parent&>(node));
    }
};