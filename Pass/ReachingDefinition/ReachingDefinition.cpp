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
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "ReachingDefinition"

namespace {
struct ReachingDefinition : public FunctionPass {
    static char ID;
    ReachingDefinition() : FunctionPass(ID) {}

    bool runOnFunction(Function& F) override {
        errs() << "\nFunction: " << F.getName() << "\n";

        vector<unsigned> storeInstructionIndices;
        vector<Value*> storeInstructionDestVars;
        unsigned instrIndex = 0;
        unsigned blockNum = 0;

        // First Pass: Get all of the indexes and destination variables of the store instructions
        for (auto &basic_block : F) {  // Iterates over basic blocks of the function 
            errs() << "Block " << blockNum++ << ":\n";

            for (auto &inst : basic_block) {  // Iterates over instructions in a basic block
                errs() << instrIndex << ": " << inst << "\n";

                if (inst.getOpcode() == Instruction::Store) {
                    errs() << " This is Store" << "\n";
                    Value* storeDestination = inst.getOperand(1);
                    errs() << " Store instr destination: " << *storeDestination << "\n";
                    storeInstructionIndices.push_back(instrIndex);
                    storeInstructionDestVars.push_back(storeDestination);
                }
                instrIndex++;
            }
            errs() << "\n";
        }

        // // Check the vector of indexes for store instructions
        // errs() << "\n" << "storeInstructionIndices" << "\n";
        // for (unsigned int i = 0; i < storeInstructionIndices.size(); i++) {
        //     errs() << storeInstructionIndices.at(i) << "\n";
        // }

        instrIndex = 0;
        blockNum = 0;
        vector<vector<unsigned>> blockGenSets = {};
        vector<vector<unsigned>> blockKillSets = {};

        // Second Pass: Add to GEN and KILL sets
        for (auto &basic_block : F) {  // Iterates over basic blocks of the function 
            vector<vector<unsigned>> GEN_KILLSETS = {};
            vector<unsigned> GEN = {};
            vector<unsigned> KILL = {};

            errs() << "Block " << blockNum << ":\n";

            for (auto &inst : basic_block) {  // Iterates over instructions in a basic block
                if (inst.getOpcode() == Instruction::Store) {
                    GEN.push_back(instrIndex);  // Each store instruction is a GEN
                    Value* storeDestination = inst.getOperand(1);

                    // Find other instructions that change the same variable; add them to block's KILL
                    for (unsigned int i = 0; i < storeInstructionDestVars.size(); i++) {
                        if ((storeInstructionDestVars.at(i) == storeDestination) && (storeInstructionIndices.at(i) != instrIndex)) {
                            KILL.push_back(storeInstructionIndices.at(i));
                        }
                    }
                }
                instrIndex++;
            }

            blockGenSets.push_back(GEN);
            blockKillSets.push_back(KILL);

            // Check the vector of GEN
            errs() << "GEN: ";
            for (unsigned int i = 0; i < GEN.size(); i++) {
                errs() << GEN.at(i) << " ";
            }
            // Check the vector of KILL
            errs() << "\nKILL: ";
            for (unsigned int i = 0; i < KILL.size(); i++) {
                errs() << KILL.at(i) << " ";
            }
            errs() << "\n\n";

            blockNum++;
        }

        // Print the GEN and KILL sets for each block
        for (unsigned int i = 0; i < blockGenSets.size(); ++i) {
            errs() << "GEN for block #" << i << ": ";
            for (unsigned int j = 0; j < blockGenSets.at(i).size(); ++j) {
                errs() << blockGenSets.at(i).at(j) << " ";
            }
            errs() << "\nKILL for block #" << i << ": ";
            for (unsigned int k = 0; k < blockKillSets.at(i).size(); ++k) {
                errs() << blockKillSets.at(i).at(k) << " ";
            }
            errs() << "\n";
        }

        return true;
    }
}; // end of struct ReachingDefinition
} // end of anonymous namespace

char ReachingDefinition::ID = 0;
static RegisterPass<ReachingDefinition> X("ReachingDefinition", "Reaching Definition Pass",
                                          false /* Only looks at CFG */,
                                          true /* Analysis Pass */);
