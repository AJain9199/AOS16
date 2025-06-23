#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <operand.h>
#include <vector>
#include <lexer.h>
#include <memory>

#define INS_REG     0x3
#define INS_REGPTR  0x2
#define INS_IMM     0x0
#define INS_IMMPTR  0x1

static uint8_t to_ins(const operandOptions op) {
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
struct __pragma( pack(push, 1) ) InstructionBytes {
    struct __pragma( pack(push, 1) ) InstructionWord {
        uint8_t opcode :    5;
        uint8_t reg :       3;
        uint8_t dir :       1;
        uint8_t type :      2;
        uint8_t mod :       1; // controls if memory access is RAM or ROM
        uint8_t relative :  1; // controls if the address is relative to the current address
        uint8_t reg2_hi :   1;
    } __pragma( pack(pop)) ins;
    uint16_t value;

    [[nodiscard]] bool has_immediate() const {
        return (ins.type == INS_IMM) || (ins.type == INS_IMMPTR);
    }

    [[nodiscard]] uint16_t get_instruction() const {
        return (ins.reg2_hi) | (ins.relative << 1) | (ins.mod << 2) | (ins.type << 3) | (ins.reg << 6) | (ins.dir << 5) | (ins.opcode << 9);
    }

    [[nodiscard]] uint16_t get_immediate() const {
        return value;
    }
} __pragma( pack(pop) );

/*
 * Stores information about a specific instruction
 */
class Instruction {
    uint8_t opcode;
    std::vector<operandOptions> operands;

public:
    Instruction(uint8_t opCode, const std::initializer_list<operandOptions> &operand_constraints);
    [[nodiscard]] std::unique_ptr<InstructionBytes> emit(std::vector<std::shared_ptr<Operand>> ops) const;
    int size(const std::vector<std::shared_ptr<Operand>>& operands);

    [[nodiscard]] unsigned long long no_operands() const {
        return operands.size();
    }

};

#endif //INSTRUCTION_H
