#pragma once

#include <memory>
#include <vector>
#include <string>

class ASTNode; // Forward declaration

class Pass {
public:
    virtual ~Pass() = default;
    virtual std::string getName() const = 0;
    virtual void run(ASTNode& node) = 0;
};