#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include "core/ASTCloner.hpp"
#include <memory>
#include <vector>
#include <unordered_set>
#include <queue>
#include <algorithm>

class TailRecursionPass : public Pass {
private:
  bool loopifyTailRecursion;

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
              rhsClones[i] = ASTCloner::clone(call->GetChild(i));
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
              // Cycle detected - for now, just skip transformation to avoid complexity
              // TODO: Implement proper cycle breaking with temporary variables
              return ASTCloner::clone(*ret);
            }

            // Emit assignments in dependency-safe order: params in 'order'
            auto seq = std::make_unique<ASTNode_Block>(ret->GetFilePos());
            for (size_t idx = 0; idx < order.size(); ++idx) {
              size_t i = order[idx];
              auto lhs = std::make_unique<ASTNode_Var>(ret->GetFilePos(), params[i]);
              auto rhs = ASTCloner::clone(*rhsClones[i]);
              seq->AddChild(std::make_unique<ASTNode_Math2>(ret->GetFilePos(), "=", std::move(lhs), std::move(rhs)));
            }
            seq->AddChild(std::make_unique<ASTNode_Continue>(ret->GetFilePos()));
            changed = true;
            return seq;
          }
        }
      }
      // Non-tail return: clone as-is
      return ASTCloner::clone(*ret);
    }

    // If: transform branches recursively
    if (auto *iff = dynamic_cast<ASTNode_If *>(&n)) {
      auto test = ASTCloner::clone(iff->GetChild(0));
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
      return ASTCloner::clone(*iff);
    }

    // Block: transform children
    if (auto *blk = dynamic_cast<ASTNode_Block *>(&n)) {
      auto out = std::make_unique<ASTNode_Block>(blk->GetFilePos());
      bool any=false;
      for (size_t i=0;i<blk->NumChildren();++i) if (blk->HasChild(i)) { bool ch=false; auto c=transformNode(fn, blk->GetChild(i), selfId, params, ch); if (ch) any=true; if (c) out->AddChild(std::move(c)); }
      if (any) changed = true; return out;
    }

    // Other nodes: clone as-is
    return ASTCloner::clone(n);
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
