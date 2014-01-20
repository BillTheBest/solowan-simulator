/*
 * solowan_rolling.c
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

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include <stdint.h>
#include <string.h>
#include "solowan_rolling.h"

#ifdef DEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

static FPStore fps;
static PStore ps;

#define BYTE_RANGE 256
// Auxiliary table for calculating fingerprints
static uint64_t fpfactors[BYTE_RANGE][BETA];

// Packet Counter
static uint64_t pktId = 1; // 0 means empty FPEntry

void hton16(unsigned char *p, uint16_t n) {
	*p++ = (n >> 8) & 0xff;
	*p = n & 0xff;
}

void hton64(unsigned char *p, uint64_t n) {
	*p++ = (n >> 56) & 0xff;
	*p++ = (n >> 48) & 0xff;
	*p++ = (n >> 40) & 0xff;
	*p++ = (n >> 32) & 0xff;
	*p++ = (n >> 24) & 0xff;
	*p++ = (n >> 16) & 0xff;
	*p++ = (n >> 8) & 0xff;
	*p = n & 0xff;
}

// Initialization tasks
void init_dedup(void) {

	int i, j;

	// Initialize auxiliary table for calculating fingerprints
	for (i=0; i<BYTE_RANGE; i++) {
		fpfactors[i][0] = i;
		for (j=1; j<BETA; j++) {
			fpfactors[i][j] = (fpfactors[i][j-1]*P) & MOD_MASK;
		}
	}

	// Initialize FPStore
	for (i=0; i<FP_STORE_SIZE; i++) fps[i].pktId = 0;

}


// Hash to index FPStore
static uint32_t hashFPStore(uint64_t fp) {

	fp = fp >> GAMMA;
	fp &= (FP_STORE_SIZE-1);
	return (uint32_t) (fp & 0xffffffff);

}

// Full calculation of the initial Rabin fingerprint
static uint64_t full_rfp(unsigned char *p) {
	int i;
	uint64_t fp = 0;
	for (i=0;i<BETA;i++) {
		fp = (fp + fpfactors[p[i]][BETA-i-1]) & MOD_MASK;
	}
	return fp;
}

// Incremental calculation of a Rabin fingerprint
static uint64_t inc_rfp(uint64_t prev_fp, unsigned char new, unsigned char dropped) {
	uint64_t fp;
	fp = ((prev_fp - fpfactors[dropped][BETA-1])*P + new) & MOD_MASK;
	return fp;

}

// dedup takes an incoming packet (packet, pktlen), processes it and, if some compression is possible,
// outputs the compressed packet (optpkt -- must be allocated by the caller, optlen). If no compression is possible, 
// optlen is the same as pktlen
void dedup(unsigned char *packet, uint16_t pktlen, unsigned char *optpkt, uint16_t *optlen) {

#ifdef DEBUG
	FILE *fpdebug;
#endif

	FPEntry pktFps[MAX_FP_PER_PKT];
	uint64_t tentativeFP;
	int fpNum = 1;
	int exploring;
	int endLoop;
	int i;
	uint32_t hash;
	FPEntry *fpp;
//	static int printed = 0;

	DBG(("Entrando en dedup pktlen %d\n",pktlen));
	DBG(("Estado: pktId %" PRIu64 "\n",pktId));

#ifdef DEBUG
	fpdebug = fopen("depurando.comp.txt","a");
#endif

	// Default return values, no changes
	*optlen = pktlen;

	// Do not optimize packets shorter than length of fingerprinted strings
	if (pktlen < BETA) {
		DBG(("Saliendo de dedup por longitud insuficiente %d\n",pktlen));
		return;
	}

	// Store packet in PS
	// PENDING: What if already in PS (rtx, identical content) ?
	uint32_t pktIdx = (uint32_t) (pktId % PKT_STORE_SIZE);
	DBG(("Estado: pktIdx %" PRIu32 "\n",pktIdx));
	memcpy(ps[pktIdx].pkt,packet,pktlen);
	ps[pktIdx].len = pktlen;

	// Calculate initial fingerprint
	pktFps[0].fp = full_rfp(packet);
	pktFps[0].pktId = pktId;
	pktFps[0].offset = 0;

	DBG(("Initial fingerprint %" PRIx64 "\n",pktFps[0].fp));

	// Calculate other relevant fingerprints
	tentativeFP = pktFps[0].fp;
	exploring = BETA;
	endLoop = (exploring >= pktlen);
	while (!endLoop) {
		tentativeFP = inc_rfp(tentativeFP, packet[exploring], packet[exploring-BETA]);
		if ((tentativeFP & SELECT_FP_MASK) == 0) {
			pktFps[fpNum].fp = tentativeFP;
			pktFps[fpNum].pktId = pktId;
			pktFps[fpNum].offset = exploring-BETA+1;
			fpNum++;
			DBG(("Other fingerprint %" PRIx64 " pktId %" PRIu64 " offset %d new %d old %d\n",tentativeFP,pktId,pktFps[fpNum-1].offset,packet[exploring],packet[exploring-BETA]));
		}
		endLoop = ((++exploring >= pktlen) || (fpNum == MAX_FP_PER_PKT));
	}

	DBG(("Estado: fpNum %d exploring %d\n",fpNum, exploring));

	// Compressed packet format:
	//     Short int (network order) offset from end of this header to first FP descriptor
	// Compressed data: byte sequence of uncompressed chunks (variable length) and interleaved FP descriptors
	// FP descriptors pointed by the header, fixed format and length:
	//     64 bit FP (network order)
	//     16 bit left limit (network order)
	//     16 bit right limit (network order)
	//     16 bit offset from end of this header to next FP descriptor (if all ones, no more descriptors)
	// Check fingerprints in FPStore


#ifdef DEBUG
	fprintf(fpdebug,"pktId: %" PRIu64 " ",pktId);
#endif

	int orig, dest;
	void *poffsetFPD, *pliml, *plimr;
	void *poffsetFP;
	poffsetFPD = (void *) optpkt;
	orig = 0;
	dest = sizeof(uint16_t);
	for (i=0; i<fpNum; i++) {
		DBG(("Iteración %d\n",i));
		hash = hashFPStore(pktFps[i].fp);
		DBG(("Hash %" PRIu32 "\n",hash));
		fpp = &fps[hash];

		if (fpp->pktId != 0) {
			// Check if it is in store or is there has been a collision
			if (fpp->fp == pktFps[i].fp) {
				// It is in store, check if contents are the same
				uint32_t ref = fpp->pktId % PKT_STORE_SIZE;
				int ofs1 = pktFps[i].offset;
				int ofs2 = fpp->offset;
				unsigned char *storedPacket;
				storedPacket = (unsigned char *) &ps[ref].pkt[0];

				if (!memcmp(packet+ofs1,storedPacket+ofs2,BETA)) {

					// If already covered, skip
					if (orig > ofs1) {
						continue;
					}

					// Contents match, dedup content
					// Explore full matching string
					int liml = (ofs1-orig < ofs2) ? ofs1-orig : ofs2;
					int deltal = 0;
					while ((deltal < liml) && (packet[ofs1-deltal] == storedPacket[ofs2-deltal])) deltal++;
					deltal--;
					int limr = (pktlen - ofs1 < ps[ref].len - ofs2) ? pktlen - ofs1 - BETA : ps[ref].len - ofs2 - BETA;
					int deltar = 0;
					while ((deltar < limr) && (packet[ofs1+deltar+BETA-1] == storedPacket[ofs2+deltar+BETA-1])) deltar++;
					deltar--;

					// Copy uncompressed chunk before matching bytes
					memcpy(optpkt+dest,packet+orig,ofs1-deltal-orig);

#ifdef DEBUG
					fprintf(fpdebug,"UC %d-%d ",orig, ofs1-deltal-1);
#endif

					assert (ofs1 >= deltal+orig);
					hton16(poffsetFPD,ofs1-deltal-orig);

#ifdef DEBUG
					fprintf(fpdebug,"FPD ofs %d ",ofs1-deltal-orig);
#endif

					dest += ofs1-deltal-orig;

					poffsetFP = (void *) optpkt+dest;
					hton64(poffsetFP, fpp->fp);

#ifdef DEBUG
					fprintf(fpdebug,"fp %" PRIx64 " " ,fpp->fp);
#endif

					dest += sizeof(uint64_t);
					pliml = (void *) optpkt+dest;

#ifdef DEBUG
					fprintf(fpdebug,"liml %d ",ofs2-deltal);
#endif

					hton16(pliml,ofs2-deltal);
					dest += sizeof(uint16_t);

					plimr = (void *) optpkt+dest;
					assert(ofs2+deltar+BETA-1 < MAX_PKT_SIZE);
					hton16(plimr,ofs2+deltar+BETA-1);

#ifdef DEBUG
					fprintf(fpdebug,"liml %d ",ofs2+deltar+BETA-1);
#endif

					dest += sizeof(uint16_t);
					poffsetFPD = (void *) optpkt+dest;
					hton16(poffsetFPD,0xffff);
					orig = ofs1+deltar+BETA;
					dest += sizeof(uint16_t);
					*optlen = dest;

					if (orig + BETA > pktlen) {
#ifdef DEBUG
						fprintf(fpdebug," UC %d-%d ",orig,pktlen-1);
#endif
						memcpy(optpkt+dest,packet+orig,pktlen-orig);
						*optlen += pktlen-orig;
						orig = pktlen;
						break;
					}


				}  else {
					DBG(("FPs iguales, pero memcmp distinto\n"));
				}
			} else {
				DBG(("FPs distintos\n"));
			}

		}
	}

	if (orig < pktlen && *optlen != pktlen) {
		memcpy(optpkt+dest,packet+orig,pktlen-orig);
#ifdef DEBUG
		fprintf(fpdebug," UC %d-%d ",orig,pktlen-1);
#endif
		*optlen += pktlen - orig;
	}

#ifdef DEBUG
	fprintf(fpdebug,"\n");
	fclose(fpdebug);
#endif

	for (i=0; i<fpNum; i++) {
		hash = hashFPStore(pktFps[i].fp);
		fpp = &fps[hash];
		if ((pktId >= PKT_STORE_SIZE) && (fpp->pktId < pktId - PKT_STORE_SIZE)) {
			DBG(("Entrada obsoleta hash %" PRIu32 " pktId %" PRIu64 " fpp->pktId %" PRIu64 "\n",hash,pktId,fpp->pktId));
			fpp->pktId = 0;
		}
		if (fpp->pktId == 0) {
			fpp->fp = pktFps[i].fp;
			fpp->pktId = pktFps[i].pktId;
			fpp->offset = pktFps[i].offset;
		} else if ((fpp->fp == pktFps[i].fp) && !memcmp(ps[fpp->pktId % PKT_STORE_SIZE].pkt+fpp->offset,ps[pktFps[i].pktId % PKT_STORE_SIZE].pkt+pktFps[i].offset,BETA)) {
			fpp->pktId = pktFps[i].pktId;
			fpp->offset = pktFps[i].offset;
		}
	}
	pktId++;

	assert (*optlen <= MAX_PKT_SIZE);
	assert (orig <= MAX_PKT_SIZE);
	DBG(("Saliendo de dedup pktlen %d optlen %d\n",pktlen,*optlen));
}
