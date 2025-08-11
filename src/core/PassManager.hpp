#pragma once

#include "Pass.hpp"
#include <memory>
#include <vector>

class PassManager {
private:
    std::vector<std::unique_ptr<Pass>> passes;

public:
    void addPass(std::unique_ptr<Pass> pass) {
        passes.push_back(std::move(pass));
    }

    void runPasses(class ASTNode& root) {
        for (auto& pass : passes) {
            pass->run(root);
        }
    }
    
    void runPass(Pass& pass, ASTNode& root) {
        pass.run(root);
    }
};