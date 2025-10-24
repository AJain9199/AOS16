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
    add_opcode("nand", 0x3, MOV_LIKE);
    add_opcode("or", 0x4, MOV_LIKE);
    add_opcode("nor", 0x5, MOV_LIKE);
    add_opcode("xor", 0x6, MOV_LIKE);
    add_opcode("neg", 0x7, SINGLE_ARG);
    add_opcode("not", 0x8, SINGLE_ARG);
    add_opcode("shl", 0x9, SINGLE_ARG);
    add_opcode("inc", 0xa, SINGLE_ARG);
    add_opcode("dec", 0xb, SINGLE_ARG);
    add_opcode("cmp", 0xc, MOV_LIKE);
    add_opcode("mov", 0xd, MOV_LIKE);
    add_opcode("push", 0xe, SINGLE_ARG_ALL);
    add_opcode("pop", 0xf, SINGLE_ARG);
    add_opcode("swp", 0x10, NO_ARGS);
    add_opcode("jmp", 0x11, {REG | IMMEDIATE});
    add_opcode("jnz", 0x12, {REG | IMMEDIATE});
    add_opcode("call", 0x19, {REG | IMMEDIATE});
}

int main() {
    init_opcodes();
    add_register("ax", 0x0);
	add_register("bx", 0x1);
	add_register("cx", 0x2);
    add_register("dx", 0x3);
    add_register("ex", 0x4);

    Parser parser_engine("test.S");
    parser_engine.parse();

    parser_engine.write_machine_code("test.o");

    return 0;
}