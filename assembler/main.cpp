#include <iostream>
#include <Instruction.h>
#include <parse.h>

int main() {
    add_opcode("mov", 0x5, {REG | REG_PTR | IMM_PTR, REG | REG_PTR | IMM_PTR | IMMEDIATE});
    add_register("ax", 0x0);

    Parser parser_engine("test.S");
    parser_engine.parse();

    parser_engine.write_machine_code("test.o");

    std::cout << "Hello, World!" << std::endl;
    return 0;
}