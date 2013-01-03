/*
 * Copyright (C) 2013 PADL Software Pty Ltd.
 * All rights reserved.
 * Use is subject to license.
 */

#include "bid_private.h"

static BIDError
_BIDMakeClaims(
    BIDContext context,
    const char *szAudienceOrSpn,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings,
    json_t **pClaims)
{
    BIDError err;
    json_t *claims = NULL;
    json_t *cbt = NULL;
    json_t *dh = NULL;

    *pClaims = NULL;

    if (szAudienceOrSpn == NULL) {
        err = BID_S_INVALID_PARAMETER;
        goto cleanup;
    }

    claims = json_object();
    if (claims == NULL ||
        json_object_set_new(claims, "aud", json_string(szAudienceOrSpn)) < 0) {
        err = BID_S_NO_MEMORY;
        goto cleanup;
    }

    if (pbChannelBindings != NULL) {
        err = _BIDJsonBinaryValue(context, pbChannelBindings, cbChannelBindings, &cbt);
        BID_BAIL_ON_ERROR(err);

        if (json_object_set(claims, "cbt", cbt) < 0) {
            err = BID_S_NO_MEMORY;
            goto cleanup;
        }
    }

    if (context->ContextOptions & BID_CONTEXT_DH_KEYEX) {
        err = _BIDGenerateDHParams(context, &dh);
        BID_BAIL_ON_ERROR(err);

        if (json_object_set(claims, "dh", dh) < 0) {
            err = BID_S_NO_MEMORY;
            goto cleanup;
        }
    }

    err = BID_S_OK;
    *pClaims = claims;

cleanup:
    if (err != BID_S_OK)
        json_decref(claims);
    json_decref(cbt);
    json_decref(dh);

    return err;
}

BIDError
BIDAcquireAssertion(
    BIDContext context,
    const char *szAudienceOrSpn,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings,
    uint32_t ulReqFlags,
    char **pAssertion,
    BIDIdentity *pAssertedIdentity,
    time_t *ptExpiryTime,
    uint32_t *pulRetFlags)
{
    BIDError err;
    BIDBackedAssertion backedAssertion = NULL;
    json_t *claims = NULL;
    json_t *key = NULL;
    char *szAssertion = NULL;
    char *szPackedAudience = NULL;
    const char *szSiteName = NULL;
    uint32_t ulRetFlags = 0;

    *pAssertion = NULL;
    if (pAssertedIdentity != NULL)
        *pAssertedIdentity = NULL;
    if (ptExpiryTime != NULL)
        *ptExpiryTime = 0;
    if (pulRetFlags != NULL)
        *pulRetFlags = 0;

    BID_CONTEXT_VALIDATE(context);

    if ((context->ContextOptions & BID_CONTEXT_REAUTH) &&
        (ulReqFlags & BID_ACQUIRE_FLAG_NO_CACHED) == 0) {
        err = _BIDGetReauthAssertion(context, szAudienceOrSpn, pbChannelBindings, cbChannelBindings,
                                     pAssertion, pAssertedIdentity, ptExpiryTime);
        if (err == BID_S_OK) {
            ulRetFlags |= BID_VERIFY_FLAG_REAUTH;
            goto cleanup;
        }
    }

    if (context->ContextOptions & BID_USER_INTERACTION_DISABLED ||
        (ulReqFlags & BID_ACQUIRE_FLAG_NO_INTERACT)) {
        err = BID_S_INTERACT_UNAVAILABLE;
        goto cleanup;
    }

    err = _BIDMakeClaims(context, szAudienceOrSpn, pbChannelBindings, cbChannelBindings, &claims);
    BID_BAIL_ON_ERROR(err);

    if (context->ContextOptions & BID_CONTEXT_DH_KEYEX) {
        json_t *dh = json_object_get(claims, "dh");

        err = _BIDGenerateDHKey(context, dh, &key);
        BID_BAIL_ON_ERROR(err);

        /* Copy public value to parameters so we can send them. */
        json_object_set(dh, "y", json_object_get(key, "y"));
    }

    err = _BIDPackAudience(context, claims, &szPackedAudience);
    BID_BAIL_ON_ERROR(err);

    szSiteName = strchr(szAudienceOrSpn, '/');
    if (szSiteName != NULL)
        szSiteName++;

    err = _BIDBrowserGetAssertion(context, szPackedAudience, szSiteName, &szAssertion);
    BID_BAIL_ON_ERROR(err);

    err = BIDAcquireAssertionFromString(context, szAssertion, ulReqFlags,
                                        pAssertedIdentity, ptExpiryTime, &ulRetFlags);
    BID_BAIL_ON_ERROR(err);

    if (pAssertedIdentity != NULL && (context->ContextOptions & BID_CONTEXT_DH_KEYEX)) {
        BIDIdentity assertedIdentity = *pAssertedIdentity;

        assertedIdentity->PrivateAttributes = json_object();
        json_object_set(assertedIdentity->PrivateAttributes, "dh", key);
    }

    *pAssertion = szAssertion;

cleanup:
    if (pulRetFlags != NULL)
        *pulRetFlags = ulRetFlags;
    if (err != BID_S_OK)
        BIDFree(szAssertion);
    json_decref(claims);
    json_decref(key);
    _BIDReleaseBackedAssertion(context, backedAssertion);
    BIDFree(szPackedAudience);

    return err;
}

BIDError
BIDAcquireAssertionFromString(
    BIDContext context,
    const char *szAssertion,
    uint32_t ulReqFlags,
    BIDIdentity *pAssertedIdentity,
    time_t *ptExpiryTime,
    uint32_t *pulRetFlags)
{
    BIDError err;
    BIDBackedAssertion backedAssertion = NULL;

    if (pAssertedIdentity != NULL)
        *pAssertedIdentity = NULL;
    if (ptExpiryTime != NULL)
        *ptExpiryTime = 0;
    if (pulRetFlags != NULL)
        *pulRetFlags = 0;

    BID_CONTEXT_VALIDATE(context);

    err = _BIDUnpackBackedAssertion(context, szAssertion, &backedAssertion);
    BID_BAIL_ON_ERROR(err);

    if (pAssertedIdentity != NULL) {
        err = _BIDPopulateIdentity(context, backedAssertion, pAssertedIdentity);
        BID_BAIL_ON_ERROR(err);
    }

    err = BID_S_OK;

    if (ptExpiryTime != NULL)
        _BIDGetJsonTimestampValue(context, backedAssertion->Assertion->Payload, "exp", ptExpiryTime);

cleanup:
    _BIDReleaseBackedAssertion(context, backedAssertion);

    return err;
}

BIDError
BIDFreeAssertion(
    BIDContext context,
    char *assertion)
{
    BID_CONTEXT_VALIDATE(context);

    if (assertion == NULL)
        return BID_S_INVALID_PARAMETER;

    BIDFree(assertion);
    return BID_S_OK;
}
