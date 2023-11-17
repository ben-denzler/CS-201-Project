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

        unsigned instr_index = 0;

        for (auto &basic_block : F) {  // Iterates over basic blocks of the function 
            for (auto &inst : basic_block) {  // Iterates over instructions in a basic block
                errs() << instr_index << ": " << inst << "\n";

                if (inst.getOpcode() == Instruction::Store) {
                    errs() << "This is Store" << "\n";
                    Value* storeDestination = inst.getOperand(1);
                    errs() << "Store instr destination: " << *storeDestination << "\n";
                }

                instr_index++;
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
