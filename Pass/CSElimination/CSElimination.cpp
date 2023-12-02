#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "CSElimination"

namespace {
struct Expression {
    string operand1;
    string operand2;
    string opcode;

    Expression(const Value* op1Value, const Value* op2Value, const unsigned opnum) {
      // Convert Value to a string, referenced this page:
      // https://llvm.org/doxygen/classllvm_1_1raw__string__ostream.html

      string temp1 = "";
      raw_string_ostream op1Stream(temp1);
      op1Value->print(op1Stream);
      string op1 = op1Stream.str();

      string temp2 = "";
      raw_string_ostream op2Stream(temp2);
      op2Value->print(op2Stream);
      string op2 = op2Stream.str();

      unsigned op1PercentIndex = op1.find("%", 3);  // Find index of second %
      unsigned op1CommaIndex = op1.find(",", op1PercentIndex);  // Find index of second ,
      operand1 = op1.substr(op1PercentIndex, op1CommaIndex - op1PercentIndex);
      // errs() << "op1PercentIndex: " << op1PercentIndex << "\n";
      // errs() << "op1CommaIndex: " << op1CommaIndex << "\n";

      unsigned op2PercentIndex = op2.find("%", 3);  // Find index of second %
      unsigned op2CommaIndex = op2.find(",", op2PercentIndex);  // Find index of second ,
      operand2 = op2.substr(op2PercentIndex, op2CommaIndex - op2PercentIndex);
      // errs() << "op2PercentIndex: " << op2PercentIndex << "\n";
      // errs() << "op2CommaIndex: " << op2CommaIndex << "\n";

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
    }

    void print() const {
      errs() << operand1 << " " << opcode << " " << operand2 << "\n";
    }
};

struct CSElimination : public FunctionPass {
  static char ID;
  CSElimination() : FunctionPass(ID) {}

  bool runOnFunction(Function& F) override {
    errs() << "\nFunction: " << F.getName() << "\n";

    unsigned blockNum = 0;
    // Vector blockGenSets = {{""}, {"a - e", "a + b"}, {"a + b"}, {""}}
    // Holds GEN sets for each basic block
    vector<vector<Expression*>> blockGenSets = {};

    for (auto& basic_block : F) { // Iterates over basic blocks of the function
      errs() << "Block " << blockNum++ << ":\n";

      vector<Expression*> currGenSet = {};  // Current block's GEN set
      for (auto& inst : basic_block) { // Iterates over instructions in a basic block
        // Find statements A = B op C where op is {+, -, *, /}
        if (inst.getOpcode() == Instruction::Add
            || inst.getOpcode() == Instruction::Sub
            || inst.getOpcode() == Instruction::Mul
            || inst.getOpcode() == Instruction::SDiv) {
          Value* op1 = inst.getOperand(0);
          Value* op2 = inst.getOperand(1);
          errs() << "  Found A = B op C: op1 is \'" << *op1 << "\', op2 is \'" << *op2 << "\', opcode " << inst.getOpcode() << "\n";

          // Both operands should look like '%22 = load i32, i32* %2, align 4'
          // If either operand is NOT a load, it's an immediate; ignore those
          if (isa<LoadInst>(op1) && isa<LoadInst>(op2)) {
              // Add expressions to this block's GEN set
              Expression* exp = new Expression(op1, op2, inst.getOpcode());
              currGenSet.push_back(exp);
          } else {
              errs() << "  Found A = B op C, but A or B is an immediate value.\n";
          }
        }
      }
      blockGenSets.push_back(currGenSet);
    }

    // Print GEN sets for each block
    errs() << "\n";
    for (unsigned i = 0; i < blockGenSets.size(); i++) {
      errs() << "Block " << i << " GEN set:\n";
      for (unsigned j = 0; j < blockGenSets.at(i).size(); j++) {
        errs() << "  ";
        blockGenSets.at(i).at(j)->print();
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
