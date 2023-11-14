#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "ReachingDefinition"

namespace {
struct ReachingDefinition : public FunctionPass {
    static char ID;
    ReachingDefinition() : FunctionPass(ID) {}

    bool runOnFunction(Function& F) override {
        errs() << "ReachingDefinition: ";
        errs() << F.getName() << "\n";

        // Iterates over basic blocks of the function
        for (auto &basic_block : F) {            
            // Iterates over instructions in a basic block
            for (auto &inst : basic_block) {
                errs() << inst << "\n";
                if (inst.getOpcode() == Instruction::Load) {
                    errs() << "This is Load" << "\n";
                }
                if (inst.getOpcode() == Instruction::Store) {
                    errs() << "This is Store" << "\n";
                }
                if (inst.isBinaryOp()) {
                    errs() << "Op Code:" << inst.getOpcodeName() << "\n";
                    if (inst.getOpcode() == Instruction::Add) {
                        errs() << "This is Addition" << "\n";
                    }
                    if (inst.getOpcode() == Instruction::Sub) {
                        errs() << "This is Subtraction" << "\n";
                    }
                    if (inst.getOpcode() == Instruction::Mul) {
                        errs() << "This is Multiplication" << "\n";
                    }
                    if (inst.getOpcode() == Instruction::SDiv) {
                        errs() << "This is Division" << "\n";
                    }

                    // See Other classes, Instruction::Sub, Instruction::UDiv,
                    // Instruction::SDiv
                    auto *ptr = dyn_cast<User>(&inst);
                    for (auto it = ptr->op_begin(); it != ptr->op_end(); ++it) {
                        errs() << "\t" << *(*it) << "\n";
                    }
                }
            }
        }
        return true;
    }
}; // end of struct ReachingDefinition
} // end of anonymous namespace

char ReachingDefinition::ID = 0;
static RegisterPass<ReachingDefinition> X("ReachingDefinition", "Reaching Definition Pass",
                                          false /* Only looks at CFG */,
                                          true /* Analysis Pass */);
