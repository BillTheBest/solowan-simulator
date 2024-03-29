/*
 * inout.h
 *
 *  Created on: Nov 21, 2013
 *      Author: mattia
 */

#ifndef INOUT_H_
#define INOUT_H_

#include <time.h>
#include <stdint.h>

// Should we use timeval data structure which offers microseconds? <sys/time.h>
typedef struct packet
{
    size_t size;
    char filename[30];
    time_t receive_time;
    time_t send_time;
    struct packet *next;
}packet;

typedef struct hashptr{
    uint16_t position;
    struct hashptr *next;
}hashptr;

int read_structuredfile( char *datafilename, unsigned char **map, size_t *size);
int dump_packet(unsigned char *buffer, hashptr *hashlist,  uint16_t new_size, uint16_t number_of_hashes, FILE * metadatafile, FILE *outputfile, char *originfilename, int session);
int dump(unsigned char *out_packet, uint16_t out_size, FILE * metadatafile, FILE *outputfile, char *originfilename, int session, int compressed);
int help_optimizer();
int help_deoptimizer();
#endif /* INOUT_H_ */
