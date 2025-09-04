#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include <iostream>
#include <unordered_map>
#include <memory>
#include <vector>
#include "NodeCounter.hpp"

class FunctionInliningPass : public Pass {
private:
  bool inlineEnabled;
  std::unordered_map<size_t, ASTNode_Function*> functionMap;
  size_t maxInlineDepth = 3;
  size_t currentDepth = 0;
  size_t maxInlineNodes = 40; // guard on inlined expression complexity

  std::unique_ptr<ASTNode> cloneWithSubstitution(const ASTNode &node, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (auto *var = dynamic_cast<const ASTNode_Var *>(&node)) {
      size_t varId = getVarId(*var);
      if (paramMap.find(varId) != paramMap.end()) {
        return cloneWithSubstitution(*paramMap.at(varId), {});
      }
      return cloneVar(*var);
    }

    // Handle Math2 nodes
    if (auto *math2 = dynamic_cast<const ASTNode_Math2 *>(&node)) {
      return cloneMath2(*math2, paramMap);
    }

    // Handle Math1 nodes
    if (auto *math1 = dynamic_cast<const ASTNode_Math1 *>(&node)) {
      return cloneMath1(*math1, paramMap);
    }

    // Handle function calls (for nested calls in expressions)
    if (auto *call = dynamic_cast<const ASTNode_FunctionCall *>(&node)) {
      return cloneFunctionCall(*call, paramMap);
    }

    // Handle literals
    if (auto *intLit = dynamic_cast<const ASTNode_IntLit *>(&node)) {
      return cloneIntLit(*intLit);
    }

    // Handle float literals
    if (auto *floatLit = dynamic_cast<const ASTNode_FloatLit *>(&node)) {
      return cloneFloatLit(*floatLit);
    }

    // Handle string literals  
    if (auto *strLit = dynamic_cast<const ASTNode_StringLit *>(&node)) {
      return cloneStringLit(*strLit);
    }

    // Handle Return nodes
    if (auto *returnNode = dynamic_cast<const ASTNode_Return *>(&node)) {
      return cloneReturn(*returnNode, paramMap);
    }

    // Handle Block nodes
    if (auto *block = dynamic_cast<const ASTNode_Block *>(&node)) {
      return cloneBlock(*block, paramMap);
    }

    // Handle If nodes
    if (auto *ifNode = dynamic_cast<const ASTNode_If *>(&node)) {
      return cloneIf(*ifNode, paramMap);
    }

    // Handle While nodes
    if (auto *whileNode = dynamic_cast<const ASTNode_While *>(&node)) {
      return cloneWhile(*whileNode, paramMap);
    }

    // Handle casts/conversions
    if (auto *toD = dynamic_cast<const ASTNode_ToDouble *>(&node)) {
      auto child = cloneWithSubstitution(toD->GetChild(0), paramMap);
      return std::make_unique<ASTNode_ToDouble>(std::move(child));
    }
    if (auto *toI = dynamic_cast<const ASTNode_ToInt *>(&node)) {
      auto child = cloneWithSubstitution(toI->GetChild(0), paramMap);
      return std::make_unique<ASTNode_ToInt>(std::move(child));
    }
    if (auto *toS = dynamic_cast<const ASTNode_ToString *>(&node)) {
      auto child = cloneWithSubstitution(toS->GetChild(0), paramMap);
      return std::make_unique<ASTNode_ToString>(std::move(child));
    }

    // Handle indexing and size
    if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&node)) {
      auto base = cloneWithSubstitution(idx->GetChild(0), paramMap);
      auto off = cloneWithSubstitution(idx->GetChild(1), paramMap);
      return std::make_unique<ASTNode_Indexing>(idx->GetFilePos(), std::move(base), std::move(off));
    }
    if (auto *sz = dynamic_cast<const ASTNode_Size *>(&node)) {
      auto base = cloneWithSubstitution(sz->GetChild(0), paramMap);
      return std::make_unique<ASTNode_Size>(sz->GetFilePos(), std::move(base));
    }

    return nullptr;
  }

  std::unique_ptr<ASTNode_Var> cloneVar(const ASTNode_Var &var) {
    size_t varId = getVarId(var);
    return std::make_unique<ASTNode_Var>(var.GetFilePos(), varId);
  }

  std::unique_ptr<ASTNode_IntLit> cloneIntLit(const ASTNode_IntLit &intLit) {
    return std::make_unique<ASTNode_IntLit>(intLit.GetFilePos(), intLit.GetValue());
  }

  std::unique_ptr<ASTNode_FloatLit> cloneFloatLit(const ASTNode_FloatLit &floatLit) {
    return std::make_unique<ASTNode_FloatLit>(floatLit.GetFilePos(), floatLit.GetValue());
  }

  std::unique_ptr<ASTNode_StringLit> cloneStringLit(const ASTNode_StringLit &strLit) {
    return std::make_unique<ASTNode_StringLit>(strLit.GetFilePos(), strLit.GetValue());
  }

  std::unique_ptr<ASTNode_Math2> cloneMath2(const ASTNode_Math2 &math2, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (math2.NumChildren() >= 2) {
      auto leftClone = cloneWithSubstitution(math2.GetChild(0), paramMap);
      auto rightClone = cloneWithSubstitution(math2.GetChild(1), paramMap);
      
      if (leftClone && rightClone) {
        std::string op = const_cast<ASTNode_Math2&>(math2).GetOp();
        return std::make_unique<ASTNode_Math2>(math2.GetFilePos(), op, std::move(leftClone), std::move(rightClone));
      }
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode_Math1> cloneMath1(const ASTNode_Math1 &math1, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (math1.NumChildren() == 1) {
      auto child = cloneWithSubstitution(math1.GetChild(0), paramMap);
      std::string op = const_cast<ASTNode_Math1&>(math1).GetOp();
      return std::make_unique<ASTNode_Math1>(math1.GetFilePos(), op, std::move(child));
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode_Return> cloneReturn(const ASTNode_Return &returnNode, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (returnNode.NumChildren() >= 1) {
      auto valueClone = cloneWithSubstitution(returnNode.GetChild(0), paramMap);
      if (valueClone) {
        return std::make_unique<ASTNode_Return>(returnNode.GetFilePos(), std::move(valueClone));
      }
    }
    // If no children or clone failed, create a return with a dummy integer literal
    auto dummyZero = std::make_unique<ASTNode_IntLit>(returnNode.GetFilePos(), 0);
    return std::make_unique<ASTNode_Return>(returnNode.GetFilePos(), std::move(dummyZero));
  }

  std::unique_ptr<ASTNode_Block> cloneBlock(const ASTNode_Block &block, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    auto newBlock = std::make_unique<ASTNode_Block>(block.GetFilePos());
    
    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (block.HasChild(i)) {
        auto clonedChild = cloneWithSubstitution(block.GetChild(i), paramMap);
        if (clonedChild) {
          newBlock->AddChild(std::move(clonedChild));
        }
      }
    }
    
    return newBlock;
  }

  std::unique_ptr<ASTNode_If> cloneIf(const ASTNode_If &ifNode, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (ifNode.NumChildren() >= 2) {
      auto conditionClone = cloneWithSubstitution(ifNode.GetChild(0), paramMap);
      auto thenClone = cloneWithSubstitution(ifNode.GetChild(1), paramMap);
      
      if (conditionClone && thenClone) {
        if (ifNode.NumChildren() >= 3) {
          auto elseClone = cloneWithSubstitution(ifNode.GetChild(2), paramMap);
          if (elseClone) {
            return std::make_unique<ASTNode_If>(ifNode.GetFilePos(), std::move(conditionClone), std::move(thenClone), std::move(elseClone));
          }
        } else {
          return std::make_unique<ASTNode_If>(ifNode.GetFilePos(), std::move(conditionClone), std::move(thenClone));
        }
      }
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode_While> cloneWhile(const ASTNode_While &whileNode, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    if (whileNode.NumChildren() >= 2) {
      auto conditionClone = cloneWithSubstitution(whileNode.GetChild(0), paramMap);
      auto bodyClone = cloneWithSubstitution(whileNode.GetChild(1), paramMap);
      
      if (conditionClone && bodyClone) {
        return std::make_unique<ASTNode_While>(whileNode.GetFilePos(), std::move(conditionClone), std::move(bodyClone));
      }
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode> cloneFunctionCall(const ASTNode_FunctionCall &call, const std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    std::vector<std::unique_ptr<ASTNode>> args;
    for (size_t i = 0; i < call.NumChildren(); ++i) {
      args.push_back(cloneWithSubstitution(call.GetChild(i), paramMap));
    }
    return std::make_unique<ASTNode_FunctionCall>(call.GetFilePos(), call.GetFunId(), std::move(args));
  }

public:
  FunctionInliningPass(bool enabled) : inlineEnabled(enabled) {}

  std::string getName() const override { return "FunctionInlining"; }

  void run(ASTNode &node) override {
    if (!inlineEnabled) {
      return;
    }

    // Two-phase approach
    collectFunctions(node);
    inlineFunctionCalls(node);
  }

private:
  void collectFunctions(ASTNode &node) {
    if (auto *function = dynamic_cast<ASTNode_Function *>(&node)) {
    size_t funId = function->GetFunId();
      functionMap[funId] = function;
    }
    
    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          collectFunctions(parent->GetChild(i));
        }
      }
    }
  }

  void inlineFunctionCalls(ASTNode &node) {
    if (currentDepth >= maxInlineDepth) {
      return;
    }

    // Handle different node types that can contain function calls
    if (auto *math2 = dynamic_cast<ASTNode_Math2 *>(&node)) {
      inlineMath2FunctionCalls(*math2);
    } else if (auto *block = dynamic_cast<ASTNode_Block *>(&node)) {
      inlineBlockFunctionCalls(*block);
    } else if (auto *returnNode = dynamic_cast<ASTNode_Return *>(&node)) {
      inlineReturnFunctionCalls(*returnNode);
    } else if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      // Recursively process children
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          inlineFunctionCalls(parent->GetChild(i));
        }
      }
    }
  }

  void inlineMath2FunctionCalls(ASTNode_Math2 &math2) {
    // Check if this is an assignment with a function call on the RHS
    std::string op = math2.GetOp();
    if (op == "=" && math2.NumChildren() >= 2) {
      // Check if RHS is a function call
      if (auto *funcCall = dynamic_cast<ASTNode_FunctionCall *>(&math2.GetChild(1))) {
        if (shouldInlineFunction(*funcCall)) {
          auto inlined = createInlinedExpression(*funcCall);
          if (inlined) {
            // Replace the function call with the inlined expression
            math2.ReplaceChild(1, std::move(inlined));
          }
        }
      }
    }
    
    // Recursively process children
    for (size_t i = 0; i < math2.NumChildren(); ++i) {
      if (math2.HasChild(i)) {
        inlineFunctionCalls(math2.GetChild(i));
      }
    }
  }

  void inlineBlockFunctionCalls(ASTNode_Block &block) {
    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (block.HasChild(i)) {
        inlineFunctionCalls(block.GetChild(i));
      }
    }
  }

  void inlineReturnFunctionCalls(ASTNode_Return &returnNode) {
    if (returnNode.NumChildren() >= 1) {
      if (auto *funcCall = dynamic_cast<ASTNode_FunctionCall *>(&returnNode.GetChild(0))) {
        if (shouldInlineFunction(*funcCall)) {
          auto inlined = createInlinedExpression(*funcCall);
          if (inlined) {
            returnNode.ReplaceChild(0, std::move(inlined));
          }
        }
      } else {
        // Recursively process the return expression
        inlineFunctionCalls(returnNode.GetChild(0));
      }
    }
  }

  bool shouldInlineFunction(const ASTNode_FunctionCall &funcCall) {
    size_t funId = funcCall.GetFunId();
    
    if (functionMap.find(funId) == functionMap.end()) {
      return false;
    }
    
    ASTNode_Function *function = functionMap[funId];
    
    // Check parameter count matches
    const auto& paramIds = function->GetParamIds();
    if (funcCall.NumChildren() != paramIds.size()) {
      return false; // Parameter count mismatch
    }
    // Extract a return expression we could inline
    const ASTNode *bodyNode = &function->GetChild(0);
    const ASTNode_Return *ret = nullptr;
    if (auto *block = dynamic_cast<const ASTNode_Block *>(bodyNode)) {
      if (block->NumChildren() == 1 && block->HasChild(0)) {
        ret = dynamic_cast<const ASTNode_Return *>(&block->GetChild(0));
      }
    } else {
      ret = dynamic_cast<const ASTNode_Return *>(bodyNode);
    }
    if (!ret || ret->NumChildren() != 1) return false;

    // Candidate expression to inline
    const ASTNode &expr = ret->GetChild(0);

    // Guard: expression size
    NodeCounter counter;
    const_cast<ASTNode&>(expr).Accept(counter);
    if (static_cast<size_t>(counter.getCount()) > maxInlineNodes) return false;

    // Guard: side-effect free expression
    if (hasSideEffects(expr)) return false;

    // Guard: argument duplication safety
    std::vector<size_t> usage(paramIds.size(), 0);
    for (size_t i = 0; i < paramIds.size(); ++i) {
      usage[i] = countVarUsage(expr, paramIds[i]);
    }
    for (size_t i = 0; i < paramIds.size(); ++i) {
      if (usage[i] <= 1) continue;
      // If used multiple times, ensure argument is simple and pure
      const ASTNode &arg = funcCall.GetChild(i);
      if (hasSideEffects(arg)) return false;
      NodeCounter ac; const_cast<ASTNode&>(arg).Accept(ac);
      if (ac.getCount() > 3) return false; // too complex to duplicate safely
    }

    // Depth guard
    if (currentDepth >= maxInlineDepth) return false;

    return true;
  }

  std::unique_ptr<ASTNode> createInlinedExpression(const ASTNode_FunctionCall &funcCall) {
    size_t funId = funcCall.GetFunId();
    ASTNode_Function *function = functionMap[funId];
    
    if (!function || function->NumChildren() == 0) {
      return nullptr;
    }

    // For simple cases without parameters, just clone the function body
    // This handles our test cases which have simple single-expression functions
    currentDepth++;
    
    const ASTNode &functionBody = function->GetChild(0);
    std::unique_ptr<ASTNode> result;
    
    // If it's a block with a single return statement, extract the return expression
    if (auto *block = dynamic_cast<const ASTNode_Block *>(&functionBody)) {
      if (block->NumChildren() == 1 && block->HasChild(0)) {
        if (auto *returnNode = dynamic_cast<const ASTNode_Return *>(&block->GetChild(0))) {
          if (returnNode->NumChildren() == 1) {
            // Clone the return expression with parameter substitution
            std::unordered_map<size_t, std::unique_ptr<ASTNode>> paramMap;
            createParameterMap(funcCall, *function, paramMap);
            result = cloneWithSubstitution(returnNode->GetChild(0), paramMap);
          }
        }
      }
    }
    // If it's a direct return statement
    else if (auto *returnNode = dynamic_cast<const ASTNode_Return *>(&functionBody)) {
      if (returnNode->NumChildren() == 1) {
        std::unordered_map<size_t, std::unique_ptr<ASTNode>> paramMap;
        createParameterMap(funcCall, *function, paramMap);
        result = cloneWithSubstitution(returnNode->GetChild(0), paramMap);
      }
    }
    
    currentDepth--;
    return result;
  }

  void createParameterMap(const ASTNode_FunctionCall &funcCall, const ASTNode_Function &function, 
                         std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap) {
    const auto& paramIds = function.GetParamIds();
    
    // Map function parameters to call arguments
    for (size_t i = 0; i < funcCall.NumChildren() && i < paramIds.size(); ++i) {
      if (funcCall.HasChild(i)) {
        // Map parameter variable ID to argument expression
        size_t paramId = paramIds[i];
        paramMap[paramId] = cloneWithSubstitution(funcCall.GetChild(i), {});
      }
    }
  }

  // Helper methods
  size_t getVarId(const ASTNode_Var &var) { return var.GetVarId(); }

  // Check if a subtree may produce side effects or duplicate work hazards
  bool hasSideEffects(const ASTNode &node) {
    if (dynamic_cast<const ASTNode_While *>(&node)) return true;
    if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&node)) {
      std::string op = const_cast<ASTNode_Math2*>(m2)->GetOp();
      if (op == "=") return true; // assignment
    }
    if (dynamic_cast<const ASTNode_Break *>(&node)) return true;
    if (dynamic_cast<const ASTNode_Continue *>(&node)) return true;
    if (dynamic_cast<const ASTNode_FunctionCall *>(&node)) return true; // conservative

    if (auto *p = dynamic_cast<const ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < p->NumChildren(); ++i) {
        if (p->HasChild(i) && hasSideEffects(p->GetChild(i))) return true;
      }
    }
    return false;
  }

  // Count occurrences of variable id in subtree
  size_t countVarUsage(const ASTNode &node, size_t varId) {
    size_t cnt = 0;
    if (auto *v = dynamic_cast<const ASTNode_Var *>(&node)) {
      if (v->GetVarId() == varId) cnt++;
    }
    if (auto *p = dynamic_cast<const ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < p->NumChildren(); ++i) {
        if (p->HasChild(i)) cnt += countVarUsage(p->GetChild(i), varId);
      }
    }
    return cnt;
  }
};
