#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include "SymbolTable.hpp"
#include "core/ASTCloner.hpp"
#include <algorithm>
#include <memory>
#include <vector>

class TailRecursionPass : public Pass {
private:
  SymbolTable &symbols;
  bool loopifyTailRecursion;
  bool enableAccumulatorOptimization;
  bool enableMutualRecursion;
  size_t maxRecursionDepth;

public:
  TailRecursionPass(SymbolTable &symbols, bool loopify, bool accumulator = false, bool mutual = false,
                    size_t maxDepth = 1000)
      : symbols(symbols), loopifyTailRecursion(loopify), enableAccumulatorOptimization(accumulator),
        enableMutualRecursion(mutual), maxRecursionDepth(maxDepth) {}

  std::string getName() const override { return "TailRecursion"; }

  void run(ASTNode &node) override {
    if (!loopifyTailRecursion)
      return;

    if (auto *fn = dynamic_cast<ASTNode_Function *>(&node)) {
      optimizeFunction(*fn);
    } else if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i))
          run(parent->GetChild(i));
      }
    }
  }

private:
  std::unique_ptr<ASTNode> makeDefaultLiteral(const Type &type, FilePos pos) {
    if (type.IsInt())
      return std::make_unique<ASTNode_IntLit>(pos, 0);
    if (type.IsChar())
      return std::make_unique<ASTNode_CharLit>(pos, 0);
    if (type.IsDouble())
      return std::make_unique<ASTNode_FloatLit>(pos, 0.0);
    if (type.IsString())
      return std::make_unique<ASTNode_StringLit>(pos, "");
    return std::make_unique<ASTNode_IntLit>(pos, 0);
  }

  std::unique_ptr<ASTNode> makeDefaultReturnExpr(ASTNode_Function &fn) {
    const Type &returnType = symbols.GetType(fn.GetFunId()).ReturnType();
    return makeDefaultLiteral(returnType, fn.GetFilePos());
  }

  void optimizeFunction(ASTNode_Function &fn) {
    if (fn.NumChildren() == 0 || !fn.HasChild(0))
      return;

    auto transformedBody = transformForTailCalls(fn, fn.GetChild(0));
    if (!transformedBody)
      return;

    auto cond = std::make_unique<ASTNode_IntLit>(fn.GetFilePos(), 1);
    auto whileNode =
        std::make_unique<ASTNode_While>(fn.GetFilePos(), std::move(cond), std::move(transformedBody));
    auto newBlock = std::make_unique<ASTNode_Block>(fn.GetFilePos());
    newBlock->AddChild(std::move(whileNode));
    newBlock->AddChild(
        std::make_unique<ASTNode_Return>(fn.GetFilePos(), makeDefaultReturnExpr(fn)));

    fn.ReplaceChild(0, std::move(newBlock));
  }

  std::unique_ptr<ASTNode> transformForTailCalls(ASTNode_Function &fn, ASTNode &body) {
    size_t selfId = fn.GetFunId();
    const auto &params = fn.GetParamIds();

    if (auto *block = dynamic_cast<ASTNode_Block *>(&body)) {
      auto newBlock = std::make_unique<ASTNode_Block>(block->GetFilePos());
      bool changed = false;
      for (size_t i = 0; i < block->NumChildren(); ++i) {
        if (!block->HasChild(i))
          continue;
        auto child = transformNode(fn, block->GetChild(i), selfId, params, changed);
        if (child)
          newBlock->AddChild(std::move(child));
      }
      if (!changed)
        return nullptr;
      return newBlock;
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode> transformNode(ASTNode_Function &fn, ASTNode &n, size_t selfId,
                                         const std::vector<size_t> &params, bool &changed) {
    if (auto *ret = dynamic_cast<ASTNode_Return *>(&n)) {
      if (ret->NumChildren() >= 1) {
        if (auto *call = dynamic_cast<ASTNode_FunctionCall *>(&ret->GetChild(0))) {
          if (call->GetFunId() == selfId && call->NumChildren() == params.size()) {
            const size_t k = params.size();
            std::vector<ASTNode::ptr_t> argClones;
            argClones.reserve(k);
            bool ok = true;
            for (size_t i = 0; i < k; ++i) {
              auto clonedArg = ASTCloner::clone(call->GetChild(i));
              if (!clonedArg) {
                ok = false;
                break;
              }
              argClones.push_back(std::move(clonedArg));
            }
            if (!ok) {
              return ASTCloner::clone(*ret);
            }

            auto tailNode =
                std::make_unique<ASTNode_TailCallLoop>(ret->GetFilePos(), params, std::move(argClones));
            tailNode->TypeCheck(symbols);
            changed = true;
            return tailNode;
          }
        }
      }
      return ASTCloner::clone(*ret);
    }

    if (auto *iff = dynamic_cast<ASTNode_If *>(&n)) {
      auto test = ASTCloner::clone(iff->GetChild(0));
      if (iff->NumChildren() == 2) {
        bool chThen = false;
        auto thenB = transformNodeAsBlock(fn, iff->GetChild(1), selfId, params, chThen);
        if (chThen)
          changed = true;
        if (test && thenB)
          return std::make_unique<ASTNode_If>(iff->GetFilePos(), std::move(test), std::move(thenB));
      } else if (iff->NumChildren() == 3) {
        bool chThen = false, chElse = false;
        auto thenB = transformNodeAsBlock(fn, iff->GetChild(1), selfId, params, chThen);
        auto elseB = transformNodeAsBlock(fn, iff->GetChild(2), selfId, params, chElse);
        if (chThen || chElse)
          changed = true;
        if (test && thenB && elseB)
          return std::make_unique<ASTNode_If>(iff->GetFilePos(), std::move(test), std::move(thenB),
                                              std::move(elseB));
      }
      return ASTCloner::clone(*iff);
    }

    if (auto *blk = dynamic_cast<ASTNode_Block *>(&n)) {
      auto out = std::make_unique<ASTNode_Block>(blk->GetFilePos());
      bool any = false;
      for (size_t i = 0; i < blk->NumChildren(); ++i) {
        if (!blk->HasChild(i))
          continue;
        bool ch = false;
        auto c = transformNode(fn, blk->GetChild(i), selfId, params, ch);
        if (ch)
          any = true;
        if (c)
          out->AddChild(std::move(c));
      }
      if (any)
        changed = true;
      return out;
    }

    return ASTCloner::clone(n);
  }

  std::unique_ptr<ASTNode> transformNodeAsBlock(ASTNode_Function &fn, ASTNode &n, size_t selfId,
                                                const std::vector<size_t> &params, bool &changed) {
    if (auto *blk = dynamic_cast<ASTNode_Block *>(&n)) {
      return transformNode(fn, *blk, selfId, params, changed);
    } else {
      bool ch = false;
      auto wrapped = std::make_unique<ASTNode_Block>(n.GetFilePos());
      auto nodeTx = transformNode(fn, n, selfId, params, ch);
      if (ch)
        changed = true;
      if (nodeTx)
        wrapped->AddChild(std::move(nodeTx));
      return wrapped;
    }
  }
};
