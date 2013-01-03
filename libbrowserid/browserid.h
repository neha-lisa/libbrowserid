/*
 * Copyright (C) 2013 PADL Software Pty Ltd.
 * All rights reserved.
 * Use is subject to license.
 */

#ifndef _BROWSERID_H_
#define _BROWSERID_H_ 1

#include <inttypes.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BID_S_OK,
    BID_S_NO_CONTEXT,
    BID_S_NO_MEMORY,
    BID_S_NOT_IMPLEMENTED,
    BID_S_INVALID_PARAMETER,
    BID_S_INVALID_USAGE,
    BID_S_UNAVAILABLE,
    BID_S_UNKNOWN_JSON_KEY,
    BID_S_INVALID_JSON,
    BID_S_INVALID_BASE64,
    BID_S_INVALID_ASSERTION,
    BID_S_CANNOT_ENCODE_JSON,
    BID_S_CANNOT_ENCODE_BASE64,
    BID_S_TOO_MANY_CERTS,
    BID_S_UNTRUSTED_ISSUER,
    BID_S_INVALID_ISSUER,
    BID_S_MISSING_ISSUER,
    BID_S_MISSING_AUDIENCE,
    BID_S_BAD_AUDIENCE,
    BID_S_EXPIRED_ASSERTION,
    BID_S_EXPIRED_CERT,
    BID_S_INVALID_SIGNATURE,
    BID_S_MISSING_ALGORITHM,
    BID_S_UNKNOWN_ALGORITHM,
    BID_S_INVALID_KEY,
    BID_S_INVALID_KEYSET,
    BID_S_NO_KEY,
    BID_S_CRYPTO_ERROR,
    BID_S_HTTP_ERROR,
    BID_S_BUFFER_TOO_SMALL,
    BID_S_BUFFER_TOO_LONG,
    BID_S_REMOTE_VERIFY_FAILURE,
    BID_S_MISSING_PRINCIPAL,
    BID_S_UNKNOWN_PRINCIPAL_TYPE,
    BID_S_MISSING_CERT,
    BID_S_UNKNOWN_ATTRIBUTE,
    BID_S_MISSING_CHANNEL_BINDINGS,
    BID_S_CHANNEL_BINDINGS_MISMATCH,
    BID_S_NO_SESSION_KEY,
    BID_S_DOCUMENT_NOT_MODIFIED,
    BID_S_INTERACT_UNAVAILABLE,
    BID_S_INTERACT_FAILURE,
    BID_S_INVALID_AUDIENCE_URN,
    BID_S_INVALID_JSON_WEB_TOKEN,
    BID_S_NO_MORE_ITEMS,
    BID_S_CACHE_OPEN_ERROR,
    BID_S_CACHE_READ_ERROR,
    BID_S_CACHE_WRITE_ERROR,
    BID_S_CACHE_CLOSE_ERROR,
    BID_S_CACHE_LOCK_ERROR,
    BID_S_CACHE_LOCK_TIMEOUT,
    BID_S_CACHE_UNLOCK_ERROR,
    BID_S_CACHE_DESTROY_ERROR,
    BID_S_CACHE_PERMISSION_DENIED,
    BID_S_CACHE_INVALID_VERSION,
    BID_S_CACHE_SCHEME_UNKNOWN,
    BID_S_CACHE_ALREADY_EXISTS,
    BID_S_CACHE_NOT_FOUND,
    BID_S_CACHE_KEY_NOT_FOUND,
    BID_S_REPLAYED_ASSERTION,
    BID_S_DH_PARAM_GENERATION_FAILURE,
    BID_S_DH_KEY_GENERATION_FAILURE,
    BID_S_DH_CHECK_P_NOT_PRIME,
    BID_S_DH_CHECK_P_NOT_SAFE_PRIME,
    BID_S_DH_NOT_SUITABLE_GENERATOR,
    BID_S_DH_UNABLE_TO_CHECK_GENERATOR,
    BID_S_NO_TICKET_CACHE,
    BID_S_BAD_TICKET_CACHE,
    BID_S_UNKNOWN_ERROR_CODE,
} BIDError;

BIDError
BIDErrorToString(
    BIDError error,
    const char **pString);

typedef struct BIDContextDesc *BIDContext;

#define BID_C_NO_CONTEXT                ((BIDContext)0)
#define BID_C_NO_IDENTITY               ((BIDIdentity)0)

/*
 * Context is used by user-agent.
 */
#define BID_CONTEXT_USER_AGENT          0x00000001

/*
 * Context is used by relying party.
 */
#define BID_CONTEXT_RP                  0x00000002

/*
 * Context uses remote verification service.
 */
#define BID_CONTEXT_VERIFY_REMOTE       0x00000004

/*
 * Context uses persistent authority cache.
 */
#define BID_CONTEXT_AUTHORITY_CACHE     0x00000008

/*
 * Context is for GSS, required for channel binding support.
 */
#define BID_CONTEXT_GSS                 0x00000010

/*
 * Use replay cache.
 */
#define BID_CONTEXT_REPLAY_CACHE        0x00000020

/*
 * Disable interaction with user.
 */
#define BID_USER_INTERACTION_DISABLED   0x00000040

/*
 * Do not prompt user if the browser already has a key
 * for the user. Note that this is independent of reauth
 * credentials; this just forces navigator.id.request()
 * to click OK. Use with care.
 */
#define BID_CONTEXT_CACHED_BROWSER_KEY  0x00000080

/*
 * DH key exchange
 */
#define BID_CONTEXT_DH_KEYEX            0x00000100

/*
 * Fast reauthentication support (requires replay cache on RP).
 */
#define BID_CONTEXT_REAUTH              0x00000200

/*
 * Context management.
 */
BIDError
BIDAcquireContext(uint32_t ulContextOptions, BIDContext *pContext);

BIDError
BIDReleaseContext(BIDContext context);

#define BID_PARAM_AUDIENCES             0x00000001
#define BID_PARAM_SECONDARY_AUTHORITIES 0x00000002
#define BID_PARAM_VERIFIER_URL          0x00000003
#define BID_PARAM_JSON_ERROR_INFO       0x00000004 /* debug only */
#define BID_PARAM_MAX_DELEGATIONS       0x00000005
#define BID_PARAM_SKEW                  0x00000006
#define BID_PARAM_CONTEXT_OPTIONS       0x00000007
#define BID_PARAM_REPLAY_CACHE          0x00000008
#define BID_PARAM_AUTHORITY_CACHE       0x00000009
#define BID_PARAM_DH_KEYEX_SIZE         0x0000000A
#define BID_PARAM_TICKET_CACHE          0x0000000B
#define BID_PARAM_TICKET_LIFETIME       0x0000000C

BIDError
BIDSetContextParam(BIDContext context, uint32_t ulParam, void *value);

BIDError
BIDGetContextParam(BIDContext context, uint32_t ulParam, void **pValue);

struct BIDIdentityDesc;
typedef struct BIDIdentityDesc *BIDIdentity;

/*
 * User agent.
 */
#define BID_ACQUIRE_FLAG_NO_INTERACT    0x00000001
#define BID_ACQUIRE_FLAG_NO_CACHED      0x00000002

BIDError
BIDAcquireAssertionFromString(
    BIDContext context,
    const char *szAssertion,
    uint32_t ulFlags,
    BIDIdentity *pAssertedIdentity,
    time_t *pExpiryTime,
    uint32_t *pulFlags);

BIDError
BIDAcquireAssertion(
    BIDContext context,
    const char *szAudienceOrSpn,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings,
    uint32_t ulFlags,
    char **pAssertion,
    BIDIdentity *pAssertedIdentity,
    time_t *pExpiryTime,
    uint32_t *pulFlags);

BIDError
BIDFreeAssertion(
    BIDContext context,
    char *assertion);


/*
 * Verifier.
 */

#define BID_VERIFY_FLAG_REMOTE     0x00000001
#define BID_VERIFY_FLAG_REAUTH     0x00000002

BIDError
BIDVerifyAssertion(
    BIDContext context,
    const char *szAssertion,
    const char *szAudienceOrSpn,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings,
    time_t tVerificationTime,
    uint32_t pulReqFlags,
    BIDIdentity *pVerifiedIdentity,
    time_t *pExpiryTime,
    uint32_t *pulVerifyFlags);

BIDError
BIDGetIdentityAudience(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue);

BIDError
BIDGetIdentitySubject(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue);

BIDError
BIDGetIdentityIssuer(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue);

BIDError
BIDGetIdentityAttribute(
    BIDContext context,
    BIDIdentity identity,
    const char *attribute,
    const char **pValue);

BIDError
BIDGetIdentityExpiryTime(
    BIDContext context,
    BIDIdentity identity,
    time_t *value);

#ifdef JANSSON_H
/* Caller frees reference; pass NULL to get root object. */
BIDError
BIDGetIdentityJsonObject(
    BIDContext context,
    BIDIdentity identity,
    const char *attribute,
    json_t **pJsonValue);
#endif

BIDError
BIDGetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    unsigned char **pY,
    size_t *pcbY);

BIDError
BIDSetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    const unsigned char *Y,
    size_t cbY);

BIDError
BIDGetIdentitySessionKey(
    BIDContext context,
    BIDIdentity identity,
    unsigned char **ppbSessionKey,
    size_t *pcbSessionKey);

BIDError
BIDFreeIdentitySessionKey(
    BIDContext context,
    BIDIdentity identity,
    unsigned char *pbSessionKey,
    size_t cbSessionKey);

BIDError
BIDReleaseIdentity(
    BIDContext context,
    BIDIdentity identity);

BIDError
BIDGetIdentityReauthTicket(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue);

BIDError
BIDStoreTicketInCache(
    BIDContext context,
    BIDIdentity identity,
    const char *szAudienceOrSpn,
    const char *szTicket);

struct BIDJWTDesc;
typedef struct BIDJWTDesc *BIDJWT;

#ifdef JANSSON_H
BIDError
BIDMakeJsonWebToken(
    BIDContext context,
    json_t *Payload,
    const unsigned char *pbKey,
    size_t cbKey,
    char **pszJwt,
    size_t *pchJwt);

BIDError
BIDParseJsonWebToken(
    BIDContext context,
    const char *szJwt,
    BIDJWT *pJwt,
    json_t **pPayload);

BIDError
BIDVerifyJsonWebToken(
    BIDContext context,
    BIDJWT jwt,
    const unsigned char *pbKey,
    size_t cbKey);

BIDError
BIDFreeData(
    BIDContext context,
    char *s);

BIDError
BIDReleaseJsonWebToken(
    BIDContext context,
    BIDJWT jwt);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _BROWSERID_H_ */