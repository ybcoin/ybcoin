#ifndef SCRYPT_MINE_H
#define SCRYPT_MINE_H

#include <stdint.h>
#include <stdlib.h>

#include "util.h"
#include "net.h"

/*typedef struct
{
    unsigned int version;
    uint256 prev_block;
    uint256 merkle_root;
    unsigned int timestamp;
    unsigned int bits;
    unsigned int nonce;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->version);
        version = this->version;
        READWRITE(prev_block);
        READWRITE(merkle_root);
        READWRITE(timestamp);
        READWRITE(bits);
        READWRITE(nonce);
    )

} block_header;*/

class CBlockHeader;

void *scrypt_buffer_alloc();
void scrypt_buffer_free(void *scratchpad);

unsigned int scanhash_scrypt(CBlockHeader *pdata,
    uint32_t max_nonce, uint32_t &hash_count,
    void *result, CBlockHeader *res_header, unsigned char Nfactor);

void scrypt_hash(const void* input, size_t inputlen, uint32_t *res, unsigned char Nfactor);

#endif // SCRYPT_MINE_H
