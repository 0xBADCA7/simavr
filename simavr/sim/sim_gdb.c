/*
	sim_gdb.c

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

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include "sim_avr.h"
#include "avr_eeprom.h"

typedef struct avr_gdb_t {
	avr_t * avr;
	int		listen;	// listen socket
	int		s;		// current gdb connection
	
	pthread_t thread;

	uint32_t	query_len;
	char		query[1024];

	uint32_t	watchmap;
	struct {
		uint32_t	pc;
		uint32_t	len;
		int kind;
	} watch[32];
} avr_gdb_t;

    // decode line text hex to binary
int read_hex_string(const char * src, uint8_t * buffer, int maxlen)
{
    uint8_t * dst = buffer;
    int ls = 0;
    uint8_t b = 0;
    while (*src && maxlen--) {
        char c = *src++;
        switch (c) {
            case 'a' ... 'f':   b = (b << 4) | (c - 'a' + 0xa); break;
            case 'A' ... 'F':   b = (b << 4) | (c - 'A' + 0xa); break;
            case '0' ... '9':   b = (b << 4) | (c - '0'); break;
            default:
                if (c > ' ') {
                    fprintf(stderr, "%s: huh '%c' (%s)\n", __FUNCTION__, c, src);
                    return -1;
                }
                continue;
        }
        if (ls & 1) {
            *dst++ = b; b = 0;
        }
        ls++;
    }

    return dst - buffer;
}

static void gdb_send_reply(avr_gdb_t * g, char * cmd)
{
	uint8_t reply[1024];
	uint8_t * dst = reply;
	uint8_t check = 0;
	*dst++ = '$';
	while (*cmd) {
		check += *cmd;
		*dst++ = *cmd++;
	}
	sprintf((char*)dst, "#%02x", check);
	printf("%s '%s'\n", __FUNCTION__, reply);
	send(g->s, reply, dst - reply + 3, 0);
}

static void gdb_send_quick_status(avr_gdb_t * g, uint8_t signal)
{
	char cmd[64];

	sprintf(cmd, "T%02x20:%02x;21:%02x%02x;22:%02x%02x%02x00;",
		signal, g->avr->data[R_SREG], 
		g->avr->data[R_SPL], g->avr->data[R_SPH],
		g->avr->pc & 0xff, (g->avr->pc>>8)&0xff, (g->avr->pc>>16)&0xff);
	gdb_send_reply(g, cmd);
}

static int gdb_change_breakpoint(avr_gdb_t * g, int set, int kind, uint32_t addr, uint32_t len)
{
	printf("set %d kind %d addr %08x len %d (map %08x)\n", set, kind, addr, len, g->watchmap);
	if (set) {
		if (g->watchmap == 0xffffffff)
			return -1;	// map full

		// check to see if it exists
		for (int i = 0; i < 32; i++)
			if ((g->watchmap & (1 << i)) && g->watch[i].pc == addr) {
				g->watch[i].len = len;
				return 0;
			}
		for (int i = 0; i < 32; i++)
			if (!(g->watchmap & (1 << i))) {
				g->watchmap |= (1 << i);
				g->watch[i].len = len;
				g->watch[i].pc = addr;
				g->watch[i].kind = kind;
				return 0;
			}
	} else {
		for (int i = 0; i < 32; i++)
			if ((g->watchmap & (1 << i)) && g->watch[i].pc == addr) {
				g->watchmap &= ~(1 << i);
				g->watch[i].len = 0;
				g->watch[i].pc = 0;
				g->watch[i].kind = 0;
				return 0;
			}
	}
	return -1;
}

static void gdb_handle_command(avr_gdb_t * g)
{
	avr_t * avr = g->avr;
	char * cmd = g->query;
	char rep[1024];
	uint8_t command = *cmd++;
	switch (command) {
		case '?':
			gdb_send_reply(g, "S00");
			break;
		case 'p': {
			unsigned int regi = 0;
			sscanf(cmd, "%x", &regi);
			switch (regi) {
				case 0 ... 31:
					sprintf(rep, "%02x", g->avr->data[regi]);
					break;
				case 32:
					sprintf(rep, "%02x", g->avr->data[R_SREG]);
					break;
				case 33:
					sprintf(rep, "%02x%02x", g->avr->data[R_SPL], g->avr->data[R_SPH]);
					break;
				case 34:
					sprintf(rep, "%02x%02x%02x00", 
						g->avr->pc & 0xff, (g->avr->pc>>8)&0xff, (g->avr->pc>>16)&0xff);
					break;
			}
			gdb_send_reply(g, rep);			
		}	break;
		case 'm': {
			uint32_t addr, len;
			sscanf(cmd, "%x,%x", &addr, &len);
			printf("read memory %08x, %08x\n", addr, len);
			uint8_t * src = NULL;
			if (addr < 0xffff) {
				src = avr->flash + addr;
			} else if (addr >= 0x800000 && (addr - 0x800000) <= avr->ramend) {
				src = avr->data + addr - 0x800000;
			} else if (addr >= 0x810000 && (addr - 0x810000) <= (16*1024)) {
				avr_eeprom_desc_t ee = {.offset = (addr - 0x810000)};
				avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &ee);
				if (ee.ee)
					src = ee.ee;
				else
					gdb_send_reply(g, "E01");
			} else {
				gdb_send_reply(g, "E01");
				break;
			}
			char * dst = rep;
			while (len--) {
				sprintf(dst, "%02x", *src++);
				dst += 2;
			}
			*dst = 0;
			gdb_send_reply(g, rep);
		}	break;
		case 'M': {
			uint32_t addr, len;
			sscanf(cmd, "%x,%x", &addr, &len);
			printf("write memory %08x, %08x\n", addr, len);
			char * start = strchr(cmd, ':');
			if (!start) {
				gdb_send_reply(g, "E01");
				break;
			}
			if (addr < 0xffff) {
				read_hex_string(start + 1, avr->flash + addr, strlen(start+1));
				gdb_send_reply(g, "OK");			
			} else if (addr >= 0x800000 && (addr - 0x800000) <= avr->ramend) {
				read_hex_string(start + 1, avr->data + addr - 0x800000, strlen(start+1));
				gdb_send_reply(g, "OK");							
			} else
				gdb_send_reply(g, "E01");			
		}	break;
		case 'c': {
			avr->state = cpu_Running;
		}	break;
		case 's': {
			avr->state = cpu_Step;
		}	break;
		case 'Z': 
		case 'z': {
			uint32_t kind, addr, len;
			sscanf(cmd, "%d,%x,%x", &kind, &addr, &len);
			printf("breakbpoint %d, %08x, %08x\n", kind, addr, len);
			switch (kind) {
				case 0:	// software breakpoint
				case 1:	// hardware breakpoint
					if (addr <= avr->flashend) {
						if (gdb_change_breakpoint(g, command == 'Z', kind, addr, len))
							gdb_send_reply(g, "E01");
						else
							gdb_send_reply(g, "OK");
					} else
						gdb_send_reply(g, "E01");		// out of flash address
					break;
				case 2: // write watchpoint
				case 3: // read watchpoint
				case 4: // access watchpoint
				default:
					gdb_send_reply(g, "");
			}	
		}	break;
		default:
			gdb_send_reply(g, "");
	}
}

void avr_gdb_processor(avr_t * avr)
{
	if (!avr || !avr->gdb)
		return;	
	avr_gdb_t * g = avr->gdb;

	if (g->watchmap && avr->state == cpu_Running) {
		for (int i = 0; i < 32; i++)
			if ((g->watchmap & (1 << i)) && g->watch[i].pc == avr->pc) {
				printf("avr_gdb_processor hit breakpoint at %08x\n", avr->pc);
				gdb_send_quick_status(g, 0);
				avr->state = cpu_Stopped;
			}		
	}
	if (avr->state == cpu_StepDone) {
		gdb_send_quick_status(g, 0);
		avr->state = cpu_Stopped;
	}
	if (avr->gdb->query_len) {
		g->query_len = 0;
	
	//	printf("avr_gdb_handle_query got a query '%s'\n", g->query);
		gdb_handle_command(g);
	}
}


static void * gdb_network_handler(void * param)
{
	avr_gdb_t * g = (avr_gdb_t*)param;

	do {
		if (listen(g->listen, 1)) {
			perror("gdb_network_handler listen");
			sleep(5);
			continue;
		}
		
		struct sockaddr_in address = { 0 };
		socklen_t ad_len = sizeof(address);

		g->s = accept(g->listen, (struct sockaddr*)&address, &ad_len);

		if (g->s == -1) {
			perror("gdb_network_handler accept");
			sleep(5);
			continue;
		}
		// should make that thread safe... 
		g->avr->state = cpu_Stopped;
		
		do {
			fd_set read_set;
			FD_ZERO(&read_set);
			FD_SET(g->s, &read_set);

			struct timeval timo = { 1, 0000 };	// short, but not too short interval
			/*int ret =*/ select(g->s + 1, &read_set, NULL, NULL, &timo);

			if (FD_ISSET(g->s, &read_set)) {
				uint8_t buffer[1024];
				
				ssize_t r = recv(g->s, buffer, sizeof(buffer)-1, 0);

				if (r == 0) {
					printf("%s connection closed\n", __FUNCTION__);
					break;
				}
				if (r == -1) {
					perror("gdb_network_handler recv");
					break;
				}
				buffer[r] = 0;
			//	printf("%s: received %d bytes\n'%s'\n", __FUNCTION__, r, buffer);
			//	hdump("gdb", buffer, r);

				uint8_t * src = buffer;
				while (*src == '+' || *src == '-')
					src++;
				if (*src == 3) {
					src++;
					g->query[0] = 3;
					g->query_len = 1; // pass it on ?
				}
				if (*src  == '$') {
					// strip checksum
					uint8_t * end = buffer + r - 1;
					while (end > src && *end != '#')
						*end-- = 0;
					*end = 0;
					src++;
					printf("GDB command = '%s'\n", src);

					send(g->s, "+", 1, 0);

					strcpy(g->query, (char*)src);
					g->query_len = strlen((char*)src);
				}
			}
		} while(1);
		
		close(g->s);
			
	} while(1);
	
	return NULL;
}


int avr_gdb_init(avr_t * avr)
{
	avr_gdb_t * g = malloc(sizeof(avr_gdb_t));
	memset(g, 0, sizeof(avr_gdb_t));

	avr->gdb = NULL;

	if ((g->listen = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Can't create socket: %s", strerror(errno));
		return -1;
	}

	int i = 1;
	setsockopt(g->listen, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_port = htons (1234);

	if (bind(g->listen, (struct sockaddr *) &address, sizeof(address))) {
		fprintf(stderr, "Can not bind socket: %s", strerror(errno));
		return -1;
	}
	printf("avr_gdb_init listening on port %d\n", 1234);
	g->avr = avr;
	avr->gdb = g;

	pthread_create(&g->thread, NULL, gdb_network_handler, g);

	return 0;
}
