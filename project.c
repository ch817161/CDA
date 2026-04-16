#include "spimcore.h"

/* ALU */
void ALU(unsigned A, unsigned B, char ALUControl, unsigned *ALUresult, char *Zero)
{
    // Apply operations based on the 3-bit ALUControl signal [cite: 162]
    switch (ALUControl) {
        case 0: *ALUresult = A + B; break; // Add
        case 1: *ALUresult = A - B; break; // Subtract
        case 2: *ALUresult = ((int)A < (int)B) ? 1 : 0; break; // Set less than (signed)
        case 3: *ALUresult = (A < B) ? 1 : 0; break; // Set less than (unsigned)
        case 4: *ALUresult = A & B; break; // AND
        case 5: *ALUresult = A | B; break; // OR
        case 6: *ALUresult = B << 16; break; // Load upper immediate: Shift left extended 16 bits
        case 7: *ALUresult = ~A; break; // NOT A
    }

    // Assign Zero to 1 if the result is zero; otherwise, assign 0 [cite: 160]
    *Zero = (*ALUresult == 0) ? 1 : 0;
}

/* instruction fetch */
int instruction_fetch(unsigned PC, unsigned *Mem, unsigned *instruction)
{
    // Halt if not word-aligned or out of 64kB range [cite: 98, 111]
    if (PC % 4 != 0 || PC > 65536) 
        return 1;

    // Convert byte address to word index for Mem array [cite: 24, 35]
    *instruction = Mem[PC / 4]; 
    return 0;
}

/* instruction partition */
void instruction_partition(unsigned instruction, unsigned *op, unsigned *r1, unsigned *r2, unsigned *r3, unsigned *funct, unsigned *offset, unsigned *jsec)
{
    // Extract fields using bitmasking and shifting [cite: 175, 176]
    *op     = (instruction >> 26) & 0x3F;
    *r1     = (instruction >> 21) & 0x1F;
    *r2     = (instruction >> 16) & 0x1F;
    *r3     = (instruction >> 11) & 0x1F;
    *funct  = instruction & 0x3F;
    *offset = instruction & 0xFFFF;
    *jsec   = instruction & 0x3FFFFFF;
}

/* instruction decode */
int instruction_decode(unsigned op, struct_controls *controls)
{
    // Initialize all to 0/disabled first [cite: 182]
    controls->RegDst = 0; controls->Jump = 0; controls->Branch = 0;
    controls->MemRead = 0; controls->MemtoReg = 0; controls->ALUOp = 0;
    controls->MemWrite = 0; controls->ALUSrc = 0; controls->RegWrite = 0;

    switch (op) {
        case 0x00: // R-type
            controls->RegDst = 1; controls->RegWrite = 1; controls->ALUOp = 7; break;
        case 0x08: // addi
            controls->RegWrite = 1; controls->ALUSrc = 1; controls->ALUOp = 0; break;
        case 0x23: // lw
            controls->MemRead = 1; controls->RegWrite = 1; controls->ALUSrc = 1; controls->MemtoReg = 1; break;
        case 0x2b: // sw (Use 2 for "don't cares") [cite: 184]
            controls->MemWrite = 1; controls->RegDst = 2; controls->ALUSrc = 1; controls->MemtoReg = 2; break;
        case 0x04: // beq (Use 2 for "don't cares") [cite: 184]
            controls->Branch = 1; controls->ALUOp = 1; controls->RegDst = 2; controls->MemtoReg = 2; break;
        case 0x02: // j (Use 2 for "don't cares") [cite: 184]
            controls->Jump = 1; controls->RegDst = 2; controls->MemtoReg = 2; break;
        case 0x0f: // lui
            controls->RegWrite = 1; controls->ALUSrc = 1; controls->ALUOp = 6; break;
        case 0x0a: // slti
            controls->RegWrite = 1; controls->ALUSrc = 1; controls->ALUOp = 2; break;
        case 0x0b: // sltiu
            controls->RegWrite = 1; controls->ALUSrc = 1; controls->ALUOp = 3; break;
        default: return 1; // Illegal instruction [cite: 108]
    }
    return 0;
}

/* Read Register */
void read_register(unsigned r1, unsigned r2, unsigned *Reg, unsigned *data1, unsigned *data2)
{
    *data1 = Reg[r1];
    *data2 = Reg[r2];
}

/* Sign Extend */
void sign_extend(unsigned offset, unsigned *extended_value)
{
    // Handle two's complement extension [cite: 191]
    if ((offset >> 15) & 1) 
        *extended_value = offset | 0xFFFF0000;
    else 
        *extended_value = offset & 0x0000FFFF;
}

/* ALU operations */
int ALU_operations(unsigned data1, unsigned data2, unsigned extended_value, unsigned funct, char ALUOp, char ALUSrc, unsigned *ALUresult, char *Zero)
{
    char control = ALUOp;
    if (ALUOp == 7) { // R-type decoding [cite: 52, 54]
        switch (funct) {
            case 0x20: control = 0; break; // add
            case 0x22: control = 1; break; // sub
            case 0x24: control = 4; break; // and
            case 0x25: control = 5; break; // or
            case 0x2a: control = 2; break; // slt
            case 0x2b: control = 3; break; // sltu
            default: return 1;
        }
    }
    // MUX for ALU input B [cite: 58]
    ALU(data1, (ALUSrc == 1) ? extended_value : data2, control, ALUresult, Zero);
    return 0;
}

/* Read / Write Memory */
int rw_memory(unsigned ALUresult, unsigned data2, char MemWrite, char MemRead, unsigned *memdata, unsigned *Mem)
{
    if (MemRead == 1 || MemWrite == 1) {
        // Halt if address is not word-aligned or out of bounds [cite: 56, 110]
        if (ALUresult % 4 != 0 || ALUresult > 0xFFFF) return 1; 
        if (MemRead == 1) *memdata = Mem[ALUresult >> 2];
        if (MemWrite == 1) Mem[ALUresult >> 2] = data2;
    }
    return 0;
}

/* Write Register */
void write_register(unsigned r2, unsigned r3, unsigned memdata, unsigned ALUresult, char RegWrite, char RegDst, char MemtoReg, unsigned *Reg)
{
    if (RegWrite == 1) {
        // MUX for destination register [cite: 42, 43]
        unsigned dest = (RegDst == 1) ? r3 : r2;
        // MUX for data to write [cite: 165]
        if (dest !=0) {
            Reg[dest] = (MemtoReg == 1) ? memdata : ALUresult;
        }
    }
}

/* PC update */
void PC_update(unsigned jsec, unsigned extended_value, char Branch, char Jump, char Zero, unsigned *PC)
{
    unsigned next_PC = *PC + 4;
    
    if (Jump == 1) {
        // Concatenate upper 4 bits of PC+4 with 26-bit jsec << 2 [cite: 232]
        *PC = (next_PC & 0xF0000000) | (jsec << 2);
    } else if (Branch == 1 && Zero == 1) {
        // Branch target is (PC+4) + (offset << 2) [cite: 130]
        *PC = next_PC + (extended_value << 2);
    } else {
        *PC = next_PC;
    }
}

