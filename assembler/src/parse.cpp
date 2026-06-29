#include <iomanip>
#include <parse.h>
#include <algorithm>
#include <fstream>
#include <filesystem>

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

    string id = lexer.getIdentifier();
    lexer.getToken();
    if (lexer == PUNCTUATION && lexer == ':') {
        lexer.eat(':');
        define_label(id, current_address);
        return;
    }

    ranges::transform(id, id.begin(), [](const char c) { return tolower(c); });

    if (id == "dd") {
        parseDd();
        return;
    }

    if (!opcodes.contains(id)) {
        err("Unknown opcode");
    }

    if (opcodes[id]->no_operands() == 0) {
        add_machine_code(opcodes[id], {});
    } else {
        add_machine_code(opcodes[id], parseOperands());
    }
}

/*
 * Parses directives. Supported directives:
 * 1. .uint8/uint16 value [,value ...]
 * 2. .text / .data  — section declarations
 */
void Parser::parseDirective() {
    const string directive = lexer.eat_id();

    if (directive == "data") {
        if (current_section == Section::TEXT) {
            err(".data section must be declared before .text");
        }
        has_sections = true;
        current_section = Section::DATA;
        current_address = 2; // address 0-1 reserved for jmp header
        return;
    }

    if (directive == "text") {
        if (current_section == Section::TEXT) {
            err("duplicate .text section");
        }
        has_sections = true;
        current_section = Section::TEXT;
        current_address = 2 + section_word_count(data_code);
        return;
    }

    const vector<shared_ptr<Operand> > operands = parseOperands();

    if (directive == "uint8" || directive == "uint16") {
        for (const shared_ptr<Operand> &op: operands) {
            add_machine_code(op->value);
        }
    }
}

/*
 * Handles the dd pseudo-instruction.
 *   dd <file>       — appends raw binary file contents as 16-bit words (odd files zero-padded)
 *   dd val          — emits val as one 16-bit word
 *   dd N, v1...vN  — emits v1..vN as N 16-bit words
 */
void Parser::parseDd() {
    if (lexer == STRING) {
        const string s = lexer.getString();
        lexer.getToken();
        for (const char c : s) {
            add_machine_code(static_cast<uint16_t>(static_cast<uint8_t>(c)));
        }
        return;
    }

    if (lexer == '<') {
        const string path = lexer.read_angle_path();
        const auto resolved = (std::filesystem::path(lexer.getLocation().first).parent_path() / path).string();
        ifstream bin(resolved, ios::binary);
        if (!bin.is_open()) err("dd: cannot open file '" + path + "'");
        int hi;
        while ((hi = bin.get()) != EOF) {
            const int lo = bin.get();
            add_machine_code(static_cast<uint16_t>(
                (static_cast<uint8_t>(hi) << 8) | (lo == EOF ? 0 : static_cast<uint8_t>(lo))
            ));
        }
        return;
    }

    const vector<shared_ptr<Operand>> operands = parseOperands();
    if (operands.size() == 1) {
        add_machine_code(static_cast<uint16_t>(operands[0]->value));
    } else {
        const int n = operands[0]->value;
        if (static_cast<int>(operands.size()) - 1 != n) {
            err("dd: count does not match number of values");
        }
        for (int i = 1; i <= n; i++) {
            add_machine_code(static_cast<uint16_t>(operands[i]->value));
        }
    }
}

/*
 * Parses a list of operands of the form: op, op1, op2...
 */
std::vector<std::shared_ptr<Operand> > Parser::parseOperands() {
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
        lexer.eat(')');

        op->make_pointer();
        return op;
    }

    if (lexer == PUNCTUATION && lexer == '[') {
        lexer.eat('[');
        auto op = parseSubOperand();
        lexer.eat(']');

        op->make_pointer(true);
        return op;
    }

    return parseSubOperand();
}

/*
 * Parses labels, registers, and immediate values.
 */
std::shared_ptr<Operand> Parser::parseSubOperand() {
    if (lexer == NUM) {
        auto ret = make_shared<Operand>(IMMEDIATE, lexer.getNumber());
        lexer.getToken();
        return ret;
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
        auto s = lexer.eat_id();
        return make_shared<Operand>(REG, regtab[s]);
    }

    err("Unknown operand value");
}


/*
 * Parses a label definition. This method also resolves pending references to this label (if any).
 */
void Parser::define_label(const std::string &name, const int val) {
    symtab[name] = val;

    for (const shared_ptr<Operand> &op: future_resolution[name]) {
        op->value = val;
    }
}

void Parser::add_machine_code(const std::shared_ptr<Instruction> &instr,
                              const std::vector<std::shared_ptr<Operand> > &operands) {
    auto& target = (current_section == Section::DATA) ? data_code
                 : (current_section == Section::TEXT) ? text_code
                 : machine_code;
    target.emplace_back(instr, operands);
    current_address += instr->size(operands);
}

void Parser::add_machine_code(const uint16_t constant) {
    auto& target = (current_section == Section::DATA) ? data_code
                 : (current_section == Section::TEXT) ? text_code
                 : machine_code;
    target.emplace_back(constant);
    current_address += 1;
}

uint16_t Parser::section_word_count(const std::vector<MachineCodeInstance>& code) {
    uint16_t count = 0;
    for (const auto& item : code) {
        count += item.is_constant ? 1 : static_cast<uint16_t>(item.instruction->size(item.operands));
    }
    return count;
}

void Parser::emit_section(std::fstream& out, const std::vector<MachineCodeInstance>& code) {
    for (const auto& i : code) {
        if (i.is_constant) {
            out.put(i.constant >> 8);
            out.put(i.constant & 0xFF);
        } else {
            const std::unique_ptr<InstructionBytes> ins = i.instruction->emit(i.operands);
            uint16_t ins_bin = ins->get_instruction();
            out.put(ins_bin >> 8);
            out.put(ins_bin & 0xFF);
            if (ins->has_immediate() && !i.operands.empty()) {
                ins_bin = ins->get_immediate();
                out.put(ins_bin >> 8);
                out.put(ins_bin & 0xFF);
            }
        }
    }
}

void Parser::write_machine_code(const std::string &filename) const {
    fstream outfile;
    outfile.open(filename, ios_base::binary | ios_base::out);

    if (has_sections) {
        const uint16_t text_start = 2 + section_word_count(data_code);

        // jmp <text_start> — opcode 0x12, IMM type (INS_IMM=0), all other fields 0
        const uint16_t jmp_instr = static_cast<uint16_t>(0x12) << 9;
        outfile.put(jmp_instr >> 8);
        outfile.put(jmp_instr & 0xFF);
        outfile.put(text_start >> 8);
        outfile.put(text_start & 0xFF);

        emit_section(outfile, data_code);
        emit_section(outfile, text_code);
    } else {
        emit_section(outfile, machine_code);
    }

    outfile.close();
}

void add_register(const std::string &name, const int val) {
    regtab[name] = val;
}

void Parser::parse() {
    lexer.getToken();
    while (lexer != EOF_TOKEN) {
        parseStatement();
    }
}

void add_opcode(const std::string &name, const uint8_t opcode, const initializer_list<operandOptions> &operands) {
    opcodes[name] = make_shared<Instruction>(opcode, operands);
}
