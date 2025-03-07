#ifndef OPERAND_H
#define OPERAND_H

/*
 * Manage allowable operands for a specific instruction
 */

typedef uint8_t operandOptions;

#define REGISTER    0b0001;
#define IMMEDIATE   0b0010;
#define REG_PTR     0b0100;
#define IMM_PTR     0b1000;


struct Operand {
    uint8_t type;
    uint16_t value;

    [[nodiscard]] bool satisfies(const operandOptions constraints) const {
        return constraints & type;
    }
};

#endif //OPERAND_H
