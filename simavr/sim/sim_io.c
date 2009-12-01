/*
	sim_io.c

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_io.h"

int avr_ioctl(avr_t *avr, uint32_t ctl, void * io_param)
{
	avr_io_t * port = avr->io_port;
	int res = -1;
	while (port && res == -1) {
		if (port->ioctl)
			res = port->ioctl(avr, port, ctl, io_param);
		port = port->next;
	}
	return res;
}

void avr_register_io(avr_t *avr, avr_io_t * io)
{
	io->next = avr->io_port;
	avr->io_port = io;
}

void avr_register_io_read(avr_t *avr, uint8_t addr, avr_io_read_t readp, void * param)
{
	avr->ior[AVR_DATA_TO_IO(addr)].param = param;
	avr->ior[AVR_DATA_TO_IO(addr)].r = readp;
}

void avr_register_io_write(avr_t *avr, uint8_t addr, avr_io_write_t writep, void * param)
{
	avr->iow[AVR_DATA_TO_IO(addr)].param = param;
	avr->iow[AVR_DATA_TO_IO(addr)].w = writep;
}

struct avr_irq_t * avr_io_getirq(avr_t * avr, uint32_t ctl, int index)
{
	avr_io_t * port = avr->io_port;
	while (port) {
		if (port->irq && port->irq_ioctl_get == ctl && port->irq_count > index)
			return port->irq + index;
		port = port->next;
	}
	return NULL;
	
}
