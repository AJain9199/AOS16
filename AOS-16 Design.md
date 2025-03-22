# AOS-16

## Features

-   16-bit data and address buses
-   Harvard architecture
-   GPU hardware double bufferring
-   Memory Mapped IO

## Instruction

-   5-bit Opcode for a total of 32 distinct instructions
-   Direction Bit: Determines whether contents move from operand 1 to 2 (0) or operand 2 to 1 (1)
-   Reg: 3-bit register select
-   M/C/Reg: 2 bits to control whether operand is an immediate, immediate pointer, register pointer or simply a register, for a total of 5 bits.
-   If the 'C' flag is set, the instruction must be 4 bytes long. Other instructions are 2 bytes long.

### Instruction Set

-   00000: ADD reg/m, m/imm16/reg
-   00001: AND reg/m, m/imm16/reg
-   00010: OR reg/m, m/imm16/reg
-   00011: NOR reg/m, m/imm16/reg
-   00100: XOR reg/m, m/imm16/reg
-   00101: NEG reg/m, m/imm16/reg
-   00110: NOT reg/m, m/imm16/reg
-   00111: RSH reg/m
-   01001: INC reg/m
-   01010: DEC reg/m
-   01011: CMP reg/m, m/imm16/reg
-   01100: MOV reg/m, m/imm16/reg
-   01101: PUSH m/imm16/reg
-   01110: POP m/reg
-   01111: SWP
-   10000: JMP imm16/reg
-   10001: JNZ imm16/reg
-   10010: JEQ imm16/reg
-   10011: JLT imm16/reg
-   10100: JLE imm16/reg
-   10101: JGT imm16/reg
-   10110: JGE imm16/reg
-   10111: HLT
-   11000: CALL imm16/reg/m
-   11001: RET
-   11010: INT imm4
-   11011: IRET
-   11100 - 11110: UNALLOCATED
-   11111: NOP (Data is all high in EEPROM upon erase)

## Registers

-   000: AX
-   001: BX
-   010: CX
-   011: DX
-   100: EX
-   101: FX
-   110: SP: Stack Pointer
-   111: STATUS

### STATUS REGISTER

The bits in the status register, from LSB to MSB:

-   0: GPU READY
-   1: ZF
-   2: CF
-   3: SF
-   4-15: unallocated

## Memory Map

0x0000...0x7FF7: GP RAM
0x7FF8...0x7FFF: MEMORY MAPPED IO
0x8FFF...0xFFFF: FRAMEBUFFER

## Control Word

-   0: MI: Memory Write
-   1: MO: Memory Out
-   2: MSL: Memory Select (0 for RAM, 1 for ROM)
-   3: MAI: Memory Address In
-   4: IRI: Instruction Register In
-   5...7: RSA[3]: Register Select for Bus A (see REGISTERS)
-   8: RGI: Register In
-   9: RGO: Register Out
-   10: AAI: ALU A In
-   11: ABI: ALU B In
-   12...15: ALF[4]: ALU Opcode
-   16: /ALO: ALU Out
-   17: PCE: Program Counter Enable
-   18: /PCL: Program Counter Load/Jump
-   19: PCO: Program Counter Out
-   20: HLT: Global Halt
-   21: MCR: Microcode Reset
-   22: SWAP: Swap the double buffer selection
