#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <operand.h>
#include <vector>
#include <lexer.h>
#include <memory>

#define INS_REG     0x0
#define INS_REGPTR  0x1
#define INS_IMM     0x2
#define INS_IMMPTR  0x3

static uint8_t to_ins(operandOptions op) {
    switch (op) {
        case REG:
            return INS_REG;
        case REG_PTR:
            return INS_REGPTR;
        case IMMEDIATE:
            return INS_IMM;
        case IMM_PTR:
            return INS_IMMPTR;
        default:
            return 0;
    }
}

/*
 * Instruction format for the architecture
 */
__attribute__((packed)) struct InstructionBytes {
    __attribute__((packed)) struct InstructionWord {
        uint8_t opcode :    5;
        uint8_t dir :       1;
        uint8_t reg :       3;
        uint8_t type :      2;
        uint8_t mod :       1; // controls if memory access is RAM or ROM
        uint8_t relative :  1; // controls if the address is relative to the current address
        uint8_t reg2_hi :   1;
        uint8_t zero :      2;
    } ins;
    uint16_t value;

    [[nodiscard]] bool has_immediate() const {
        return (ins.type == INS_IMM) || (ins.type == INS_IMMPTR);
    }

    [[nodiscard]] uint16_t get_instruction() const {
        return ins.opcode;
    }

    [[nodiscard]] uint16_t get_immediate() const {
        return value;
    }
};

/*
 * Stores information about a specific instruction
 */
class Instruction {
    uint8_t opcode;
    std::vector<operandOptions> operands;

public:
    Instruction(uint8_t opCode, const std::initializer_list<operandOptions> &operand_constraints);
    [[nodiscard]] std::unique_ptr<InstructionBytes> emit(std::vector<Operand> &ops) const;
    int size(std::vector<std::shared_ptr<Operand>> operands);
};

#endif //INSTRUCTION_H
