#ifndef PARSE_H
#define PARSE_H

#include <Instruction.h>
#include <Operand.h>
#include <map>
#include <memory>
#include <lexer.h>
#include <string>


/*
 * Container class for parsing the assembly code
 */
class Parser {
    struct MachineCodeInstance {
        bool is_constant = false;
        uint16_t constant = 0;

        std::shared_ptr<Instruction> instruction;
        std::vector<std::shared_ptr<Operand>> operands;

        explicit MachineCodeInstance(const uint16_t constant) : is_constant(true), constant(constant) {}
        MachineCodeInstance(const std::shared_ptr<Instruction> &instr, const std::vector<std::shared_ptr<Operand>> &operands) : instruction(instr), operands(operands) {}
    };

    std::vector<MachineCodeInstance> machine_code;

    void add_machine_code(std::shared_ptr<Instruction> instr, std::vector<std::shared_ptr<Operand>> operands);
    void add_machine_code(uint16_t constant);

    static std::map<std::string, int> regtab;
    static void add_register(const std::string& name, int val);

    static std::map<std::string, std::shared_ptr<Instruction>> opcodes;
    static void add_opcode(const std::string &name, uint8_t opcode, const std::initializer_list<unsigned char> &operands);

    std::map<std::string, int> symtab;
    void define_label(const std::string &name, int val);

    // labels not yet defined in the symbol table are registered for "future resolution"
    // define_label() will resolve these labels
    std::map<std::string, std::vector<std::shared_ptr<Operand>>> future_resolution;

    uint16_t current_address = 0;

    // parser methods
    void parseStatement();
    void parseDirective();
    std::vector<std::shared_ptr<Operand>> parseOperands();
    std::shared_ptr<Operand> parseOperand();
    std::shared_ptr<Operand> parseSubOperand();

    void err(const std::string &msg) {
        error(lexer.getLocation(), msg);
    }

    Lexer lexer;
};

#endif //PARSE_H
