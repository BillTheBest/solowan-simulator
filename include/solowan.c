/*
 * solowan.c
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
 *     Created on: Dec 12, 2013
 *         Author: Mattia Peirano
 *          Email: mattia.peirano[at]centeropenmiddleware.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "murmurhash.h"
#include "dedup01.h"
#include "inout.h"
#include "solowan.h"

#define SEED 42

int optimize(hashtable as, char *buffered_packet, char *packet_ptr, size_t packet_size, size_t *new_packet_size, hashptr **hash_head, uint16_t *number_of_hashes){
	char *block_ptr;
	size_t readed; // Number of read bytes in a packet
	hashptr *hashpointer, *tail_hash;
	uint32_t hash, remaining;
	int i;

	block_ptr = packet_ptr;
	readed =0; // Number of read bytes in a packet
	*new_packet_size=0; // New packet size
	*hash_head = NULL;
	*number_of_hashes = 0;

	while(packet_size - readed >= CHUNK){ // LOOP OVER BLOCKS
		hashpointer = NULL;
		readed += CHUNK;
		//		hash = SuperFastHash (block_ptr, CHUNK); // Calculate the hash
		hash = MurmurHash2(block_ptr, CHUNK, SEED);
		// Check if the data is already present
		if(check(as,hash)){// If the chunk exists
			// Check if it is a collision
			if(!check_collision(as, block_ptr, hash)){ // It's a collision
				// Don't save the block and write the original message
				memcpy(buffered_packet + *new_packet_size, block_ptr, CHUNK); // Write to buffer
				*new_packet_size += CHUNK; // Update the counter with the block size
			}else{ // No collision = it's the same block
				(*number_of_hashes)++; // Add one to the number of hashes in the packet
				hashpointer = malloc(sizeof(hashptr)); // Allocate the memory for an element
				if(hashpointer == NULL){
					printf("Error allocating memory!\n");
					exit(1);
				}
				hashpointer->position = *new_packet_size; //Store the position of the hash
				hashpointer->next = NULL;
				// Enter the new hash to the end of the list
				if(*hash_head == NULL){
					*hash_head = hashpointer;
					tail_hash = hashpointer;
				}else{
					tail_hash->next = hashpointer;
					tail_hash = hashpointer;
				}
				memcpy(buffered_packet + *new_packet_size, &hash, sizeof(hash)); // Write to buffer
				*new_packet_size += sizeof(hash);
			}
		}else{ // The chunk does not exists
			i = put_block(as, block_ptr, hash); // Store the block
			if (!i){
				printf("ERROR: Deduplicating.");
				exit(1);
			}
			memcpy(buffered_packet + *new_packet_size, block_ptr, CHUNK); // Write to buffer
			*new_packet_size += CHUNK; // Update the counter with the block size
		}
		block_ptr += CHUNK;
		//			limit++;
	}/* END LOOP OVER CHUNKS INSIDE A PACKET */

	// Deal with the rest of the packet
	remaining = BUFFER_SIZE - readed; // So we still have data in the packet that could not fill a block
	if (remaining != 0){
		memcpy(buffered_packet + *new_packet_size, block_ptr, remaining); // Write to buffer
		new_packet_size += remaining;
	}


	return 0;
}

int optimize2(hashtable ht, char *in_packet, size_t in_packet_size, char *out_packet, size_t *out_packet_size){
	char *block_ptr;
	char buffer[BUFFER_SIZE*2];
	size_t readed, remaining; // Number of read bytes in a packet
	hashptr *hashpointer, *tail_hash;
	uint32_t hash;
	int i;

	block_ptr = in_packet;
	readed =0; // Number of read bytes in a packet
	*out_packet_size=0; // New packet size
	size_t buffer_size = 0;
	hashptr *hash_head = NULL;
	uint16_t number_of_hashes = 0;

	while(in_packet_size - readed >= CHUNK){ // LOOP OVER BLOCKS
		hashpointer = NULL;
		readed += CHUNK;
		//hash = SuperFastHash (block_ptr, CHUNK); // Calculate the hash
		hash = MurmurHash2(block_ptr, CHUNK, SEED);
		// Check if the data is already present
		if(check(ht,hash)){// If the chunk exists
			// Check if it is a collision
			if(!check_collision(ht, block_ptr, hash)){ // It's a collision
				// Don't save the block and write the original message
				memcpy(buffer + buffer_size, block_ptr, CHUNK); // Write to buffer
				buffer_size += CHUNK; // Update the counter with the block size
			}else{ // No collision = it's the same block
				number_of_hashes++; // Add one to the number of hashes in the packet
				hashpointer = malloc(sizeof(hashptr)); // Allocate the memory for an element
				if(hashpointer == NULL){
					printf("Error allocating memory!\n");
					exit(1);
				}
				hashpointer->position = buffer_size; //Store the position of the hash
				hashpointer->next = NULL;
				// Enter the new hash to the end of the list
				if(hash_head == NULL){
					hash_head = hashpointer;
					tail_hash = hashpointer;
				}else{
					tail_hash->next = hashpointer;
					tail_hash = hashpointer;
				}
				memcpy(buffer + buffer_size, &hash, sizeof(hash)); // Write to buffer
				buffer_size += sizeof(hash);
			}
		}else{ // The chunk does not exists
			i = put_block(ht, block_ptr, hash); // Store the block
			if (!i){
				printf("ERROR: Deduplicating.");
				exit(1);
			}
			memcpy(buffer + buffer_size, block_ptr, CHUNK); // Write to buffer
			buffer_size += CHUNK; // Update the counter with the block size
		}
		block_ptr += CHUNK;
		//			limit++;
	}/* END LOOP OVER CHUNKS INSIDE A PACKET */

	remaining = in_packet_size - readed; // So we still have data in the packet that could not fill a block
	if (remaining != 0){
		memcpy(buffer + buffer_size, block_ptr, remaining); // Write to buffer
		buffer_size += remaining;
	}

	hashptr *tmp;
	size_t writed = 0;

	//For opennop integration
	//	if(number_of_hashes != 0){

	// Write the packet header
	memcpy(out_packet, &number_of_hashes, sizeof(uint16_t));// Number of Hashes in the packet
	writed += sizeof(uint16_t);

	while(hash_head != NULL){ // List of Hashes
		memcpy(out_packet + writed, &hash_head->position, sizeof(uint16_t));
		writed += sizeof(uint16_t);
		tmp=hash_head;
		hash_head = hash_head->next;
		free(tmp);
	}
	//	}
	// Write the metadata with packet size
	memcpy(out_packet + writed, buffer, buffer_size);
	writed += buffer_size;

	*out_packet_size = writed;

	return 0;
}

int deoptimize(hashtable as, char *input_packet_ptr, size_t input_packet_size, char *regenerated_packet, size_t *output_packet_size){
	int i = 0;
	uint16_t n_hashes;
	uint16_t hash_position;
	char *data;
	uint16_t k;
	uint16_t handled = 0, remaining = 0;
	char received_chunk[CHUNK];
	uint32_t hash_ptr, hash;
	char *floating_ptr;
	size_t data_size = input_packet_size;

	floating_ptr = input_packet_ptr; // The floating_ptr points to the beginning of the packet
	memcpy(&n_hashes, floating_ptr, sizeof(uint16_t)); // Copy the number of hashes in the packet
	floating_ptr += sizeof(uint16_t); // Push the pointer forward
	data_size -= sizeof(uint16_t);
	data = floating_ptr + (n_hashes * sizeof(uint16_t)); // Data points to the beginning of the data (right after he header)
	k = 1; // Utility variable
	handled = 0; // Number of Bytes of a packet already handled
	*output_packet_size = 0; // Index over the regenerated buffer
	while(k <= n_hashes){
		data_size -= sizeof(uint16_t);
		memcpy(&hash_position, floating_ptr, sizeof(uint16_t)); // Read the position of the hash in the header
		floating_ptr += sizeof(uint16_t); // Push the pointer forward to the next hash pointer

		while(hash_position - handled >= CHUNK){ // It means that there is enough data to be hashed and stored
			//Store the chunk in the table
			//			hash = SuperFastHash (data + handled, CHUNK); // Calculate the hash
			hash = MurmurHash2(data + handled, CHUNK, SEED);
			if(check(as,hash)){// If the chunk exists
				// Do nothing: chunk is not stored
			}else{ // The chunk does not exist
				i = put_block(as, data + handled , hash); // Store the block
				if (!i){
					printf("ERROR: Deduplicating.");
					exit(1);
				}
			}
			memcpy(regenerated_packet + *output_packet_size, data + handled, CHUNK);
			*output_packet_size += CHUNK;
			handled += CHUNK;
		}

		assert(handled == hash_position);
		memcpy(&hash_ptr,data + hash_position,sizeof(uint32_t));
		handled += sizeof(uint32_t);
		// Retrieve the block
		i = get_block(hash_ptr,as, received_chunk);
		if(i==0){
			printf("Error regenerating the packet. Chunk not found!\n");
			exit(1);
		}
		memcpy(regenerated_packet + *output_packet_size, received_chunk, CHUNK);
		*output_packet_size += CHUNK;
		k++;
	}// End of hashes

	while(data_size - handled >= CHUNK){ // LOOP OVER THE REST OF THE PACKET
		//		hash = SuperFastHash (data + handled, CHUNK); // Calculate the hash
		hash = MurmurHash2(data + handled, CHUNK, SEED);
		if(check(as,hash)){// If the chunk exists
			// Do nothing: chunk is already present
		}else{ // The chunk does not exist
			i = put_block(as, data + handled, hash); // Store the block
			if (!i){
				printf("ERROR: Deduplicating.");
				exit(1);
			}
		}
		memcpy(regenerated_packet + *output_packet_size, data + handled, CHUNK);
		*output_packet_size += CHUNK;
		handled += CHUNK;
	}

	remaining = data_size - handled;
	memcpy(regenerated_packet + *output_packet_size, data + handled, remaining);
	*output_packet_size += remaining;
	handled += remaining;
	assert(handled == data_size);
	return 0;
}

