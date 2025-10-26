#pragma once

#include "ASTNode.hpp"
#include <memory>
#include <vector>

// Shared AST cloner for use across optimization passes
class ASTCloner {
public:
  static std::unique_ptr<ASTNode> clone(const ASTNode &node) {
    // Handle all node types with dispatch
    if (auto *b = dynamic_cast<const ASTNode_Block *>(&node)) return cloneBlock(*b);
    if (auto *w = dynamic_cast<const ASTNode_While *>(&node)) return cloneWhile(*w);
    if (auto *i = dynamic_cast<const ASTNode_If *>(&node)) return cloneIf(*i);
    if (auto *r = dynamic_cast<const ASTNode_Return *>(&node)) return cloneReturn(*r);
    if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&node)) return cloneMath2(*m2);
    if (auto *m1 = dynamic_cast<const ASTNode_Math1 *>(&node)) return cloneMath1(*m1);
    if (auto *v = dynamic_cast<const ASTNode_Var *>(&node)) return cloneVar(*v);
    if (auto *il = dynamic_cast<const ASTNode_IntLit *>(&node)) return cloneIntLit(*il);
    if (auto *fl = dynamic_cast<const ASTNode_FloatLit *>(&node)) return cloneFloatLit(*fl);
    if (auto *ch = dynamic_cast<const ASTNode_CharLit *>(&node)) return cloneCharLit(*ch);
    if (auto *sl = dynamic_cast<const ASTNode_StringLit *>(&node)) return cloneStringLit(*sl);
    if (auto *fc = dynamic_cast<const ASTNode_FunctionCall *>(&node)) return cloneFunctionCall(*fc);
    if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&node)) return cloneIndexing(*idx);
    if (auto *sz = dynamic_cast<const ASTNode_Size *>(&node)) return cloneSize(*sz);
    if (auto *td = dynamic_cast<const ASTNode_ToDouble *>(&node)) return cloneToDouble(*td);
    if (auto *ti = dynamic_cast<const ASTNode_ToInt *>(&node)) return cloneToInt(*ti);
    if (auto *ts = dynamic_cast<const ASTNode_ToString *>(&node)) return cloneToString(*ts);
    if (auto *fn = dynamic_cast<const ASTNode_Function *>(&node)) return cloneFunction(*fn);
    if (auto *br = dynamic_cast<const ASTNode_Break *>(&node)) return cloneBreak(*br);
    if (auto *ct = dynamic_cast<const ASTNode_Continue *>(&node)) return cloneContinue(*ct);
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneVar(const ASTNode_Var &var) {
    return std::make_unique<ASTNode_Var>(var.GetFilePos(), var.GetVarId());
  }

private:
  static std::unique_ptr<ASTNode_Block> cloneBlock(const ASTNode_Block &block) {
    auto out = std::make_unique<ASTNode_Block>(block.GetFilePos());
    for (size_t i = 0; i < block.NumChildren(); ++i) {
      if (block.HasChild(i)) {
        auto child = clone(block.GetChild(i));
        if (child) out->AddChild(std::move(child));
      }
    }
    return out;
  }

  static std::unique_ptr<ASTNode> cloneWhile(const ASTNode_While &wh) {
    if (wh.NumChildren() < 2) return nullptr;
    auto cond = clone(wh.GetChild(0));
    auto body = clone(wh.GetChild(1));
    if (cond && body) {
      return std::make_unique<ASTNode_While>(wh.GetFilePos(), std::move(cond), std::move(body));
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneIf(const ASTNode_If &ifn) {
    if (ifn.NumChildren() == 2) {
      auto test = clone(ifn.GetChild(0));
      auto thenBranch = clone(ifn.GetChild(1));
      if (test && thenBranch) {
        return std::make_unique<ASTNode_If>(ifn.GetFilePos(), std::move(test), std::move(thenBranch));
      }
    } else if (ifn.NumChildren() == 3) {
      auto test = clone(ifn.GetChild(0));
      auto thenBranch = clone(ifn.GetChild(1));
      auto elseBranch = clone(ifn.GetChild(2));
      if (test && thenBranch && elseBranch) {
        return std::make_unique<ASTNode_If>(ifn.GetFilePos(), std::move(test), std::move(thenBranch), std::move(elseBranch));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneReturn(const ASTNode_Return &ret) {
    if (ret.NumChildren() >= 1) {
      auto expr = clone(ret.GetChild(0));
      if (expr) {
        return std::make_unique<ASTNode_Return>(ret.GetFilePos(), std::move(expr));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneMath2(const ASTNode_Math2 &math2) {
    if (math2.NumChildren() >= 2) {
      auto left = clone(math2.GetChild(0));
      auto right = clone(math2.GetChild(1));
      if (left && right) {
        return std::make_unique<ASTNode_Math2>(math2.GetFilePos(), math2.GetOp(), std::move(left), std::move(right));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneMath1(const ASTNode_Math1 &math1) {
    if (math1.NumChildren() >= 1) {
      auto child = clone(math1.GetChild(0));
      if (child) {
        return std::make_unique<ASTNode_Math1>(math1.GetFilePos(), math1.GetOp(), std::move(child));
      }
    }
    return nullptr;
  }

private:
  static std::unique_ptr<ASTNode> cloneIntLit(const ASTNode_IntLit &intLit) {
    return std::make_unique<ASTNode_IntLit>(intLit.GetFilePos(), intLit.GetValue());
  }

  static std::unique_ptr<ASTNode> cloneFloatLit(const ASTNode_FloatLit &floatLit) {
    return std::make_unique<ASTNode_FloatLit>(floatLit.GetFilePos(), floatLit.GetValue());
  }

  static std::unique_ptr<ASTNode> cloneCharLit(const ASTNode_CharLit &charLit) {
    // Extract value from typename (hacky but matches existing pattern)
    const std::string tn = charLit.GetTypeName();
    int val = 0;
    auto pos = tn.find(": ");
    if (pos != std::string::npos) {
      try { val = std::stoi(tn.substr(pos + 2)); } catch (...) { val = 0; }
    }
    return std::make_unique<ASTNode_CharLit>(charLit.GetFilePos(), val);
  }

  static std::unique_ptr<ASTNode> cloneStringLit(const ASTNode_StringLit &str) {
    return std::make_unique<ASTNode_StringLit>(str.GetFilePos(), str.GetValue());
  }

  static std::unique_ptr<ASTNode> cloneFunctionCall(const ASTNode_FunctionCall &call) {
    std::vector<std::unique_ptr<ASTNode>> args;
    args.reserve(call.NumChildren());
    for (size_t i = 0; i < call.NumChildren(); ++i) {
      if (call.HasChild(i)) {
        auto arg = clone(call.GetChild(i));
        if (arg) args.push_back(std::move(arg));
      }
    }
    return std::make_unique<ASTNode_FunctionCall>(call.GetFilePos(), call.GetFunId(), std::move(args));
  }

  static std::unique_ptr<ASTNode> cloneIndexing(const ASTNode_Indexing &idx) {
    if (idx.NumChildren() >= 2) {
      auto base = clone(idx.GetChild(0));
      auto index = clone(idx.GetChild(1));
      if (base && index) {
        return std::make_unique<ASTNode_Indexing>(idx.GetFilePos(), std::move(base), std::move(index));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneSize(const ASTNode_Size &sz) {
    if (sz.NumChildren() >= 1) {
      auto arg = clone(sz.GetChild(0));
      if (arg) {
        return std::make_unique<ASTNode_Size>(sz.GetFilePos(), std::move(arg));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneToDouble(const ASTNode_ToDouble &td) {
    if (td.NumChildren() >= 1) {
      auto arg = clone(td.GetChild(0));
      if (arg) {
        return std::make_unique<ASTNode_ToDouble>(std::move(arg));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneToInt(const ASTNode_ToInt &ti) {
    if (ti.NumChildren() >= 1) {
      auto arg = clone(ti.GetChild(0));
      if (arg) {
        return std::make_unique<ASTNode_ToInt>(std::move(arg));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneToString(const ASTNode_ToString &ts) {
    if (ts.NumChildren() >= 1) {
      auto arg = clone(ts.GetChild(0));
      if (arg) {
        return std::make_unique<ASTNode_ToString>(std::move(arg));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneFunction(const ASTNode_Function &fn) {
    if (fn.NumChildren() >= 1) {
      auto body = clone(fn.GetChild(0));
      if (body) {
        // Create a dummy token for the constructor - this is a limitation of the current design
        emplex::Token dummyToken;
        dummyToken.line_id = fn.GetFilePos().line;
        dummyToken.col_id = fn.GetFilePos().col;
        return std::make_unique<ASTNode_Function>(dummyToken, fn.GetFunId(), 
                                                  fn.GetParamIds(), std::move(body));
      }
    }
    return nullptr;
  }

  static std::unique_ptr<ASTNode> cloneBreak(const ASTNode_Break &brk) {
    return std::make_unique<ASTNode_Break>(brk.GetFilePos());
  }

  static std::unique_ptr<ASTNode> cloneContinue(const ASTNode_Continue &cont) {
    return std::make_unique<ASTNode_Continue>(cont.GetFilePos());
  }
};
