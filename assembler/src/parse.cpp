#include <iomanip>
#include <parse.h>

using namespace std;


/*
 * Parses a statement in general form. There are three types of statements:
 * 1. Regular instruction (mov %ax, %bx)
 * 2. Directives (.byte 0x0)
 * 3. Labels (label:)
 */
void Parser::parseStatement() {
    if (lexer == PUNCTUATION) {
        lexer.eat('.');
        parseDirective();
        return;
    }

    const string id = lexer.getIdentifier();
    lexer.getToken();
    if (lexer == PUNCTUATION && lexer == ':') {
        lexer.eat(':');
        define_label(id, current_address);
    }

    add_machine_code(opcodes[id], parseOperands());
}

/*
 * Parses directives. Supported directives:
 * 1. .uint8/uint16 value [,value ...]
 */
void Parser::parseDirective() {
    const string directive = lexer.eat_id();
    const vector<shared_ptr<Operand>> operands = parseOperands();

    if (directive == "uint8" || directive == "uint16") {
        for (const shared_ptr<Operand> & op : operands) {
            add_machine_code(op->value);
        }
    }
}

/*
 * Parses a list of operands of the form: op, op1, op2...
 */
std::vector<std::shared_ptr<Operand>> Parser::parseOperands() {
    auto op = parseOperand();
    vector operands = {op};
    while (lexer == ',') {
        lexer.eat(',');
        operands.push_back(parseOperand());
    }
    return operands;
}

/* Parses an operand of the types:
 * 1. immediate value (1, 0x0, 0b1... or labels)
 * 2. register (%ax, %bx...)
 * 3. immediate pointer ((0x0), (0b1) or (label))
 * 4. register pointer ((%ax), (%bx)...)
 */
std::shared_ptr<Operand> Parser::parseOperand() {
    if (lexer == PUNCTUATION && lexer == '(') {
        lexer.eat('(');
        auto op = parseSubOperand();
        lexer.getToken();
        lexer.eat(')');

        op->make_pointer();
        return op;
    }

    return parseSubOperand();
}

/*
 * Parses labels, registers, and immediate values.
 */
std::shared_ptr<Operand> Parser::parseSubOperand() {
    if (lexer == NUM) {
        return make_shared<Operand>(IMMEDIATE, lexer.getNumber());
    }

    // label
    if (lexer == ID) {
        const string name = lexer.eat_id();
        auto op = make_shared<Operand>(IMMEDIATE, 0);
        if (symtab.contains(name)) {
            op->value = symtab[name];
        } else {
            future_resolution[name].push_back(op);
        }

        return op;
    }

    // register
    if (lexer == PUNCTUATION) {
        lexer.eat('%');
        return make_shared<Operand>(REG, regtab[lexer.eat_id()]);
    }

    err("Unknown operand value");
}



/*
 * Parses a label definition. This method also resolves pending references to this label (if any).
 */
void Parser::define_label(const std::string &name, const int val) {
    symtab[name] = val;

    for (const shared_ptr<Operand> &op : future_resolution[name]) {
        op->value = val;
    }
}

void Parser::add_machine_code(const std::shared_ptr<Instruction>& instr, const std::vector<std::shared_ptr<Operand>>& operands) {
    machine_code.emplace_back(instr, operands);
    current_address += instr->size(operands);
}

void Parser::add_machine_code(const uint16_t constant) {
    machine_code.emplace_back(constant);
    current_address += 1;
}

void Parser::write_machine_code(const std::string &filename) {
    fstream outfile;
    outfile.open(filename, ios_base::binary | ios_base::out);

    for (const auto &i : machine_code) {
        if (i.is_constant) {
            outfile << i.constant;
        } else {
            const std::unique_ptr<InstructionBytes> ins = i.instruction->emit(i.operands);
            uint16_t ins_bin = ins->get_instruction();
            outfile.put(ins_bin >> 8);
            outfile.put(ins_bin & 0xFF);
            if (ins->has_immediate()) {
                ins_bin = ins->get_immediate();
                outfile.put(ins_bin >> 8);
                outfile.put(ins_bin & 0xFF);
            }
        }
    }

    outfile.close();
}

void add_register(const std::string& name, const int val) {
    regtab[name] = val;
}

void Parser::parse() {
    while (lexer.getToken() != EOF_TOKEN) {
        parseStatement();
    }
}

void add_opcode(const std::string &name, const uint8_t opcode, const initializer_list<operandOptions> &operands) {
    opcodes[name] = make_shared<Instruction>(opcode, operands);
}


