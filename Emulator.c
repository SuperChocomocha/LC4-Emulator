/*------------------------------------------------------------------------------
 * Max Guo
 * November 25, 2011
 * LC4 Emulator
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 * TODO: - CMPU and CMPIU do not work as intended
 *       - LDR and STR can read and write to code segments
 *       - 32-bit printf negative
 *       - doesn't quit if in script
 *       - segfaults on empty input (if user just presses Enter)
 *----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 * USAGE: - compiled using gcc Emulator.c -o Emulator
 *        
 *        - look at
 *              http://www.cis.upenn.edu/~milom/cse240-Fall06/pennsim/pennsim-dist.html
 *              http://www.cis.upenn.edu/~milom/cse240-Fall06/pennsim/pennsim-guide.html
 *              http://www.cis.upenn.edu/~milom/cse240-Fall06/pennsim/pennsim-manual.html
 *
 *              to see how the original PennSim works
 *
 *        - the load command is "ld" in PennSim, but "load" for this emulator
 *
 *        - many times it will say
 *              "Error: attempting to access memory outside of code"
 *              when the continue command is issued, this is NOT a bug as
 *              PennSim would also give an error, HOWEVER if PennSim DOES NOT
 *              give an error when the emulator DOES, then that IS a bug as
 *              the emulator should mimic PennSim
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#define LITTLE_ONE(I) (((I) << 4) | ((I) >> 4)) /* parsing little endian */
#define LITTLE_TWO(I) (((I) << 8) | ((I) >> 8))
#define PARSE_FOUR_OP(I) ((I) >> 12) /* parsing ops */
#define PARSE_FIVE_OP(I) ((I) >> 11)
#define PARSE_SEVEN_OP(I) ((I) >> 9)
#define PARSE_5_3(I) (((I) >> 3) & 7)
#define PARSE_11_9(I) (((I) >> 9) & 7)
#define PARSE_8_6(I) (((I) >> 6) & 7)
#define PARSE_2_0(I) ((I) & 7)
#define PARSE_BRANCH_CONST_IMM(I) ((I) & 511) /* parsing branch and CONST imm */
#define PARSE_ARITH_LOGIC_IMM_SOP(I) (((I) >> 5) & 1) /* parsing arithmetic and logic operations */
#define PARSE_ARITH_LOGIC_IMM(I) ((I) & 31)
#define PARSE_CMP_SOP(I) (((I) >> 7) & 2) /* parsing compare operations */
#define PARSE_CMP_IMM(I) ((I) & 127)
#define PARSE_6(I) ((I) & 63) /* parsing load store */
#define PARSE_8(I) ((I) & 255) /* parsing HICONST */
#define PARSE_SHIFT_SOP(I) (((I) >> 4) & 3) /* parsing shifting */
#define PARSE_4(I) ((I) & 15)
#define PARSE_11(I) ((I) & 2047) /* parsing JSR and JMP */
#define PARSE_SIGN_9(I) (((I) >> 8) & 1) /* parsing to test for sign */

/* registers, memory, breakpoints */
signed int registers[8];
unsigned short PSR = 0x8002;
char *NZP_CC = "Z";
unsigned short PC = 0x8200;
unsigned short memory[65536];
unsigned short breakpoints[65536];

/* PennSim commands */
char reset_command[] = "reset";
char load_command[] = "load";
char set_command[] = "set";
char break_command[] = "break";
char step_command[] = "step";
char continue_command[] = "continue";
char print_command[] = "print";
char script_command[] = "script";
char quit_command[] = "quit";

/* update NZP */
void update_NZP(unsigned short register_num) {
	if ((signed short)registers[register_num] < 0) {
		NZP_CC = "N";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0004;
		} else {
			PSR = (PSR & 0x0000) | 0x0004;
		}
	}
	if ((signed short)registers[register_num] == 0) {
		NZP_CC = "Z";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0002;
		} else {
			PSR = (PSR & 0x0000) | 0x0002;
		}
	}
	if ((signed short)registers[register_num] > 0) {
		NZP_CC = "P";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0001;
		} else {
			PSR = (PSR & 0x0000) | 0x0001;
		}
	}
}

/* LC4 commands */

/* branch commands */
void BRn(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 4) == 4) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRnz(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 4) == 4 || (PSR & 2) == 2) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRnp(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 4) == 4 || (PSR & 1) == 1) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRz(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 2) == 2) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRzp(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 2) == 2 || (PSR & 1) == 1) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRp(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	if ((PSR & 1) == 1) {
		PC = PC + 1 + sign_extended_num;
	} else {
		PC++;
	}
}

void BRnzp(signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	PC = PC + 1 + sign_extended_num;
}

/* arithmetic commands */
void ADD_r(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((signed short)registers[s]) + ((signed short)registers[t]);
	update_NZP(d);
}

void MUL(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((signed short)registers[s]) * ((signed short)registers[t]);
	update_NZP(d);
}

void SUB(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((signed short)registers[s]) - ((signed short)registers[t]);
	update_NZP(d);
}

void DIV(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((signed short)registers[s]) / ((signed short)registers[t]);
	update_NZP(d);
}

void ADD_imm(unsigned short d, unsigned short s, signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 4) & 1) == 1) {
		sign_extended_num = imm | 0xffe0;
	}
	registers[d] = ((signed short)registers[s]) + sign_extended_num;
	update_NZP(d);
}

/* compare commands */
void CMP(unsigned short s, unsigned short t) {
	if (((signed short)registers[s]) < ((signed short)registers[t])) {
		NZP_CC = "N";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0004;
		} else {
			PSR = (PSR & 0x0000) | 0x0004;
		}
	}
	if (((signed short)registers[s]) == ((signed short)registers[t])) {
		NZP_CC = "Z";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0002;
		} else {
			PSR = (PSR & 0x0000) | 0x0002;
		}
	}
	if (((signed short)registers[s]) > ((signed short)registers[t])) {
		NZP_CC = "P";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0001;
		} else {
			PSR = (PSR & 0x8000) | 0x0001;
		}
	}
}

void CMPU(unsigned short s, unsigned short t) {
	if (((unsigned short)registers[s]) < (unsigned short)registers[t]) {
		NZP_CC = "N";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0004;
		} else {
			PSR = (PSR & 0x0000) | 0x0004;
		}
	}
	if (((unsigned short)registers[s]) == ((unsigned short)registers[t])) {
		NZP_CC = "Z";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0002;
		} else {
			PSR = (PSR & 0x0000) | 0x0002;
		}
	}
	if (((unsigned short)registers[s]) > ((unsigned short)registers[t])) {
		NZP_CC = "P";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0001;
		} else {
			PSR = (PSR & 0x8000) | 0x0001;
		}
	}
}

void CMPI(unsigned short s, signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 6) & 1) == 1) {
		sign_extended_num = imm | 0xff80;
	}
	if (((signed short)registers[s]) < sign_extended_num) {
		NZP_CC = "N";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0004;
		} else {
			PSR = (PSR & 0x0000) | 0x0004;
		}
	}
	if (((signed short)registers[s]) == sign_extended_num) {
		NZP_CC = "Z";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0002;
		} else {
			PSR = (PSR & 0x0000) | 0x0002;
		}
	}
	if (((signed short)registers[s]) > sign_extended_num) {
		NZP_CC = "P";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0001;
		} else {
			PSR = (PSR & 0x8000) | 0x0001;
		}
	}
}

void CMPIU(unsigned short s, unsigned short imm) {
	if (((unsigned short)registers[s]) < imm) {
		NZP_CC = "N";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0004;
		} else {
			PSR = (PSR & 0x0000) | 0x0004;
		}
	}
	if (((unsigned short)registers[s]) == imm) {
		NZP_CC = "Z";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0002;
		} else {
			PSR = (PSR & 0x0000) | 0x0002;
		}
	}
	if (((unsigned short)registers[s]) > imm) {
		NZP_CC = "P";
		if ((PSR >> 15) == 1) {
			PSR = (PSR & 0x8000) | 0x0001;
		} else {
			PSR = (PSR & 0x8000) | 0x0001;
		}
	}
}

/* JSR */
void JSR(signed short imm) {
	registers[7] = PC + 1;
	PC = (PC & 0x8000) | (imm << 4);
	NZP_CC = "P";
	if ((PSR >> 15) == 1) {
		PSR = (PSR & 0x8000) | 0x0001;
	} else {
		PSR = (PSR & 0x8000) | 0x0001;
	}
}

void JSRR(unsigned short num) {
	if (num == 7) {
		unsigned short temp = registers[num];
		registers[7] = PC + 1;
		PC = temp;
	} else {
		registers[7] = PC + 1;
		PC = registers[num];
	}
	NZP_CC = "P";
	if ((PSR >> 15) == 1) {
		PSR = (PSR & 0x8000) | 0x0001;
	} else {
		PSR = (PSR & 0x8000) | 0x0001;
	}
}

/* logic commands */
void AND_r(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((unsigned short)registers[s]) & ((unsigned short)registers[t]);
	update_NZP(d);
}

void NOT(unsigned short d, unsigned short s) {
	registers[d] = ~((unsigned short)registers[s]);
	update_NZP(d);
}

void OR(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((unsigned short)registers[s]) | ((unsigned short)registers[t]);
	update_NZP(d);
}

void XOR(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((unsigned short)registers[s]) ^ ((unsigned short)registers[t]);
	update_NZP(d);
}

void AND_imm(unsigned short d, unsigned short s, signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 4) & 1) == 1) {
		sign_extended_num = imm | 0xffe0;
	}
	registers[d] = ((unsigned short)registers[s]) & sign_extended_num;
	update_NZP(d);
}

/* load store */
void LDR(unsigned short d, unsigned short s, signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 5) & 1) == 1) {
		sign_extended_num = imm | 0xffc0;
	}
	registers[d] = memory[((unsigned short)registers[s]) + sign_extended_num];
	update_NZP(d);
}

void STR(unsigned short d, unsigned short s, signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 5) & 1) == 1) {
		sign_extended_num = imm | 0xffc0;
	}
	memory[((unsigned short)registers[s]) + sign_extended_num] = registers[d];
}

/* RTI */
void RTI() {
	PC = registers[7];
	PSR = (PSR ^ 0x8000);
}

/* CONST */
void CONST(unsigned short d, signed short imm) {
	signed short sign_extended_num = imm;
	if (PARSE_SIGN_9(imm) == 1) {
		sign_extended_num = imm | 0xfe00;
	}
	registers[d] = sign_extended_num;
	update_NZP(d);
}

/* shift commands */
void SLL(unsigned short d, unsigned short s, unsigned short imm) {
	registers[d] = ((signed short)registers[s]) << imm;
	update_NZP(d);
}

void SRA(unsigned short d, unsigned short s, unsigned short imm) {
	if ((registers[s] >> 15) == 1) {
		registers[d] = ((((signed short)registers[s]) ^ 0x8000) >> imm) + 0x8000;
	} else {
		registers[d] = ((signed short)registers[s]) >> imm;
	}
	update_NZP(d);
}

void SRL(unsigned short d, unsigned short s, unsigned short imm) {
	registers[d] = ((unsigned short)registers[s]) >> imm;
	update_NZP(d);
}

void MOD(unsigned short d, unsigned short s, unsigned short t) {
	registers[d] = ((signed short)registers[s]) % ((signed short)registers[t]);
	update_NZP(d);
}

/* JMP */
void JMPR(unsigned short s) {
	PC = registers[s];
}

void JMP(signed short imm) {
	signed short sign_extended_num = imm;
	if (((imm >> 10) & 1) == 1) {
		sign_extended_num = imm | 0xf800;
	}
	PC = PC + 1 + sign_extended_num;
}

/* HICONST */
void HICONST(unsigned short d, unsigned short imm) {
	registers[d] = (((signed short)registers[d]) & 0x00FF) | (imm << 8);
	update_NZP(d);
}

void TRAP(unsigned short imm) {
	registers[7] = PC + 1;
	PC = 0x8000 | imm;
	PSR = 0x8001;
}

/* PennSim commands */

/* reset command */
void reset() {
	int i;
	for (i = 0; i < 8; i++) {
		registers[i] = 0x0000;
	}
	PSR = 0x8002;
	PC = 0x8200;
	NZP_CC = "Z";
	for (i = 0; i < 65536; i++) {
		memory[i] = 0x0000;
	}
}

/* load command */
int load(char *filename) {
	unsigned short read;
	unsigned short little_endian;
	
	/* check for .obj extension */
	char *dot = strchr(filename, '.');
	if (dot != NULL) {
		printf("Loading object file %s.obj\n", filename);
		return 1;
	}

	strcat(filename, ".obj");
	
	printf("Loading object file %s\n", filename);

	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return 1;
	}

	/* read file - parses .obj file to find headers */
	while (fread(&read, 2, 1, file) == 1) {
		little_endian = LITTLE_TWO(read);
		if (little_endian == 0xcade) { /* Code section */
			unsigned short address;
			unsigned short n_body;
			fread(&address, 2, 1, file);
			fread(&n_body, 2, 1, file);
			address = LITTLE_TWO(address);
			n_body = LITTLE_TWO(n_body);
			while (n_body > 0) { /* read and store all of code section */
				unsigned short opcode;
				fread(&opcode, 2, 1, file);
				memory[address] = LITTLE_TWO(opcode);
				address++;
				n_body--;
			}
		} else if (little_endian == 0xdada) { /* Data section */
			unsigned short address;
			unsigned short n_body;
			fread(&address, 2, 1, file);
			fread(&n_body, 2, 1, file);
			address = LITTLE_TWO(address);
			n_body = LITTLE_TWO(n_body);
			while (n_body > 0) { /* read and store all of data section */
				unsigned short data;
				fread(&data, 2, 1, file);
				memory[address] = LITTLE_TWO(data);
				address++;
				n_body--;
			}
		} else if (little_endian == 0xc3b7) { /* Symbol section */
			unsigned short address;
			unsigned char n_body;
			fread(&address, 2, 1, file);
			fread(&n_body, 1, 1, file);
			address = LITTLE_TWO(address);
			n_body = LITTLE_ONE(n_body);
			while (n_body > 0) { /* read all of symbol section */
				unsigned short character;
				fread(&character, 1, 1, file);
				character = LITTLE_ONE(character);
				n_body--;
			}
		} else if (little_endian == 0xf17e) { /* Filename section */
			unsigned short address;
			unsigned char n_body;
			fread(&address, 2, 1, file);
			fread(&n_body, 1, 1, file);
			address = LITTLE_TWO(address);
			n_body = LITTLE_ONE(n_body);
			while (n_body > 0) { /* read all of filename section */
				unsigned short character;
				fread(&character, 2, 1, file);
				character = LITTLE_ONE(character);
				n_body--;
			}
		} else if (little_endian == 0x715e) { /* Line number section */
			unsigned short address;
			unsigned short line;
			unsigned short file_index;
			fread(&address, 2, 1, file);
			fread(&line, 2, 1, file);
			fread(&file_index, 2, 1, file);
			address = LITTLE_TWO(address);
			line = LITTLE_TWO(line);
			file_index = LITTLE_TWO(file_index);
		}
	}
	fclose(file);
	return 0;
}

/* set command */
void set(char *d, char *v) {
	unsigned short value;
	char x_value[2];
	x_value[0] = v[0];
	x_value[1] = '\0';
	char hex[sizeof(v) + 1];
	int i;

	/* parse hex or decimal */
	for (i = 0; i < sizeof(hex); i++) {
		if (i == 0) {
			hex[i] = '0';
		} else {
			hex[i] = v[i - 1];
		}
	}
	if (strcmp(x_value, "x") == 0) {
		sscanf(hex, "%hx", &value);
	} else {
		sscanf(v, "%hd", &value);
	}

	/* find correct register or PC */
	if (strcmp(d, "R0") == 0) {
		registers[0] = value;
		printf("Register R0 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R1") == 0) {
		registers[1] = value;
		printf("Register R1 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R2") == 0) {
		registers[2] = value;
		printf("Register R2 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R3") == 0) {
		registers[3] = value;
		printf("Register R3 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R4") == 0) {
		registers[4] = value;
		printf("Register R4 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R5") == 0) {
		registers[5] = value;
		printf("Register R5 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R6") == 0) {
		registers[6] = value;
		printf("Register R6 updated to value x%04x\n", value);
	}
	if (strcmp(d, "R7") == 0) {
		registers[7] = value;
		printf("Register R7 updated to value x%04x\n", value);
	}
	if (strcmp(d, "PC") == 0) {
		PC = value;
		printf("Register PC updated to value x%04x\n", value);
	}
}

/* break command */
int break_set_clear_command(char *argument, char *address) {
	char break_set[] = "set";
	char break_clear[] = "clear";
	unsigned int memory;
	char x_value[2];
	x_value[0] = address[0];
	x_value[1] = '\0';
	char hex[sizeof(address) + 1];
	int i;

	/* parse hex */
	for (i = 0; i < sizeof(hex); i++) {
		if (i == 0) {
			hex[i] = '0';
		} else {
			hex[i] = address[i - 1];
		}
	}
	sscanf(hex, "%x", &memory);
	
	if (memory < 0 || memory >= 0xffff) {
		return 1;
	}

	/* set or clear breakpoint */
	if (strcmp(argument, break_set) == 0) {
		breakpoints[((unsigned short)memory)] = 1;
	}
	if (strcmp(argument, break_clear) == 0) {
		breakpoints[((unsigned short)memory)] = 0;
	}
	return 0;
}

/* step command */
int step() {
	unsigned short opcode = memory[PC];

	if (opcode == 0x0000) {
		PC++;
	}

	unsigned short op = PARSE_SEVEN_OP(opcode);

	/* branch */
	signed short imm = PARSE_BRANCH_CONST_IMM(opcode);
	if (op == 0x0004) {
		BRn(imm);
	}
	if (op == 0x0006) {
		BRnz(imm);
	}
	if (op == 0x0005) {
		BRnp(imm);
	}
	if (op == 0x0002) {
		BRz(imm);
	}
	if (op == 0x0003) {
		BRzp(imm);
	}
	if (op == 0x0001) {
		BRp(imm);
	}
	if (op == 0x0007) {
		BRnzp(imm);
	}

	op = PARSE_FOUR_OP(opcode);

	/* arithmetic */
	if (op == 0x0001) {
		unsigned short sop = PARSE_5_3(opcode);
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short rs = PARSE_8_6(opcode);
		unsigned short rt = PARSE_2_0(opcode);
		unsigned short imm_sop = PARSE_ARITH_LOGIC_IMM_SOP(opcode);
		signed short imm = PARSE_ARITH_LOGIC_IMM(opcode);
		if (sop == 0x0000) {
			ADD_r(rd, rs, rt);
		}
		if (sop == 0x0001) {
			MUL(rd, rs, rt);
		}
		if (sop == 0x0002) {
			SUB(rd, rs, rt);
		}
		if (sop == 0x0003) {
			DIV(rd, rs, rt);
		}
		if (imm_sop == 0x0001) {
			ADD_imm(rd, rs, imm);
		}
		PC++;
	}
	
	/* compare */
	if (op == 0x0002) {
		unsigned short sop = PARSE_CMP_SOP(opcode);
		unsigned short rs = PARSE_11_9(opcode);
		unsigned short rt = PARSE_2_0(opcode);
		unsigned short uimm = PARSE_CMP_IMM(opcode);
		signed short imm = PARSE_CMP_IMM(opcode);
		if (sop == 0x0000) {
			CMP(rs, rt);
		}
		if (sop == 0x0001) {
			CMPU(rs, rt);
		}
		if (sop == 0x0002) {
			CMPI(rs, imm);
		}
		if (sop == 0x0003) {
			CMPIU(rs, uimm);
		}
		PC++;
	}

	/* logic */
	if (op == 0x0005) {
		unsigned short sop = PARSE_5_3(opcode);
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short rs = PARSE_8_6(opcode);
		unsigned short rt = PARSE_2_0(opcode);
		unsigned short imm_sop = PARSE_ARITH_LOGIC_IMM_SOP(opcode);
		signed short imm = PARSE_ARITH_LOGIC_IMM(opcode);
		if (sop == 0x0000) {
			AND_r(rd, rs, rt);
		}
		if (sop == 0x0001) {
			NOT(rd, rs);
		}
		if (sop == 0x0002) {
			OR(rd, rs, rt);
		}
		if (sop == 0x0003) {
			XOR(rd, rs, rt);
		}
		if (imm_sop == 0x0001) {
			AND_imm(rd, rs, imm);
		}
		PC++;
	}

	/* load store */
	if (op == 0x0006) {
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short rs = PARSE_8_6(opcode);
		signed short imm = PARSE_6(opcode);
		LDR(rd, rs, imm);
		PC++;
	}
	if (op == 0x0007) {
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short rs = PARSE_8_6(opcode);
		signed short imm = PARSE_6(opcode);
		STR(rd, rs, imm);
		PC++;
	}

	/* RTI */
	if (op == 0x0008) {
		RTI();
	}

	/* CONST and HICONST */
	if (op == 0x0009) {
		unsigned short rd = PARSE_11_9(opcode);
		signed short imm = PARSE_BRANCH_CONST_IMM(opcode);
		CONST(rd, imm);
		PC++;
	}
	if (op == 0x000D) {
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short imm = PARSE_8(opcode);
		HICONST(rd, imm);
		PC++;
	}

	/* shifts and mod */
	if (op == 0x000A) {
		unsigned short sop = PARSE_SHIFT_SOP(opcode);
		unsigned short uimm = PARSE_4(opcode);
		unsigned short rd = PARSE_11_9(opcode);
		unsigned short rs = PARSE_8_6(opcode);
		unsigned short rt = PARSE_2_0(opcode);
		if (sop == 0x0000) {
			SLL(rd, rs, uimm);
		}
		if (sop == 0x0001) {
			SRA(rd, rs, uimm);
		}
		if (sop == 0x0002) {
			SRL(rd, rs, uimm);
		}
		if (sop == 0x0003) {
			MOD(rd, rs, rt);
		}
		PC++;
	}

	/* TRAP */
	if (op == 0x000F) {
		unsigned short imm = PARSE_8(opcode);
		TRAP(imm);
	}

	op = PARSE_FIVE_OP(opcode);

	/* JSR */
	if (op == 0x0009) {
		signed short imm = PARSE_11(opcode);
		JSR(imm);
	}
	if (op == 0x0008) {
		unsigned short rs = PARSE_8_6(opcode);
		JSRR(rs);
	}

	/* JMP */
	if (op == 0x0018) {
		unsigned short rs = PARSE_8_6(opcode);
		JMPR(rs);
	}
	if (op == 0x0019) {
		signed short imm = PARSE_11(opcode);
		JMP(imm);
	}

	if (breakpoints[PC] == 1) {
		printf("Hit breakpoint\n");
		return 1;
	}

	if ((PC >= 0x2000 && PC < 0x8000) || (PC >= 0xa000)) {
		printf("Error: Attempting to access memory outside of code\n");
		return 1;
	}
	if ((PC >= 0x8000 && PC < 0xa000) && ((PSR >> 15) != 1)) {
		printf("Error: User does not have privilege\n");
		return 1;
	}
	return 0;
}

/* print command */
void print() {
	printf("[R0: x%04x,R1: x%04x,R2: x%04x,R3: x%04x,R4: x%04x,R5: x%04x,R6: x%04x,R7: x%04x]\n", registers[0], registers[1], registers[2], registers[3], registers[4], registers[5], registers[6], registers[7]);
	printf("PC = x%04x\n", PC);
	printf("PSR = x%04x\n", PSR);
	printf("CC = %s\n", NZP_CC);
}

int script(char *filename) {
	char input[80];
	char *dot = strchr(filename, '.');
	if (dot == NULL) {
		return 1;
	}

	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		return 1;
	}

	while (fgets(input, sizeof(input), file) != NULL) {
		char *newline = strchr(input, '\n');
		if (newline != NULL) {
			*newline = '\0';
		}

		char *command = strtok(input, " ");
		
		if (strcmp(command, reset_command) == 0) { /* reset */
			reset();
			printf("System reset\n");
		} else if (strcmp(command, load_command) == 0) { /* load */
			char *file = strtok(NULL, " ");
			if (file == NULL) {
				printf("load <filename>\n");
			} else {
				int i = load(file);
				if (i == 1) {
					printf("Error: File not found.\n");
				}
			}
		} else if (strcmp(command, set_command) == 0) { /* set */
			char *destination = strtok(NULL, " ");
			char *value;
			if (destination == NULL) {
				printf("set <register> <value>\n");
			} else {
				value = strtok(NULL, " ");
				if (value == NULL) {
					printf("set <register> <value>\n");
				} else {
					set(destination, value);
				}
			}
		} else if (strcmp(command, break_command) == 0) { /* break */
			char *argument = strtok(NULL, " ");
			char *address;
			if (argument == NULL) {
				printf("break <set|clear> <address>\n");
			} else {
				address = strtok(NULL, " ");
				if (address == NULL) {
					printf("break <set|clear> <address>\n");
				} else {
					break_set_clear_command(argument, address);
				}
			}
		} else if (strcmp(command, step_command) == 0) { /* step */
			step();
		} else if (strcmp(command, continue_command) == 0) { /* continue */
			while (step() != 1) {}
		} else if (strcmp(command, print_command) == 0) { /* print */
			print();
		} else if (strcmp(command, quit_command) == 0) { /* quit */
			break;
		} else {
			printf("Unknown Command: %s\n", command);
		}
	}
	fclose(file);
	return 0;
}

/* runs PennSim, main method */
int main() {
	while (1) {
		/* read user input */
		char input[255];
		fputs("Enter a command: ", stdout);
		fflush(stdout);
	
		/* parse command and strip newline */
		if (fgets(input, sizeof input, stdin) != NULL) {
			char *newline = strchr(input, '\n');
			if (newline != NULL) {
				*newline = '\0';
			}
		}
		char *command = strtok(input, " ");
		
		/* see what command it is and perform necessary actions */
		if (strcmp(command, reset_command) == 0) { /* reset */
			reset();
			printf("System reset\n");
		} else if (strcmp(command, load_command) == 0) { /* load */
			char *filename = strtok(NULL, " ");
			if (filename == NULL) {
				printf("load <filename>\n");
			} else {
				int i = load(filename);
				if (i == 1) {
					printf("Error: File not found.\n");
				}
			}
		} else if (strcmp(command, set_command) == 0) { /* set */
			char *destination = strtok(NULL, " ");
			char *value;
			if (destination == NULL) {
				printf("set <register> <value>\n");
			} else {
				value = strtok(NULL, " ");
				if (value == NULL) {
					printf("set <register> <value>\n");
				} else {
					set(destination, value);
				}
			}
		} else if (strcmp(command, break_command) == 0) { /* break */
			char *argument = strtok(NULL, " ");
			char *address;
			if (argument == NULL) {
				printf("break <set|clear> <address>\n");
			} else {
				address = strtok(NULL, " ");
				if (address == NULL) {
					printf("break <set|clear> <address>\n");
				} else {
					break_set_clear_command(argument, address);
				}
			}
		} else if (strcmp(command, step_command) == 0) { /* step */
			step();
		} else if (strcmp(command, continue_command) == 0) { /* continue */
			while (step() != 1) {}
		} else if (strcmp(command, print_command) == 0) { /* print */
			print();
		} else if (strcmp(command, script_command) == 0) { /* script */
			char *filename = strtok(NULL, " ");
			if (filename == NULL) {
				printf("script <filename>\n");
			} else {
				int i = script(filename);
				if (i == 1) {
					printf("Error: File not found.\n");
				}
			}
		} else if (strcmp(command, quit_command) == 0) { /* quit */
			break;
		} else {
			printf("Unknown Command: %s\n", command);
		}
	}
	return 0;
}

