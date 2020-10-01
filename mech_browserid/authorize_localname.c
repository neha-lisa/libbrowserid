/*
 * Copyright (c) 2011, JANET(UK)
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
 * Local authorization services.
 */

#include "gssapiP_bid.h"

OM_uint32 GSSAPI_CALLCONV
gssspi_authorize_localname(OM_uint32 *minor,
                           const gss_name_t name GSSBID_UNUSED,
                           gss_const_buffer_t local_user GSSBID_UNUSED,
                           gss_const_OID local_nametype GSSBID_UNUSED)
{
    /*
     * Returning GSS_S_UNAVAILABLE will force the mechanism glue, at
     * least in the MIT implementation, to compare for equivalence.
     *
     * Moonshot returns GSS_S_UNAUTHORIZED here to force the glue to
     * only use attribute-based authorization, but it is less likely
     * that BrowserID certificates will contain useful POSIX account
     * information, we're going to allow the direct equivalence fallback
     * test here by returning GSS_S_UNAVAILABLE.
     */

    *minor = 0;
    return GSS_S_UNAVAILABLE;
}