#include "spimcore.h"


/* ALU */
/* 10 Points */
void ALU(unsigned A,unsigned B,char ALUControl,unsigned *ALUresult,char *Zero)
{
    // Apply operations based on the 3-bit ALUControl signal 
    switch (ALUControl) {
        case 0: // 000: Z = A + B 
            *ALUresult = A + B;
            break;
        case 1: // 001: Z = A - B 
            *ALUresult = A - B;
            break;
        case 2: // 010: if A < B, Z = 1; otherwise, Z = 0 (Signed) 
            if ((int)A < (int)B)
                *ALUresult = 1;
            else
                *ALUresult = 0;
            break;
        case 3: // 011: if A < B, Z = 1; otherwise, Z = 0 (Unsigned) 
            if (A < B)
                *ALUresult = 1;
            else
                *ALUresult = 0;
            break;
        case 4: // 100: Z = A AND B 
            *ALUresult = A & B;
            break;
        case 5: // 101: Z = A OR B 
            *ALUresult = A | B;
            break;
        case 6: // 110: Shift B left by 16 bits 
            *ALUresult = B << 16;
            break;
        case 7: // 111: Z = NOT A 
            *ALUresult = ~A;
            break;
    }

    // Assign Zero to 1 if the result is zero; otherwise, assign 0 [cite: 155]
    if (*ALUresult == 0)
        *Zero = 1;
    else
        *Zero = 0;

}

/* instruction fetch */
/* 10 Points */
int instruction_fetch(unsigned PC,unsigned *Mem,unsigned *instruction)
{
    // Check for word alignment: PC must be a multiple of 4
    // Also ensure PC is within the 64kB memory range (0x0000 to 0xFFFF)
    if (PC % 4 != 0 || PC > 0xFFFF) {
        return 1; // Halt: improper alignment or out of bounds
    }

    // Since memory is an array of words, index is PC / 4
    // Each index in Mem[] holds a 32-bit MIPS instruction
    *instruction = Mem[PC >> 2];

    return 0; // Success
}


/* instruction partition */
/* 10 Points */
void instruction_partition(unsigned instruction, unsigned *op, unsigned *r1,unsigned *r2, unsigned *r3, unsigned *funct, unsigned *offset, unsigned *jsec)
{
    // Extracting bits using bitmasking and shifting as per MIPS standard
    // Opcode: bits 31-26
    *op = (instruction & 0xFC000000) >> 26;

    // Register 1 (rs): bits 25-21
    *r1 = (instruction & 0x03E00000) >> 21;

    // Register 2 (rt): bits 20-16
    *r2 = (instruction & 0x001F0000) >> 16;

    // Register 3 (rd): bits 15-11
    *r3 = (instruction & 0x0000F800) >> 11;

    // Function code: bits 5-0 (used for R-type instructions)
    *funct = (instruction & 0x0000003F);

    // Offset: bits 15-0 (used for I-type/immediate instructions)
    *offset = (instruction & 0x0000FFFF);

    // Jump target: bits 25-0 (used for J-type instructions)
    *jsec = (instruction & 0x03FFFFFF);
}



/* instruction decode */
/* 15 Points */
int instruction_decode(unsigned op,struct_controls *controls)
{
// Initialize all controls to 0 (disabled) to start fresh
    controls->RegDst = 0;
    controls->Jump = 0;
    controls->Branch = 0;
    controls->MemRead = 0;
    controls->MemtoReg = 0;
    controls->ALUOp = 0;
    controls->MemWrite = 0;
    controls->ALUSrc = 0;
    controls->RegWrite = 0;

    switch (op) {
        case 0x00: // R-type (add, sub, and, or, slt, sltu)
            controls->RegDst = 1;
            controls->RegWrite = 1;
            controls->ALUOp = 7; // 111: Look at funct field
            break;

        case 0x08: // addi
            controls->ALUSrc = 1;
            controls->RegWrite = 1;
            controls->ALUOp = 0; // 000: Addition
            break;

        case 0x23: // lw (load word)
            controls->ALUSrc = 1;
            controls->MemtoReg = 1;
            controls->RegWrite = 1;
            controls->MemRead = 1;
            controls->ALUOp = 0; // 000: Addition for address
            break;

        case 0x2b: // sw (store word)
            controls->RegDst = 2; // Don't care
            controls->ALUSrc = 1;
            controls->MemtoReg = 2; // Don't care
            controls->MemWrite = 1;
            controls->ALUOp = 0; // 000: Addition for address
            break;

        case 0x04: // beq (branch on equal)
            controls->RegDst = 2; // Don't care
            controls->Branch = 1;
            controls->MemtoReg = 2; // Don't care
            controls->ALUOp = 1; // 001: Subtraction for comparison
            break;

        case 0x0f: // lui (load upper immediate)
            controls->ALUSrc = 1;
            controls->RegWrite = 1;
            controls->ALUOp = 6; // 110: Shift left 16
            break;

        case 0x0a: // slti (set less than immediate)
            controls->ALUSrc = 1;
            controls->RegWrite = 1;
            controls->ALUOp = 2; // 010: Set less than
            break;

        case 0x0b: // sltiu (set less than immediate unsigned)
            controls->ALUSrc = 1;
            controls->RegWrite = 1;
            controls->ALUOp = 3; // 011: Set less than unsigned
            break;

        case 0x02: // j (jump)
            controls->Jump = 1;
            controls->RegDst = 2; // Don't care
            controls->MemtoReg = 2; // Don't care
            controls->ALUOp = 0; // Don't care
            break;

        default:
            return 1; // Halt: Illegal instruction opcode
    }

    return 0; // Success
}

/* Read Register */
/* 5 Points */
void read_register(unsigned r1,unsigned r2,unsigned *Reg,unsigned *data1,unsigned *data2)
{
// Simply fetch the values stored in the registers indexed by r1 and r2
    // These values are then passed to the ALU or Memory units
    *data1 = Reg[r1];
    *data2 = Reg[r2];
}


/* Sign Extend */
/* 10 Points */
void sign_extend(unsigned offset,unsigned *extended_value)
{
// Extract the sign bit (the 16th bit of the offset)
    unsigned sign = (offset >> 15) & 0x1;

    if (sign == 1) {
        // If negative, fill the upper 16 bits with 1s
        *extended_value = offset | 0xFFFF0000;
    } else {
        // If positive, fill the upper 16 bits with 0s
        *extended_value = offset & 0x0000FFFF;
    }
}

/* ALU operations */
/* 10 Points */
int ALU_operations(unsigned data1,unsigned data2,unsigned extended_value,unsigned funct,char ALUOp,char ALUSrc,unsigned *ALUresult,char *Zero)
{
unsigned inputB;
    char finalALUControl = ALUOp;

    // ALUSrc determines if the second ALU input is data2 or the immediate value
    if (ALUSrc == 1) {
        inputB = extended_value;
    } else {
        inputB = data2;
    }

    // If ALUOp is 7 (111), this is an R-type instruction
    // We must decode the funct field to get the correct ALU signal
    if (ALUOp == 7) {
        switch (funct) {
            case 0x20: // add
                finalALUControl = 0; // 000
                break;
            case 0x22: // sub
                finalALUControl = 1; // 001
                break;
            case 0x24: // and
                finalALUControl = 4; // 100
                break;
            case 0x25: // or
                finalALUControl = 5; // 101
                break;
            case 0x2a: // slt (signed)
                finalALUControl = 2; // 010
                break;
            case 0x2b: // sltu (unsigned)
                finalALUControl = 3; // 011
                break;
            default:
                return 1; // Halt: Unknown funct code
        }
    }

    // Call the ALU function we wrote earlier to perform the operation
    ALU(data1, inputB, finalALUControl, ALUresult, Zero);

    return 0; // Success
}

/* Read / Write Memory */
/* 10 Points */
int rw_memory(unsigned ALUresult,unsigned data2,char MemWrite,char MemRead,unsigned *memdata,unsigned *Mem)
{
// If we are reading or writing, we must check for alignment and bounds
    if (MemRead == 1 || MemWrite == 1) {
        // Address must be a multiple of 4 and within the 64kB range
        if (ALUresult % 4 != 0 || ALUresult > 0xFFFF) {
            return 1; // Halt: improper alignment or out of bounds
        }

        // Perform memory read
        if (MemRead == 1) {
            *memdata = Mem[ALUresult >> 2];
        }

        // Perform memory write
        if (MemWrite == 1) {
            Mem[ALUresult >> 2] = data2;
        }
    }

    return 0; // Success
}


/* Write Register */
/* 10 Points */
void write_register(unsigned r2,unsigned r3,unsigned memdata,unsigned ALUresult,char RegWrite,char RegDst,char MemtoReg,unsigned *Reg)
{
// Only perform write if RegWrite is enabled
    if (RegWrite == 1) {
        unsigned dest_reg;
        unsigned data_to_write;

        // Multiplexer for Destination Register
        if (RegDst == 1) {
            dest_reg = r3; // R-type
        } else {
            dest_reg = r2; // I-type
        }

        // Multiplexer for Data to Write
        if (MemtoReg == 1) {
            data_to_write = memdata; // Loading from memory
        } else {
            data_to_write = ALUresult; // Result from ALU
        }

        Reg[dest_reg] = data_to_write;
    }
}

/* PC update */
/* 10 Points */
void PC_update(unsigned jsec,unsigned extended_value,char Branch,char Jump,char Zero,unsigned *PC)
{
// 1. Standard increment: Move to the next word address (PC + 4)
    // All branches are relative to this "next" PC
    unsigned next_PC = *PC + 4;

    // 2. Handle Branch (e.g., beq)
    // If Branch signal is 1 and ALU result was zero (Zero == 1)
    if (Branch == 1 && Zero == 1) {
        // Offset is shifted left by 2 because it's a word offset
        // Then added to the already incremented next_PC (PC + 4)
        *PC = next_PC + (extended_value << 2);
    } 
    // 3. Handle Jump
    else if (Jump == 1) {
        // Jump target is formed by taking the top 4 bits of (PC + 4)
        // and concatenating the 26-bit jsec shifted left by 2
        *PC = (next_PC & 0xF0000000) | (jsec << 2);
    } 
    // 4. Default move
    else {
        *PC = next_PC;
    }
}

