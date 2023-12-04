#include "llvm/IR/BasicBlock.h"
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
#include <iostream>
#include <fstream>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "CSElimination"

namespace {
vector<unsigned> AddNumToVectorElements(vector<unsigned> vec, unsigned int val) {
    for (unsigned int i = 0; i < vec.size(); i++) {
        vec.at(i) = vec.at(i) + val;
    }
    return vec;
}

vector<unsigned> sortAndRemoveDuplicates(vector<unsigned> vec) {
    set<unsigned> vecAsSet(vec.begin(), vec.end());
    vector<unsigned> temp(vecAsSet.begin(), vecAsSet.end());
    return temp;
}

string GetInstrDestination(Instruction& instr, unsigned opNumber) {
    string temp = "";
    raw_string_ostream stream(temp);
    instr.print(stream);
    string instrString = stream.str();

    if (opNumber == 1) {
        unsigned instrPercentIndex = instrString.find("%", 0);
        unsigned instrEqualIndex = instrString.find(" =", instrPercentIndex); // Find index of " ="
        return instrString.substr(instrPercentIndex, instrEqualIndex - instrPercentIndex);
    }
    // Isolate operand for store instructions
    // For 'Store i32 %10, i32* %2, align 4 (store w/ destination:   %2 = alloca i32, align 4)', isolate %10
    else if (opNumber == 2) {
        unsigned instrPercentIndex = instrString.find("%", 0);              // Find index of first %
        unsigned instrCommaIndex = instrString.find(", ", instrPercentIndex); // Find index of second ,
        return instrString.substr(instrPercentIndex, instrCommaIndex - instrPercentIndex);
    }
    return "ERROR";
}

string GetValueOperand(const Value* value, unsigned opNumber) {
    // Convert Value to a string, referenced this page:
    // https://llvm.org/doxygen/classllvm_1_1raw__string__ostream.html
    string temp = "";
    raw_string_ostream opStream(temp);
    value->print(opStream);
    string op1 = opStream.str();

    string operand1 = "";

    // Isolate operand for assignment instructions
    // For '%10 = sub nsw i32 %8, %9', isolate %10
    if (opNumber == 1) {
        unsigned op1PercentIndex = op1.find("%", 0);              // Find index of first %
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
        vector<vector<Expression*>> blockGenSetsAvail = {};
        vector<vector<Expression*>> blockKilledSetsAvail = {};
        vector<vector<Expression*>> blockInSetsAvail = {};
        vector<vector<Expression*>> blockOutSetsAvail = {};
        unsigned blockNum = 0;

        // PASS 1: Create GEN sets for each block
        errs() << "PASS 1: Create GEN sets for each block\n";
        unsigned instructionIndex = 0;
        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            errs() << "Block " << blockNum++ << ":\n";

            vector<Expression*> currGenSet = {}; // Current block's GEN set

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
            blockGenSetsAvail.push_back(currGenSet);
        }
        errs() << "\n";

        // Print GEN sets for each block
        errs() << "Print GEN sets for each block:\n";
        for (unsigned i = 0; i < blockGenSetsAvail.size(); i++) {
            errs() << "Block " << i << " GEN set:\n";
            for (unsigned j = 0; j < blockGenSetsAvail.at(i).size(); j++) {
                errs() << "  ";
                blockGenSetsAvail.at(i).at(j)->print();
            }
        }
        errs() << "\n";

        blockNum = 0;

        // PASS 2: Create KILL sets for each block
        errs() << "PASS 2: Create KILL sets for each block\n";
        instructionIndex = 0;
        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            errs() << "Block " << blockNum << ":\n";

            vector<Expression*> currKilledSet = {}; // Current block's KILL set

            for (auto& inst : basic_block) { // Iterates over instructions in a basic block
                // Find statements A = ~ where A is an operand in this block's expressions
                if (inst.getOpcode() == Instruction::Store) {
                    string storeDestination = GetValueOperand(inst.getOperand(1), 1);
                    errs() << "  Found A = B op C where A is \'" << storeDestination << "\'\n";

                    // Check if an expression in this block used the same storeDestination as an operand
                    unsigned numGenExpsInBlock = blockGenSetsAvail.at(blockNum).size();
                    for (unsigned j = 0; j < numGenExpsInBlock; j++) {
                        Expression* genSetExpression = blockGenSetsAvail.at(blockNum).at(j);
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
            blockKilledSetsAvail.push_back(currKilledSet);
            blockNum++;
        }
        errs() << "\n";

        // Print KILL sets for each block
        errs() << "Print KILL sets for each block:\n";
        for (unsigned i = 0; i < blockKilledSetsAvail.size(); i++) {
            errs() << "Block " << i << " KILL set:\n";
            for (unsigned j = 0; j < blockKilledSetsAvail.at(i).size(); j++) {
                errs() << "  ";
                blockKilledSetsAvail.at(i).at(j)->print();
            }
        }
        errs() << "\n";

        // PASS 3: If exp1 kills exp2 in a block and exp1 comes after exp2, remove exp2 from block's GEN set
        // exp2 is not available at the end of B, so is not generated
        errs() << "PASS 3: Update GEN sets with KILL for each block\n";
        for (unsigned i = 0; i < blockGenSetsAvail.size(); i++) { // For each block
            errs() << "Updating GEN set for block " << i << "...\n";

            for (unsigned j = 0; j < blockGenSetsAvail.at(i).size(); j++) { // For each GEN expression in block
                Expression* genSetExpression = blockGenSetsAvail.at(i).at(j);
                errs() << "  Checking GEN expression: ";
                genSetExpression->print();

                for (unsigned k = 0; k < blockKilledSetsAvail.at(i).size(); k++) { // For each KILL expression in block
                    Expression* killedSetExpression = blockKilledSetsAvail.at(i).at(k);
                    // If expression is in GEN and KILL and KILL comes after GEN, exp doesn't reach end of block
                    if ((expsEqualWithoutIndex(*genSetExpression, *killedSetExpression)) && (genSetExpression->index < killedSetExpression->index)) {
                        errs() << "    Deleted: ";
                        genSetExpression->print();
                        blockGenSetsAvail.at(i).at(j) = new Expression("", "", "", -1); // Set block's GEN exp to empty
                    }
                }
            }
        }

        // Clean GEN sets for each block (remove empty Expressions)
        for (unsigned i = 0; i < blockGenSetsAvail.size(); i++) {
            for (unsigned j = 0; j < blockGenSetsAvail.at(i).size(); j++) {
                if(blockGenSetsAvail.at(i).at(j)->index == -1) {
                    // Move this element to the last and then pop_back
                    Expression* temp = blockGenSetsAvail.at(i).at(blockGenSetsAvail.at(i).size() - 1);
                    blockGenSetsAvail.at(i).at(blockGenSetsAvail.at(i).size() - 1) = blockGenSetsAvail.at(i).at(j);
                    blockGenSetsAvail.at(i).at(j) = temp;
                    blockGenSetsAvail.at(i).pop_back();
                }
            }
        }
        errs() << "\n";

        blockNum = 0;
        instructionIndex = 0;

        // PASS 4: Create IN and OUT sets for each block
        errs() << "PASS 4: Create IN and OUT sets for each block\n";
        for (auto& basic_block : F) { // Iterates over basic blocks of the function
            vector<Expression*> currInSet = {};
            vector<Expression*> currOutSet = {};
            
            
            // Initial block has no IN set
            if (blockNum == 0) {
                currInSet = {};
            }
            // Current block's IN set is the intersection of its predecessors' OUTs
            else {
                unsigned predecessorIndex = 0;
                for (auto* pred : predecessors(&basic_block)) {
                    unsigned predBlockNum = 0;
                    for (auto& predBlock : F) { // Find block number for predecessor
                        if (&predBlock == pred) {
                            break;
                        }
                        predBlockNum++;
                    }
                    // Start with all of the first predecessor's OUT expressions
                    if (predecessorIndex == 0) {
                        for (unsigned int i = 0; i < blockOutSetsAvail.at(predBlockNum).size(); i++) {
                            currInSet.push_back(blockOutSetsAvail.at(predBlockNum).at(i));
                        }
                    }
                    else {
                        // First predecessor's OUTs must be in all other predecessors' OUTs
                        for (unsigned int i = 0; i < currInSet.size(); i++) {
                            bool isInAllPredecessors = false;
                            for (unsigned int j = 0; j < blockOutSetsAvail.at(predBlockNum).size(); j++) {
                                if (expsEqualWithoutIndex(*currInSet.at(i), *blockOutSetsAvail.at(predBlockNum).at(j))) {
                                    isInAllPredecessors = true;
                                    break;
                                }
                            }
                            if (!isInAllPredecessors) {
                                // Delete expression from currInSet
                                // Send current expression to back, then pop
                                Expression* temp = currInSet.at(i);
                                currInSet.at(i) = currInSet.at(currInSet.size() - 1);
                                currInSet.at(currInSet.size() - 1) = temp;
                                currInSet.pop_back();
                            }
                        }
                    }
                    predecessorIndex++;
                }
            }

            // Update currKillSet to consider predecessors' OUT expressions
            vector<Expression*> currKilledSet = blockKilledSetsAvail.at(blockNum); // Current block's KILL set
            for (auto& inst : basic_block) { // Iterates over instructions in a basic block
                // Find statements A = ~ where A is an operand in this block's expressions
                if (inst.getOpcode() == Instruction::Store) {
                    string storeDestination = GetValueOperand(inst.getOperand(1), 1);
                    errs() << "  Found A = B op C where A is \'" << storeDestination << "\'\n";

                    // Check if an expression in this block used the same storeDestination as an IN's operand
                    for (unsigned j = 0; j < currInSet.size(); j++) {
                        Expression* inSetExpression = currInSet.at(j);
                        errs() << "    Checking IN expression " << j << " in block " << blockNum << "...\n";

                        // Does destination match either operand in this expression?
                        if (storeDestination == inSetExpression->operand1 || storeDestination == inSetExpression->operand2) {
                            errs() << "      Found match: ";
                            inSetExpression->print();

                            // Add the *killed* expression to this block's kill set
                            // Index of the killed expression is where it was killed
                            Expression* killedExp = new Expression(
                                inSetExpression->operand1,
                                inSetExpression->operand2,
                                inSetExpression->opcode,
                                instructionIndex);
                            currKilledSet.push_back(killedExp);
                        }
                    }
                }
                instructionIndex++;
            }

            blockKilledSetsAvail.at(blockNum) = currKilledSet;

            // OUT = (IN - KILL) + GEN
            // currOutSet = (currInSet - currKilledSet) + blockGenSets.at(blockNum);

            currOutSet = blockGenSetsAvail.at(blockNum);
            vector<Expression*> inMinusKill = {};

            // Find IN - KILL for current block
            for (unsigned int i = 0; i < currInSet.size(); i++) {
                bool shouldKill = false;
                for (unsigned int j = 0; j < currKilledSet.size(); j++) {
                    if (expsEqualWithoutIndex(*currInSet.at(i), *currKilledSet.at(j))) {
                        shouldKill = true;
                    }
                }
                // Save expressions in IN that are not in KILL
                if (!shouldKill) {
                    inMinusKill.push_back(currInSet.at(i));
                }
            }

            // Find GEN + (IN - KILL)
            for (unsigned int i = 0; i < inMinusKill.size(); i++) {
                bool alreadyExists = false;
                for (unsigned int j = 0; j < currOutSet.size(); j++) {
                    if (inMinusKill.at(i) == currOutSet.at(j)) {
                        alreadyExists = true;
                    }
                }
                if (!alreadyExists) {
                    currOutSet.push_back(inMinusKill.at(i));
                }
            }

            // Add IN and OUT sets for the current block
            blockInSetsAvail.push_back(currInSet);
            blockOutSetsAvail.push_back(currOutSet);
            blockNum++;
        }

        // ===============================
        //    FIND REACHING DEFINITIONS
        // ===============================

        errs() << "\nFinding Reaching Definitions for function: " << F.getName();

        vector<unsigned> storeInstructionIndices;
        vector<Value*> storeInstructionDestVars;
        unsigned instrIndex = 0;
        blockNum = 0;

        // First Pass: Get all of the indexes and destination variables of the store instructions
        for (auto &basic_block : F) {  // Iterates over basic blocks of the function 
            for (auto &inst : basic_block) {  // Iterates over instructions in a basic block
                if (inst.getOpcode() == Instruction::Store) {
                    Value* storeDestination = inst.getOperand(1);
                    storeInstructionIndices.push_back(instrIndex);
                    storeInstructionDestVars.push_back(storeDestination);
                }
                instrIndex++;
            }
            blockNum++;
        }

        instrIndex = 0;
        blockNum = 0;
        vector<vector<unsigned>> blockGenSetsReach = {};
        vector<vector<unsigned>> blockKillSetsReach = {};

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
            blockGenSetsReach.push_back(GEN);
            blockKillSetsReach.push_back(KILL);
            blockNum++;
        }

        vector<unsigned> setTemp = {};
        vector<vector<unsigned>> blockInSetsReach(blockNum, setTemp);
        vector<vector<unsigned>> blockOutSetsReach(blockNum, setTemp);

        instrIndex = 0;
        blockNum = 0;

        // Create the IN and OUT set for each block
        for (auto &basic_block : F) {
            vector<unsigned> IN = {};
            vector<unsigned> OUT = {};

            // Initial block has no IN
            if (blockNum == 0) {
                IN = {};
            }
            else {
                // Use the block's predecessors for IN
                for (auto *pred: predecessors(&basic_block)) {
                    unsigned predBlockNum = 0;
                    for (auto &funcBlock : F) {
                        // Find block number for predecessor
                        if (&funcBlock == pred) {
                            break;
                        }
                        predBlockNum++;
                    }
                    // IN is the OUT of the predecessor
                    for (unsigned int i = 0; i < blockOutSetsReach.at(predBlockNum).size(); i++) {
                        IN.push_back(blockOutSetsReach.at(predBlockNum).at(i));
                    }
                }
            }

            // Convert GEN, KILL, IN, OUT to sets to sort and remove duplicates
            set<unsigned> currGen(blockGenSetsReach.at(blockNum).begin(), blockGenSetsReach.at(blockNum).end());
            set<unsigned> currKill(blockKillSetsReach.at(blockNum).begin(), blockKillSetsReach.at(blockNum).end());
            set<unsigned> currIn(IN.begin(), IN.end());
            set<unsigned> currOut(OUT.begin(), OUT.end());

            // OUT = (IN - KILL) + GEN; result is stored in currOut
            set_difference(currIn.begin(), currIn.end(), currKill.begin(), currKill.end(), inserter(currOut, currOut.begin()));
            set_union(currGen.begin(), currGen.end(), currOut.begin(), currOut.end(), inserter(currOut, currOut.begin()));
            
            // Turn OUT back into a vector
            vector<unsigned> temp(currOut.begin(), currOut.end());
            OUT = temp;

            // Update IN and OUT for the block
            blockInSetsReach.at(blockNum) = IN;
            blockOutSetsReach.at(blockNum) = OUT;

            blockNum++;
        }

        // Print IN, OUT, GEN, KILL for each block's reaching definitions
        for (unsigned int i = 0; i < blockGenSetsReach.size(); ++i) {
            errs() << "\nBlock " << i << " reaching definitions:";
            errs() << "\n  IN: ";
            blockInSetsReach.at(i) = sortAndRemoveDuplicates(blockInSetsReach.at(i));
            for (unsigned int j = 0; j < blockInSetsReach.at(i).size(); ++j) {
                errs() << blockInSetsReach.at(i).at(j) << " ";
            }
            errs() << "\n  GEN: ";
            blockGenSetsReach.at(i) = sortAndRemoveDuplicates(blockGenSetsReach.at(i));
            for (unsigned int m = 0; m < blockGenSetsReach.at(i).size(); ++m) {
                errs() << blockGenSetsReach.at(i).at(m) << " ";
            }
            errs() << "\n  KILL: ";
            blockKillSetsReach.at(i) = sortAndRemoveDuplicates(blockKillSetsReach.at(i));
            for (unsigned int n = 0; n < blockKillSetsReach.at(i).size(); ++n) {
                errs() << blockKillSetsReach.at(i).at(n) << " ";
            }
            errs() << "\n  OUT: ";
            blockOutSetsReach.at(i) = sortAndRemoveDuplicates(blockOutSetsReach.at(i));
            for (unsigned int k = 0; k < blockOutSetsReach.at(i).size(); ++k) {
                errs() << blockOutSetsReach.at(i).at(k) << " ";
            }
            errs() << "\n";
        }
        
        // ===============================
        //    END REACHING DEFINITIONS
        // ===============================

        // Print all IN, GEN, KILL, and OUT sets for every block's available expressions
        errs() << "\nAvailable Expressions for each block:";
        for (unsigned i = 0; i < blockInSetsAvail.size(); ++i) {
            errs() << "\nBlock " << i << " available expressions:";
            errs() << "\n  IN:\n";
            for (unsigned j = 0; j < blockInSetsAvail.at(i).size(); ++j) {
                errs() << "    ";
                blockInSetsAvail.at(i).at(j)->print();
            }
            errs() << "  GEN:\n";
            for (unsigned j = 0; j < blockGenSetsAvail.at(i).size(); ++j) {
                errs() << "    ";
                blockGenSetsAvail.at(i).at(j)->print();
            }
            errs() << "  KILL:\n";
            for (unsigned j = 0; j < blockKilledSetsAvail.at(i).size(); ++j) {
                errs() << "    ";
                blockKilledSetsAvail.at(i).at(j)->print();
            }
            errs() << "  OUT:\n";
            for (unsigned j = 0; j < blockOutSetsAvail.at(i).size(); ++j) {
                errs() << "    ";
                blockOutSetsAvail.at(i).at(j)->print();
            }
        }
        errs() << "\n";

        blockNum = 0;
        instructionIndex = 0;

        // PASS 5: transformation for CSElimination
        errs() << "PASS 5: Transform for CSElimination\n";
        vector<unsigned int> changeTheseLines = {};

        for (auto& basic_block : F) {
            for (auto& inst : basic_block) {
                // If statement is A = B op C in block S
                if (inst.getOpcode() == Instruction::Add || inst.getOpcode() == Instruction::Sub || inst.getOpcode() == Instruction::Mul || inst.getOpcode() == Instruction::SDiv) {
                    Value* op1 = inst.getOperand(0);
                    Value* op2 = inst.getOperand(1);
                    //errs() << "  Found A = B op C: op1 is \'" << *op1 << "\', op2 is \'" << *op2 << "\', opcode " << inst.getOpcode() << "\n";

                    // Both operands should look like '%22 = load i32, i32* %2, align 4'
                    // If either operand is NOT a load, it's an immediate; ignore those
                    if (isa<LoadInst>(op1) && isa<LoadInst>(op2)) {
                        // Save the expression
                        Expression* exp = new Expression(op1, op2, inst.getOpcode(), instructionIndex);

                        // Is the expression available at entry of this block?
                        bool expIsAvailableAtEntry = false;
                        string availDestination = "";
                        unsigned int availIndex = 0;
                        vector<string> availDestinations = {};

                        // For every available expression that matches, add its' destination to the vector availIndex
                        for (unsigned i = 0; i < blockInSetsAvail.at(blockNum).size(); ++i) {
                            if (expsEqualWithoutIndex(*exp, *blockInSetsAvail.at(blockNum).at(i))) {
                                expIsAvailableAtEntry = true;
                                availIndex = blockInSetsAvail.at(blockNum).at(i)->index;

                                // Get the destination of the expression from the in set
                                unsigned innerInstrIndex = 0;
                                for (auto& basic_block : F) {
                                    for (auto& inst : basic_block) {
                                        // Find the line that matches the index then
                                        if (innerInstrIndex == availIndex) {
                                            availDestination = GetInstrDestination(inst, 1);
                                            availDestinations.push_back(availDestination);
                                        }
                                        innerInstrIndex++;
                                    }
                                }
                                break;
                            }
                        }


                        if (expIsAvailableAtEntry) {
                            errs() << "Looking for ";
                            for (unsigned int i = 0; i < availDestinations.size(); i++) {
                                errs() << availDestinations.at(i) << " ";
                            }
                            vector<unsigned> defsReachingBlock = blockInSetsReach.at(blockNum);  // Indices of instructions
                            errs() << "Block " << blockNum << "\n";
                            // Look at store instruction and grab the location of the value
                            // ex: 11: store i32 %10, i32* %2, align 4, isolate %10
                            for (unsigned i = 0; i < defsReachingBlock.size(); ++i) {
                                unsigned innerInstrIndex = 0;
                                for (auto& basic_block : F) {
                                    for (auto& inst : basic_block) {
                                        if (innerInstrIndex == defsReachingBlock.at(i)) {
                                            errs() << "   " << innerInstrIndex << ": " << inst << "\n";
                                            string reachingValue = GetInstrDestination(inst, 2);
                                            for (unsigned int i = 0; i < availDestinations.size(); i++) {
                                                errs() << "      Compare " << reachingValue << " with " << availDestinations.at(i) << "\n";
                                                if (reachingValue == availDestinations.at(i)) {
                                                    errs() << "******Found " << innerInstrIndex << ": " << inst << "\n";
                                                    changeTheseLines.push_back(innerInstrIndex);
                                                }
                                            }
                                        }
                                        innerInstrIndex++;
                                    }
                                }
                            }
                        }

                    } else {
                        errs() << "  Found A = B op C, but A or B is an immediate value.\n";
                    }

                }
                instructionIndex++;
            }
            blockNum++;
        }

        // Pass 6: Change the lines
        unsigned innerInstrIndex = 0;
        ofstream outputFile("optimizedCode.txt");
        for (unsigned int i = 0; i < changeTheseLines.size(); i++) {
            outputFile << "  %tmp" << i << " = alloca i32, align 4\n";
            changeTheseLines = AddNumToVectorElements(changeTheseLines, 1);
            innerInstrIndex++;
        }

        int lineChangedXTimes = 0;
        int currVariableNum = 0;
        for (auto& basic_block : F) {
            for (auto& instr : basic_block) {
                string temp = "";
                raw_string_ostream stream(temp);
                instr.print(stream);
                string instrString = stream.str();
                for (unsigned int i = 0; i < changeTheseLines.size(); i++) {
                    if (changeTheseLines.at(i) == innerInstrIndex) {
                        // Fix store line
                        unsigned instrStringPercentIndex = instrString.find("%", 13);             // Find index of second %
                        unsigned instrStringCommaIndex = instrString.find(",", instrStringPercentIndex); // Find index of second ,
                        instrString.replace(instrStringPercentIndex, instrStringCommaIndex - instrStringPercentIndex, "%tmp" + to_string(i));
                        outputFile << instrString << "\n";


                        //Add load line
                        outputFile <<  "  %" << ++currVariableNum << " = load i32, i32* %tmp" << to_string(i) << ", align 4\n";
                        lineChangedXTimes++;
                        break;
                    }
                }
                if (lineChangedXTimes > 0 && (isa<LoadInst>(instr) || isa<AllocaInst>(instr)) || isa<BinaryOperator>(instr) || isa<ICmpInst>(instr)) {
                        unsigned instrStringPercentIndex = instrString.find("%", 0);             // Find index of second %
                        unsigned instrStringCommaIndex = instrString.find(" =", instrStringPercentIndex); // Find index of second =
                        currVariableNum = stoi(instrString.substr(instrStringPercentIndex + 1, instrStringCommaIndex - instrStringPercentIndex));
                        errs() << to_string(currVariableNum) << " ";

                        instrStringPercentIndex = instrString.find("%", 0);             // Find index of second %
                        instrStringCommaIndex = instrString.find(" =", instrStringPercentIndex); // Find index of second =
                        instrString.replace(instrStringPercentIndex, instrStringCommaIndex - instrStringPercentIndex, "%" + to_string(currVariableNum + lineChangedXTimes));
                        outputFile << instrString << "\n";
                }
                else {
                    outputFile << instrString << "\n";
                }

                innerInstrIndex++;
            }
        }

        return true; // Indicate this is a Transform pass
    }
}; // end of struct CSElimination
}  // end of anonymous namespace

char CSElimination::ID = 0;
static RegisterPass<CSElimination> X("CSElimination", "CSElimination Pass",
                                     false /* Only looks at CFG */,
                                     true /* Tranform Pass */);
