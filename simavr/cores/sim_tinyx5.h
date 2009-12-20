/*
	sim_tinyx5.h

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	                     Jon Escombe <lists@dresco.co.uk>

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


#ifndef __SIM_TINYX5_H__
#define __SIM_TINYX5_H__

#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_adc.h"
#include "avr_timer.h"

void tx5_init(struct avr_t * avr);
void tx5_reset(struct avr_t * avr);

/*
 * This is a template for all of the tinyx5 devices, hopefully
 */
struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_extint_t	extint;
	avr_ioport_t	portb;
	avr_adc_t		adc;
	avr_timer_t	timer0, timer1;
};

#ifdef SIM_CORENAME

#ifndef SIM_VECTOR_SIZE
#error SIM_VECTOR_SIZE is not declared
#endif
#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

struct mcu_t SIM_CORENAME = {
	.core = {
		.mmcu = SIM_MMCU,
		DEFAULT_CORE(SIM_VECTOR_SIZE),

		.init = tx5_init,
		.reset = tx5_reset,
	},
	AVR_EEPROM_DECLARE(EE_RDY_vect),
	.extint = {
		AVR_EXTINT_TINY_DECLARE(0, 'B', PB2, GIFR),
	},
	.portb = {
		.name = 'B',  .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE),
			.raised = AVR_IO_REGBIT(GIFR, PCIF),
			.vector = PCINT0_vect,
		},
		.r_pcint = PCMSK,
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1), AVR_IO_REGBIT(ADMUX, REFS2), },
		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1), AVR_IO_REGBIT(ADCSRB, ADTS2),},
		.bin = AVR_IO_REGBIT(ADCSRB, BIN),
		.ipr = AVR_IO_REGBIT(ADCSRA, IPR),

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
	.timer0 = {
		.name = '0',
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM(),
			[7] = AVR_TIMER_WGM_FASTPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_ocra = OCR0A,
		.r_ocrb = OCR0B,
		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
			.raised = AVR_IO_REGBIT(TIFR, OCF0A),
			.vector = TIMER0_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
			.raised = AVR_IO_REGBIT(TIFR, OCF0B),
			.vector = TIMER0_COMPB_vect,
		},
	},
	.timer1 = {
		.name = '1',
		// no wgm bits
		.cs = { AVR_IO_REGBIT(TCCR1, CS10), AVR_IO_REGBIT(TCCR1, CS11), AVR_IO_REGBIT(TCCR1, CS12), AVR_IO_REGBIT(TCCR1, CS13) },
		.cs_div = { 0, 0, 1 /* 2 */, 2 /* 4 */, 3 /* 8 */, 4 /* 16 */ },

		.r_ocra = OCR1A,
		.r_ocrb = OCR1B,
		.r_ocrc = OCR1C,
		.r_tcnt = TCNT1,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
			.raised = AVR_IO_REGBIT(TIFR, OCF1A),
			.vector = TIMER1_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
			.raised = AVR_IO_REGBIT(TIFR, OCF1B),
			.vector = TIMER1_COMPB_vect,
		},
	},


};
#endif /* SIM_CORENAME */

#endif /* __SIM_TINYX5_H__ */
