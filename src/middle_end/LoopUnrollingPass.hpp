#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Structure to hold loop analysis results
struct LoopPattern {
  std::string loopVar;    // Variable being incremented/decremented
  std::string comparison; // "<=", ">=", "<", ">", "!=", "=="
  std::string boundVar;   // Variable or constant being compared to
  int increment;          // +1, -1, etc.
  bool isConstantBound;   // true if RHS is constant or simple variable
  bool isUnrollable;      // can this loop be unrolled?
  ASTNode *incrementNode; // Points to the increment statement

  LoopPattern() : increment(0), isConstantBound(false), isUnrollable(false), incrementNode(nullptr) {}
};

class LoopUnrollingPass : public Pass {
private:
  int unrollFactor;

  // Helper classes for loop analysis and transformation
  class LoopAnalyzer {
  public:
    static LoopPattern analyzeLoop(ASTNode_While &loop) {
      LoopPattern pattern;

      if (loop.NumChildren() < 2) {
        return pattern; // Not a valid while loop
      }

      // Analyze the condition (child 0)
      ASTNode &condition = loop.GetChild(0);
      std::string leftVar, op, rightVar;

      if (!isSimpleComparison(condition, leftVar, op, rightVar)) {
        return pattern; // Complex condition, can't unroll
      }

      // Analyze the body (child 1)
      if (auto *body = dynamic_cast<ASTNode_Block *>(&loop.GetChild(1))) {
        // Check for unsuitable control flow patterns
        if (hasUnsuitableControlFlow(*body)) {
          return pattern; // Contains break/continue/complex control flow, can't unroll
        }

        // Look for increment statement
        ASTNode *incrementNode = findIncrementStatement(*body, leftVar);
        if (!incrementNode) {
          return pattern; // No simple increment found
        }

        int increment;
        std::string incVar;
        if (!isSimpleIncrement(*incrementNode, incVar, increment)) {
          return pattern; // Complex increment, can't unroll
        }

        if (incVar != leftVar) {
          return pattern; // Increment variable doesn't match condition variable
        }

        // Check if increment is consistent (no other assignments to the loop variable)
        if (hasMultipleAssignments(*body, leftVar)) {
          return pattern; // Multiple assignments to loop variable, can't unroll safely
        }

        // Check if this is a reasonable pattern to unroll
        if (op == "<=" || op == "<" || op == ">" || op == ">=" || op == "!=" || op == "==") {
          pattern.loopVar = leftVar;
          pattern.comparison = op;
          pattern.boundVar = rightVar;
          pattern.increment = increment;
          pattern.isConstantBound = true; // For now, assume it's reasonable to unroll
          pattern.isUnrollable = true;
          pattern.incrementNode = incrementNode;
        }
      }

      return pattern;
    }

  private:
    static bool isSimpleIncrement(ASTNode &node, std::string &varName, int &increment) {
      // Check if this is an assignment node (Math2 with "=" operator)
      if (auto *math2 = dynamic_cast<ASTNode_Math2 *>(&node)) {
        std::string typeName = math2->GetTypeName();
        if (typeName == "MATH2: =") {
          // This is an assignment, check the pattern: var = var + constant
          if (math2->NumChildren() >= 2) {
            // Left side should be a variable
            if (auto *leftVar = dynamic_cast<ASTNode_Var *>(&math2->GetChild(0))) {
              varName = getVarId(*leftVar);

              // Right side should be var + constant or var - constant
              if (auto *rightMath = dynamic_cast<ASTNode_Math2 *>(&math2->GetChild(1))) {
                std::string rightOpName = rightMath->GetTypeName();
                std::string rightOp;
                if (rightOpName.find("MATH2: ") == 0) {
                  rightOp = rightOpName.substr(7);

                  if (rightOp == "+" || rightOp == "-") {
                    if (rightMath->NumChildren() >= 2) {
                      // Check if left side of addition is the same variable
                      if (auto *addLeftVar = dynamic_cast<ASTNode_Var *>(&rightMath->GetChild(0))) {
                        if (getVarId(*addLeftVar) == varName) {
                          // Check if right side is a constant
                          if (auto *constNode = dynamic_cast<ASTNode_IntLit *>(&rightMath->GetChild(1))) {
                            increment = getIntValue(*constNode);
                            if (rightOp == "-")
                              increment = -increment;
                            return true;
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      return false;
    }

    static bool isSimpleComparison(ASTNode &node, std::string &leftVar, std::string &op, std::string &rightVar) {
      // Check for infinite loop condition (constant 1)
      if (auto *intLit = dynamic_cast<ASTNode_IntLit*>(&node)) {
        if (getIntValue(*intLit) == 1) {
          return false; // while (1) infinite loop, not suitable for unrolling
        }
      }
      
      // Check if this is a Math2 node (binary operation)
      if (auto *math2 = dynamic_cast<ASTNode_Math2 *>(&node)) {
        // Extract operator from GetTypeName (format: "MATH2: op")
        std::string typeName = math2->GetTypeName();
        if (typeName.find("MATH2: ") == 0) {
          op = typeName.substr(7); // Extract operator after "MATH2: "

          // Check if it's a comparison operator
          if (op == "<=" || op == "<" || op == ">=" || op == ">" || op == "!=" || op == "==") {

            if (math2->NumChildren() >= 2) {
              // Check left side - should be a variable
              if (auto *leftVarNode = dynamic_cast<ASTNode_Var *>(&math2->GetChild(0))) {
                leftVar = getVarId(*leftVarNode);

                // Check right side - can be variable or integer literal
                if (auto *rightVarNode = dynamic_cast<ASTNode_Var *>(&math2->GetChild(1))) {
                  rightVar = getVarId(*rightVarNode);
                  return true;
                } else if (auto *rightIntNode = dynamic_cast<ASTNode_IntLit *>(&math2->GetChild(1))) {
                  rightVar = std::to_string(getIntValue(*rightIntNode));
                  return true;
                }
              }
            }
          }
        }
      }
      return false;
    }

    static ASTNode *findIncrementStatement(ASTNode_Block &body, const std::string &varName) {
      // Look through the block for an assignment to varName
      for (size_t i = 0; i < body.NumChildren(); ++i) {
        if (body.HasChild(i)) {
          ASTNode &child = body.GetChild(i);
          std::string incVar;
          int increment;
          if (isSimpleIncrement(child, incVar, increment) && incVar == varName) {
            return &child;
          }
        }
      }
      return nullptr;
    }

    static std::string getOperator(ASTNode_Math2 &math2) {
      std::string typeName = math2.GetTypeName();
      if (typeName.find("MATH2: ") == 0) {
        return typeName.substr(7);
      }
      return "";
    }

    static std::string getVarId(ASTNode_Var &var) {
      std::string typeName = var.GetTypeName();
      if (typeName.find("VAR: ") == 0) {
        return typeName.substr(5); // Extract var_id after "VAR: "
      }
      return "";
    }

    static int getIntValue(ASTNode_IntLit &intLit) {
      std::string typeName = intLit.GetTypeName();
      if (typeName.find("INT_LIT:") == 0) {
        return std::stoi(typeName.substr(8)); // Extract value after "INT_LIT:"
      }
      return 0;
    }

    // Check for control flow statements that make loops unsuitable for unrolling
    static bool hasUnsuitableControlFlow(ASTNode &node) {
      std::string typeName = node.GetTypeName();
      
      // Check for break, continue, return statements
      if (typeName == "BREAK" || typeName == "CONTINUE" || typeName == "RETURN") {
        return true;
      }
      
      // Check for nested loops (while, for)
      if (typeName == "WHILE" || typeName == "FOR") {
        return true;
      }
      
      // Check for infinite loops (condition is constant 1)
      if (typeName == "WHILE") {
        if (auto *whileNode = dynamic_cast<ASTNode_While*>(&node)) {
          if (whileNode->NumChildren() > 0) {
            ASTNode &condition = whileNode->GetChild(0);
            if (auto *intLit = dynamic_cast<ASTNode_IntLit*>(&condition)) {
              if (getIntValue(*intLit) == 1) {
                return true; // while (1) infinite loop
              }
            }
          }
        }
      }
      
      // Recursively check children
      if (auto *parent = dynamic_cast<ASTNode_Parent*>(&node)) {
        for (size_t i = 0; i < parent->NumChildren(); ++i) {
          if (parent->HasChild(i)) {
            if (hasUnsuitableControlFlow(parent->GetChild(i))) {
              return true;
            }
          }
        }
      }
      
      return false;
    }

    // Check if there are multiple assignments to the loop variable
    static bool hasMultipleAssignments(ASTNode_Block &body, const std::string &varName) {
      int assignmentCount = 0;
      
      for (size_t i = 0; i < body.NumChildren(); ++i) {
        if (body.HasChild(i)) {
          ASTNode &child = body.GetChild(i);
          if (isAssignmentTo(child, varName)) {
            assignmentCount++;
            if (assignmentCount > 1) {
              return true; // Found multiple assignments
            }
          }
        }
      }
      
      return false;
    }

    // Check if a node is an assignment to a specific variable
    static bool isAssignmentTo(ASTNode &node, const std::string &varName) {
      if (auto *math2 = dynamic_cast<ASTNode_Math2*>(&node)) {
        std::string typeName = math2->GetTypeName();
        if (typeName == "MATH2: =" && math2->NumChildren() >= 1) {
          if (auto *leftVar = dynamic_cast<ASTNode_Var*>(&math2->GetChild(0))) {
            return getVarId(*leftVar) == varName;
          }
        }
      }
      return false;
    }
  };

  class ASTCloner {
  public:
    static std::unique_ptr<ASTNode> clone(const ASTNode &node) {
      // Basic cloning using dynamic_cast to handle different node types

      // Handle Math2 nodes (binary operations)
      if (auto *math2 = dynamic_cast<const ASTNode_Math2 *>(&node)) {
        return cloneMath2(*math2);
      }

      // Handle Variable nodes
      if (auto *var = dynamic_cast<const ASTNode_Var *>(&node)) {
        return cloneVar(*var);
      }

      // Handle Integer literal nodes
      if (auto *intLit = dynamic_cast<const ASTNode_IntLit *>(&node)) {
        return cloneIntLit(*intLit);
      }

      // Handle Block nodes
      if (auto *block = dynamic_cast<const ASTNode_Block *>(&node)) {
        return cloneBlock(*block);
      }

      // For now, return nullptr for unsupported node types
      return nullptr;
    }

    static std::unique_ptr<ASTNode_Block> cloneBlock(const ASTNode_Block &block) {
      // Create a new block at the same file position
      auto newBlock = std::make_unique<ASTNode_Block>(block.GetFilePos());

      // Clone each child and add to the new block
      for (size_t i = 0; i < block.NumChildren(); ++i) {
        if (block.HasChild(i)) {
          auto clonedChild = clone(block.GetChild(i));
          if (clonedChild) {
            newBlock->AddChild(std::move(clonedChild));
          }
        }
      }

      return newBlock;
    }

    static std::unique_ptr<ASTNode_Var> cloneVar(const ASTNode_Var &var) {
      // Extract variable ID from the type name
      std::string typeName = var.GetTypeName();
      if (typeName.find("VAR: ") == 0) {
        size_t varId = std::stoul(typeName.substr(5));
        return std::make_unique<ASTNode_Var>(var.GetFilePos(), varId);
      }
      return nullptr;
    }

  private:
    static std::unique_ptr<ASTNode_Math2> cloneMath2(const ASTNode_Math2 &math2) {
      if (math2.NumChildren() >= 2) {
        auto leftClone = clone(math2.GetChild(0));
        auto rightClone = clone(math2.GetChild(1));

        if (leftClone && rightClone) {
          std::string op = getOperator(const_cast<ASTNode_Math2 &>(math2));
          return std::make_unique<ASTNode_Math2>(math2.GetFilePos(), op, std::move(leftClone), std::move(rightClone));
        }
      }
      return nullptr;
    }

    static std::unique_ptr<ASTNode_IntLit> cloneIntLit(const ASTNode_IntLit &intLit) {
      std::string typeName = intLit.GetTypeName();
      if (typeName.find("INT_LIT:") == 0) {
        int value = std::stoi(typeName.substr(8));
        return std::make_unique<ASTNode_IntLit>(intLit.GetFilePos(), value);
      }
      return nullptr;
    }

    static std::string getOperator(ASTNode_Math2 &math2) {
      std::string typeName = math2.GetTypeName();
      if (typeName.find("MATH2: ") == 0) {
        return typeName.substr(7);
      }
      return "";
    }
  };

public:
  LoopUnrollingPass(int factor) : unrollFactor(factor) {}

  std::string getName() const override { return "LoopUnrolling"; }

  void run(ASTNode &node) override {
    if (unrollFactor <= 1) {
      return;
    }

    // Check if this is a function node and process it
    if (auto *function = dynamic_cast<ASTNode_Function *>(&node)) {
      processFunction(*function);
    }
    // Check if this is a block node and process it
    else if (auto *block = dynamic_cast<ASTNode_Block *>(&node)) {
      processBlock(*block);
    }
    // For other node types, traverse children if it's a parent node
    else if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          run(parent->GetChild(i));
        }
      }
    }
  }

private:
  void processFunction(ASTNode_Function &function) {
    // Process the function body (should be child 0)
    if (function.NumChildren() > 0 && function.HasChild(0)) {
      run(function.GetChild(0));
    }
  }

  void processBlock(ASTNode_Block &block) {
    // Process each statement in the block
    // We need to be careful about modifying the block while iterating
    std::vector<std::pair<size_t, std::unique_ptr<ASTNode_Block>>> replacements;

    // First pass: identify while loops and create replacements
    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (block.HasChild(i)) {
        if (auto *whileLoop = dynamic_cast<ASTNode_While *>(&block.GetChild(i))) {
          LoopPattern pattern;
          if (isUnrollableLoop(*whileLoop, pattern)) {
            // Create unrolled block containing both main and remainder loops
            auto unrolledBlock = createUnrolledBlock(*whileLoop, pattern);
            if (unrolledBlock) {
              // Store the replacement for later
              replacements.emplace_back(i, std::move(unrolledBlock));
            }
          }
        } else {
          // Recursively process other nodes
          run(block.GetChild(i));
        }
      }
    }

    // Second pass: actually replace the loops (in reverse order to maintain indices)
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
      size_t index = it->first;
      auto &replacement = it->second;

      // Use the new ReplaceChild method
      block.ReplaceChild(index, std::move(replacement));
    }

    // Third pass: process any remaining loops that weren't unrolled
    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (block.HasChild(i)) {
        // Check if this child was just replaced (skip processing to avoid infinite recursion)
        bool wasReplaced = false;
        for (const auto &replacement : replacements) {
          if (replacement.first == i) {
            wasReplaced = true;
            break;
          }
        }

        if (!wasReplaced) {
          if (auto *whileLoop = dynamic_cast<ASTNode_While *>(&block.GetChild(i))) {
            // Process the loop body recursively
            if (whileLoop->NumChildren() > 1 && whileLoop->HasChild(1)) {
              run(whileLoop->GetChild(1));
            }
          } else if (auto *subBlock = dynamic_cast<ASTNode_Block *>(&block.GetChild(i))) {
            // Process sub-blocks recursively (but not our unrolled blocks)
            run(*subBlock);
          }
        }
      }
    }
  }

  // Helper method to replace a child in a block using the new ReplaceChild method
  bool replaceChildInBlock(ASTNode_Block &block, size_t index, std::unique_ptr<ASTNode_Block> replacement) {
    if (index >= block.NumChildren()) {
      return false;
    }

    block.ReplaceChild(index, std::move(replacement));
    return true;
  }

  bool isUnrollableLoop(ASTNode_While &loop, LoopPattern &pattern) {
    pattern = LoopAnalyzer::analyzeLoop(loop);
    return pattern.isUnrollable;
  }

  // Create a block containing both unrolled and remainder loops  
  std::unique_ptr<ASTNode_Block> createUnrolledBlock(ASTNode_While &original, const LoopPattern &pattern) {
    auto block = std::make_unique<ASTNode_Block>(original.GetFilePos());

    // Create the main unrolled loop (handles multiples of unrollFactor)
    auto unrolledLoop = createMainUnrolledLoop(original, pattern);
    if (unrolledLoop) {
      block->AddChild(std::move(unrolledLoop));
    }

    // Create the remainder loop (handles the leftover iterations)
    auto remainderLoop = createRemainderLoop(original, pattern);
    if (remainderLoop) {
      block->AddChild(std::move(remainderLoop));
    }

    return block;
  }

  std::unique_ptr<ASTNode_While> createMainUnrolledLoop(ASTNode_While &original, const LoopPattern &pattern) {
    if (original.NumChildren() < 2) {
      return nullptr;
    }

    // Get the original condition and body
    auto *originalBody = dynamic_cast<ASTNode_Block *>(&original.GetChild(1));
    if (!originalBody) {
      return nullptr;
    }

    // Create the fully unrolled body (handles unrollFactor iterations at once)
    auto unrolledBody = createFullyUnrolledBody(*originalBody, pattern);
    if (!unrolledBody) {
      return nullptr;
    }

    // Create adjusted condition for the main loop
    auto adjustedCondition = adjustConditionForUnrolling(original.GetChild(0), pattern);
    if (!adjustedCondition) {
      return nullptr;
    }

    // Create the new while loop with fully unrolled body
    return std::make_unique<ASTNode_While>(original.GetFilePos(), std::move(adjustedCondition),
                                           std::move(unrolledBody));
  }

  std::unique_ptr<ASTNode_While> createRemainderLoop(ASTNode_While &original, const LoopPattern &pattern) {
    // The remainder loop uses the original condition but starts where the main loop left off
    // Since we're modifying the same loop variable, the remainder loop should just be the original loop
    if (original.NumChildren() < 2) {
      return nullptr;
    }

    auto conditionClone = ASTCloner::clone(original.GetChild(0));
    auto bodyClone = ASTCloner::clone(original.GetChild(1));

    if (conditionClone && bodyClone) {
      return std::make_unique<ASTNode_While>(original.GetFilePos(), std::move(conditionClone), std::move(bodyClone));
    }

    return nullptr;
  }

  std::unique_ptr<ASTNode> adjustConditionForUnrolling(const ASTNode &originalCondition, const LoopPattern &pattern) {
    // For loops like "i < N", we need to adjust the condition to "i < N - (unrollFactor - 1)"
    // This ensures the unrolled loop doesn't exceed the original bounds
    
    if (auto *math2 = dynamic_cast<const ASTNode_Math2 *>(&originalCondition)) {
      if (math2->NumChildren() >= 2) {
        std::string op = getOperator(*const_cast<ASTNode_Math2*>(math2));
        
        if (op == "<" && pattern.increment > 0) {
          // Clone the left side (loop variable)
          auto leftClone = ASTCloner::clone(math2->GetChild(0));
          
          // Clone the bound and subtract (unrollFactor - 1)
          auto boundClone = ASTCloner::clone(math2->GetChild(1));
          auto adjustment = std::make_unique<ASTNode_IntLit>(originalCondition.GetFilePos(), unrollFactor - 1);
          
          // Create: bound - (unrollFactor - 1)
          auto adjustedBound = std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "-",
                                                               std::move(boundClone), std::move(adjustment));
          
          // Create: left < adjustedBound
          if (leftClone) {
            return std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "<", std::move(leftClone),
                                                   std::move(adjustedBound));
          }
        }
        else if (op == "<=" && pattern.increment > 0) {
          // For <=, adjust to <= bound - unrollFactor
          auto leftClone = ASTCloner::clone(math2->GetChild(0));
          auto boundClone = ASTCloner::clone(math2->GetChild(1));
          auto adjustment = std::make_unique<ASTNode_IntLit>(originalCondition.GetFilePos(), unrollFactor);
          
          auto adjustedBound = std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "-",
                                                               std::move(boundClone), std::move(adjustment));
          
          if (leftClone) {
            return std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "<=", std::move(leftClone),
                                                   std::move(adjustedBound));
          }
        }
        else if (op == ">" && pattern.increment < 0) {
          // Decrement loop: i > N becomes i > N + (unrollFactor - 1)
          auto leftClone = ASTCloner::clone(math2->GetChild(0));
          auto boundClone = ASTCloner::clone(math2->GetChild(1));
          auto adjustment = std::make_unique<ASTNode_IntLit>(originalCondition.GetFilePos(), unrollFactor - 1);
          
          auto adjustedBound = std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "+",
                                                               std::move(boundClone), std::move(adjustment));
          
          if (leftClone) {
            return std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), ">", std::move(leftClone),
                                                   std::move(adjustedBound));
          }
        }
        else if (op == ">=" && pattern.increment < 0) {
          // Decrement loop: i >= N becomes i >= N + unrollFactor
          auto leftClone = ASTCloner::clone(math2->GetChild(0));
          auto boundClone = ASTCloner::clone(math2->GetChild(1));
          auto adjustment = std::make_unique<ASTNode_IntLit>(originalCondition.GetFilePos(), unrollFactor);
          
          auto adjustedBound = std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), "+",
                                                               std::move(boundClone), std::move(adjustment));
          
          if (leftClone) {
            return std::make_unique<ASTNode_Math2>(originalCondition.GetFilePos(), ">=", std::move(leftClone),
                                                   std::move(adjustedBound));
          }
        }
      }
    }
    
    // If we can't adjust, use the original condition (may not be optimal but safe)
    return ASTCloner::clone(originalCondition);
  }

  std::unique_ptr<ASTNode_Block> createFullyUnrolledBody(const ASTNode_Block &originalBody, const LoopPattern &pattern) {
    auto unrolledBlock = std::make_unique<ASTNode_Block>(originalBody.GetFilePos());

    // For each unroll iteration, clone the body but skip the increment statement
    for (int i = 0; i < unrollFactor; ++i) {
      for (size_t j = 0; j < originalBody.NumChildren(); ++j) {
        if (originalBody.HasChild(j)) {
          const ASTNode &child = originalBody.GetChild(j);
          
          // Skip the increment statement - we'll handle it separately
          if (&child == pattern.incrementNode) {
            continue;
          }
          
          // Clone and substitute induction variables for this iteration
          auto clonedChild = cloneWithInductionVariableSubstitution(child, pattern.loopVar, i * pattern.increment);
          if (clonedChild) {
            unrolledBlock->AddChild(std::move(clonedChild));
          }
        }
      }
    }

    // Add a single increment by the unroll factor at the end
    auto finalIncrement = createFinalIncrement(pattern);
    if (finalIncrement) {
      unrolledBlock->AddChild(std::move(finalIncrement));
    }

    return unrolledBlock;
  }

  static std::string getOperator(ASTNode_Math2 &math2) {
    std::string typeName = math2.GetTypeName();
    if (typeName.find("MATH2: ") == 0) {
      return typeName.substr(7);
    }
    return "";
  }

private:
  // Clone a node while substituting loop variable references with loop_var + offset
  std::unique_ptr<ASTNode> cloneWithInductionVariableSubstitution(const ASTNode &node, const std::string &loopVar, int offset) {
    // Handle variable nodes - substitute if it's the loop variable
    if (auto *var = dynamic_cast<const ASTNode_Var *>(&node)) {
      std::string varId = getVarId(*const_cast<ASTNode_Var*>(var));
      if (varId == loopVar && offset != 0) {
        // Create (loop_var + offset) instead of just loop_var
        auto originalVar = ASTCloner::cloneVar(*var);
        auto offsetLit = std::make_unique<ASTNode_IntLit>(node.GetFilePos(), offset);
        
        // Choose the correct operation based on sign of offset
        std::string op = (offset >= 0) ? "+" : "-";
        if (offset < 0) {
          // For negative offsets, use subtraction with positive value
          offsetLit = std::make_unique<ASTNode_IntLit>(node.GetFilePos(), -offset);
        }
        
        return std::make_unique<ASTNode_Math2>(node.GetFilePos(), op, std::move(originalVar), std::move(offsetLit));
      } else {
        // Regular variable or offset is 0, just clone normally
        return ASTCloner::cloneVar(*var);
      }
    }

    // Handle Math2 nodes recursively
    if (auto *math2 = dynamic_cast<const ASTNode_Math2 *>(&node)) {
      if (math2->NumChildren() >= 2) {
        auto leftClone = cloneWithInductionVariableSubstitution(math2->GetChild(0), loopVar, offset);
        auto rightClone = cloneWithInductionVariableSubstitution(math2->GetChild(1), loopVar, offset);
        
        if (leftClone && rightClone) {
          std::string op = getOperator(*const_cast<ASTNode_Math2*>(math2));
          return std::make_unique<ASTNode_Math2>(math2->GetFilePos(), op, std::move(leftClone), std::move(rightClone));
        }
      }
    }

    // Handle other node types with regular cloning
    return ASTCloner::clone(node);
  }

  // Create the final increment statement: loop_var = loop_var + (unrollFactor * original_increment)
  std::unique_ptr<ASTNode> createFinalIncrement(const LoopPattern &pattern) {
    // Create a variable reference for the loop variable
    if (!pattern.incrementNode) {
      return nullptr;
    }

    // Extract the variable ID from the increment node
    if (auto *math2 = dynamic_cast<const ASTNode_Math2 *>(pattern.incrementNode)) {
      if (math2->NumChildren() >= 1) {
        if (auto *varNode = dynamic_cast<const ASTNode_Var *>(&math2->GetChild(0))) {
          // Clone the variable for left side of assignment
          auto leftVar = ASTCloner::cloneVar(*varNode);
          
          // Clone the variable for right side of addition
          auto rightVarForAdd = ASTCloner::cloneVar(*varNode);
          
          // Create the total increment: unrollFactor * pattern.increment
          int totalIncrement = unrollFactor * pattern.increment;
          auto totalIncrementLit = std::make_unique<ASTNode_IntLit>(pattern.incrementNode->GetFilePos(), totalIncrement);
          
          // Create: loop_var + totalIncrement
          auto addition = std::make_unique<ASTNode_Math2>(pattern.incrementNode->GetFilePos(), "+", 
                                                         std::move(rightVarForAdd), std::move(totalIncrementLit));
          
          // Create: loop_var = (loop_var + totalIncrement)
          return std::make_unique<ASTNode_Math2>(pattern.incrementNode->GetFilePos(), "=", 
                                               std::move(leftVar), std::move(addition));
        }
      }
    }
    
    return nullptr;
  }

  static std::string getVarId(ASTNode_Var &var) {
    std::string typeName = var.GetTypeName();
    if (typeName.find("VAR: ") == 0) {
      return typeName.substr(5); // Extract var_id after "VAR: "
    }
    return "";
  }
};
