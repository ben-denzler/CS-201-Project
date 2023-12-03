#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
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
string GetValueOperand(const Value* value, unsigned opNumber) {
    // Convert Value to a string, referenced this page:
    // https://llvm.org/doxygen/classllvm_1_1raw__string__ostream.html
    string temp = "";
    raw_string_ostream opStream(temp);
    value->print(opStream);
    string op1 = opStream.str();

    string operand1 = "";

    // Isolate operand for store instructions
    if (opNumber == 1) {
        unsigned op1PercentIndex = op1.find("%", 0);              // Find index of second %
        unsigned op1CommaIndex = op1.find(" =", op1PercentIndex); // Find index of second ,
        operand1 = op1.substr(op1PercentIndex, op1CommaIndex - op1PercentIndex);
    }
    // Isolate operand for load instructions
    // For ''%6 = load i32, i32* %b, align 4', isolate %b
    else if (opNumber == 2) {
        unsigned op1PercentIndex = op1.find("%", 3);             // Find index of second %
        unsigned op1CommaIndex = op1.find(",", op1PercentIndex); // Find index of second ,
        operand1 = op1.substr(op1PercentIndex, op1CommaIndex - op1PercentIndex);
    }

    return operand1;
}

struct Expression {
    string operand1;
    string operand2;
    string opcode;
    unsigned index;

    // Overload equality operator to compare Expressions
    // Indices can be different
    bool operator==(const Expression& exp) const {
        return (operand1 == exp.operand1) && (operand2 == exp.operand2) && (opcode == exp.opcode) && (index == exp.index);
    }

    // For set operations
    bool operator<(const Expression& exp) const {
        return index < exp.index;
    }

    Expression(string op1Value, string op2Value, string opnum, unsigned loc) {
        operand1 = op1Value;
        operand2 = op2Value;
        opcode = opnum;
        index = loc;
    }

    Expression(const Value* op1Value, const Value* op2Value, const unsigned opnum, unsigned loc) {
        operand1 = GetValueOperand(op1Value, 2);
        operand2 = GetValueOperand(op2Value, 2);

        if (opnum == 13) {
            opcode = "+";
        } else if (opnum == 15) {
            opcode = "-";
        } else if (opnum == 17) {
            opcode = "*";
        } else if (opnum == 20) {
            opcode = "/";
        } else {
            errs() << "Something bad happened! :(\n";
            errs() << "opnum wasn't what we expected. It was: " << opnum << "\n";
            exit(1);
        }

        index = loc;
    }

    void print() const {
        errs() << operand1 << " " << opcode << " " << operand2 << " @ index " << index << "\n";
    }
};

bool expsEqualWithoutIndex(const Expression& exp1, const Expression& exp2) {
    return (exp1.operand1 == exp2.operand1) && (exp1.operand2 == exp2.operand2) && (exp1.opcode == exp2.opcode);
}

struct CSElimination : public FunctionPass {
    static char ID;
    CSElimination() : FunctionPass(ID) {}

    bool runOnFunction(Function& F) override {
        errs() << "\nFunction: " << F.getName() << "\n";

        // Vectors look like {{""}, {"a - e", "a + b"}, {"a + b"}, {""}}
        vector<vector<Expression*>> blockGenSets = {};
        vector<vector<Expression*>> blockKilledSets = {};
        vector<vector<Expression*>> blockInSets = {};
        vector<vector<Expression*>> blockOutSets = {};
        unsigned blockNum = 0;

        // PASS 1: Create GEN sets for each block
        errs() << "PASS 1: Create GEN sets for each block\n";
        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            errs() << "Block " << blockNum++ << ":\n";

            vector<Expression*> currGenSet = {}; // Current block's GEN set
            unsigned instructionIndex = 0;

            for (auto& inst : basic_block) { // Iterates over instructions in a basic block
                // Find statements A = B op C where op is {+, -, *, /}
                if (inst.getOpcode() == Instruction::Add || inst.getOpcode() == Instruction::Sub || inst.getOpcode() == Instruction::Mul || inst.getOpcode() == Instruction::SDiv) {
                    Value* op1 = inst.getOperand(0);
                    Value* op2 = inst.getOperand(1);
                    errs() << "  Found A = B op C: op1 is \'" << *op1 << "\', op2 is \'" << *op2 << "\', opcode " << inst.getOpcode() << "\n";

                    // Both operands should look like '%22 = load i32, i32* %2, align 4'
                    // If either operand is NOT a load, it's an immediate; ignore those
                    if (isa<LoadInst>(op1) && isa<LoadInst>(op2)) {
                        // Add expressions to this block's GEN set
                        Expression* exp = new Expression(op1, op2, inst.getOpcode(), instructionIndex);
                        currGenSet.push_back(exp);
                    } else {
                        errs() << "  Found A = B op C, but A or B is an immediate value.\n";
                    }
                }
                instructionIndex++;
            }
            blockGenSets.push_back(currGenSet);
        }
        errs() << "\n";

        // Print GEN sets for each block
        errs() << "Print GEN sets for each block:\n";
        for (unsigned i = 0; i < blockGenSets.size(); i++) {
            errs() << "Block " << i << " GEN set:\n";
            for (unsigned j = 0; j < blockGenSets.at(i).size(); j++) {
                errs() << "  ";
                blockGenSets.at(i).at(j)->print();
            }
        }
        errs() << "\n";

        blockNum = 0;

        // PASS 2: Create KILL sets for each block
        errs() << "PASS 2: Create KILL sets for each block\n";
        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            errs() << "Block " << blockNum << ":\n";

            vector<Expression*> currKilledSet = {}; // Current block's KILL set
            unsigned instructionIndex = 0;

            for (auto& inst : basic_block) { // Iterates over instructions in a basic block
                // Find statements A = ~ where A is an operand in this block's expressions
                if (inst.getOpcode() == Instruction::Store) {
                    string storeDestination = GetValueOperand(inst.getOperand(1), 1);
                    errs() << "  Found A = B op C where A is \'" << storeDestination << "\'\n";

                    // Check if an expression in this block used the same storeDestination as an operand
                    unsigned numGenExpsInBlock = blockGenSets.at(blockNum).size();
                    for (unsigned j = 0; j < numGenExpsInBlock; j++) {
                        Expression* genSetExpression = blockGenSets.at(blockNum).at(j);
                        errs() << "    Checking GEN expression " << j << " in block " << blockNum << "...\n";

                        // FIXME: currently only looking at current block GEN set, not IN set of predecessors
                        // Does destination match either operand in this expression?
                        if (storeDestination == genSetExpression->operand1 || storeDestination == genSetExpression->operand2) {
                            errs() << "      Found match: ";
                            genSetExpression->print();

                            // Add the *killed* expression to this block's kill set
                            // Index of the killed expression is where it was killed
                            Expression* killedExp = new Expression(
                                genSetExpression->operand1,
                                genSetExpression->operand2,
                                genSetExpression->opcode,
                                instructionIndex);
                            currKilledSet.push_back(killedExp);
                        }
                    }
                }
                instructionIndex++;
            }
            blockKilledSets.push_back(currKilledSet);
            blockNum++;
        }
        errs() << "\n";

        // Print KILL sets for each block
        errs() << "Print KILL sets for each block\n";
        for (unsigned i = 0; i < blockKilledSets.size(); i++) {
            errs() << "Block " << i << " KILL set:\n";
            for (unsigned j = 0; j < blockKilledSets.at(i).size(); j++) {
                errs() << "  ";
                blockKilledSets.at(i).at(j)->print();
            }
        }
        errs() << "\n";

        // PASS 3: If exp1 kills exp2 in a block and exp1 comes after exp2, remove exp2 from block's GEN set
        // exp2 is not available at the end of B, so is not generated
        errs() << "PASS 3: Update GEN sets with KILL for each block\n";
        for (unsigned i = 0; i < blockGenSets.size(); i++) { // For each block
            errs() << "Updating GEN set for block " << i << "...\n";

            for (unsigned j = 0; j < blockGenSets.at(i).size(); j++) { // For each GEN expression in block
                Expression* genSetExpression = blockGenSets.at(i).at(j);
                errs() << "  Checking GEN expression: ";
                genSetExpression->print();

                for (unsigned k = 0; k < blockKilledSets.at(i).size(); k++) { // For each KILL expression in block
                    Expression* killedSetExpression = blockKilledSets.at(i).at(k);
                    // If expression is in GEN and KILL and KILL comes after GEN, exp doesn't reach end of block
                    if ((expsEqualWithoutIndex(*genSetExpression, *killedSetExpression)) && (genSetExpression->index < killedSetExpression->index)) {
                        errs() << "    Deleted: ";
                        genSetExpression->print();
                        blockGenSets.at(i).at(j) = new Expression("", "", "", -1); // Set block's GEN exp to empty
                    }
                }
            }
        }

        // Clean GEN sets for each block (remove empty Expressions)
        for (unsigned i = 0; i < blockGenSets.size(); i++) {
            for (unsigned j = 0; j < blockGenSets.at(i).size(); j++) {
                if(blockGenSets.at(i).at(j)->index == -1) {
                    // Move this element to the last and then pop_back
                    Expression* temp = blockGenSets.at(i).at(blockGenSets.at(i).size() - 1);
                    blockGenSets.at(i).at(blockGenSets.at(i).size() - 1) = blockGenSets.at(i).at(j);
                    blockGenSets.at(i).at(j) = temp;
                    blockGenSets.at(i).pop_back();
                }
            }
        }
        errs() << "\n";

        blockNum = 0;

        // PASS 4: Create IN and OUT sets for each block
        errs() << "PASS 4: Create IN and OUT sets for each block\n";
        for (auto& basic_block : F) { // Iterates over basic blocks of the function

            vector<Expression*> currInSet = {};
            vector<Expression*> currOutSet = {};
            unsigned numPredecessors = 0;
            
            // Initial block has no IN
            if (blockNum == 0) {
                currInSet = {};
            }
            // Create currInSet using the Intersection of predecessor's OUTs
            else {
                for (auto* pred : predecessors(&basic_block)) {
                    unsigned predBlockNum = 0;
                    for (auto& predBlock : F) { // Find block number for predecessor
                        if (&predBlock == pred) {
                            break;
                        }
                        predBlockNum++;
                    }
                    // Create OutSets
                    if (numPredecessors == 0) {
                        for (unsigned int i = 0; i < blockOutSets.at(predBlockNum).size(); i++) {
                            currInSet.push_back(blockOutSets.at(predBlockNum).at(i));
                        }
                    }
                    else {
                        for (unsigned int i = 0; i < currInSet.size(); i++) {
                            bool isInBothOuts = false;
                            for (unsigned int j = 0; j < blockOutSets.at(predBlockNum).size(); j++) {
                                if (expsEqualWithoutIndex(*currInSet.at(i), *blockOutSets.at(predBlockNum).at(j))) {
                                    isInBothOuts = true;
                                    break;
                                }
                            }   
                            if (!isInBothOuts) {
                                // delete expression from currInSet
                                Expression* temp = currInSet.at(i);
                                currInSet.at(i) = currInSet.at(currInSet.size() - 1);
                                currInSet.at(currInSet.size() - 1) = temp;
                                currInSet.pop_back();
                            }
                        }
                    }
                    
                    numPredecessors++;
                }
            }

            // OUT = (IN - KILL) + GEN
            // currOutSet = (currInSet - blockKilledSets.at(blockNum)) + blockGenSets.at(blockNum);

            set<Expression*> currGen(blockGenSets.at(blockNum).begin(), blockGenSets.at(blockNum).end());
            set<Expression*> currKill(blockKilledSets.at(blockNum).begin(), blockKilledSets.at(blockNum).end());
            set<Expression*> currIn(currInSet.begin(), currInSet.end());
            set<Expression*> currOut(currOutSet.begin(), currOutSet.end());

            set_difference(currIn.begin(), currIn.end(), currKill.begin(), currKill.end(), inserter(currOut, currOut.begin()));
            set_union(currGen.begin(), currGen.end(), currOut.begin(), currOut.end(), inserter(currOut, currOut.begin()));

            vector<Expression*> outResult(currOut.begin(), currOut.end());
            currOutSet = outResult;

            // Add IN and OUT set for that particular block
            blockInSets.push_back(currInSet);
            blockOutSets.push_back(currOutSet);
            blockNum++;
        }

        for (unsigned i = 0; i < blockInSets.size(); ++i) {
            errs() << "\nBlock " << i << " IN set:\n";
            for (unsigned j = 0; j < blockInSets.at(i).size(); ++j) {
                errs() << "  ";
                blockInSets.at(i).at(j)->print();
            }
            errs() << "\nBlock " << i << " GEN set:\n";
            for (unsigned j = 0; j < blockGenSets.at(i).size(); ++j) {
                errs() << "  ";
                blockGenSets.at(i).at(j)->print();
            }
            errs() << "\nBlock " << i << " KILL set:\n";
            for (unsigned j = 0; j < blockKilledSets.at(i).size(); ++j) {
                errs() << "  ";
                blockKilledSets.at(i).at(j)->print();
            }
            errs() << "\nBlock " << i << " OUT set:\n";
            for (unsigned j = 0; j < blockOutSets.at(i).size(); ++j) {
                errs() << "  ";
                blockOutSets.at(i).at(j)->print();
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
