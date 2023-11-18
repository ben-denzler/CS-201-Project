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

vector<unsigned> sortAndRemoveDuplicates(vector<unsigned> vec) {
    set<unsigned> vecAsSet(vec.begin(), vec.end());
    vector<unsigned> temp(vecAsSet.begin(), vecAsSet.end());
    return temp;
}

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
                errs() << instrIndex << ": " << inst;

                if (inst.getOpcode() == Instruction::Store) {
                    Value* storeDestination = inst.getOperand(1);
                    errs() << " (store w/ destination: " << *storeDestination << ")";
                    storeInstructionIndices.push_back(instrIndex);
                    storeInstructionDestVars.push_back(storeDestination);
                }
                errs() << "\n";
                instrIndex++;
            }
            errs() << "\n";
        }

        instrIndex = 0;
        blockNum = 0;
        vector<vector<unsigned>> blockGenSets = {};
        vector<vector<unsigned>> blockKillSets = {};

        // Second Pass: Add to GEN and KILL sets
        for (auto &basic_block : F) {  // Iterates over basic blocks of the function 
            vector<vector<unsigned>> GEN_KILLSETS = {};
            vector<unsigned> GEN = {};
            vector<unsigned> KILL = {};

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
            blockNum++;
        }

        vector<unsigned> setTemp = {};
        vector<vector<unsigned>> blockInSets(blockNum, setTemp);
        vector<vector<unsigned>> blockOutSets(blockNum, setTemp);

        instrIndex = 0;
        blockNum = 0;

        // Create the IN and OUT set for each block
        for (auto &basic_block : F) {
            vector<unsigned> IN = {};
            vector<unsigned> OUT = {};

            // Initial block
            if (blockNum == 0) {
                IN = {};
            }
            else {
                for (auto *pred: predecessors(&basic_block)) {
                    // Calculate IN, UNION all OUTs of predecessors
                    unsigned predBlockNum = 0;
                    for (auto &funcBlock : F) {
                        if (&funcBlock == pred) {
                            break;
                        }
                        predBlockNum++;
                    }
                    for (unsigned int i = 0; i < blockOutSets.at(predBlockNum).size(); i++) {
                        IN.push_back(blockOutSets.at(predBlockNum).at(i));
                    }
                }
            }

            set<unsigned> currGen(blockGenSets.at(blockNum).begin(), blockGenSets.at(blockNum).end());
            set<unsigned> currKill(blockKillSets.at(blockNum).begin(), blockKillSets.at(blockNum).end());
            set<unsigned> currIn(IN.begin(), IN.end());
            set<unsigned> currOut(OUT.begin(), OUT.end());
            // OUT = blockGenSets.at(blockNum) + (IN - blockKillSets.at(blockNum))
            set_difference(currIn.begin(), currIn.end(), currKill.begin(), currKill.end(), inserter(currOut, currOut.begin()));
            set_union(currGen.begin(), currGen.end(), currOut.begin(), currOut.end(), inserter(currOut, currOut.begin()));
            
            // Cast out from a set to a vector
            vector<unsigned> temp(currOut.begin(), currOut.end());
            OUT = temp;

            // Update sets
            blockInSets.at(blockNum) = IN;
            blockOutSets.at(blockNum) = OUT;

            blockNum++;
        }

        // Print IN, OUT, GEN, KILL for each block
        for (unsigned int i = 0; i < blockGenSets.size(); ++i) {
            errs() << "\nBlock " << i << ":";

            errs() << "\n  IN: ";
            blockInSets.at(i) = sortAndRemoveDuplicates(blockInSets.at(i));
            for (unsigned int j = 0; j < blockInSets.at(i).size(); ++j) {
                errs() << blockInSets.at(i).at(j) << " ";
            }

            errs() << "\n  OUT: ";
            blockOutSets.at(i) = sortAndRemoveDuplicates(blockOutSets.at(i));
            for (unsigned int k = 0; k < blockOutSets.at(i).size(); ++k) {
                errs() << blockOutSets.at(i).at(k) << " ";
            }

            errs() << "\n  GEN: ";
            blockGenSets.at(i) = sortAndRemoveDuplicates(blockGenSets.at(i));
            for (unsigned int m = 0; m < blockGenSets.at(i).size(); ++m) {
                errs() << blockGenSets.at(i).at(m) << " ";
            }

            errs() << "\n  KILL: ";
            blockKillSets.at(i) = sortAndRemoveDuplicates(blockKillSets.at(i));
            for (unsigned int n = 0; n < blockKillSets.at(i).size(); ++n) {
                errs() << blockKillSets.at(i).at(n) << " ";
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
