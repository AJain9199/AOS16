#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <Operand.h>
#include <vector>
#include <Lexer.h>

#define INS_REG     0x0
#define INS_REGPTR  0x1
#define INS_IMM     0x2
#define INS_IMMPTR  0x3

/*
 * Instruction format for the architecture
 */
struct InstructionBytes {
    uint8_t opcode :    5;
    uint8_t dir :       1;
    uint8_t reg :       3;
    uint8_t type :      2;
    uint8_t mod :       1;
    uint8_t reloc :     1;
    uint16_t value;

    [[nodiscard]] bool has_immediate() const {
        return (type == INS_IMM) || (type == INS_IMMPTR);
    }

    [[nodiscard]] uint8_t size() const {
        return has_immediate()?2:1;
    }
};

/*
 * Stores information about a specific instruction
 */
class Instruction {
    uint8_t opcode;
    std::vector<operandOptions> operands;

    Instruction(uint8_t opCode, const std::initializer_list<operandOptions> &operand_constraints);
    std::unique_ptr<InstructionBytes> emit(std::unique_ptr<Operand> op1, std::unique_ptr<Operand> op2);
};

#endif //INSTRUCTION_H
