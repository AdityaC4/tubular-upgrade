#pragma once

#include "ASTNode.hpp"
#include "Pass.hpp"
#include <iostream>

class TailRecursionPass : public Pass {
private:
  bool loopifyTailRecursion;

public:
  TailRecursionPass(bool loopify) : loopifyTailRecursion(loopify) {}

  std::string getName() const override { return "TailRecursion"; }

  void run(ASTNode &node) override {
    if (!loopifyTailRecursion) {
      return;
    }
  }
};
