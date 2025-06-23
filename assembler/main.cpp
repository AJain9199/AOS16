#include <iostream>
#include <Instruction.h>
#include <parse.h>

#define MOV_LIKE {REG | REG_PTR | IMM_PTR, REG | REG_PTR | IMM_PTR | IMMEDIATE}
#define SINGLE_ARG {REG|REG_PTR|IMM_PTR}
#define SINGLE_ARG_ALL {REG | REG_PTR | IMM_PTR | IMMEDIATE}
#define NO_ARGS {}

void init_opcodes() {
    add_opcode("add", 0x0, MOV_LIKE);
    add_opcode("sub", 0x1, MOV_LIKE);
    add_opcode("and", 0x2, MOV_LIKE);
    add_opcode("or", 0x3, MOV_LIKE);
    add_opcode("nor", 0x4, MOV_LIKE);
    add_opcode("xor", 0x5, MOV_LIKE);
    // add_opcode("neg", 0x6, SINGLE_ARG);
    add_opcode("not", 0x7, SINGLE_ARG);
    add_opcode("rsh", 0x8, SINGLE_ARG);
    add_opcode("inc", 0x9, SINGLE_ARG);
    add_opcode("dec", 0xa, SINGLE_ARG);
    add_opcode("cmp", 0xb, MOV_LIKE);
    add_opcode("mov", 0x6, MOV_LIKE);
    add_opcode("push", 0xd, SINGLE_ARG_ALL);
    add_opcode("pop", 0xe, SINGLE_ARG);
    add_opcode("swp", 0xf, NO_ARGS);
}

int main() {
    init_opcodes();
    add_register("ax", 0x0);

    Parser parser_engine("test.S");
    parser_engine.parse();

    parser_engine.write_machine_code("test.o");

    return 0;
}