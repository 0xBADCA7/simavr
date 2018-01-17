/* vim: ts=4
	shavr.c

	Copyright 2017 Michel Pollet <buserror@gmail.com>

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

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>

#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "uart_pty.h"

#include "sim_args.h"
#include "history_avr.h"

uart_pty_t uart_pty;
avr_t * avr = NULL;
elf_firmware_t *code = NULL;

avr_t *
sim_prepare(
	sim_args_t * a ); // TODO: Move to a header


typedef struct shavr_runtime_t {
	char	avr_flash_path[1024];
	int		avr_flash_fd;
} shavr_runtime_t;

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
static void
avr_special_init(
		avr_t * avr,
		void * data)
{
	shavr_runtime_t *flash_data = (shavr_runtime_t *)data;

	printf("%s\n", __func__);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
									O_RDWR|O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0) {
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
static void
avr_special_deinit(
		avr_t* avr,
		void * data)
{
	shavr_runtime_t *flash_data = (shavr_runtime_t *)data;

	printf("%s\n", __func__);
	lseek(flash_data->avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to save flash memory\n");
		perror(flash_data->avr_flash_path);
	}
	close(flash_data->avr_flash_fd);
	uart_pty_stop(&uart_pty);
}

static void *
avr_run_thread(
		void * ignore)
{
	while (1) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
	return NULL;
}

int
main(
		int argc,
		const char *argv[])
{
	shavr_runtime_t flash_data;
	sim_args_t args;

	if (sim_args_parse(&args, argc, argv, NULL)) {
		exit(1);
	}

	avr = sim_prepare(&args);
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}
	if (args.flash_file[0]) {
		strncpy(flash_data.avr_flash_path,
			args.flash_file,
			sizeof(flash_data.avr_flash_path));
		flash_data.avr_flash_fd = 0;
		// register our own functions
		avr->custom.init = avr_special_init;
		avr->custom.deinit = avr_special_deinit;
		avr->custom.data = &flash_data;
	}

	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

	code = &args.f;

	history_avr_init();

//	pthread_t run;
	//pthread_create(&run, NULL, avr_run_thread, avr);

	printf("Running...\n");
	while (1) {
		history_avr_idle();
	}
}
