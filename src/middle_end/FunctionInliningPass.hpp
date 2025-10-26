#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include "SymbolTable.hpp"
#include "../core/ASTCloner.hpp"
#include "NodeCounter.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class FunctionInliningPass : public Pass {
private:
  struct FunctionInfo {
    ASTNode_Function *func = nullptr;
    bool recursive = false;
    bool inlineable = false;
    size_t nodeCount = 0;
    const ASTNode *returnExpr = nullptr;
    std::vector<size_t> paramIds;
    std::unordered_set<size_t> paramSet;
    std::unordered_map<size_t, size_t> paramUsage;
  };

  SymbolTable &symbols;
  bool enabled;
  bool aggressive;
  bool allowRecursive;
  size_t maxDepth;
  size_t maxNodes;

  std::unordered_map<size_t, FunctionInfo> functionInfos;

public:
  FunctionInliningPass(SymbolTable &symbolsRef, bool enabledFlag, bool aggressiveFlag = false,
                       bool allowRecursiveInline = false, size_t depthLimit = 3,
                       size_t nodeLimit = 40, size_t /*unusedSizeLimit*/ = 100)
      : symbols(symbolsRef), enabled(enabledFlag), aggressive(aggressiveFlag),
        allowRecursive(allowRecursiveInline), maxDepth(depthLimit), maxNodes(nodeLimit) {}

  std::string getName() const override { return "FunctionInlining"; }

  void run(ASTNode &root) override {
    if (!enabled) {
      return;
    }

    functionInfos.clear();
    collectFunctions(root);
    detectRecursiveFunctions();
    analyseFunctions();
    inlineNode(root, 0);
    functionInfos.clear();
  }

private:
  static const ASTNode &GetConstChild(const ASTNode_Parent &parent, size_t idx) {
    return const_cast<ASTNode_Parent &>(parent).GetChild(idx);
  }

  static size_t GetVarId(const ASTNode_Var &var) { return var.GetVarId(); }

  void collectFunctions(ASTNode &node) {
    if (auto *fn = dynamic_cast<ASTNode_Function *>(&node)) {
      FunctionInfo info;
      info.func = fn;
      info.paramIds = fn->GetParamIds();
      info.paramSet.insert(info.paramIds.begin(), info.paramIds.end());
      functionInfos.emplace(fn->GetFunId(), std::move(info));
    }

    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i)) {
          collectFunctions(parent->GetChild(i));
        }
      }
    }
  }

  void detectRecursiveFunctions() {
    for (auto &entry : functionInfos) {
      auto &info = entry.second;
      info.recursive = hasCallTo(*info.func, entry.first);
    }
  }

  bool hasCallTo(const ASTNode &node, size_t funId) const {
    if (auto *call = dynamic_cast<const ASTNode_FunctionCall *>(&node)) {
      if (call->GetFunId() == funId) {
        return true;
      }
    }

    if (auto *parent = dynamic_cast<const ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (parent->HasChild(i) && hasCallTo(GetConstChild(*parent, i), funId)) {
          return true;
        }
      }
    }
    return false;
  }

  void analyseFunctions() {
    for (auto &entry : functionInfos) {
      auto &info = entry.second;
      info.inlineable = analyseFunction(info);
    }
  }

  bool analyseFunction(FunctionInfo &info) {
    const ASTNode *expr = extractReturnExpression(*info.func);
    if (!expr) {
      return false;
    }

    std::unordered_map<size_t, size_t> usage;
    if (!isPureExpression(*expr, info, usage)) {
      return false;
    }

    for (auto &[paramId, count] : usage) {
      if (count > 1) {
        return false;
      }
    }

    NodeCounter counter;
    const_cast<ASTNode &>(*expr).Accept(counter);
    info.nodeCount = static_cast<size_t>(counter.getCount());
    size_t limit = aggressive ? maxNodes * 2 : maxNodes;
    if (info.nodeCount > limit) {
      return false;
    }

    info.returnExpr = expr;
    info.paramUsage = std::move(usage);
    return true;
  }

  const ASTNode *extractReturnExpression(ASTNode_Function &fn) {
    if (!fn.HasChild(0)) {
      return nullptr;
    }
    ASTNode &body = fn.GetChild(0);

    if (auto *ret = dynamic_cast<ASTNode_Return *>(&body)) {
      if (ret->NumChildren() == 1 && ret->HasChild(0)) {
        return &ret->GetChild(0);
      }
      return nullptr;
    }

    if (auto *block = dynamic_cast<ASTNode_Block *>(&body)) {
      if (block->NumChildren() != 1 || !block->HasChild(0)) {
        return nullptr;
      }
      if (auto *ret = dynamic_cast<ASTNode_Return *>(&block->GetChild(0))) {
        if (ret->NumChildren() == 1 && ret->HasChild(0)) {
          return &ret->GetChild(0);
        }
      }
    }
    return nullptr;
  }

  bool isPureExpression(const ASTNode &expr, FunctionInfo &info,
                        std::unordered_map<size_t, size_t> &usage) {
    if (dynamic_cast<const ASTNode_IntLit *>(&expr) || dynamic_cast<const ASTNode_FloatLit *>(&expr) ||
        dynamic_cast<const ASTNode_CharLit *>(&expr) || dynamic_cast<const ASTNode_StringLit *>(&expr)) {
      return true;
    }

    if (auto *var = dynamic_cast<const ASTNode_Var *>(&expr)) {
      size_t id = GetVarId(*var);
      if (!info.paramSet.count(id)) {
        return false;
      }
      usage[id]++;
      return true;
    }

    if (auto *m1 = dynamic_cast<const ASTNode_Math1 *>(&expr)) {
      return isPureExpression(GetConstChild(*m1, 0), info, usage);
    }

    if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&expr)) {
      const std::string &op = const_cast<ASTNode_Math2 *>(m2)->GetOp();
      if (op == "=") {
        return false;
      }
      return isPureExpression(GetConstChild(*m2, 0), info, usage) &&
             isPureExpression(GetConstChild(*m2, 1), info, usage);
    }

    if (auto *conv = dynamic_cast<const ASTNode_ToDouble *>(&expr)) {
      return isPureExpression(GetConstChild(*conv, 0), info, usage);
    }
    if (auto *conv = dynamic_cast<const ASTNode_ToInt *>(&expr)) {
      return isPureExpression(GetConstChild(*conv, 0), info, usage);
    }
    if (auto *conv = dynamic_cast<const ASTNode_ToString *>(&expr)) {
      return isPureExpression(GetConstChild(*conv, 0), info, usage);
    }

    if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&expr)) {
      return isPureExpression(GetConstChild(*idx, 0), info, usage) &&
             isPureExpression(GetConstChild(*idx, 1), info, usage);
    }
    if (auto *sz = dynamic_cast<const ASTNode_Size *>(&expr)) {
      return isPureExpression(GetConstChild(*sz, 0), info, usage);
    }

    // Conservative: disallow nested function calls or control structures
    if (dynamic_cast<const ASTNode_FunctionCall *>(&expr)) {
      return false;
    }
    if (dynamic_cast<const ASTNode_Parent *>(&expr)) {
      return false;
    }

    return false;
  }

  void inlineNode(ASTNode &node, size_t depth) {
    if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) {
        if (!parent->HasChild(i)) {
          continue;
        }

        ASTNode &child = parent->GetChild(i);
        if (auto *call = dynamic_cast<ASTNode_FunctionCall *>(&child)) {
          if (auto replacement = tryInlineCall(*call, depth)) {
            parent->ReplaceChild(i, std::move(replacement));
            inlineNode(parent->GetChild(i), depth);
            continue;
          }
        }
        inlineNode(child, depth);
      }
    }
  }

  std::unique_ptr<ASTNode> tryInlineCall(ASTNode_FunctionCall &call, size_t depth) {
    std::vector<std::unique_ptr<ASTNode>> args;
    args.reserve(call.NumChildren());
    for (size_t i = 0; i < call.NumChildren(); ++i) {
      args.push_back(ASTCloner::clone(call.GetChild(i)));
    }
    auto result = tryInlineCall(call.GetFunId(), args, depth);
    if (result) {
      result->TypeCheck(symbols);
    }
    return result;
  }

  std::unique_ptr<ASTNode>
  tryInlineCall(size_t funId, const std::vector<std::unique_ptr<ASTNode>> &args, size_t depth) {
    auto it = functionInfos.find(funId);
    if (it == functionInfos.end()) {
      return nullptr;
    }
    auto &info = it->second;

    if (!info.inlineable) {
      return nullptr;
    }
    if (info.recursive && !allowRecursive) {
      return nullptr;
    }
    if (depth >= maxDepth) {
      return nullptr;
    }
    if (args.size() != info.paramIds.size()) {
      return nullptr;
    }

    std::unordered_map<size_t, std::unique_ptr<ASTNode>> substitution;
    substitution.reserve(info.paramIds.size());
    for (size_t i = 0; i < info.paramIds.size(); ++i) {
      substitution.emplace(info.paramIds[i], ASTCloner::clone(*args[i]));
    }

    auto inlined = inlineExpression(*info.returnExpr, substitution, depth + 1);
    if (!inlined) {
      return nullptr;
    }
    return inlined;
  }

  std::unique_ptr<ASTNode>
  inlineExpression(const ASTNode &expr, std::unordered_map<size_t, std::unique_ptr<ASTNode>> &paramMap,
                   size_t depth) {
    if (dynamic_cast<const ASTNode_IntLit *>(&expr) || dynamic_cast<const ASTNode_FloatLit *>(&expr) ||
        dynamic_cast<const ASTNode_CharLit *>(&expr) || dynamic_cast<const ASTNode_StringLit *>(&expr)) {
      return ASTCloner::clone(expr);
    }

    if (auto *var = dynamic_cast<const ASTNode_Var *>(&expr)) {
      size_t id = GetVarId(*var);
      auto it = paramMap.find(id);
      if (it != paramMap.end()) {
        if (!it->second) {
          return nullptr;
        }
        auto out = std::move(it->second);
        return out;
      }
      return ASTCloner::clone(expr);
    }

    if (auto *m1 = dynamic_cast<const ASTNode_Math1 *>(&expr)) {
      auto child = inlineExpression(GetConstChild(*m1, 0), paramMap, depth);
      if (!child) {
        return nullptr;
      }
      std::string op = const_cast<ASTNode_Math1 *>(m1)->GetOp();
      return std::make_unique<ASTNode_Math1>(expr.GetFilePos(), op, std::move(child));
    }

    if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&expr)) {
      auto left = inlineExpression(GetConstChild(*m2, 0), paramMap, depth);
      auto right = inlineExpression(GetConstChild(*m2, 1), paramMap, depth);
      if (!left || !right) {
        return nullptr;
      }
      std::string op = const_cast<ASTNode_Math2 *>(m2)->GetOp();
      return std::make_unique<ASTNode_Math2>(expr.GetFilePos(), op, std::move(left), std::move(right));
    }

    if (auto *conv = dynamic_cast<const ASTNode_ToDouble *>(&expr)) {
      auto child = inlineExpression(GetConstChild(*conv, 0), paramMap, depth);
      if (!child) {
        return nullptr;
      }
      return std::make_unique<ASTNode_ToDouble>(std::move(child));
    }
    if (auto *conv = dynamic_cast<const ASTNode_ToInt *>(&expr)) {
      auto child = inlineExpression(GetConstChild(*conv, 0), paramMap, depth);
      if (!child) {
        return nullptr;
      }
      return std::make_unique<ASTNode_ToInt>(std::move(child));
    }
    if (auto *conv = dynamic_cast<const ASTNode_ToString *>(&expr)) {
      auto child = inlineExpression(GetConstChild(*conv, 0), paramMap, depth);
      if (!child) {
        return nullptr;
      }
      return std::make_unique<ASTNode_ToString>(std::move(child));
    }

    if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&expr)) {
      auto base = inlineExpression(GetConstChild(*idx, 0), paramMap, depth);
      auto index = inlineExpression(GetConstChild(*idx, 1), paramMap, depth);
      if (!base || !index) {
        return nullptr;
      }
      return std::make_unique<ASTNode_Indexing>(expr.GetFilePos(), std::move(base), std::move(index));
    }
    if (auto *sz = dynamic_cast<const ASTNode_Size *>(&expr)) {
      auto arg = inlineExpression(GetConstChild(*sz, 0), paramMap, depth);
      if (!arg) {
        return nullptr;
      }
      return std::make_unique<ASTNode_Size>(expr.GetFilePos(), std::move(arg));
    }

    if (auto *call = dynamic_cast<const ASTNode_FunctionCall *>(&expr)) {
      std::vector<std::unique_ptr<ASTNode>> args;
      args.reserve(call->NumChildren());
      for (size_t i = 0; i < call->NumChildren(); ++i) {
        auto childExpr = inlineExpression(GetConstChild(*call, i), paramMap, depth);
        if (!childExpr) {
          return nullptr;
        }
        args.push_back(std::move(childExpr));
      }

      if (auto nested = tryInlineCall(call->GetFunId(), args, depth)) {
        nested->TypeCheck(symbols);
        return nested;
      }

      return std::make_unique<ASTNode_FunctionCall>(expr.GetFilePos(), call->GetFunId(), std::move(args));
    }

    return nullptr;
  }
};
