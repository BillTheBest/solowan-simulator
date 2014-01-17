/*
 * hashtable.h
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
 *     Created on: Nov 14, 2013
 *         Author: Mattia Peirano
 *          Email: mattia.peirano[at]centeropenmiddleware.com
 */

#ifndef DEDUP_H_
#define DEDUP_H_

#include <stdint.h>
#include "as.h"

#define BUFFER_SIZE 1460
#define CHUNK 400

#define NREG 20000000
#define NORMALIZE(hash) hash%NREG

#define LOG_DEDUP 0

typedef as hashtable;

int create_hashmap(hashtable *as);

/*
 * Check functions: it checks the presence of a chunk in the
 * hash table pointed by *as.
 *
 * It returns:	1 if the chunk exists
 * 				0 if not
 * */
int check(hashtable *as, uint32_t hash);

/*
 * Deduplicate function: it writes the chunk in the hash table
 * using the hash parameter as a key.
 *
 * It returns:	1 if it successes
 * 				0 if not
 *
 * If the chunk exists, the content is put in the retrieved_chunk buffer
 *
 * */
int put_block(hashtable *as, char *buffer, uint32_t hash);

/*
 * Regenerate function: it checks the presence of a chunk in the
 * hash table pointed by *as and returns the content.
 *
 * It returns:	1 if the chunk exists
 * 				0 if not
 *
 * If the chunk exists, the content is put in the retrieved_chunk buffer
 *
 * */
int get_block(uint32_t hash, hashtable *as, char *retrieved_chunk);

int check_collision(hashtable *as, char *block_ptr, uint32_t hash);

int remove_table(hashtable as);

#endif /* 01DEDUP_H_ */
