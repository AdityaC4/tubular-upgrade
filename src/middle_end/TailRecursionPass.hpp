#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include <memory>
#include <vector>
#include <unordered_set>
#include <queue>
#include <algorithm>

class TailRecursionPass : public Pass {
private:
  bool loopifyTailRecursion;

  // Lightweight AST cloner
  struct Cloner {
    static std::unique_ptr<ASTNode> clone(const ASTNode &n) {
      if (auto *b = dynamic_cast<const ASTNode_Block *>(&n)) return cloneBlock(*b);
      if (auto *w = dynamic_cast<const ASTNode_While *>(&n)) return cloneWhile(*w);
      if (auto *i = dynamic_cast<const ASTNode_If *>(&n)) return cloneIf(*i);
      if (auto *r = dynamic_cast<const ASTNode_Return *>(&n)) return cloneReturn(*r);
      if (auto *m2 = dynamic_cast<const ASTNode_Math2 *>(&n)) return cloneMath2(*m2);
      if (auto *m1 = dynamic_cast<const ASTNode_Math1 *>(&n)) return cloneMath1(*m1);
      if (auto *v = dynamic_cast<const ASTNode_Var *>(&n)) return std::make_unique<ASTNode_Var>(v->GetFilePos(), v->GetVarId());
      if (auto *il = dynamic_cast<const ASTNode_IntLit *>(&n)) return std::make_unique<ASTNode_IntLit>(il->GetFilePos(), il->GetValue());
      if (auto *fl = dynamic_cast<const ASTNode_FloatLit *>(&n)) return std::make_unique<ASTNode_FloatLit>(fl->GetFilePos(), fl->GetValue());
      if (auto *ch = dynamic_cast<const ASTNode_CharLit *>(&n)) return std::make_unique<ASTNode_CharLit>(ch->GetFilePos(), (int)(ch->GetTypeName().size()>0));
      if (auto *sl = dynamic_cast<const ASTNode_StringLit *>(&n)) return std::make_unique<ASTNode_StringLit>(sl->GetFilePos(), sl->GetValue());
      if (auto *fc = dynamic_cast<const ASTNode_FunctionCall *>(&n)) return cloneCall(*fc);
      if (auto *idx = dynamic_cast<const ASTNode_Indexing *>(&n)) return cloneIndexing(*idx);
      if (auto *sz = dynamic_cast<const ASTNode_Size *>(&n)) return cloneSize(*sz);
      if (auto *br = dynamic_cast<const ASTNode_Break *>(&n)) return std::make_unique<ASTNode_Break>(n.GetFilePos());
      if (auto *ct = dynamic_cast<const ASTNode_Continue *>(&n)) return std::make_unique<ASTNode_Continue>(n.GetFilePos());
      return nullptr;
    }

    static std::unique_ptr<ASTNode_Block> cloneBlock(const ASTNode_Block &b) {
      auto out = std::make_unique<ASTNode_Block>(b.GetFilePos());
      for (size_t i = 0; i < b.NumChildren(); ++i) if (b.HasChild(i)) { auto c = clone(b.GetChild(i)); if (c) out->AddChild(std::move(c)); }
      return out;
    }
    static std::unique_ptr<ASTNode> cloneWhile(const ASTNode_While &w) {
      if (w.NumChildren()<2) return nullptr; auto t = clone(w.GetChild(0)); auto a = clone(w.GetChild(1)); if (t&&a) return std::make_unique<ASTNode_While>(w.GetFilePos(), std::move(t), std::move(a)); return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneIf(const ASTNode_If &i) {
      if (i.NumChildren()==2) { auto t=clone(i.GetChild(0)); auto a=clone(i.GetChild(1)); if(t&&a) return std::make_unique<ASTNode_If>(i.GetFilePos(), std::move(t), std::move(a)); }
      else if (i.NumChildren()==3) { auto t=clone(i.GetChild(0)); auto a=clone(i.GetChild(1)); auto e=clone(i.GetChild(2)); if(t&&a&&e) return std::make_unique<ASTNode_If>(i.GetFilePos(), std::move(t), std::move(a), std::move(e)); }
      return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneReturn(const ASTNode_Return &r) {
      if (r.NumChildren()<1) return nullptr; auto e=clone(r.GetChild(0)); if(e) return std::make_unique<ASTNode_Return>(r.GetFilePos(), std::move(e)); return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneMath2(const ASTNode_Math2 &m) {
      if (m.NumChildren()<2) return nullptr; auto l=clone(m.GetChild(0)); auto r=clone(m.GetChild(1)); if(l&&r) return std::make_unique<ASTNode_Math2>(m.GetFilePos(), m.GetOp(), std::move(l), std::move(r)); return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneMath1(const ASTNode_Math1 &m) {
      if (m.NumChildren()<1) return nullptr; auto c=clone(m.GetChild(0)); if(c) return std::make_unique<ASTNode_Math1>(m.GetFilePos(), m.GetOp(), std::move(c)); return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneCall(const ASTNode_FunctionCall &c) {
      std::vector<std::unique_ptr<ASTNode>> args; args.reserve(c.NumChildren());
      for (size_t i=0;i<c.NumChildren();++i) if(c.HasChild(i)){ auto a=clone(c.GetChild(i)); if(a) args.push_back(std::move(a)); }
      return std::make_unique<ASTNode_FunctionCall>(c.GetFilePos(), c.GetFunId(), std::move(args));
    }
    static std::unique_ptr<ASTNode> cloneIndexing(const ASTNode_Indexing &idx) {
      if (idx.NumChildren()<2) return nullptr; auto b=clone(idx.GetChild(0)); auto i=clone(idx.GetChild(1)); if(b&&i) return std::make_unique<ASTNode_Indexing>(idx.GetFilePos(), std::move(b), std::move(i)); return nullptr;
    }
    static std::unique_ptr<ASTNode> cloneSize(const ASTNode_Size &sz) {
      if (sz.NumChildren()<1) return nullptr; auto a=clone(sz.GetChild(0)); if(a) return std::make_unique<ASTNode_Size>(sz.GetFilePos(), std::move(a)); return nullptr;
    }
  };

public:
  TailRecursionPass(bool loopify) : loopifyTailRecursion(loopify) {}

  std::string getName() const override { return "TailRecursion"; }

  void run(ASTNode &node) override {
    if (!loopifyTailRecursion) return;

    if (auto *fn = dynamic_cast<ASTNode_Function *>(&node)) {
      optimizeFunction(*fn);
    } else if (auto *parent = dynamic_cast<ASTNode_Parent *>(&node)) {
      for (size_t i = 0; i < parent->NumChildren(); ++i) if (parent->HasChild(i)) run(parent->GetChild(i));
    }
  }

  private:
  static void CollectParamDeps(const ASTNode &node, const std::vector<size_t> &params, std::unordered_set<size_t> &out) {
    if (auto *v = dynamic_cast<const ASTNode_Var*>(&node)) {
      size_t vid = v->GetVarId();
      for (size_t idx=0; idx<params.size(); ++idx) if (params[idx]==vid) { out.insert(idx); break; }
    } else if (auto *p = dynamic_cast<const ASTNode_Parent*>(&node)) {
      for (size_t i=0;i<p->NumChildren();++i) if (p->HasChild(i)) CollectParamDeps(p->GetChild(i), params, out);
    }
  }

  void optimizeFunction(ASTNode_Function &fn) {
    // Preconditions: single body child
    if (fn.NumChildren() == 0 || !fn.HasChild(0)) return;

    // Build transformed body: replace tail calls with param reassignments + continue
    auto transformedBody = transformForTailCalls(fn, fn.GetChild(0));
    if (!transformedBody) return; // no change

    // Wrap in while (1) { transformedBody } followed by an unreachable return
    // Adding a dummy return after the loop satisfies Wasm validation that the
    // function with a result type leaves a value; this path is not taken.
    auto cond = std::make_unique<ASTNode_IntLit>(fn.GetFilePos(), 1);
    auto whileNode = std::make_unique<ASTNode_While>(fn.GetFilePos(), std::move(cond), std::move(transformedBody));
    auto newBlock = std::make_unique<ASTNode_Block>(fn.GetFilePos());
    newBlock->AddChild(std::move(whileNode));
    newBlock->AddChild(std::make_unique<ASTNode_Return>(fn.GetFilePos(), std::make_unique<ASTNode_IntLit>(fn.GetFilePos(), 0)));

    // Replace the body with the block (loop + dummy return)
    fn.ReplaceChild(0, std::move(newBlock));
  }

  std::unique_ptr<ASTNode> transformForTailCalls(ASTNode_Function &fn, ASTNode &body) {
    size_t selfId = fn.GetFunId();
    const auto &params = fn.GetParamIds();

    // Produce a new block which is a transformed clone of the original body
    if (auto *block = dynamic_cast<ASTNode_Block *>(&body)) {
      auto newBlock = std::make_unique<ASTNode_Block>(block->GetFilePos());
      bool changed = false;
      for (size_t i = 0; i < block->NumChildren(); ++i) {
        if (!block->HasChild(i)) continue;
        auto child = transformNode(fn, block->GetChild(i), selfId, params, changed);
        if (child) newBlock->AddChild(std::move(child));
      }
      if (!changed) return nullptr;
      return newBlock;
    }
    return nullptr;
  }

  std::unique_ptr<ASTNode> transformNode(ASTNode_Function &fn, ASTNode &n, size_t selfId,
                                          const std::vector<size_t> &params, bool &changed) {
    // Tail-return of self call: return f(args...)
    if (auto *ret = dynamic_cast<ASTNode_Return *>(&n)) {
      if (ret->NumChildren() >= 1) {
        if (auto *call = dynamic_cast<ASTNode_FunctionCall *>(&ret->GetChild(0))) {
          if (call->GetFunId() == selfId && call->NumChildren() == params.size()) {
            // Analyze dependencies among parameter updates to avoid clobbering.
            const size_t k = params.size();
            std::vector<std::unique_ptr<ASTNode>> rhsClones(k);
            std::vector<std::unordered_set<size_t>> deps(k); // param indices used by each RHS

            // Prepare RHS clones and dependencies
            for (size_t i = 0; i < k; ++i) {
              rhsClones[i] = Cloner::clone(call->GetChild(i));
              if (rhsClones[i]) CollectParamDeps(*rhsClones[i], params, deps[i]);
            }

            // Determine which params actually change (skip identity assignments var=var)
            auto isIdentity = [&](size_t i)->bool{
              if (auto *v = dynamic_cast<ASTNode_Var*>(rhsClones[i].get())) {
                return v->GetVarId() == params[i];
              }
              return false;
            };

            std::vector<size_t> nodes; nodes.reserve(k);
            for (size_t i=0;i<k;++i) if (!isIdentity(i)) nodes.push_back(i);

            // Build in-degree for Kahn's algorithm: edge i->j if RHS_i uses param j
            std::vector<int> indeg(k, 0);
            std::vector<std::vector<size_t>> graph(k);
            // Edge direction: i -> j (assign i must occur BEFORE j) because RHS_i reads j's old value.
            for (size_t i: nodes) {
              for (size_t j: deps[i]) {
                if (i==j) continue;
                if (std::find(nodes.begin(), nodes.end(), j) == nodes.end()) continue;
                graph[i].push_back(j);
                indeg[j]++;
              }
            }
            std::queue<size_t> q; std::vector<size_t> order; order.clear();
            for (size_t i: nodes) if (indeg[i]==0) q.push(i);
            while(!q.empty()){
              size_t u=q.front(); q.pop(); order.push_back(u);
              for (size_t v: graph[u]) { if (--indeg[v]==0) q.push(v); }
            }

            if (order.size() != nodes.size()) {
              // cycle detected (e.g., gcd or fib accum). Skip transform.
              return Cloner::clone(*ret);
            }

            // Emit assignments in dependency-safe order: params in 'order'
            auto seq = std::make_unique<ASTNode_Block>(ret->GetFilePos());
            for (size_t idx = 0; idx < order.size(); ++idx) {
              size_t i = order[idx];
              auto lhs = std::make_unique<ASTNode_Var>(ret->GetFilePos(), params[i]);
              auto rhs = Cloner::clone(*rhsClones[i]);
              seq->AddChild(std::make_unique<ASTNode_Math2>(ret->GetFilePos(), "=", std::move(lhs), std::move(rhs)));
            }
            seq->AddChild(std::make_unique<ASTNode_Continue>(ret->GetFilePos()));
            changed = true;
            return seq;
          }
        }
      }
      // Non-tail return: clone as-is
      return Cloner::clone(*ret);
    }

    // If: transform branches recursively
    if (auto *iff = dynamic_cast<ASTNode_If *>(&n)) {
      auto test = Cloner::clone(iff->GetChild(0));
      if (iff->NumChildren() == 2) {
        bool chThen=false; auto thenB = transformNodeAsBlock(fn, iff->GetChild(1), selfId, params, chThen);
        if (chThen) changed = true;
        if (test && thenB) return std::make_unique<ASTNode_If>(iff->GetFilePos(), std::move(test), std::move(thenB));
      } else if (iff->NumChildren() == 3) {
        bool chThen=false, chElse=false;
        auto thenB = transformNodeAsBlock(fn, iff->GetChild(1), selfId, params, chThen);
        auto elseB = transformNodeAsBlock(fn, iff->GetChild(2), selfId, params, chElse);
        if (chThen||chElse) changed = true;
        if (test && thenB && elseB) return std::make_unique<ASTNode_If>(iff->GetFilePos(), std::move(test), std::move(thenB), std::move(elseB));
      }
      // Fallback clone
      return Cloner::clone(*iff);
    }

    // Block: transform children
    if (auto *blk = dynamic_cast<ASTNode_Block *>(&n)) {
      auto out = std::make_unique<ASTNode_Block>(blk->GetFilePos());
      bool any=false;
      for (size_t i=0;i<blk->NumChildren();++i) if (blk->HasChild(i)) { bool ch=false; auto c=transformNode(fn, blk->GetChild(i), selfId, params, ch); if (ch) any=true; if (c) out->AddChild(std::move(c)); }
      if (any) changed = true; return out;
    }

    // Other nodes: clone as-is
    return Cloner::clone(n);
  }

  std::unique_ptr<ASTNode> transformNodeAsBlock(ASTNode_Function &fn, ASTNode &n, size_t selfId,
                                                const std::vector<size_t> &params, bool &changed) {
    if (auto *blk = dynamic_cast<ASTNode_Block *>(&n)) {
      return transformNode(fn, *blk, selfId, params, changed);
    } else {
      bool ch=false; auto wrapped = std::make_unique<ASTNode_Block>(n.GetFilePos());
      auto nodeTx = transformNode(fn, n, selfId, params, ch); if (ch) changed=true; if (nodeTx) wrapped->AddChild(std::move(nodeTx)); return wrapped;
    }
  }
};
