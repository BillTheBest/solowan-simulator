/*
 * solowan_rolling.h
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
 *     Created on: Nov 18, 2013
 *         Author: Francisco Javier Ruiz Piñar
 *          Email: javier.ruiz@centeropenmiddleware.com
 */


#include <stdint.h>

// This is the header file for the de-duplicator code (dedup.c) and the uncompressor code (uncomp.c)
// De-duplication is based on the algorithm described in:
// A Protocol-Independent Technique for Eliminating Redundant Network Traffic. 
// Neil T. Spring and David Wetherall. 
// Proceedings SIGCOMM '00 
// Proceedings of the conference on Applications, Technologies, Architectures, and Protocols for Computer Communication.
// READ THAT ARTICLE BEFORE TRYING TO UNDERSTAND THIS CODE

// Fingerpint store has a size FPS_FACTOR greater than the maximum number of fingerprints (FP) stored.
// A hash table should not be too crowded
#define FPS_FACTOR 3

// We calculate MAX_FP_PER_PKT Rabin fingerprints in each packet
// At present, the fingerprint selection algorithm is very simple: 
// The first MAX_FP_PER_PKT fingerprints which have the lowest GAMMA (see below) bits equal to 0 are chosen
// This algirithm is very quick, but performs poorly when there are too many repeated bytes on the packet, as then there will be many identical consecuive fingerprints
// Winnowing should be a much better algortihm for chosing fingerprints. FUTURE WORK.

#define MAX_FP_PER_PKT 32

// This is the size of the packet store. The greater, the better for detecting data redundancy, but it needs more RAM memory
#define PKT_STORE_SIZE (1048576 / 8)

// Size of FP store
#define FP_STORE_SIZE (MAX_FP_PER_PKT*PKT_STORE_SIZE*FPS_FACTOR)

// Maximum packet size
// The algorithm is agnostic on packet contents. They may include protocol headers, BUT:
// (SEE BELOW) There must be an external indicator in a received packet that allows the receiver to know if the packet is optimized or not
// This indicator may be a flag in the TCP or IP headers
// OUR CODE CANNOT TELL IF A PACKET IS OPTIMIZED, OR NOT, BASED SOLELY ON ITS CONTENTS
#define MAX_PKT_SIZE 1500

// 2^M is the base of the fingerprint modular arithmetic (see section 4.1 of article)
#define M 60

// Factor used in calculations of Rabin fingerprints (a large prime number) (see section 4.1 of article)
#define P 1048583

// Fingerprints are calculated on fingerprints of length = BETA (see figure 4.2 of article for justification of this value)
#define BETA 64

// See above: fingerprints are selected using a simple rule: their lowest GAMMA bits equal to 0 (see figure 4.2 of article for justification of this value)
#define GAMMA 5

// Auxiliary masks depending on M and GAMMA
#define SELECT_FP_MASK ((1<<GAMMA) -1)
#define MOD_MASK ((1L<<M)-1)

// Each packet is given an identifier, pktId
// According to article, a 64 bit integer is used to keep pktId
// 2^64 bits is a very, very large number
// No need for _modular_ increment of pktId
// At 1 Gpps (approx. 2^30 pps), we would need 2^34 seconds to wrap pktId values
// That is, more than five centuries before wrapping values
// This software won't run so much time
#define MAX_PKT_ID UINT64_MAX

// Type definition for the entries in the fingerprint store 
typedef struct {
	// Fingerprint value
	uint64_t fp;
	// pktId of the packet holding the string with the previous fingerprint
	uint64_t pktId;
	// offset (from the beginning of the packet) where the string begins
        uint16_t offset;
} FPEntry;

// Type definition for the fingerprint store
// We use static arrays for fingerprint and packet stores
// No need for mallocs and complicated memory management in our program
// BUT the host machine should have enough RAM
// AND SWAP SHOULD BE DISABLED 
typedef FPEntry FPStore[FP_STORE_SIZE];

// Type definition for a data packet
typedef unsigned char Pkt[MAX_PKT_SIZE];

// Type definition for the entries in the packet store
typedef struct {
	// Data packet
	Pkt pkt;
	// Actual packet length
        uint16_t len;
	// Packet hash
        uint32_t hash;
} PktEntry;


// Packet store
typedef PktEntry PStore[PKT_STORE_SIZE];

// Deduplication API (implemented in dedup.c)

// Uncompression return
#define	UNCOMP_OK		0
#define	UNCOMP_FP_NOT_FOUND	1

typedef struct {
	unsigned char code;
	uint64_t fp;
	uint32_t hash;
} UncompReturnStatus;

// Initialization tasks
extern void init_dedup(void);

// De-duplication function
// Input parameter: packet (pointer to an array of unsigned char holding the packet to be optimized)
// Input parameter: pktlen (actual length of packet -- 16 bit unsigned integer)
// Output parameter: optpkt (pointer to an array of unsigned char where the optimized packet contents are copied). VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY.
// Output parameter: optlen (actual length of optimized packet -- pointer to a 16 bit unsigned integer). 
// VERY IMPORTANT: IF OPTLEN IS THE SAME AS PKTLEN, NO OPTIMIZATION IS POSSIBLE AND OPTPKT HAS NO VALID CONTENTS
extern void dedup(unsigned char *packet, uint16_t pktlen, unsigned char *optpkt, uint16_t *optlen);

// Recovery function
// Input parameter: fp (uint64_t holding the fingerpint)
// Input parameter: hash (uint32_t holding the hash)
// Output parameter: recpkt (pointer to array of unsigned char where recovered content should be copied, if found. VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY.
// Output parameter: reclen (pointer to uint16_t where length of recovered content is returned
// VERY IMPORTANT: IF RECLEN IS 0, NO CONTENT HAS BEEN RECOVERED 
extern void recover(uint64_t fp, uint32_t hash, unsigned char *recpkt, uint16_t *reclen);

// Uncompression API (implemented in uncomp.c)

// Initialization tasks
extern void init_uncomp(void);

// Update uncompressor packet cache
// Input parameter: packet (pointer to an array of unsigned char holding an uncompressed received packet)
// Input parameter: pktlen (actual length of packt -- 16 bit unsigned integer)
extern void update_caches(unsigned char *packet, uint16_t pktlen);

// Uncompress received optimized packet
// Input parameter: optpkt (pointer to an array of unsigned char holding an optimized packet). 
// Input parameter: optlen (actual length of optimized packet -- 16 bit unsigned integer). 
// Output parameter: packet (pointer to an array of unsigned char holding the uncompressed packet). VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY
// Output parameter: pktlen (actual length of uncompressed packet -- pointer to a 16 bit unsigned integer)
// Output parameter: status (pointer to UncompReturnStatus, holding the return status of the call. Must be allocated by caller.)
//			status.code == UNCOMP_OK		packet successfully uncompressed, packet and pktlen output parameters updated 
//			status.code == UNCOMP_FP_NOT_FOUND	packet unsuccessfully uncompressed due to a fingerprint not found or not matching stored hash
//								packet and pktlen not modified
//								status.fp and status.hash hold the missed values
// This function also calls update_caches when the packet is successfully uncompressed, no need to call update_caches externally
extern void uncomp(unsigned char *packet, uint16_t *pktlen, unsigned char *optpkt, uint16_t optlen, UncompReturnStatus *status);

