/*
	sim_core.h

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIM_CORE_H_
#define SIM_CORE_H_

/*
 * Instruction decoder, run ONE instruction
 */
uint16_t avr_run_one(avr_t * avr);

/*
 * These are for internal access to the stack (for interupts)
 */
uint16_t _avr_sp_get(avr_t * avr);
void _avr_sp_set(avr_t * avr, uint16_t sp);
void _avr_push16(avr_t * avr, uint16_t v);

/*
 * Get a "pretty" register name
 */
const char * avr_regname(uint8_t reg);


/* 
 * DEBUG bits follow 
 * These will diseapear when gdb arrives
 */
void avr_dump_state(avr_t * avr);

#define DUMP_REG() { \
				for (int i = 0; i < 32; i++) printf("%s=%02x%c", avr_regname(i), avr->data[i],i==15?'\n':' ');\
				printf("\n");\
				uint16_t y = avr->data[R_YL] | (avr->data[R_YH]<<8);\
				for (int i = 0; i < 20; i++) printf("Y+%02d=%02x ", i, avr->data[y+i]);\
				printf("\n");\
		}


#if AVR_STACK_WATCH
#define DUMP_STACK() \
		for (int i = avr->stack_frame_index; i; i--) {\
			int pci = i-1;\
			printf("\e[31m*** %04x: %-25s sp %04x\e[0m\n",\
					avr->stack_frame[pci].pc, avr->codeline[avr->stack_frame[pci].pc>>1]->symbol, avr->stack_frame[pci].sp);\
		}
#else
#define DUMP_STACK()
#endif

#define CRASH()  {\
		DUMP_REG();\
		printf("*** CYCLE %lld PC %04x\n", avr->cycle, avr->pc);\
		for (int i = OLD_PC_SIZE-1; i > 0; i--) {\
			int pci = (avr->old_pci + i) & 0xf;\
			printf("\e[31m*** %04x: %-25s RESET -%d; sp %04x\e[0m\n",\
					avr->old[pci].pc, avr->codeline[avr->old[pci].pc>>1]->symbol, OLD_PC_SIZE-i, avr->old[pci].sp);\
		}\
		printf("Stack Ptr %04x/%04x = %d \n", _avr_sp_get(avr), avr->ramend, avr->ramend - _avr_sp_get(avr));\
		DUMP_STACK();\
		exit(1);\
	}

#endif /* SIM_CORE_H_ */
