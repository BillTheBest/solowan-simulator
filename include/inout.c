/*
 * inout.c
 *
 *     Copyright 2014 Universidad Politécnica de Madrid - Center for Open Middleware
 *     (http://www.centeropenmiddleware.com)
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 *     Created on: Nov 21, 2013
 *         Author: Mattia Peirano
 *          Email: mattia.peirano[at]centeropenmiddleware.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h> /* mmap() is defined in this header */
#include <fcntl.h>

#include "inout.h"

int read_structuredfile( char *datafilename, char **map, size_t *size){
	FILE * datafile, metafile;
	struct stat statbuf;
	char *tmp;


	/* open the input file */
	if ((datafile = open (datafilename, O_RDONLY)) < 0){
		printf ("can't open %s for reading", datafilename);
		exit(0);
	}

	if (fstat (datafile,&statbuf) < 0){
		printf ("fstat error: input file <<%s>>\n", datafilename);
		exit(0);
	}


	*map = (char *)mmap (0, statbuf.st_size, PROT_READ, MAP_SHARED, datafile, 0);
	/* mmap the input file */
	if (*map == (caddr_t) -1){
		printf ("mmap error for input");
		exit(0);
	}

	*size = statbuf.st_size;
	return 1;
}



int writestructuredfile( char *datafilename, char *metafilename, packet *head ){
	FILE * datafile, metafile;
	struct stat statbuf;
	char *map;

	if ((datafile = open (datafilename, O_RDONLY)) < 0)
		printf ("can't open %s for reading", datafilename);

	return 0;
}

int dump_packet(char *buffer, hashptr *hashlist, uint16_t new_size, uint16_t number_of_hashes, FILE * metadatafile, FILE *outputfile, char *originfilename, int session){
	hashptr *tmp;
	int writed = 0;

	// Write the packet header
	fwrite(&number_of_hashes, 1, sizeof(uint16_t), outputfile); // Number of Hashes in the packet
 	writed += sizeof(uint16_t);
	while(hashlist != NULL){ // List of Hashes
		fwrite(&hashlist->position, sizeof(uint16_t), 1, outputfile);
		writed += sizeof(uint16_t);
		tmp=hashlist;
		hashlist = hashlist->next;
		free(tmp);
	}
	// Write the metadata with packet size
	fwrite(buffer,new_size,1,outputfile);
	writed += new_size;
	fprintf(metadatafile, "%s\t%u\t%d\n", originfilename, new_size + sizeof(uint16_t) + (number_of_hashes * sizeof(uint16_t)), session);
	return writed;
}

int dump_metadata(char *out_packet, uint16_t out_size, FILE * metadatafile, FILE *outputfile, char *originfilename, int session){
	hashptr *tmp;
	// Write the metadata with packet size
	fwrite(out_packet, out_size, 1, outputfile);
	fprintf(metadatafile, "%s\t%u\t%d\n", originfilename, out_size, session);
	return out_size;
}

int help() {
	printf("Usage: ./optimizer FILE1 FILE2\n");
	return 1;
}
