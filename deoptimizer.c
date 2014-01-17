/*
 * deoptimizer.c
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
#include <assert.h>
#include "include/hashtable.h"
#include "include/inout.h"
#include "include/solowan.h"
#include "include/solowan_rolling.h"

#define BASIC
//#define ROLLING
#define MIN_REQUIRED 3
#define WANMETAMESSAGE "wan_message.meta"
#define WANMESSAGE "wan_message"
#define OUTPUTFILE "outputfile"
#define NUMBER_OF_STRINGS 255
#define STRING_LENGTH 255

int run_deoptimization(char *inputfile, char *inputmetafile, hashtable as){

	unsigned char *mmap;
	unsigned char *packet_ptr;
	uint16_t size = 0;
	uint16_t input_packet_size=0;
	FILE *wan_metamessage, *outputfile;
	char *outputname = NULL;
	char filename[200], tmp[210];
	int session, compressed;
	unsigned char regenerated_packet[BUFFER_SIZE];
	uint16_t regenerated_packet_size;
	int i;

	/*
	 * Receiving part: we try to recompose the original file
	 *
	 * */
	printf("\n--------------------------------------------------\n");
	printf("RECEIVING PACKETS: %s",inputfile);
	printf("\n--------------------------------------------------\n");

	// Read input file with mmap
	read_structuredfile(inputfile, &mmap, &size);

	wan_metamessage = fopen(inputmetafile, "r");
	if(!wan_metamessage){
		printf("Error opening file %s\n",inputmetafile );
		exit(1);
	}

	packet_ptr = mmap;

	// This loop read line by line the metadata file
	while ( fscanf(wan_metamessage, "%s %hu %d %d\n", filename, &input_packet_size, &session, &compressed) != EOF ){ /* Read a line with packet specification*/
		if(compressed){
#ifdef BASIC
			deoptimize(as, packet_ptr, input_packet_size, regenerated_packet, &regenerated_packet_size);
#endif

#ifdef ROLLING
			uncomp(regenerated_packet, &regenerated_packet_size, packet_ptr, input_packet_size);
#endif
			sprintf(tmp, "%s.%d", filename, session);// Output file name depends on session
			outputfile = fopen(tmp, "a+");
			if(!outputfile){
				printf("Error opening file %s\n", outputname);
				exit(1);
			}
			fwrite(regenerated_packet, regenerated_packet_size , 1, outputfile);
			packet_ptr += input_packet_size;
			fclose(outputfile);
		}else{

#ifdef BASIC
			cache(as, packet_ptr, input_packet_size);
#endif

#ifdef ROLLING
			update_caches(packet_ptr,input_packet_size);
#endif

			sprintf(tmp, "%s.%d", filename, session);// Output file name depends on session
			outputfile = fopen(tmp, "a+");
			if(!outputfile){
				printf("Error opening file %s\n", outputname);
				exit(1);
			}
			fwrite(packet_ptr, input_packet_size , 1, outputfile);
			packet_ptr += input_packet_size;
			fclose(outputfile);
		}
	}
	fclose ( wan_metamessage);
	i = munmap(mmap, size); // Released mapped memory
	if(i==-1){
		printf("Error unmapping memory!!");

	}
	printf("\n--------------------------------------------------\n");
	printf("RECEPTION FINISHED");
	printf("\n--------------------------------------------------\n");

	return 0;
}

int main (int argc, char ** argv){
	int verbose = 0;
	hashtable as_deoptimizer;
	int i, c;
	char files[NUMBER_OF_STRINGS][STRING_LENGTH]; /* files to read */
	//	char file[STRING_LENGTH]; /* files to read */
	int numfiles = 0;
	char metafilename[100];

	if (argc < MIN_REQUIRED) {
		return help();
	}

#ifdef BASIC
	// Create a table of NREG positions: library 01dedup.h
	i = create_hashmap(&as_deoptimizer);
	if(i){
		printf("Table created \n");
	}else{
		printf("Error creating table: EXIT! \n");
		exit(1);
	}
#endif

#ifdef ROLLING
	init_dedup();
	init_uncomp();
#endif

	while (1)
	{
		static struct option long_options[] =
		{
				{"verbose", no_argument,       0, 'v'},
				{"dir",     required_argument, 0, 'd'},
				{"file",    required_argument, 0, 'f'},
				{"meta",    required_argument, 0, 'm'},
				{"output",  required_argument, 0, 'o'},
				{"stats",   required_argument, 0, 's'},
				{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long (argc, argv, "vd:f:m:o:s:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{

		case 'v':
			printf ("option -v\n");
			verbose = 1;
			break;

		case 'd':
			if(verbose)
				printf ("option -d with value '%s'\n", optarg);
			break;

		case 'f':
			if(verbose)
				printf ("option -f with value '%s'\n", optarg);
			strcpy (files[numfiles], optarg);
			numfiles++;
			break;

		case 'm':
			if(verbose)
				printf ("option -m with value '%s'\n", optarg);
			strcpy (metafilename, optarg);
			break;

		case 'o':
			if(verbose)
				printf ("option -o with value '%s'\n", optarg);
			break;

		case 's':
			if(verbose)
				printf ("option -s with value '%s'\n", optarg);
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

	if(verbose){
		printf("File to read: ");
		for(i = 0; i < numfiles; i++) printf("%s ",files[i]);
		printf("\n");
		printf ("Metadata file name file = %s \n", metafilename);
	}

	run_deoptimization(files[0], metafilename, as_deoptimizer); // We handle just one metadata file

#ifdef BASIC
	i = remove_table(as_deoptimizer);
	if(i==0){
		printf("Error deleting table!!");
	}else{
		printf("\nTable deleted.\n");
	}
#endif


	return 0;
}
