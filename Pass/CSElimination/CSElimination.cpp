#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "CSElimination"

namespace {
struct CSElimination : public FunctionPass {
    static char ID;
    CSElimination() : FunctionPass(ID) {}

    bool runOnFunction(Function& F) override {
        errs() << "\nFunction: " << F.getName() << "\n";
        unsigned blockNum = 0;

        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            errs() << "Block " << blockNum++ << ":\n";

            for (auto& inst : basic_block) { // Iterates over instructions in a basic block

                // Find statements A = B op C
                if (inst.getOpcode() == Instruction::Add
                    || inst.getOpcode() == Instruction::Sub
                    || inst.getOpcode() == Instruction::Mul
                    || inst.getOpcode() == Instruction::SDiv) {
                    Value* op1 = inst.getOperand(0);
                    Value* op2 = inst.getOperand(1);
                    errs() << "op1: " << *op1 << "\n";
                    errs() << "op2: " << *op2 << "\n";
                }
            }
        }

        return true; // Indicate this is a Transform pass
    }
}; // end of struct CSElimination
} // end of anonymous namespace

char CSElimination::ID = 0;
static RegisterPass<CSElimination> X("CSElimination", "CSElimination Pass",
                                     false /* Only looks at CFG */,
                                     true /* Tranform Pass */);
