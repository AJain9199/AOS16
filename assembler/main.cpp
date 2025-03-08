#include <iostream>
#include <Instruction.h>
#include <parse.h>

int main() {
    Parser::add_opcode("mov", 0x5, {REG | REG_PTR | IMM_PTR, REG|REG_PTR|IMM_PTR|IMMEDIATE});
    std::cout << "Hello, World!" << std::endl;
    return 0;
}