#include <instruction.h>

Instruction::Instruction(const uint8_t opCode, const std::initializer_list<operandOptions> &operand_constraints) : opcode(opCode), operands(operand_constraints) {
}

std::unique_ptr<InstructionBytes> Instruction::emit(std::vector<std::shared_ptr<Operand>> ops) const {
    if (operands.size() != ops.size()) {
        throw std::runtime_error("Invalid number of operands");
    }

    for (int i = 0; i < ops.size(); i++) {
        if (!ops[i]->satisfies(operands[i])) {
            throw std::runtime_error("Invalid operand type");
        }
    }

    auto instr = std::make_unique<InstructionBytes>();
    instr->ins.opcode = opcode;

    if (ops.size() == 1) {
        instr->ins.type = to_ins(ops[0]->type);
        if (ops[0]->satisfies(REG | REG_PTR)) {
            instr->ins.reg = ops[0]->value;
        } else {
            instr->value = ops[0]->value;
        }
    } else if (ops.size() == 2) {
        if (ops[0]->type != REG && ops[1]->type != REG) {
            throw std::runtime_error("One operand must be a register.");
        }

        if (ops[0]->type == REG) {
            instr->ins.dir = RL;
            instr->ins.reg = ops[0]->value;
        } else {
            instr->ins.dir = LR;
            instr->ins.reg = ops[1]->value;

            std::swap(ops[0], ops[1]);
        }

        instr->ins.type = to_ins(ops[1]->type);
        if (ops[1]->satisfies(REG | REG_PTR)) {
            instr->ins.mod = ops[1]->value & (1 << 2);
            instr->ins.relative = ops[1]->value & (1 << 1);
            instr->ins.reg2_hi = ops[1]->value & 1;
        } else {
            instr->value = ops[1]->value;
        }

        instr->ins.mod = ops[1]->is_rom?1:0;
    }

    return instr;
}

int Instruction::size(const std::vector<std::shared_ptr<Operand>>& operands) {
    for (const auto &op : operands) {
        if (op->satisfies(IMMEDIATE | IMM_PTR)) {
            return 2;
        }
    }
    return 1;
}
