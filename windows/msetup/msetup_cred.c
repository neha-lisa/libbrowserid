/*
 * Copyright (C) 2012 PADL Software Pty Ltd.
 * All rights reserved.
 * Use is subject to license.
 *
 * CONFIDENTIAL
 *
 * msetup credential attribute helper functions
 */

#include "msetup.h"

typedef DWORD (*MsCredAttrHandlerFn)(
    LPWSTR TargetName,
    LPWSTR UserName,
    LPWSTR StringValue,
    PCREDENTIAL_ATTRIBUTE Attribute,
    BOOLEAN *pbFreeAttr);

static DWORD
MsSetCredCaCert(LPWSTR TargetName,
                LPWSTR UserName,
                LPWSTR CaCertFilename,
                PCREDENTIAL_ATTRIBUTE Attribute,
                BOOLEAN *pbFreeAttr)
{
    Attribute->Value = (PBYTE)CaCertFilename;
    Attribute->ValueSize = wcslen(CaCertFilename) * sizeof(WCHAR);
    *pbFreeAttr = FALSE;

    return ERROR_SUCCESS;
}

static DWORD
FindCertBySubject(  
    LPWSTR Store,
    LPWSTR SubjectName,
    HCERTSTORE *pCs,
    PCCERT_CONTEXT *pCc)
{
    HCERTSTORE cs = NULL;
    PCCERT_CONTEXT cc = NULL;

    cs = CertOpenStore(CERT_STORE_PROV_SYSTEM,
                       0,
                       (HCRYPTPROV)NULL,
                       CERT_SYSTEM_STORE_CURRENT_USER |
                        CERT_STORE_OPEN_EXISTING_FLAG |
                        CERT_STORE_READONLY_FLAG,
                       Store);
    if (cs == NULL) {
        return GetLastError();
    }

    cc = CertFindCertificateInStore(cs,
                                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                    0,
                                    CERT_FIND_SUBJECT_STR,
                                    SubjectName,
                                    NULL);
    if (cc == NULL) {
        CertCloseStore(cs, 0);
        return GetLastError();
    }

    *pCs = cs;
    *pCc = cc;

    return ERROR_SUCCESS;
}

static DWORD
MsSetCredServerCert(LPWSTR TargetName,
                    LPWSTR UserName,
                    LPWSTR CertName,
                    PCREDENTIAL_ATTRIBUTE Attribute,
                    BOOLEAN *pbFreeAttr)
{
    HCERTSTORE cs = NULL;
    PCCERT_CONTEXT cc = NULL;
    DWORD dwResult;

    dwResult = FindCertBySubject(L"MY", CertName, &cs, &cc);
    if (dwResult == CRYPT_E_NOT_FOUND)
        dwResult = FindCertBySubject(L"TrustedPeople", CertName, &cs, &cc);
    if (dwResult != ERROR_SUCCESS)
        goto cleanup;

    CertGetCertificateContextProperty(cc, CERT_HASH_PROP_ID,
                                      NULL, &Attribute->ValueSize);
    if (Attribute->ValueSize == 0) {
        dwResult = GetLastError();
        goto cleanup;
    }

    Attribute->Value = LocalAlloc(LPTR, Attribute->ValueSize);
    if (Attribute->Value == NULL) {
        dwResult = GetLastError();
        goto cleanup;
    }

    *pbFreeAttr = TRUE;

    if (!CertGetCertificateContextProperty(cc, CERT_HASH_PROP_ID,
                                           Attribute->Value,
                                           &Attribute->ValueSize)) {
        dwResult = GetLastError();
        goto cleanup;
    }

    dwResult = ERROR_SUCCESS;

cleanup:
    if (cs != NULL)
        CertCloseStore(cs, 0);
    if (cc != NULL)
        CertFreeCertificateContext(cc);

    return dwResult;
}

static DWORD
MsSetCredSubjectName(LPWSTR TargetName,
                     LPWSTR UserName,
                     LPWSTR SubjectName,
                     PCREDENTIAL_ATTRIBUTE Attribute,
                     BOOLEAN *pbFreeAttr)
{
    DWORD cbSize;

    if (!CertStrToName(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                       SubjectName,
                       CERT_X500_NAME_STR,
                       NULL,
                       NULL,
                       &cbSize,
                       NULL))
        return GetLastError();

    Attribute->Value = LocalAlloc(LPTR, cbSize);
    if (Attribute->Value == NULL)
        return GetLastError();

    if (!CertStrToName(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                       SubjectName,
                       CERT_X500_NAME_STR,
                       NULL,
                       Attribute->Value,
                       &Attribute->ValueSize,
                       NULL)) {
        LocalFree(Attribute->Value);
        return GetLastError();
    }

    *pbFreeAttr = TRUE;
    return ERROR_SUCCESS;
}

static DWORD
MsSetCredSubjectAltName(LPWSTR TargetName,
                        LPWSTR UserName,
                        LPWSTR SubjectAltName,
                        PCREDENTIAL_ATTRIBUTE Attribute,
                        BOOLEAN *pbFreeAttr)
{
    Attribute->Value = (PBYTE)SubjectAltName;
    Attribute->ValueSize = wcslen(SubjectAltName) * sizeof(WCHAR);
    *pbFreeAttr = FALSE;

    return ERROR_SUCCESS;
}

static struct _MS_CRED_ATTR_HANDLER {
    LPWSTR Attribute;
    MsCredAttrHandlerFn AttrHandler;
} msCredAttrHandlers[] = {
    { NULL,                                 NULL                        },
    { L"Moonshot_CACertificate",            MsSetCredCaCert             },
    { L"Moonshot_ServerCertificateHash",    MsSetCredServerCert         },
    { L"Moonshot_SubjectNameConstraint",    MsSetCredSubjectName        },
    { L"Moonshot_SubjectAltNameConstraint", MsSetCredSubjectAltName     },
};

DWORD
MsSetCredAttribute(
    LPWSTR TargetName,
    LPWSTR UserName,
    DWORD dwAttribute,
    LPWSTR AttributeValue)
{
    DWORD dwResult;
    CREDENTIAL Credential = { 0 };
    CREDENTIAL_ATTRIBUTE Attribute = { 0 };
    struct _MS_CRED_ATTR_HANDLER *Handler;
    BOOLEAN bFreeAttr = FALSE;

    if (dwAttribute == 0 || dwAttribute >= MS_CRED_ATTR_MAX)
        return ERROR_INVALID_PARAMETER;

    Handler = &msCredAttrHandlers[dwAttribute];

    assert(Handler->Attribute != NULL);
    assert(Handler->AttrHandler != NULL);

    Attribute.Keyword = Handler->Attribute;
    Attribute.Flags = 0;

    dwResult = Handler->AttrHandler(TargetName,
                                    UserName,
                                    AttributeValue,
                                    &Attribute,
                                    &bFreeAttr);
    if (dwResult != ERROR_SUCCESS)
        return dwResult;

    Credential.Flags = 0;
    Credential.Type = CRED_TYPE_DOMAIN_PASSWORD;
    Credential.TargetName = TargetName;
    Credential.CredentialBlobSize = 0;
    Credential.CredentialBlob = NULL;
    Credential.AttributeCount = 1;
    Credential.Attributes = &Attribute;
    Credential.UserName = UserName;

    if (!CredWrite(&Credential, CRED_PRESERVE_CREDENTIAL_BLOB))
        dwResult = GetLastError();
    else
        dwResult = ERROR_SUCCESS;

    if (bFreeAttr && Attribute.Value != NULL)
        LocalFree(Attribute.Value);

    return dwResult;
}

DWORD
MsSetDefaultCertStore(
    HKEY hSspKey,
    LPWSTR Store)
{
    DWORD dwResult;

    if (Store != NULL) {
        dwResult = RegSetValueEx(hSspKey, L"DefaultCertStore", 0,
                                 REG_SZ, (PBYTE)Store,
                                 (wcslen(Store) + 1) * sizeof(WCHAR));
    } else {
        dwResult = RegDeleteValue(hSspKey, L"DefaultCertStore");
    }

    return dwResult;
}

DWORD
MsGetDefaultCertStore(
    HKEY hSspKey,
    LPWSTR *pStore)
{
    DWORD dwResult;
    DWORD dwType = REG_SZ;
    DWORD dwSize = 0;

    dwResult = RegQueryValueEx(hSspKey, L"DefaultCertStore", NULL, &dwType,
                               NULL, &dwSize);
    if (dwResult != ERROR_SUCCESS)
        return dwResult;
    else if (dwType != REG_SZ)
        return ERROR_INVALID_PARAMETER;

    *pStore = LocalAlloc(LPTR, dwSize + sizeof(WCHAR));

    dwResult = RegQueryValueEx(hSspKey, L"DefaultCertStore", NULL, &dwType,
                               (PBYTE)*pStore, &dwSize);
    if (dwResult == ERROR_SUCCESS)
        (*pStore)[dwSize / sizeof(WCHAR)] = 0;

    return dwResult;
}