
#include "cpuminer-config.h"
#include "miner.h"
#include "magimath.h"

#include <gmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <sph_sha2.h>
#include <sph_keccak.h>
#include <sph_haval.h>
#include <sph_tiger.h>
#include <sph_whirlpool.h>
#include <sph_ripemd.h>

static void mpz_set_uint256(mpz_t r, uint8_t *u)
{
    mpz_import(r, 32 / sizeof(unsigned long), -1, sizeof(unsigned long), -1, 0, u);
}

static void mpz_get_uint256(mpz_t r, uint8_t *u)
{
    u=0;
    mpz_export(u, 0, -1, sizeof(unsigned long), -1, 0, r);
}

static void mpz_set_uint512(mpz_t r, uint8_t *u)
{
    mpz_import(r, 64 / sizeof(unsigned long), -1, sizeof(unsigned long), -1, 0, u);
}

static void set_one_if_zero(uint8_t *hash512) {
    for (int i = 0; i < 32; i++) {
        if (hash512[i] != 0) {
            return;
        }
    }
    hash512[0] = 1;
}

static bool fulltest_m7hash(const uint32_t *hash32, const uint32_t *target32)
{
    int i;
    bool rc = true;

    const unsigned char *hash = (const unsigned char *)hash32;
    const unsigned char *target = (const unsigned char *)target32;
    for (i = 31; i >= 0; i--) {
        if (hash[i] != target[i]) {
            rc = hash[i] < target[i];
            break;
        }
    }

    return rc;
}

#define BITS_PER_DIGIT 3.32192809488736234787
#define EPS (DBL_EPSILON)

#define NM7M 5
#define SW_DIVS 5
#define M7_MIDSTATE_LEN 76
int scanhash_m7m_hash(int thr_id, uint32_t *pdata, const uint32_t *ptarget,
    uint64_t max_nonce, unsigned long *hashes_done, bool fhash_test, bool fcpu_dec, struct timespec cpu_dec_time)
{
    uint32_t data[32] __attribute__((aligned(128)));
    uint32_t *data_p64 = data + (M7_MIDSTATE_LEN / sizeof(data[0]));
    uint32_t hash[8] __attribute__((aligned(32)));
    uint8_t bhash[7][64] __attribute__((aligned(32)));
    uint32_t n = pdata[19] - 1;
    const uint32_t first_nonce = pdata[19];
    char data_str[161], hash_str[65], target_str[65];
    uint8_t *bdata = 0;
    mpz_t bns[8];
    int rc = 0;
    int bytes, nnNonce2;

    mpz_t product;
    mpz_init(product);

    for(int i=0; i < 8; i++){
        mpz_init(bns[i]);
    }

    memcpy(data, pdata, 80);

    sph_sha256_context       ctx_final_sha256;

    sph_sha256_context       ctx_sha256;
    sph_sha512_context       ctx_sha512;
    sph_keccak512_context    ctx_keccak;
    sph_whirlpool_context    ctx_whirlpool;
    sph_haval256_5_context   ctx_haval;
    sph_tiger_context        ctx_tiger;
    sph_ripemd160_context    ctx_ripemd;

    sph_sha256_init(&ctx_sha256);
    sph_sha256 (&ctx_sha256, data, M7_MIDSTATE_LEN);
    
    sph_sha512_init(&ctx_sha512);
    sph_sha512 (&ctx_sha512, data, M7_MIDSTATE_LEN);
    
    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, data, M7_MIDSTATE_LEN);

    sph_whirlpool_init(&ctx_whirlpool);
    sph_whirlpool (&ctx_whirlpool, data, M7_MIDSTATE_LEN);
    
    sph_haval256_5_init(&ctx_haval);
    sph_haval256_5 (&ctx_haval, data, M7_MIDSTATE_LEN);

    sph_tiger_init(&ctx_tiger);
    sph_tiger (&ctx_tiger, data, M7_MIDSTATE_LEN);

    sph_ripemd160_init(&ctx_ripemd);
    sph_ripemd160 (&ctx_ripemd, data, M7_MIDSTATE_LEN);

    sph_sha256_context       ctx2_sha256;
    sph_sha512_context       ctx2_sha512;
    sph_keccak512_context    ctx2_keccak;
    sph_whirlpool_context    ctx2_whirlpool;
    sph_haval256_5_context   ctx2_haval;
    sph_tiger_context        ctx2_tiger;
    sph_ripemd160_context    ctx2_ripemd;

    do {
        data[19] = ++n;
        nnNonce2 = (int)(data[19]/2);
        memset(bhash, 0, 7 * 64);

        ctx2_sha256 = ctx_sha256;
        sph_sha256 (&ctx2_sha256, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_sha256_close(&ctx2_sha256, (void*)(bhash[0]));

        ctx2_sha512 = ctx_sha512;
        sph_sha512 (&ctx2_sha512, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_sha512_close(&ctx2_sha512, (void*)(bhash[1]));
        
        ctx2_keccak = ctx_keccak;
        sph_keccak512 (&ctx2_keccak, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_keccak512_close(&ctx2_keccak, (void*)(bhash[2]));

        ctx2_whirlpool = ctx_whirlpool;
        sph_whirlpool (&ctx2_whirlpool, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_whirlpool_close(&ctx2_whirlpool, (void*)(bhash[3]));
        
        ctx2_haval = ctx_haval;
        sph_haval256_5 (&ctx2_haval, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_haval256_5_close(&ctx2_haval, (void*)(bhash[4]));

        ctx2_tiger = ctx_tiger;
        sph_tiger (&ctx2_tiger, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_tiger_close(&ctx2_tiger, (void*)(bhash[5]));

        ctx2_ripemd = ctx_ripemd;
        sph_ripemd160 (&ctx2_ripemd, data_p64, 80 - M7_MIDSTATE_LEN);
        sph_ripemd160_close(&ctx2_ripemd, (void*)(bhash[6]));

        for(int i=0; i < 7; i++){
            set_one_if_zero(bhash[i]);
            mpz_set_uint512(bns[i],bhash[i]);
        }

        mpz_set_ui(bns[7],0);

        for(int i=0; i < 7; i++){
	        mpz_add(bns[7], bns[7], bns[i]);
        }

        mpz_set_ui(product,1);

        for(int i=0; i < 8; i++){
            mpz_mul(product,product,bns[i]);
        }

        mpz_pow_ui(product, product, 2); 

        bytes = mpz_sizeinbase(product, 256);
        bdata = (uint8_t *)realloc(bdata, bytes);
        mpz_export((void *)bdata, NULL, -1, 1, 0, 0, product);

        sph_sha256_init(&ctx_final_sha256);
        sph_sha256 (&ctx_final_sha256, bdata, bytes);
        sph_sha256_close(&ctx_final_sha256, (void*)(hash));

        int digits=(int)((sqrt((double)(nnNonce2))*(1.+EPS))/9000+75);
        int iterations=20;
        mpf_set_default_prec((long int)(digits*BITS_PER_DIGIT+16));

        mpz_t magipi;
        mpz_t magisw;
        mpf_t magifpi;
        mpf_t mpa1, mpb1, mpt1, mpp1;
        mpf_t mpa2, mpb2, mpt2, mpp2;
        mpf_t mpsft;

        mpz_init(magipi);
        mpz_init(magisw);
        mpf_init(magifpi);
        mpf_init(mpsft);
        mpf_init(mpa1);
        mpf_init(mpb1);
        mpf_init(mpt1);
        mpf_init(mpp1);

        mpf_init(mpa2);
        mpf_init(mpb2);
        mpf_init(mpt2);
        mpf_init(mpp2);

        uint32_t usw_;
        usw_ = sw_(nnNonce2, SW_DIVS);
        if (usw_ < 1) usw_ = 1;
        mpz_set_ui(magisw, usw_);
        uint32_t mpzscale=mpz_size(magisw);

        for(int i=0; i < NM7M; i++){
            if (mpzscale > 1000) {
                mpzscale = 1000;
            }
            else if (mpzscale < 1) {
                mpzscale = 1;
            }

            mpf_set_ui(mpa1, 1);
            mpf_set_ui(mpb1, 2);
            mpf_set_d(mpt1, 0.25*mpzscale);
            mpf_set_ui(mpp1, 1);
            mpf_sqrt(mpb1, mpb1);
            mpf_ui_div(mpb1, 1, mpb1);
            mpf_set_ui(mpsft, 10);

            for(int j=0; j <= iterations; j++){
	            mpf_add(mpa2, mpa1,  mpb1);
            	mpf_div_ui(mpa2, mpa2, 2);
            	mpf_mul(mpb2, mpa1, mpb1);
            	mpf_abs(mpb2, mpb2);
            	mpf_sqrt(mpb2, mpb2);
            	mpf_sub(mpt2, mpa1, mpa2);
            	mpf_abs(mpt2, mpt2);
            	mpf_sqrt(mpt2, mpt2);
            	mpf_mul(mpt2, mpt2, mpp1);
            	mpf_sub(mpt2, mpt1, mpt2);
            	mpf_mul_ui(mpp2, mpp1, 2);
            	mpf_swap(mpa1, mpa2);
            	mpf_swap(mpb1, mpb2);
            	mpf_swap(mpt1, mpt2);
            	mpf_swap(mpp1, mpp2);
            }

            mpf_add(magifpi, mpa1, mpb1);
            mpf_pow_ui(magifpi, magifpi, 2);
            mpf_div_ui(magifpi, magifpi, 4);
            mpf_abs(mpt1, mpt1);
            mpf_div(magifpi, magifpi, mpt1);

            mpf_pow_ui(mpsft, mpsft, digits/2);
            mpf_mul(magifpi, magifpi, mpsft);

            mpz_set_f(magipi, magifpi);

            mpz_add(product,product,magipi);
            mpz_add(product,product,magisw);

            mpz_set_uint256(bns[0], (void*)(hash));
            mpz_add(bns[7], bns[7], bns[0]);

            mpz_mul(product,product,bns[7]);
            mpz_cdiv_q (product, product, bns[0]);
            if (mpz_sgn(product) <= 0) mpz_set_ui(product,1);

            bytes = mpz_sizeinbase(product, 256);
            mpzscale=bytes;
            bdata = (uint8_t *)realloc(bdata, bytes);
            mpz_export(bdata, NULL, -1, 1, 0, 0, product);

            sph_sha256_init(&ctx_final_sha256);
            sph_sha256 (&ctx_final_sha256, bdata, bytes);
            sph_sha256_close(&ctx_final_sha256, (void*)(hash));

        }

        mpz_clear(magipi);
        mpz_clear(magisw);
        mpf_clear(magifpi);
        mpf_clear(mpsft);
        mpf_clear(mpa1);
        mpf_clear(mpb1);
        mpf_clear(mpt1);
        mpf_clear(mpp1);

        mpf_clear(mpa2);
        mpf_clear(mpb2);
        mpf_clear(mpt2);
        mpf_clear(mpp2);

	if (fcpu_dec && n%10 == 0) nanosleep(&cpu_dec_time, NULL);
	
        if (!fhash_test) rc = fulltest_m7hash(hash, ptarget);
        if (rc && !fhash_test) {
            if (opt_debug) {
                bin2hex(hash_str, (unsigned char *)hash, 32);
                bin2hex(target_str, (unsigned char *)ptarget, 32);
                bin2hex(data_str, (unsigned char *)data, 80);
                applog(LOG_DEBUG, "DEBUG: [%d thread] Found share!\ndata   %s\nhash   %s\ntarget %s", thr_id, 
                    data_str,
                    hash_str,
                    target_str);
            }

            pdata[19] = data[19];

            goto out;
        }
    } while (n < max_nonce && !work_restart[thr_id].restart);

    pdata[19] = n;

out:
    for(int i=0; i < 8; i++){
        mpz_clear(bns[i]);
    }
    mpz_clear(product);
    free(bdata);

    *hashes_done = n - first_nonce + 1;
    return rc;
}



