#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include <stdint.h>
#include <string.h>
#include "solowan_rolling.h"
#include "MurmurHash3.h"

#ifdef DEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

static FPStore fps;
static PStore ps;

#define SEED 0x8BADF00D
#define BYTE_RANGE 256
// Auxiliary table for calculating fingerprints
static uint64_t fpfactors[BYTE_RANGE][BETA];

// Packet Counter
static uint64_t pktId = 1; // 0 means empty FPEntry

uint16_t ntoh16(unsigned char *p) {
	uint16_t res = *p++;
	return (res << 8) + *p;
}

uint32_t ntoh32(unsigned char *p) {
	uint32_t res = *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	return res;
}

uint64_t ntoh64(unsigned char *p) {
	uint64_t res = *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	res = (res << 8) + *p++;
	return res;
}

// Initialization tasks
void init_uncomp(void) {

	int i, j;

	// Initiaize auxiliary table for calculating fingerprints
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

// update_caches takes an incoming uncompressed packet (packet, pktlen) and updates fingerprint pointers and packet cache
// PENDING: behaviour when a compressed packet includes uncached fingerprints
// PENDING: think of handling the same cache on both sides (merge with dedup.c)

void update_caches(unsigned char *packet, uint16_t pktlen) {

	FPEntry pktFps[MAX_FP_PER_PKT];
	uint64_t tentativeFP;
	int fpNum = 1;
	int exploring;
	int endLoop;
	int i;
	uint32_t hash, computedPacketHash;
	FPEntry *fpp;

	// Store packet in PS
	// PENDING: What if already in PS (rtx, identical content) ?
	uint32_t pktIdx = (uint32_t) (pktId % PKT_STORE_SIZE);
	DBG(("Estado: pktIdx %" PRIu32 "\n",pktIdx));
	memcpy(ps[pktIdx].pkt,packet,pktlen);
	ps[pktIdx].len = pktlen;
	MurmurHash3_x86_32  (packet, pktlen, SEED, (void *) &computedPacketHash);
	ps[pktIdx].hash = computedPacketHash;

	// Calculate initial fingerprint
	pktFps[0].fp = full_rfp(packet);
	pktFps[0].pktId = pktId;
	pktFps[0].offset = 0;

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
		}
		endLoop = ((++exploring >= pktlen) || (fpNum == MAX_FP_PER_PKT));
	}

	for (i=0; i<fpNum; i++) {
		hash = hashFPStore(pktFps[i].fp);
		fpp = &fps[hash];
		// Anyway, empty obsolete entries
		if ((pktId >= PKT_STORE_SIZE) && (fpp->pktId < pktId - PKT_STORE_SIZE)) {
			fpp->pktId = 0;
		}

		// If not in FPStore, store it
		// It is not in store if the entry is empty or has been obsoleted
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
}

// Uncompress received optimized packet
// Input parameter: optpkt (pointer to an array of unsigned char holding an optimized packet).
// Input parameter: optlen (actual length of optimized packet -- 16 bit unsigned integer).
// Output parameter: packet (pointer to an array of unsigned char holding the uncompressed packet). VERY IMPORTANT: THE CALLER MUST ALLOCATE THIS ARRAY
// Output parameter: pktlen (actual length of uncompressed packet -- pointer to a 16 bit unsigned integer)
// Output parameter: status (pointer to UncompReturnStatus, holding the return status of the call. Must be allocated by caller.)
//                      status.code == UNCOMP_OK                packet successfully uncompressed, packet and pktlen output parameters updated
//                      status.code == UNCOMP_FP_NOT_FOUND      packet unsuccessfully uncompressed due to a fingerprint not found or not matching stored hash
//                                                              packet and pktlen not modified
//                                                              status.fp and status.hash hold the missed values
// This function also calls update_caches when the packet is successfully uncompressed, no need to call update_caches externally

extern void uncomp(unsigned char *packet, uint16_t *pktlen, unsigned char *optpkt, uint16_t optlen, UncompReturnStatus *status) {


	uint64_t tentativeFP;
	uint32_t hash;
	uint32_t tentativePktHash;
	uint16_t offset;
	uint16_t orig = 0;
	FPEntry *fpp;
	uint16_t left, right;

#ifdef DEBUG
	FILE *fpdebug;
#endif

	int fpproc = 0;

	DBG(("Entrando en uncomp optlen %d\n",optlen));
	DBG(("Estado: pktId %" PRIu64 "\n",pktId));

#ifdef DEBUG
	fpdebug=fopen("depurando.uncomp.txt","a");
	fprintf(fpdebug,"pktId: %" PRIu64 " ",pktId);
#endif

	*pktlen=0;

	// Compressed packet format:
	//     Short int (network order) offset from end of this header to first FP descriptor
	// Compressed data: byte sequence of uncompressed chunks (variable length) and interleaved FP descriptors
	// FP descriptors pointed by the header, fixed format and length:
	//     64 bit FP (network order)
	//     32 bit packet hash (network order)
	//     16 bit left limit (network order)
	//     16 bit right limit (network order)
	//     16 bit offset from end of this header to next FP descriptor (if all ones, no more descriptors)
	// Check fingerprints in FPStore

	offset = ntoh16(optpkt);
	optpkt += sizeof(uint16_t);
	optlen -= sizeof(uint16_t);
	if (offset > 0) {
		memcpy(packet+orig,optpkt,offset);
#ifdef DEBUG
		fprintf(fpdebug," UC %d-%d FPD ofs %d ",orig, orig+offset -1, offset);
#endif
		orig += offset;
		*pktlen += offset;
		optpkt += offset;
		optlen -= offset;
	}

	while (optlen > 0) {
		fpproc++;
		tentativeFP = ntoh64(optpkt);
#ifdef DEBUG
		fprintf(fpdebug,"fp %" PRIx64 "  ",tentativeFP);
#endif
		optpkt += sizeof(uint64_t);
		optlen -= sizeof(uint64_t);

		tentativePktHash = ntoh32(optpkt);
#ifdef DEBUG
		fprintf(fpdebug,"hash %d ",tentativePktHash);
#endif
		optpkt += sizeof(uint32_t);
		optlen -= sizeof(uint32_t);

		hash = hashFPStore(tentativeFP);
		fpp = &fps[hash];

		// if (fpp->fp != tentativeFP) printf("HORROR HORROR HORROR stored %" PRIx64 " tentative %" PRIx64 "\n",fpp->fp, tentativeFP);
		if ((fpp->fp != tentativeFP) || (ps[fpp->pktId % PKT_STORE_SIZE].hash != tentativePktHash)) {
			status->code = UNCOMP_FP_NOT_FOUND;
			status->fp = tentativeFP;
			status->hash = tentativePktHash;
			return;
		}


		left = ntoh16(optpkt);
#ifdef DEBUG
		fprintf(fpdebug,"left %d ",left);
#endif
		optpkt += sizeof(uint16_t);
		optlen -= sizeof(uint16_t);

		right = ntoh16(optpkt);
#ifdef DEBUG
		fprintf(fpdebug,"right %d ",right);
#endif
		optpkt += sizeof(uint16_t);
		optlen -= sizeof(uint16_t);

		unsigned char *zzz5;
		unsigned int idxx = fpp->pktId % PKT_STORE_SIZE;
		zzz5 = ps[idxx].pkt;
		memcpy(packet+orig,zzz5+left,right-left+1);
		orig += right-left+1;
		*pktlen += right-left+1;

		offset = ntoh16(optpkt);
#ifdef DEBUG
		fprintf(fpdebug,"ofs %d ",offset);
#endif
		optpkt += sizeof(uint16_t);
		optlen -= sizeof(uint16_t);
		if (offset == 0xffff) {
			memcpy(packet+orig,optpkt,optlen);
#ifdef DEBUG
			fprintf(fpdebug,"UC %d-%d ",orig,orig+optlen-1);
#endif
			orig += optlen;
			*pktlen += optlen;
			optlen = 0;
		} else {
			memcpy(packet+orig,optpkt,offset);
#ifdef DEBUG
			fprintf(fpdebug,"UC %d-%d ",orig,orig+offset-1);
#endif
			orig += offset;
			*pktlen += offset;
			optpkt += offset;
			optlen -= offset;
		}
	}

#ifdef DEBUG
	fprintf(fpdebug,"\n");
	fclose(fpdebug);
#endif

	assert(*pktlen <= MAX_PKT_SIZE);
	assert(orig <= MAX_PKT_SIZE);
	update_caches(packet,*pktlen); // At present this is extremely inefficient
	status->code = UNCOMP_OK;
}
