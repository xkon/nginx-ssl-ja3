/*
 * Copyright (C) 2017 Paulo Pacheco
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ngx_ssl_ja3.h"
#include <ngx_md5.h>

static const unsigned short GREASE[] = {
    0x0a0a,
    0x1a1a,
    0x2a2a,
    0x3a3a,
    0x4a4a,
    0x5a5a,
    0x6a6a,
    0x7a7a,
    0x8a8a,
    0x9a9a,
    0xaaaa,
    0xbaba,
    0xcaca,
    0xdada,
    0xeaea,
    0xfafa,
};


static int
ngx_ssl_ja3_is_ext_greased(int id)
{
    for (size_t i = 0; i < (sizeof(GREASE) / sizeof(GREASE[0])); ++i) {
        if (id == GREASE[i]) {
            return 1;
        }
    }
    return 0;
}


static const int nid_list[] = {
    NID_sect163k1,        /* sect163k1 (1) */
    NID_sect163r1,        /* sect163r1 (2) */
    NID_sect163r2,        /* sect163r2 (3) */
    NID_sect193r1,        /* sect193r1 (4) */
    NID_sect193r2,        /* sect193r2 (5) */
    NID_sect233k1,        /* sect233k1 (6) */
    NID_sect233r1,        /* sect233r1 (7) */
    NID_sect239k1,        /* sect239k1 (8) */
    NID_sect283k1,        /* sect283k1 (9) */
    NID_sect283r1,        /* sect283r1 (10) */
    NID_sect409k1,        /* sect409k1 (11) */
    NID_sect409r1,        /* sect409r1 (12) */
    NID_sect571k1,        /* sect571k1 (13) */
    NID_sect571r1,        /* sect571r1 (14) */
    NID_secp160k1,        /* secp160k1 (15) */
    NID_secp160r1,        /* secp160r1 (16) */
    NID_secp160r2,        /* secp160r2 (17) */
    NID_secp192k1,        /* secp192k1 (18) */
    NID_X9_62_prime192v1, /* secp192r1 (19) */
    NID_secp224k1,        /* secp224k1 (20) */
    NID_secp224r1,        /* secp224r1 (21) */
    NID_secp256k1,        /* secp256k1 (22) */
    NID_X9_62_prime256v1, /* secp256r1 (23) */
    NID_secp384r1,        /* secp384r1 (24) */
    NID_secp521r1,        /* secp521r1 (25) */
    NID_brainpoolP256r1,  /* brainpoolP256r1 (26) */
    NID_brainpoolP384r1,  /* brainpoolP384r1 (27) */
    NID_brainpoolP512r1,  /* brainpool512r1 (28) */
    EVP_PKEY_X25519,      /* X25519 (29) */
};


static unsigned char
ngx_ssl_ja3_nid_to_cid(int nid)
{
    unsigned char sz = (sizeof(nid_list) / sizeof(nid_list[0]));

    for (unsigned char i = 0; i < sz; i++) {
        if (nid == nid_list[i]) {
            return i+1;
        }
    }
    return 0;
}

static size_t
ngx_ssj_ja3_num_digits(int n)
{
    int c = 0;
    if (n < 9) {
        return 1;
    }
    for (; n; n /= 10) {
        ++c;
    }
    return c;
}

static void
ngx_ssl_ja3_detail_print(ngx_pool_t *pool, ngx_ssl_ja3_t *ja3, ngx_str_t *out)
{
    /* Version */
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: Version:  %d\n", ja3->version);

    /* Ciphers */
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: ciphers: length: %d\n",
                   ja3->ciphers_sz);

    for (size_t i = 0; i < ja3->ciphers_sz; ++i) {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                       pool->log, 0, "ssl_ja3: |    cipher: %d",
                       ja3->ciphers[i]
        );
    }

    /* Extensions */
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: extensions: length: %d\n",
                   ja3->extensions_sz);

    for (size_t i = 0; i < ja3->extensions_sz; ++i) {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                       pool->log, 0, "ssl_ja3: |    extension: %d",
                       ja3->extensions[i]
        );
    }

    /* Eliptic Curves */
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: curves: length: %d\n",
                   ja3->curves_sz);

    for (size_t i = 0; i < ja3->curves_sz; ++i) {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                       pool->log, 0, "ssl_ja3: |    curves: %d",
                       ja3->curves[i]
        );
    }

    /* EC Format Points */
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: formats: length: %d\n",
                   ja3->point_formats_sz);
    for (size_t i = 0; i < ja3->point_formats_sz; ++i) {
        ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                       pool->log, 0, "ssl_ja3: |    format: %d",
                       ja3->point_formats[i]
        );
    }
}


void
ngx_ssl_ja3_fp(ngx_pool_t *pool, ngx_ssl_ja3_t *ja3, ngx_str_t *out)
{
    size_t                    len = 0, cur = 0;

    if (pool == NULL || ja3 == NULL || out == NULL) {
        return;
    }

    len += ngx_ssj_ja3_num_digits(ja3->version);            /* Version */
    ++len;                                                  /* ',' separator */

    if (ja3->ciphers_sz) {
        for (size_t i = 0; i < ja3->ciphers_sz; ++i) {
            len += ngx_ssj_ja3_num_digits(ja3->ciphers[i]); /* cipher [i] */
        }
        len += (ja3->ciphers_sz - 1);                       /* '-' separators */
    }
    ++len;                                                  /* ',' separator */

    if (ja3->extensions_sz) {
        for (size_t i = 0; i < ja3->extensions_sz; ++i) {
            len += ngx_ssj_ja3_num_digits(ja3->extensions[i]); /* ext [i] */
        }
        len += (ja3->extensions_sz - 1);                   /* '-' separators */
    }
    ++len;                                                  /* ',' separator */

    if (ja3->curves_sz) {
        for (size_t i = 0; i < ja3->curves_sz; ++i) {
            len += ngx_ssj_ja3_num_digits(ja3->curves[i]); /* curves [i] */
        }
        len += (ja3->curves_sz - 1);                       /* '-' separators */
    }
    ++len;                                                  /* ',' separator */

    if (ja3->point_formats_sz) {
        for (size_t i = 0; i < ja3->point_formats_sz; ++i) {
            len += ngx_ssj_ja3_num_digits(ja3->point_formats[i]); /* fmt [i] */
        }
        len += (ja3->point_formats_sz - 1);                 /* '-' separators */
    }

    out->data = ngx_pnalloc(pool, len);
    out->len = len;

    len = ngx_ssj_ja3_num_digits(ja3->version) + 1;
    ngx_snprintf(out->data + cur, len, "%d,", ja3->version);
    cur += len;

    if (ja3->ciphers_sz) {
        for (size_t i = 0; i < ja3->ciphers_sz; ++i) {
            if (i > 0) {
                ngx_snprintf(out->data + (cur++), 1, "-");
            }
            len = ngx_ssj_ja3_num_digits(ja3->ciphers[i]);
            ngx_snprintf(out->data + cur, len, "%d", ja3->ciphers[i]);
            cur += len;
        }
    }
    ngx_snprintf(out->data + (cur++), 1, ",");

    if (ja3->extensions_sz) {
        for (size_t i = 0; i < ja3->extensions_sz; i++) {
            if (i > 0) {
                ngx_snprintf(out->data + (cur++), 1, "-");
            }
            len = ngx_ssj_ja3_num_digits(ja3->extensions[i]);
            ngx_snprintf(out->data + cur, len, "%d", ja3->extensions[i]);
            cur += len;
        }
    }
    ngx_snprintf(out->data + (cur++), 1, ",");

    if (ja3->curves_sz) {
        for (size_t i = 0; i < ja3->curves_sz; i++) {
            if (i > 0) {
                ngx_snprintf(out->data + (cur++), 1, "-");
            }
            len = ngx_ssj_ja3_num_digits(ja3->curves[i]);
            ngx_snprintf(out->data + cur, len, "%d", ja3->curves[i]);
            cur += len;
        }
    }
    ngx_snprintf(out->data + (cur++), 1, ",");

    if (ja3->point_formats_sz) {
        for (size_t i = 0; i < ja3->point_formats_sz; i++) {
            if (i > 0) {
                ngx_snprintf(out->data + (cur++), 1, "-");
            }
            len = ngx_ssj_ja3_num_digits(ja3->point_formats[i]);
            ngx_snprintf(out->data + cur, len, "%d", ja3->point_formats[i]);
            cur += len;
        }
    }

    out->len = cur;
    // the end of the string
    ngx_memzero(out->data+cur, 1);
    ngx_ssl_ja3_detail_print(pool, ja3, 0);
    ngx_log_debug1(NGX_LOG_DEBUG_EVENT,
                   pool->log, 0, "ssl_ja3: fp: [%V]\n", out);

}


/*
   /usr/bin/openssl s_client -connect 127.0.0.1:12345 \
           -cipher "AES128-SHA" -curves secp521r1
*/
int
ngx_ssl_ja3(ngx_connection_t *c, ngx_pool_t *pool, ngx_ssl_ja3_t *ja3) {

    ngx_ssl_session_t             *ssl_session;
    SSL                           *ssl;
    unsigned short                *ciphers_out = NULL;
    int                           *curves_out = NULL;
    int                           *point_formats_out = NULL;
    size_t                         len = 0;

    if (! c->ssl) {
        return NGX_DECLINED;
    }

    if (! c->ssl->handshaked) {
        return NGX_DECLINED;
    }

    ssl_session = ngx_ssl_get_session(c);
    if (! ssl_session) {
        return NGX_DECLINED;
    }

    ssl = c->ssl->connection;
    if ( ! ssl) {
        return NGX_DECLINED;
    }

    /* SSLVersion*/
    ja3->version = SSL_version(ssl);

    /* Cipher suites */
    ja3->ciphers = NULL;
    ja3->ciphers_sz = SSL_get0_raw_cipherlist(ssl, &ciphers_out);
    ja3->ciphers_sz /= 2;

    if (ja3->ciphers_sz && ciphers_out) {
        len = ja3->ciphers_sz * sizeof(unsigned short);
        ja3->ciphers = ngx_pnalloc(pool, len);
        if (ja3->ciphers == NULL) {
            return NGX_DECLINED;
        }

        // ngx_memcpy(ja3->ciphers, ciphers_out, len);
        size_t j=0;
        for (size_t i=0; i<ja3->ciphers_sz;++i){
            if (! ngx_ssl_ja3_is_ext_greased(ciphers_out[i])) {
                ja3->ciphers[j++] = ciphers_out[i];
            }
        }
        ja3->ciphers_sz = j;
#if NGX_HAVE_LITTLE_ENDIAN
        for (size_t i = 0; i < ja3->ciphers_sz; ++i) {
            ja3->ciphers[i] = ((ja3->ciphers[i])&0x00ff)<<8 | ((ja3->ciphers[i])&0xff00)>>8;
            // ja3->ciphers[i] >>=8;
        }
#endif
    }

    /* Extensions */
    // 存在一些问题，不会得到channel_id 和 Unknown 的type
    ja3->extensions = NULL;
    ja3->extensions_sz = 0;
    if (c->ssl->client_extensions_size && c->ssl->client_extensions) {
        len = c->ssl->client_extensions_size * sizeof(int);
        ja3->extensions = ngx_pnalloc(pool, len);
        if (ja3->extensions == NULL) {
            return NGX_DECLINED;
        }
        for (size_t i = 0; i < c->ssl->client_extensions_size; ++i) {
            if (! ngx_ssl_ja3_is_ext_greased(c->ssl->client_extensions[i])) {
                ja3->extensions[ja3->extensions_sz++] =
                    c->ssl->client_extensions[i];
            }
        }
    }

    /* Elliptic curve points */
    ja3->curves_sz = SSL_get1_curves(ssl, NULL);
    if (ja3->curves_sz) {
        curves_out = OPENSSL_malloc(ja3->curves_sz * sizeof(int));
        if (curves_out == NULL) {
            return NGX_DECLINED;
        }

        SSL_get1_curves(ssl, curves_out);

        len = ja3->curves_sz * sizeof(unsigned char);
        ja3->curves = ngx_pnalloc(pool, len);
        if (ja3->curves == NULL) {
            return NGX_DECLINED;
        }
        // GREASE fix need here
        size_t j=0;
        for (size_t i = 0; i < ja3->curves_sz; i++) {
            if (! ngx_ssl_ja3_is_ext_greased(curves_out[i])) {
                ja3->curves[j++] = ngx_ssl_ja3_nid_to_cid(curves_out[i]);
            }
        }
        ja3->curves_sz = j;

        if (curves_out) {
            OPENSSL_free(curves_out);
        }
    }

    /* Elliptic curve point formats */
    ja3->point_formats_sz = SSL_get0_ec_point_formats(ssl, &point_formats_out);
    if (ja3->point_formats_sz && point_formats_out != NULL) {

        len = ja3->point_formats_sz * sizeof(unsigned char);
        ja3->point_formats = ngx_pnalloc(pool, len);
        if (ja3->point_formats == NULL) {
            return NGX_DECLINED;
        }
        ngx_memcpy(ja3->point_formats, point_formats_out, len);
    }

    return NGX_OK;
}
