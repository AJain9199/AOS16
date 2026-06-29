#include <iostream>
#include <filesystem>
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
//    add_opcode("movr", 0xe, MOV_LIKE);
    add_opcode("push", 0xf, SINGLE_ARG_ALL);
    add_opcode("pop", 0x10, SINGLE_ARG);
    add_opcode("swp", 0x11, NO_ARGS);
    add_opcode("jmp", 0x12, {REG | IMMEDIATE});
    add_opcode("jnz", 0x13, {REG | IMMEDIATE});
    add_opcode("call", 0x19, {REG | IMMEDIATE});
    add_opcode("ret", 0x1a, NO_ARGS);
}

static void usage(const char *prog) {
    std::cerr << "Usage: " << prog << " <input.S> [-o <output>]\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string input_file;
    std::string output_file;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
            output_file = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "Error: unknown flag '" << arg << "'\n";
            usage(argv[0]);
            return 1;
        } else {
            if (!input_file.empty()) {
                std::cerr << "Error: multiple input files specified\n";
                usage(argv[0]);
                return 1;
            }
            input_file = arg;
        }
    }

    if (output_file.empty()) {
        output_file = std::filesystem::path(input_file).stem().string() + ".o";
    }

    init_opcodes();
    add_register("ax", 0x0);
    add_register("bx", 0x1);
    add_register("cx", 0x2);
    add_register("dx", 0x3);
    add_register("ex", 0x4);
    add_register("fx", 0x5);
    add_register("sp", 0x6);
    add_register("fl", 0x7);

    Parser parser_engine(input_file);
    parser_engine.parse();
    parser_engine.write_machine_code(output_file);

    return 0;
}
