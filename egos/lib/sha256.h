#ifndef _SHA256_H
#define _SHA256_H

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

typedef struct
{
    uint32 total[2];
    uint32 state[8];
    uint8 buffer[64];
}
sha256_context;

void sha256_starts( sha256_context *ctx );
void sha256_update( sha256_context *ctx, const uint8 *input, uint32 length );
void sha256_finish( sha256_context *ctx, uint8 digest[32] );

/* BEGIN ADDED BY RVR */

#define SHA256_SIZE		32

/* Size of an HMAC (SHA256 output).
 */
#define HMAC_SIZE		SHA256_SIZE

int sha256_vector(size_t num_elem, const uint8 *addr[], const size_t *len,
		  uint8 *mac);
int hmac_sha256(const uint8 *key, size_t key_len, const uint8 *data,
											size_t data_len, uint8 *mac);

/* END ADDED BY RVR */

#endif /* sha256.h */
