#pragma once

#include <memory>
#include <string>
#include <vector>

class ASTNode; // Forward declaration

class Pass {
public:
  virtual ~Pass() = default;
  virtual std::string getName() const = 0;
  virtual void run(ASTNode &node) = 0;
};
