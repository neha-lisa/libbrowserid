/*
 * Copyright (c) 2010, JANET(UK)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of JANET(UK) nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "gssapiP_eap.h"

OM_uint32
gssEapWrapOrGetMIC(OM_uint32 *minor,
                   gss_ctx_id_t ctx,
                   int conf_req_flag,
                   int *conf_state,
                   gss_iov_buffer_desc *iov,
                   int iov_count,
                   enum gss_eap_token_type toktype)
{
    krb5_error_code code = 0;
    gss_iov_buffer_t header;
    gss_iov_buffer_t padding;
    gss_iov_buffer_t trailer;
    unsigned char acceptor_flag;
    unsigned char *outbuf = NULL;
    unsigned char *tbuf = NULL;
    int key_usage;
    size_t rrc = 0;
    unsigned int gss_headerlen, gss_trailerlen;
    size_t data_length, assoc_data_length;

    if (!CTX_IS_ESTABLISHED(ctx)) {
        return GSS_S_NO_CONTEXT;
    }

    acceptor_flag = CTX_IS_INITIATOR(ctx) ? 0 : TOK_FLAG_SENDER_IS_ACCEPTOR;
    key_usage = ((toktype == TOK_TYPE_WRAP)
                 ? (CTX_IS_INITIATOR(ctx)
                    ? KRB_USAGE_INITIATOR_SEAL
                    : KRB_USAGE_ACCEPTOR_SEAL)
                 : (CTX_IS_INITIATOR(ctx)
                    ? KRB_USAGE_INITIATOR_SIGN
                    : KRB_USAGE_ACCEPTOR_SIGN));

    gssEapIovMessageLength(iov, iov_count, &data_length, &assoc_data_length);

    header = gssEapLocateIov(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
    if (header == NULL) {
        *minor = EINVAL;
        return GSS_S_FAILURE;
    }

    padding = gssEapLocateIov(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (padding != NULL)
        padding->buffer.length = 0;

    trailer = gssEapLocateIov(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);

    if (toktype == TOK_TYPE_WRAP && conf_req_flag) {
        unsigned int k5_headerlen, k5_trailerlen, k5_padlen;
        size_t ec = 0;
        size_t conf_data_length = data_length - assoc_data_length;

        code = krb5_c_crypto_length(ctx->kerberosCtx, ctx->encryptionType,
                                    KRB5_CRYPTO_TYPE_HEADER, &k5_headerlen);
        if (code != 0)
            goto cleanup;

        code = krb5_c_padding_length(ctx->kerberosCtx, ctx->encryptionType,
                                     conf_data_length + 16 /* E(Header) */,
                                     &k5_padlen);
        if (code != 0)
            goto cleanup;

        if (k5_padlen == 0 && (ctx->gssFlags & GSS_C_DCE_STYLE)) {
            /* Windows rejects AEAD tokens with non-zero EC */
            code = krb5_c_block_size(ctx->kerberosCtx, ctx->encryptionType, &ec);
            if (code != 0)
                goto cleanup;
        } else
            ec = k5_padlen;

        code = krb5_c_crypto_length(ctx->kerberosCtx, ctx->encryptionType,
                                    KRB5_CRYPTO_TYPE_TRAILER, &k5_trailerlen);
        if (code != 0)
            goto cleanup;

        gss_headerlen = 16 /* Header */ + k5_headerlen;
        gss_trailerlen = ec + 16 /* E(Header) */ + k5_trailerlen;

        if (trailer == NULL) {
            rrc = gss_trailerlen;
            /* Workaround for Windows bug where it rotates by EC + RRC */
            if (ctx->gssFlags & GSS_C_DCE_STYLE)
                rrc -= ec;
            gss_headerlen += gss_trailerlen;
        }

        if (header->type & GSS_IOV_BUFFER_FLAG_ALLOCATE) {
            code = gssEapAllocIov(header, (size_t) gss_headerlen);
        } else if (header->buffer.length < gss_headerlen)
            code = KRB5_BAD_MSIZE;
        if (code != 0)
            goto cleanup;
        outbuf = (unsigned char *)header->buffer.value;
        header->buffer.length = (size_t) gss_headerlen;

        if (trailer != NULL) {
            if (trailer->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
                code = gssEapAllocIov(trailer, (size_t) gss_trailerlen);
            else if (trailer->buffer.length < gss_trailerlen)
                code = KRB5_BAD_MSIZE;
            if (code != 0)
                goto cleanup;
            trailer->buffer.length = (size_t) gss_trailerlen;
        }

        /* TOK_ID */
        store_16_be((uint16_t)toktype, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag
                     | (conf_req_flag ? TOK_FLAG_WRAP_CONFIDENTIAL : 0)
                     | (0 ? TOK_FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xFF;
        /* EC */
        store_16_be(ec, outbuf + 4);
        /* RRC */
        store_16_be(0, outbuf + 6);
        store_64_be(ctx->sendSeq, outbuf + 8);

        /* EC | copy of header to be encrypted, located in (possibly rotated) trailer */
        if (trailer == NULL)
            tbuf = (unsigned char *)header->buffer.value + 16; /* Header */
        else
            tbuf = (unsigned char *)trailer->buffer.value;

        memset(tbuf, 0xFF, ec);
        memcpy(tbuf + ec, header->buffer.value, 16);

        code = gssEapEncrypt(ctx->kerberosCtx,
                             ((ctx->gssFlags & GSS_C_DCE_STYLE) != 0),
                             ec, rrc, ctx->encryptionKey,
                             key_usage, 0, iov, iov_count);
        if (code != 0)
            goto cleanup;

        /* RRC */
        store_16_be(rrc, outbuf + 6);

        ctx->sendSeq++;
    } else if (toktype == TOK_TYPE_WRAP && !conf_req_flag) {
    wrap_with_checksum:

        gss_headerlen = 16;

        code = krb5_c_crypto_length(ctx->kerberosCtx, ctx->encryptionType,
                                    KRB5_CRYPTO_TYPE_CHECKSUM,
                                    &gss_trailerlen);
        if (code != 0)
            goto cleanup;

        assert(gss_trailerlen <= 0xFFFF);

        if (trailer == NULL) {
            rrc = gss_trailerlen;
            gss_headerlen += gss_trailerlen;
        }

        if (header->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
            code = gssEapAllocIov(header, (size_t) gss_headerlen);
        else if (header->buffer.length < gss_headerlen)
            code = KRB5_BAD_MSIZE;
        if (code != 0)
            goto cleanup;
        outbuf = (unsigned char *)header->buffer.value;
        header->buffer.length = (size_t) gss_headerlen;

        if (trailer != NULL) {
            if (trailer->type & GSS_IOV_BUFFER_FLAG_ALLOCATE)
                code = gssEapAllocIov(trailer, (size_t) gss_trailerlen);
            else if (trailer->buffer.length < gss_trailerlen)
                code = KRB5_BAD_MSIZE;
            if (code != 0)
                goto cleanup;
            trailer->buffer.length = (size_t) gss_trailerlen;
        }

        /* TOK_ID */
        store_16_be((uint16_t)toktype, outbuf);
        /* flags */
        outbuf[2] = (acceptor_flag
                     | (0 ? TOK_FLAG_ACCEPTOR_SUBKEY : 0));
        /* filler */
        outbuf[3] = 0xFF;
        if (toktype == TOK_TYPE_WRAP) {
            /* Use 0 for checksum calculation, substitute
             * checksum length later.
             */
            /* EC */
            store_16_be(0, outbuf + 4);
            /* RRC */
            store_16_be(0, outbuf + 6);
        } else {
            /* MIC and DEL store 0xFF in EC and RRC */
            store_16_be(0xFFFF, outbuf + 4);
            store_16_be(0xFFFF, outbuf + 6);
        }
        store_64_be(ctx->sendSeq, outbuf + 8);

        code = gssEapSign(ctx->kerberosCtx, ctx->checksumType,
                          rrc, ctx->encryptionKey, key_usage,
                          iov, iov_count);
        if (code != 0)
            goto cleanup;

        ctx->sendSeq++;

        if (toktype == TOK_TYPE_WRAP) {
            /* Fix up EC field */
            store_16_be(gss_trailerlen, outbuf + 4);
            /* Fix up RRC field */
            store_16_be(rrc, outbuf + 6);
        }
    } else if (toktype == TOK_TYPE_MIC) {
        trailer = NULL;
        goto wrap_with_checksum;
    } else if (toktype == TOK_TYPE_DELETE) {
        goto wrap_with_checksum;
    } else {
        abort();
    }

    code = 0;

cleanup:
    if (code != 0)
        gssEapReleaseIov(iov, iov_count);

    *minor = code;

    if (code == 0)
        return GSS_S_FAILURE;
    else
        return GSS_S_COMPLETE;
}

OM_uint32
gss_wrap_iov(OM_uint32 *minor,
             gss_ctx_id_t ctx,
             int conf_req_flag,
             gss_qop_t qop_req,
             int *conf_state,
             gss_iov_buffer_desc *iov,
             int iov_count)
{
    return gssEapWrapOrGetMIC(minor, ctx, conf_req_flag, conf_state,
                             iov, iov_count, TOK_TYPE_WRAP);
}
