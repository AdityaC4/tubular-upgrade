#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include "../core/ASTCloner.hpp"
#include <cmath>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class LoopUnrollingPass : public Pass {
private:
  struct LoopInfo {
    size_t varId = 0;
    int step = 0;
    bool increasing = true;
    bool inclusive = false;
    bool hasLiteralBound = false;
    int boundValue = 0;
    ASTNode_Math2 *incrementNode = nullptr;
    ASTNode_Block *body = nullptr;
  };

  int unrollFactor;
  bool aggressiveUnrolling;
  bool unrollNestedLoops;
  size_t maxUnrollIterations;
  bool enablePeeling;

public:
  LoopUnrollingPass(int factor, bool aggressive = false, bool nested = false,
                    size_t maxIter = 100, bool peeling = false)
      : unrollFactor(factor), aggressiveUnrolling(aggressive),
        unrollNestedLoops(nested), maxUnrollIterations(maxIter),
        enablePeeling(peeling) {}

  std::string getName() const override { return "LoopUnrolling"; }

  void run(ASTNode &node) override {
    if (unrollFactor <= 1) {
      return;
    }
    processNode(node);
  }

private:
  void processNode(ASTNode &node) {
    if (auto *function = dynamic_cast<ASTNode_Function *>(&node)) {
      if (function->NumChildren() > 0 && function->HasChild(0)) {
        processNode(function->GetChild(0));
      }
      return;
    }

    if (auto *block = dynamic_cast<ASTNode_Block *>(&node)) {
      processBlock(*block);
      return;
    }

    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          processNode(parent->GetChild(i));
        }
      }
    }
  }

  void processBlock(ASTNode_Block &block) {
    std::vector<std::pair<size_t, std::unique_ptr<ASTNode_Block>>> replacements;

    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (!block.HasChild(i))
        continue;

      ASTNode &child = block.GetChild(i);
      if (auto *loop = dynamic_cast<ASTNode_While *>(&child)) {
        auto info = analyseLoop(*loop);
        if (info && loopEligible(*loop, *info)) {
          auto replacement = buildReplacement(*loop, *info);
          if (replacement) {
            replacements.emplace_back(i, std::move(replacement));
            continue;
          }
        }
        // Either not eligible or failed to build replacement; recurse into loop body
        if (loop->NumChildren() > 1 && loop->HasChild(1)) {
          processNode(loop->GetChild(1));
        }
      } else {
        processNode(child);
      }
    }

    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
      block.ReplaceChild(it->first, std::move(it->second));
    }
  }

  bool loopEligible(ASTNode_While &loop, const LoopInfo &info) const {
    if (!aggressiveUnrolling && !info.hasLiteralBound) {
      return false;
    }
    if (!info.increasing && !unrollNestedLoops) {
      // For now, treat decreasing loops as allowed only when nested unrolling enabled
      // to guard against unexpected patterns
      return false;
    }
    if (std::abs(info.step) != 1) {
      return false;
    }
    if (!info.hasLiteralBound) {
      return false;
    }
    (void)loop;
    return true;
  }

  std::optional<LoopInfo> analyseLoop(ASTNode_While &loop) {
    if (loop.NumChildren() < 2 || !loop.HasChild(0) || !loop.HasChild(1)) {
      return std::nullopt;
    }

    auto *condition = dynamic_cast<ASTNode_Math2 *>(&loop.GetChild(0));
    if (!condition) {
      return std::nullopt;
    }

    auto *body = dynamic_cast<ASTNode_Block *>(&loop.GetChild(1));
    if (!body) {
      return std::nullopt;
    }

    LoopInfo info;
    info.body = body;

    if (!extractCondition(*condition, info)) {
      return std::nullopt;
    }

    if (!unrollNestedLoops && containsNestedLoop(*body)) {
      return std::nullopt;
    }
    if (containsControlTransfer(*body)) {
      return std::nullopt;
    }

    if (!findIncrement(*body, info.varId, info.step, info.incrementNode)) {
      return std::nullopt;
    }

    if (info.increasing && info.step <= 0) {
      return std::nullopt;
    }
    if (!info.increasing && info.step >= 0) {
      return std::nullopt;
    }

    if (countAssignments(*body, info.varId) > 1) {
      return std::nullopt;
    }

    return info;
  }

  bool extractCondition(ASTNode_Math2 &cond, LoopInfo &info) {
    const std::string op = const_cast<ASTNode_Math2 &>(cond).GetOp();
    bool inclusive = false;
    bool increasing = true;

    if (op == "<") {
      inclusive = false;
      increasing = true;
    } else if (op == "<=") {
      inclusive = true;
      increasing = true;
    } else if (op == ">") {
      inclusive = false;
      increasing = false;
    } else if (op == ">=") {
      inclusive = true;
      increasing = false;
    } else {
      return false;
    }

    auto *leftVar = dynamic_cast<ASTNode_Var *>(&cond.GetChild(0));
    if (!leftVar) {
      return false;
    }

    info.varId = leftVar->GetVarId();
    info.inclusive = inclusive;
    info.increasing = increasing;
    info.hasLiteralBound = false;
    info.boundValue = 0;

    if (auto *lit = dynamic_cast<ASTNode_IntLit *>(&cond.GetChild(1))) {
      info.hasLiteralBound = true;
      info.boundValue = lit->GetValue();
    }

    return true;
  }

  bool findIncrement(ASTNode_Block &body, size_t varId, int &step,
                     ASTNode_Math2 *&incrementNode) {
    step = 0;
    incrementNode = nullptr;

    for (size_t i = 0; i < body.NumChildren(); ++i) {
      if (!body.HasChild(i))
        continue;
      auto *assign = dynamic_cast<ASTNode_Math2 *>(&body.GetChild(i));
      if (!assign)
        continue;
      if (const_cast<ASTNode_Math2 &>(*assign).GetOp() != "=")
        continue;
      auto *lhs = dynamic_cast<ASTNode_Var *>(&assign->GetChild(0));
      if (!lhs || lhs->GetVarId() != varId)
        continue;

      int parsedStep = 0;
      if (parseIncrement(assign->GetChild(1), varId, parsedStep)) {
        step = parsedStep;
        incrementNode = assign;
        return true;
      }
    }
    return false;
  }

  bool parseIncrement(ASTNode &expr, size_t varId, int &stepOut) {
    auto *math2 = dynamic_cast<ASTNode_Math2 *>(&expr);
    if (!math2 || math2->NumChildren() < 2) {
      return false;
    }

    const std::string op = const_cast<ASTNode_Math2 &>(*math2).GetOp();
    if (op != "+" && op != "-") {
      return false;
    }

    auto *lhsVar = dynamic_cast<ASTNode_Var *>(&math2->GetChild(0));
    auto *rhsLit = dynamic_cast<ASTNode_IntLit *>(&math2->GetChild(1));
    if (lhsVar && lhsVar->GetVarId() == varId && rhsLit) {
      int value = rhsLit->GetValue();
      stepOut = (op == "+") ? value : -value;
      return true;
    }

    if (op == "+") {
      auto *rhsVar = dynamic_cast<ASTNode_Var *>(&math2->GetChild(1));
      auto *lhsLit = dynamic_cast<ASTNode_IntLit *>(&math2->GetChild(0));
      if (rhsVar && rhsVar->GetVarId() == varId && lhsLit) {
        stepOut = lhsLit->GetValue();
        return true;
      }
    }

    return false;
  }

  bool containsNestedLoop(ASTNode &node) const {
    if (dynamic_cast<ASTNode_While *>(&node)) {
      return true;
    }
    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i) && containsNestedLoop(parent->GetChild(i))) {
          return true;
        }
      }
    }
    return false;
  }

  bool containsControlTransfer(ASTNode &node) const {
    if (dynamic_cast<ASTNode_Break *>(&node) || dynamic_cast<ASTNode_Continue *>(&node) ||
        dynamic_cast<ASTNode_Return *>(&node)) {
      return true;
    }
    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i) && containsControlTransfer(parent->GetChild(i))) {
          return true;
        }
      }
    }
    return false;
  }

  int countAssignments(ASTNode &node, size_t varId) const {
    int count = 0;
    if (auto *assign = dynamic_cast<ASTNode_Math2 *>(&node)) {
      if (const_cast<ASTNode_Math2 &>(*assign).GetOp() == "=" &&
          assign->NumChildren() >= 1) {
        if (auto *lhs = dynamic_cast<ASTNode_Var *>(&assign->GetChild(0))) {
          if (lhs->GetVarId() == varId) {
            ++count;
          }
        }
      }
    }
    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          count += countAssignments(parent->GetChild(i), varId);
        }
      }
    }
    return count;
  }

  std::unique_ptr<ASTNode_Block> buildReplacement(ASTNode_While &loop,
                                                  const LoopInfo &info) {
    auto replacement = std::make_unique<ASTNode_Block>(loop.GetFilePos());
    auto mainLoop = buildMainLoop(loop, info);
    if (mainLoop) {
      replacement->AddChild(std::move(mainLoop));
    }
    auto remainder = ASTCloner::clone(loop);
    if (remainder) {
      replacement->AddChild(std::move(remainder));
    }
    return replacement;
  }

  std::unique_ptr<ASTNode_While> buildMainLoop(ASTNode_While &loop,
                                               const LoopInfo &info) {
    if (!info.hasLiteralBound) {
      return nullptr;
    }

    auto adjustedCond = buildAdjustedCondition(loop.GetChild(0), info);
    if (!adjustedCond) {
      return nullptr;
    }

    auto unrolledBody = buildUnrolledBody(*info.body, info);
    if (!unrolledBody) {
      return nullptr;
    }

    return std::make_unique<ASTNode_While>(loop.GetFilePos(), std::move(adjustedCond),
                                           std::move(unrolledBody));
  }

  std::unique_ptr<ASTNode> buildAdjustedCondition(ASTNode &originalCond,
                                                  const LoopInfo &info) {
    if (!info.hasLiteralBound) {
      return nullptr;
    }

    const int stepAbs = std::abs(info.step);
    int adjustment = 0;
    std::string op;

    if (info.increasing) {
      if (info.inclusive) {
        adjustment = stepAbs * (unrollFactor - 1);
        op = "<=";
      } else {
        adjustment = stepAbs * unrollFactor;
        op = "<=";
      }
      int newBound = info.boundValue - adjustment;
      return makeComparison(originalCond.GetFilePos(), info.varId, op, newBound);
    } else {
      if (info.inclusive) {
        adjustment = stepAbs * (unrollFactor - 1);
        op = ">=";
      } else {
        adjustment = stepAbs * unrollFactor;
        op = ">";
      }
      int newBound = info.boundValue + adjustment;
      return makeComparison(originalCond.GetFilePos(), info.varId, op, newBound);
    }
  }

  std::unique_ptr<ASTNode_Block> buildUnrolledBody(const ASTNode_Block &body,
                                                   const LoopInfo &info) {
    auto unrolled = std::make_unique<ASTNode_Block>(body.GetFilePos());

    for (int iteration = 0; iteration < unrollFactor; ++iteration) {
      const int offset = iteration * info.step;
      for (size_t i = 0; i < body.NumChildren(); ++i) {
        if (!body.HasChild(i))
          continue;
        const ASTNode &child = body.GetChild(i);
        if (&child == info.incrementNode) {
          continue;
        }
        auto cloned = cloneWithOffset(child, info.varId, offset);
        if (!cloned) {
          return nullptr;
        }
        unrolled->AddChild(std::move(cloned));
      }
    }

    int totalStep = info.step * unrollFactor;
    auto finalIncrement =
        makeIncrement(info.incrementNode->GetFilePos(), info.varId, totalStep);
    if (!finalIncrement) {
      return nullptr;
    }
    unrolled->AddChild(std::move(finalIncrement));
    return unrolled;
  }

  std::unique_ptr<ASTNode> cloneWithOffset(const ASTNode &node, size_t varId,
                                           int offset) {
    if (auto *var = dynamic_cast<const ASTNode_Var *>(&node)) {
      if (offset == 0 || var->GetVarId() != varId) {
        return std::make_unique<ASTNode_Var>(node.GetFilePos(), var->GetVarId());
      }
      auto base = std::make_unique<ASTNode_Var>(node.GetFilePos(), var->GetVarId());
      auto absValue = std::make_unique<ASTNode_IntLit>(node.GetFilePos(), std::abs(offset));
      std::string op = offset > 0 ? "+" : "-";
      if (offset < 0) {
        absValue = std::make_unique<ASTNode_IntLit>(node.GetFilePos(), std::abs(offset));
      }
      return std::make_unique<ASTNode_Math2>(node.GetFilePos(), op, std::move(base),
                                             std::move(absValue));
    }

    if (auto *lit = dynamic_cast<const ASTNode_IntLit *>(&node)) {
      return std::make_unique<ASTNode_IntLit>(node.GetFilePos(), lit->GetValue());
    }
    if (auto *lit = dynamic_cast<const ASTNode_FloatLit *>(&node)) {
      return std::make_unique<ASTNode_FloatLit>(node.GetFilePos(), lit->GetValue());
    }
    if (auto *lit = dynamic_cast<const ASTNode_StringLit *>(&node)) {
      return std::make_unique<ASTNode_StringLit>(node.GetFilePos(), lit->GetValue());
    }

    if (auto *m1 = dynamic_cast<const ASTNode_Math1 *>(&node)) {
      auto child = cloneWithOffset(m1->GetChild(0), varId, offset);
      if (!child)
        return nullptr;
      return std::make_unique<ASTNode_Math1>(node.GetFilePos(),
                                             const_cast<ASTNode_Math1 *>(m1)->GetOp(),
                                             std::move(child));
    }

    if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&node)) {
      auto left = cloneWithOffset(m2->GetChild(0), varId, offset);
      auto right = cloneWithOffset(m2->GetChild(1), varId, offset);
      if (!left || !right)
        return nullptr;
      return std::make_unique<ASTNode_Math2>(node.GetFilePos(),
                                             const_cast<ASTNode_Math2 *>(m2)->GetOp(),
                                             std::move(left), std::move(right));
    }

    if (auto *ret = dynamic_cast<const ASTNode_Return *>(&node)) {
      if (!ret->NumChildren() || !ret->HasChild(0))
        return ASTCloner::clone(node);
      auto child = cloneWithOffset(ret->GetChild(0), varId, offset);
      if (!child)
        return nullptr;
      return std::make_unique<ASTNode_Return>(node.GetFilePos(), std::move(child));
    }

    if (auto *block = dynamic_cast<const ASTNode_Block *>(&node)) {
      auto clone = std::make_unique<ASTNode_Block>(node.GetFilePos());
      for (size_t i = 0; i < block->NumChildren(); ++i) {
        if (!block->HasChild(i))
          continue;
        auto child = cloneWithOffset(block->GetChild(i), varId, offset);
        if (!child)
          return nullptr;
        clone->AddChild(std::move(child));
      }
      return clone;
    }

    if (auto *call = dynamic_cast<const ASTNode_FunctionCall *>(&node)) {
      std::vector<std::unique_ptr<ASTNode>> args;
      args.reserve(call->NumChildren());
      for (size_t i = 0; i < call->NumChildren(); ++i) {
        if (!call->HasChild(i))
          continue;
        auto arg = cloneWithOffset(call->GetChild(i), varId, offset);
        if (!arg)
          return nullptr;
        args.push_back(std::move(arg));
      }
      return std::make_unique<ASTNode_FunctionCall>(node.GetFilePos(), call->GetFunId(),
                                                    std::move(args));
    }

    if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&node)) {
      auto base = cloneWithOffset(idx->GetChild(0), varId, offset);
      auto sub = cloneWithOffset(idx->GetChild(1), varId, offset);
      if (!base || !sub)
        return nullptr;
      return std::make_unique<ASTNode_Indexing>(node.GetFilePos(), std::move(base),
                                                std::move(sub));
    }

    if (auto *sz = dynamic_cast<const ASTNode_Size *>(&node)) {
      auto arg = cloneWithOffset(sz->GetChild(0), varId, offset);
      if (!arg)
        return nullptr;
      return std::make_unique<ASTNode_Size>(node.GetFilePos(), std::move(arg));
    }

    if (auto *conv = dynamic_cast<const ASTNode_ToDouble *>(&node)) {
      auto child = cloneWithOffset(conv->GetChild(0), varId, offset);
      if (!child)
        return nullptr;
      return std::make_unique<ASTNode_ToDouble>(std::move(child));
    }

    if (auto *conv = dynamic_cast<const ASTNode_ToInt *>(&node)) {
      auto child = cloneWithOffset(conv->GetChild(0), varId, offset);
      if (!child)
        return nullptr;
      return std::make_unique<ASTNode_ToInt>(std::move(child));
    }

    if (auto *conv = dynamic_cast<const ASTNode_ToString *>(&node)) {
      auto child = cloneWithOffset(conv->GetChild(0), varId, offset);
      if (!child)
        return nullptr;
      return std::make_unique<ASTNode_ToString>(std::move(child));
    }

    if (auto *iff = dynamic_cast<const ASTNode_If *>(&node)) {
      auto cond = cloneWithOffset(iff->GetChild(0), varId, offset);
      if (!cond)
        return nullptr;
      if (iff->NumChildren() == 2) {
        auto thenBranch = cloneWithOffset(iff->GetChild(1), varId, offset);
        if (!thenBranch)
          return nullptr;
        return std::make_unique<ASTNode_If>(node.GetFilePos(), std::move(cond),
                                            std::move(thenBranch));
      } else if (iff->NumChildren() == 3) {
        auto thenBranch = cloneWithOffset(iff->GetChild(1), varId, offset);
        auto elseBranch = cloneWithOffset(iff->GetChild(2), varId, offset);
        if (!thenBranch || !elseBranch)
          return nullptr;
        return std::make_unique<ASTNode_If>(node.GetFilePos(), std::move(cond),
                                            std::move(thenBranch), std::move(elseBranch));
      }
    }

    return ASTCloner::clone(node);
  }

  std::unique_ptr<ASTNode> makeComparison(FilePos pos, size_t varId,
                                          const std::string &op, int bound) {
    auto lhs = std::make_unique<ASTNode_Var>(pos, varId);
    auto rhs = std::make_unique<ASTNode_IntLit>(pos, bound);
    return std::make_unique<ASTNode_Math2>(pos, op, std::move(lhs), std::move(rhs));
  }

  std::unique_ptr<ASTNode> makeIncrement(FilePos pos, size_t varId, int delta) {
    auto lhs = std::make_unique<ASTNode_Var>(pos, varId);
    auto base = std::make_unique<ASTNode_Var>(pos, varId);
    auto stepLit = std::make_unique<ASTNode_IntLit>(pos, std::abs(delta));
    std::string op = delta >= 0 ? "+" : "-";
    if (delta < 0) {
      stepLit = std::make_unique<ASTNode_IntLit>(pos, std::abs(delta));
    }
    auto rhs =
        std::make_unique<ASTNode_Math2>(pos, op, std::move(base), std::move(stepLit));
    return std::make_unique<ASTNode_Math2>(pos, "=", std::move(lhs), std::move(rhs));
  }
};
