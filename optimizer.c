/*
 *optimizer.c
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
 *     Created on: Dec 2, 2013
 *         Author: Mattia Peirano
 *          Email: mattia.peirano[at]centeropenmiddleware.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "include/hashtable.h"
#include "include/inout.h"
#include "include/solowan.h"
#include "include/solowan_rolling.h"

//#define BASIC
//#define ROLLING
#define MIN_REQUIRED 2
#define NUMBER_OF_STRINGS 255
#define STRING_LENGTH 255

#define WANMESSAGE "wan_message"

int run_optimization( char *inputfile, as as, char *outputfilename, char *stats_file_name, int session){
	unsigned char *mmap;
	unsigned char *in_packet;
	size_t size = 0;
	size_t writed = 0; // Statistics: output file size
	uint16_t out_packet_size = 0;
	uint16_t in_packet_size = BUFFER_SIZE;
	size_t count = 0; 	//The number of bytes we already read in the file
	FILE *wan_message, *wan_metamessage, *stats;
	unsigned char out_packet[BUFFER_SIZE*2];
	int i = 0;
	int compressed = 0;
	char *wanmessage;
	char *wanmetamessage;

	if(outputfilename != NULL){
		wanmessage = (char *)malloc(sizeof(char)*64);
		sprintf(wanmessage,"%s",outputfilename);
		wanmetamessage = (char *)malloc(sizeof(char)*64);
		sprintf(wanmetamessage,"%s.meta",outputfilename);
	}else{
		printf("Error: output file name is NULL\n");
		exit(1);
	}

	printf("\n--------------------------------------------------\n");
	printf("START TRANSMISSION: %s", wanmessage);
	printf("\n--------------------------------------------------\n");

	// Read input file with mmap
	read_structuredfile(inputfile, &mmap, &size);

	wan_message = fopen(wanmessage, "a+");
	if(!wan_message){
		printf("Error opening file.\n");
		exit(1);
	}

	stats = fopen(stats_file_name, "a+");
	if(!stats){
		printf("Error opening file.\n");
		exit(1);
	}

	wan_metamessage = fopen(wanmetamessage, "a+");
	if(!wan_message){
		printf("Error opening file.\n");
		exit(1);
	}

	while(size - count > in_packet_size){// LOOP OVER PACKETS
		// We have a packet: we need to split it in chunks and cache it
		in_packet = mmap + count; // Pointer to the beginning of the packet

#ifdef BASIC
		optimize(as,  in_packet, in_packet_size, out_packet, &out_packet_size);
#endif

#ifdef ROLLING
		dedup(in_packet, in_packet_size, out_packet, &out_packet_size);
#endif
		compressed = out_packet_size < in_packet_size;
		if (compressed)
			writed += dump(out_packet, out_packet_size, wan_metamessage, wan_message, inputfile, session, compressed); // Write the packet to file
		else
			writed += dump(in_packet, out_packet_size, wan_metamessage, wan_message, inputfile, session, compressed); // Write the packet to file
		count += in_packet_size; // Increment count of a packet size
	} /* END LOOP OVER PACKETS */

	// Handle the rest of the file
	if(size - count > 0){
#ifdef BASIC
		optimize(as,  mmap + count, size - count, out_packet, &out_packet_size);
#endif

#ifdef ROLLING
		dedup(mmap + count, size - count, out_packet, &out_packet_size);
#endif
		compressed = out_packet_size < size - count;
		if (compressed)
			writed += dump(out_packet, out_packet_size, wan_metamessage, wan_message, inputfile, session, compressed); // Write the packet to file
		else
			writed += dump(mmap + count, size - count, wan_metamessage, wan_message, inputfile, session, compressed); // Write the packet to file
	}

	fprintf(stats,"%s\t%lu\t%lu\n", inputfile, size, writed);
	fclose(wan_metamessage);
	fclose(wan_message);
	fclose(stats);
	i = munmap(mmap, size); // Released mapped memory
	if(i==-1){
		printf("Error unmapping memory!!");
	}
	free(wanmessage);
	free(wanmetamessage);

	printf("\n--------------------------------------------------\n");
	printf("END TRANSMISSION");
	printf("\n--------------------------------------------------\n");

	return 0;
}

int main (int argc, char ** argv){
	hashtable as_optimizer = NULL;
	int i, c, j;
	FILE *stats = NULL;
	FILE *outputfile = NULL, *metaoutputfile = NULL;
	char statistics[100] = "";
	char files[NUMBER_OF_STRINGS][STRING_LENGTH]; /* files to read */
	//	char file[STRING_LENGTH]; /* files to read */
	int numfiles = 0;
	char *output = NULL;
	char metaoutput[105];
	int repeated = 1, session = 0;

	if (argc < MIN_REQUIRED) {
		return help_optimizer();
	}
#ifdef BASIC
	// Create a table of NREG positions: library 01dedup.h
	i = create_hashmap(&as_optimizer);
	if(i){
		printf("Table created \n");
	}else{
		printf("Error creating table: EXIT! \n");
		exit(1);
	}
#endif

#ifdef ROLLING
	init_dedup();
#endif

	while (1)
	{
		static struct option long_options[] =
		{
				{"verbose", no_argument,       0, 'v'},
				{"dir",     required_argument, 0, 'd'},
				{"file",    required_argument, 0, 'f'},
				{"mode",    required_argument, 0, 'm'},
				{"repeat",  required_argument, 0, 'r'},
				{"output",  required_argument, 0, 'o'},
				{"stats",   required_argument, 0, 's'},
				{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long (argc, argv, "vd:f:m:r:o:s:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{

		case 'v':
			printf ("option -v\n");
			break;

		case 'd':
			printf ("option -d with value '%s'\n", optarg);
			break;

		case 'f':
			printf ("option -f with value '%s'\n", optarg);
			strcpy (files[numfiles], optarg);
			numfiles++;
			break;

		case 'm':
			printf ("option -m with value '%s'\n", optarg);
			break;

		case 'r':
			printf ("option -r with value '%s'\n", optarg);
			repeated = atoi(optarg);
			break;

		case 'o':
			printf ("option -o with value '%s'\n", optarg);
			output = malloc(sizeof(optarg)+1);
			strcpy (output, optarg);
			break;

		case 's':
			printf ("option -s with value '%s'\n", optarg);
			strcpy (statistics, optarg);
			break;

		case '?':
			/* getopt_long already printed an error message. */
			exit (1);
			break;

		default:
			abort ();
		}
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc)
	{
		printf ("ERROR: non-option ARGV-elements: ");
		while (optind < argc)
			printf ("%s ", argv[optind++]);
		putchar ('\n');
		exit (1);
	}

	printf("----------------\n");


	printf("Files to read: ");
	for(i = 0; i < numfiles; i++) printf("%s ",files[i]);
	printf("\n");

	printf ("output file = %s \n", output);


	if (!strcmp(statistics, "")){
		strcpy(statistics, "statistics.csv");
	}

	//	// Removing header between transmissions
	//	stats = fopen(statistics, "r");
	//	if(!stats){
	//		stats = fopen(statistics, "w");
	//		fprintf(stats,"File\tRead\tWrite\n");
	//		fclose(stats);
	//	}else{
	//		fclose(stats);
	//	}
	//
	stats = fopen(statistics, "a+");
	if(!stats){
		printf("Error opening file.\n");
		exit(1);
	}
	fprintf(stats,"File\tRead\tWrite\n");
	fclose(stats);

	if (output == NULL){
		output = malloc(sizeof(WANMESSAGE));
		strcpy(output, WANMESSAGE);
	}

	outputfile = fopen(output,"w+");
	fclose(outputfile);

	sprintf(metaoutput,"%s.meta",output);

	metaoutputfile = fopen(metaoutput,"w+");
	if(!metaoutputfile){
		printf("Error opening file.\n");
		exit(1);
	}
	fclose(metaoutputfile);

	for(i = 0; i < numfiles; i++){
		for(j = 1; j <= repeated; j++){ //Repeating the tx of each file 'r' times
			run_optimization(files[i], as_optimizer, output, statistics, session++);
		}
	}

	free(output);

#ifdef BASIC
	i = remove_table(as_optimizer);
	if(i==0){
		printf("Error deleting table!!");
	}else{
		printf("\nTable deleted.\n");
	}
#endif

	return 0;
}
