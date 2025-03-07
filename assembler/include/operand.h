#ifndef OPERAND_H
#define OPERAND_H

/*
 * Manage allowable operands for a specific instruction
 */

#include <cstdint>

typedef uint8_t operandOptions;

#define REG    0b0001
#define IMMEDIATE   0b0010
#define REG_PTR     0b0100
#define IMM_PTR     0b1000


struct Operand {
    uint8_t type;
    uint16_t value;

    Operand(const uint8_t type, const uint16_t value) : type(type), value(value) {};

    [[nodiscard]] bool satisfies(const uint8_t constraints) const {
        return constraints & type;
    }

    void make_pointer() {
        if (satisfies(IMMEDIATE | REG)) {
            type <<= 2;
        }
    }

    [[nodiscard]] bool is_immediate() const {
        return satisfies(IMMEDIATE | IMM_PTR);
    }
};

#endif //OPERAND_H
