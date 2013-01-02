/*
 * Copyright (C) 2013 PADL Software Pty Ltd.
 * All rights reserved.
 * Use is subject to license.
 */

#include "bid_private.h"

BIDError
_BIDVerifierDHKeyEx(
    BIDContext context,
    BIDIdentity identity)
{
    BIDError err;
    json_t *dh;
    json_t *params;
    json_t *key = NULL;

    BID_ASSERT(context->ContextOptions & BID_CONTEXT_DH_KEYEX);
    BID_ASSERT(identity != BID_C_NO_IDENTITY);

    dh = json_object_get(identity->PrivateAttributes, "dh");
    if (dh == NULL) {
        err = BID_S_INVALID_PARAMETER;
        goto cleanup;
    }

    params = json_object_get(dh, "params");

    err = _BIDGenerateDHKey(context, params, &key);
    BID_BAIL_ON_ERROR(err);

    if (json_object_set(dh, "x", json_object_get(key, "x")) < 0 ||
        json_object_set(dh, "y", json_object_get(key, "y")) < 0) {
        err = BID_S_NO_MEMORY;
        goto cleanup;
    }

cleanup:
    json_decref(key);

    return err;
}

BIDError
BIDVerifyAssertion(
    BIDContext context,
    const char *szAssertion,
    const char *szAudience,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings,
    time_t verificationTime,
    BIDIdentity *pVerifiedIdentity,
    time_t *pExpiryTime)
{
    BIDError err;

    BID_CONTEXT_VALIDATE(context);

    *pVerifiedIdentity = BID_C_NO_IDENTITY;

    if (szAssertion == NULL || szAudience == NULL)
        return BID_S_INVALID_PARAMETER;

    if ((context->ContextOptions & BID_CONTEXT_RP) == 0)
        return BID_S_INVALID_USAGE;

    if (context->ContextOptions & BID_CONTEXT_VERIFY_REMOTE)
        err = _BIDVerifyRemote(context, szAssertion, szAudience,
                               pbChannelBindings, cbChannelBindings, verificationTime,
                               pVerifiedIdentity, pExpiryTime);
    else
        err = _BIDVerifyLocal(context, szAssertion, szAudience,
                              pbChannelBindings, cbChannelBindings, verificationTime,
                              pVerifiedIdentity, pExpiryTime);

    if (err == BID_S_OK && (context->ContextOptions & BID_CONTEXT_DH_KEYEX))
        err = _BIDVerifierDHKeyEx(context, *pVerifiedIdentity);

    return err;
}

BIDError
BIDReleaseIdentity(
    BIDContext context,
    BIDIdentity identity)
{
    if (identity == BID_C_NO_IDENTITY)
        return BID_S_INVALID_PARAMETER;

    if (identity->SessionKey != NULL) {
        memset(identity->SessionKey, 0, identity->SessionKeyLength);
        BIDFree(identity->SessionKey);
    }
    json_decref(identity->Attributes);
    json_decref(identity->PrivateAttributes);
    BIDFree(identity);

    return BID_S_OK;
}

BIDError
_BIDValidateAudience(
    BIDContext context,
    BIDBackedAssertion backedAssertion,
    const char *szAudienceOrSpn,
    const unsigned char *pbChannelBindings,
    size_t cbChannelBindings)
{
    BIDError err;
    unsigned char *pbAssertionCB = NULL;
    size_t cbAssertionCB = 0;

    if (backedAssertion->Claims == NULL)
        return BID_S_MISSING_AUDIENCE;

    if (szAudienceOrSpn != NULL) {
        const char *szAssertionSpn = json_string_value(json_object_get(backedAssertion->Claims, "aud"));

        if (szAssertionSpn == NULL) {
            err = BID_S_MISSING_AUDIENCE;
            goto cleanup;
        } else if (strcmp(szAudienceOrSpn, szAssertionSpn) != 0) {
            err = BID_S_BAD_AUDIENCE;
            goto cleanup;
        }
    }

    if (pbChannelBindings != NULL) {
        err = _BIDGetJsonBinaryValue(context, backedAssertion->Claims, "cbt", &pbAssertionCB, &cbAssertionCB);
        if (err == BID_S_UNKNOWN_JSON_KEY)
            err = BID_S_MISSING_CHANNEL_BINDINGS;
        BID_BAIL_ON_ERROR(err);

        if (cbChannelBindings != cbAssertionCB ||
            memcmp(pbChannelBindings, pbAssertionCB, cbAssertionCB) != 0) {
            err = BID_S_CHANNEL_BINDINGS_MISMATCH;
            goto cleanup;
        }
    }

    err = BID_S_OK;

cleanup:
    BIDFree(pbAssertionCB);

    return err;
}

BIDError
BIDGetIdentityAudience(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue)
{
    return BIDGetIdentityAttribute(context, identity, "audience", pValue);
}

BIDError
BIDGetIdentityEmail(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue)
{
    return BIDGetIdentityAttribute(context, identity, "email", pValue);
}

BIDError
BIDGetIdentityIssuer(
    BIDContext context,
    BIDIdentity identity,
    const char **pValue)
{
    return BIDGetIdentityAttribute(context, identity, "issuer", pValue);
}

BIDError
_BIDGetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    json_t **pY)
{
    BIDError err;
    json_t *dh = NULL;
    json_t *y = NULL;

    *pY = NULL;

    if (identity->PrivateAttributes == NULL                                   ||
        (dh     = json_object_get(identity->PrivateAttributes, "dh")) == NULL ||
        (y      = json_object_get(dh, "y")) == NULL) {
        err = BID_S_NO_KEY;
        goto cleanup;
    }

    *pY = json_object();
    if (*pY == NULL) {
        err = BID_S_NO_MEMORY;
        goto cleanup;
    }

    if (json_object_set(*pY, "y", y) < 0) {
        err = BID_S_NO_MEMORY;
        goto cleanup;
    }

    err = BID_S_OK;

cleanup:
    if (err != BID_S_OK && *pY != NULL) {
        json_decref(*pY);
        *pY = NULL;
    }

    return err;
}

BIDError
BIDGetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    unsigned char **pY,
    size_t *pcbY)
{
    BIDError err;
    json_t *dh;

    *pY = NULL;
    *pcbY = 0;

    err = _BIDGetIdentityDHPublicValue(context, identity, &dh);
    if (err != BID_S_OK)
        return err;

    err = _BIDGetJsonBinaryValue(context, dh, "y", pY, pcbY);

    return err;
}

BIDError
_BIDSetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    json_t *y)
{
    json_t *dh;
    json_t *params;

    BID_CONTEXT_VALIDATE(context);

    dh = json_object_get(identity->PrivateAttributes, "dh");
    if (dh == NULL)
        return BID_S_NO_KEY;

    params = json_object_get(dh, "params");
    if (params == NULL)
        return BID_S_NO_KEY;

    if (json_object_set(params, "y", y) < 0)
        return BID_S_NO_MEMORY;

    return BID_S_OK;
}

BIDError
BIDSetIdentityDHPublicValue(
    BIDContext context,
    BIDIdentity identity,
    const unsigned char *pY,
    size_t cbY)
{
    BIDError err;
    json_t *y;

    BID_CONTEXT_VALIDATE(context);

    err = _BIDJsonBinaryValue(context, pY, cbY, &y);
    if (err != BID_S_OK)
        return err;

    err = _BIDSetIdentityDHPublicValue(context, identity, y);

    json_decref(y);

    return err;
}

BIDError
BIDGetIdentityAttribute(
    BIDContext context,
    BIDIdentity identity,
    const char *attribute,
    const char **pValue)
{
    BIDError err;
    json_t *value;

    *pValue = NULL;

    BID_CONTEXT_VALIDATE(context);

    value = json_object_get(identity->Attributes, attribute);
    if (value == NULL) {
        err = BID_S_UNKNOWN_ATTRIBUTE;
        goto cleanup;
    }

    *pValue = json_string_value(value);
    if (*pValue == NULL) {
        err = BID_S_UNKNOWN_ATTRIBUTE;
        goto cleanup;
    }

    err = BID_S_OK;

cleanup:
    return err;
}

BIDError
BIDGetIdentityJsonObject(
    BIDContext context,
    BIDIdentity identity,
    const char *attribute,
    json_t **pJsonValue)
{
    BIDError err;
    json_t *value;

    *pJsonValue = NULL;

    BID_CONTEXT_VALIDATE(context);

    if (attribute != NULL) {
        value = json_object_get(identity->Attributes, attribute);
        if (value == NULL) {
            err = BID_S_UNKNOWN_ATTRIBUTE;
            goto cleanup;
        }
    } else {
        value = identity->Attributes;
    }

    *pJsonValue = json_incref(value);
    err = BID_S_OK;

cleanup:
    return err;
}

BIDError
BIDGetIdentitySessionKey(
    BIDContext context,
    BIDIdentity identity,
    unsigned char **ppbSessionKey,
    size_t *pcbSessionKey)
{
    BIDError err;
    json_t *dh;
    json_t *params;

    if (ppbSessionKey != NULL) {
        *ppbSessionKey = NULL;
        *pcbSessionKey = 0;
    }

    BID_CONTEXT_VALIDATE(context);

    if (identity == BID_C_NO_IDENTITY) {
        err = BID_S_INVALID_PARAMETER;
        goto cleanup;
    }

    if (identity->SessionKey == NULL) {
        dh = json_object_get(identity->PrivateAttributes, "dh");
        if (dh == NULL) {
            err = BID_S_NO_KEY;
            goto cleanup;
        }

        params = json_object_get(dh, "params");

        err = _BIDComputeDHKey(context, dh, params, &identity->SessionKey, &identity->SessionKeyLength);
        BID_BAIL_ON_ERROR(err);
    }

    if (ppbSessionKey != NULL) {
        *ppbSessionKey = BIDMalloc(identity->SessionKeyLength);
        if (*ppbSessionKey == NULL) {
            err = BID_S_NO_MEMORY;
            goto cleanup;
        }

        memcpy(*ppbSessionKey, identity->SessionKey, identity->SessionKeyLength);
        *pcbSessionKey = identity->SessionKeyLength;
    }

cleanup:
    return err;
}

BIDError
BIDFreeIdentitySessionKey(
    BIDContext context,
    BIDIdentity identity,
    unsigned char *pbSessionKey,
    size_t cbSessionKey)
{
    BIDError err;

    BID_CONTEXT_VALIDATE(context);

    if (pbSessionKey == NULL) {
        err = BID_S_INVALID_PARAMETER;
        goto cleanup;
    }

    memset(pbSessionKey, 0, cbSessionKey);
    BIDFree(pbSessionKey);

cleanup:
    return BID_S_OK;
}

BIDError
BIDGetIdentityExpiryTime(
    BIDContext context,
    BIDIdentity identity,
    const char *attribute,
    time_t *value)
{

    BID_CONTEXT_VALIDATE(context);

    if (identity == BID_C_NO_IDENTITY)
        return BID_S_INVALID_PARAMETER;

    return _BIDGetJsonTimestampValue(context, identity->Attributes, "expires", value);
}
