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
 * SAML attribute provider implementation.
 */

#include "gssapiP_bid.h"

#include <sstream>

#ifdef __APPLE__
#undef nil
#endif

#include <xercesc/util/XMLUniDefs.hpp>
#include <xmltooling/unicode.h>
#include <xmltooling/XMLToolingConfig.h>
#include <xmltooling/util/XMLHelper.h>
#include <xmltooling/util/ParserPool.h>
#include <xmltooling/util/DateTime.h>

#include <saml/exceptions.h>
#include <saml/SAMLConfig.h>
#include <saml/saml1/core/Assertions.h>
#include <saml/saml2/core/Assertions.h>
#include <saml/saml2/metadata/Metadata.h>
#include <saml/saml2/metadata/MetadataProvider.h>

using namespace xmltooling;
using namespace opensaml::saml2md;
using namespace opensaml;
using namespace xercesc;
using namespace std;

static const XMLCh
base64Binary[] = {'b','a','s','e','6','4','B','i','n','a','r','y',0};

/*
 * BIDGSSSAMLAssertionProvider is for retrieving the underlying
 * assertion.
 */
BIDGSSSAMLAssertionProvider::BIDGSSSAMLAssertionProvider(void)
{
    m_assertion = NULL;
    m_authenticated = false;
}

BIDGSSSAMLAssertionProvider::~BIDGSSSAMLAssertionProvider(void)
{
    delete m_assertion;
}

bool
BIDGSSSAMLAssertionProvider::initWithExistingContext(const BIDGSSAttributeContext *manager,
                                                     const BIDGSSAttributeProvider *ctx)
{
    /* Then we may be creating from an existing attribute context */
    const BIDGSSSAMLAssertionProvider *saml;

    GSSBID_ASSERT(m_assertion == NULL);

    if (!BIDGSSAttributeProvider::initWithExistingContext(manager, ctx))
        return false;

    saml = static_cast<const BIDGSSSAMLAssertionProvider *>(ctx);
    setAssertion(saml->getAssertion(), saml->authenticated());

    return true;
}

bool
BIDGSSSAMLAssertionProvider::initWithGssContext(const BIDGSSAttributeContext *manager,
                                                const gss_cred_id_t gssCred,
                                                const gss_ctx_id_t gssCtx)
{
    const BIDGSSJWTAttributeProvider *jwt;

    GSSBID_ASSERT(m_assertion == NULL);

    if (!BIDGSSAttributeProvider::initWithGssContext(manager, gssCred, gssCtx))
        return false;

    jwt = static_cast<const BIDGSSJWTAttributeProvider *>
        (m_manager->getProvider(ATTR_TYPE_JWT));
    if (jwt != NULL) {
        JSONObject samlAttribute = jwt->jsonRepresentation().get("saml");
        if (samlAttribute.isString()) {
            gss_buffer_desc value = samlAttribute.buffer();
            setAssertion(&value, jwt->authenticated());
        }
    } else {
        m_assertion = NULL;
    }

    return true;
}

void
BIDGSSSAMLAssertionProvider::setAssertion(const saml2::Assertion *assertion,
                                          bool authenticated)
{

    delete m_assertion;

    if (assertion != NULL) {
#ifdef __APPLE__
        m_assertion = (saml2::Assertion *)((void *)assertion->clone());
#else
        m_assertion = dynamic_cast<saml2::Assertion *>(assertion->clone());
#endif
        m_authenticated = authenticated;
    } else {
        m_assertion = NULL;
        m_authenticated = false;
    }
}

void
BIDGSSSAMLAssertionProvider::setAssertion(const gss_buffer_t buffer,
                                          bool authenticated)
{
    delete m_assertion;

    m_assertion = parseAssertion(buffer);
    m_authenticated = (m_assertion != NULL && authenticated);
}

saml2::Assertion *
BIDGSSSAMLAssertionProvider::parseAssertion(const gss_buffer_t buffer)
{
    string str((char *)buffer->value, buffer->length);
    istringstream istream(str);
    DOMDocument *doc;
    const XMLObjectBuilder *b;

    try {
        doc = XMLToolingConfig::getConfig().getParser().parse(istream);
        if (doc == NULL)
            return NULL;

        b = XMLObjectBuilder::getBuilder(doc->getDocumentElement());

#ifdef __APPLE__
        return (saml2::Assertion *)((void *)b->buildFromDocument(doc));
#else
        return dynamic_cast<saml2::Assertion *>(b->buildFromDocument(doc));
#endif
    } catch (exception &e) {
        return NULL;
    }
}

bool
BIDGSSSAMLAssertionProvider::getAttributeTypes(BIDGSSAttributeIterator addAttribute,
                                               void *data) const
{
    bool ret;

    /* just add the prefix */
    if (m_assertion != NULL)
        ret = addAttribute(m_manager, this, GSS_C_NO_BUFFER, data);
    else
        ret = true;

    return ret;
}

bool
BIDGSSSAMLAssertionProvider::setAttribute(int complete GSSBID_UNUSED,
                                          const gss_buffer_t attr,
                                          const gss_buffer_t value)
{
    if (attr == GSS_C_NO_BUFFER || attr->length == 0) {
        setAssertion(value);
        return true;
    }

    return false;
}

bool
BIDGSSSAMLAssertionProvider::deleteAttribute(const gss_buffer_t value GSSBID_UNUSED)
{
    delete m_assertion;
    m_assertion = NULL;
    m_authenticated = false;

    return true;
}

time_t
BIDGSSSAMLAssertionProvider::getExpiryTime(void) const
{
    saml2::Conditions *conditions;
    time_t expiryTime = 0;

    if (m_assertion == NULL)
        return 0;

    conditions = m_assertion->getConditions();

    if (conditions != NULL && conditions->getNotOnOrAfter() != NULL)
        expiryTime = conditions->getNotOnOrAfter()->getEpoch();

    return expiryTime;
}

OM_uint32
BIDGSSSAMLAssertionProvider::mapException(OM_uint32 *minor,
                                          std::exception &e) const
{
    if (typeid(e) == typeid(SecurityPolicyException))
        *minor = GSSBID_SAML_SEC_POLICY_FAILURE;
    else if (typeid(e) == typeid(BindingException))
        *minor = GSSBID_SAML_BINDING_FAILURE;
    else if (typeid(e) == typeid(ProfileException))
        *minor = GSSBID_SAML_PROFILE_FAILURE;
    else if (typeid(e) == typeid(FatalProfileException))
        *minor = GSSBID_SAML_FATAL_PROFILE_FAILURE;
    else if (typeid(e) == typeid(RetryableProfileException))
        *minor = GSSBID_SAML_RETRY_PROFILE_FAILURE;
    else if (typeid(e) == typeid(MetadataException))
        *minor = GSSBID_SAML_METADATA_FAILURE;
    else
        return GSS_S_CONTINUE_NEEDED;

    gssBidSaveStatusInfo(*minor, "%s", e.what());

    return GSS_S_FAILURE;
}

bool
BIDGSSSAMLAssertionProvider::getAttribute(const gss_buffer_t attr,
                                          int *authenticated,
                                          int *complete,
                                          gss_buffer_t value,
                                          gss_buffer_t display_value GSSBID_UNUSED,
                                          int *more) const
{
    string str;

    if (attr != GSS_C_NO_BUFFER && attr->length != 0)
        return false;

    if (m_assertion == NULL)
        return false;

    if (*more != -1)
        return false;

    if (authenticated != NULL)
        *authenticated = m_authenticated;
    if (complete != NULL)
        *complete = true;

    XMLHelper::serialize(m_assertion->marshall((DOMDocument *)NULL), str);

    if (value != NULL)
        duplicateBuffer(str, value);
    if (display_value != NULL)
        duplicateBuffer(str, display_value);

    *more = 0;

    return true;
}

gss_any_t
BIDGSSSAMLAssertionProvider::mapToAny(int authenticated,
                                      gss_buffer_t type_id GSSBID_UNUSED) const
{
    if (authenticated && !m_authenticated)
        return (gss_any_t)NULL;

    return (gss_any_t)m_assertion;
}

void
BIDGSSSAMLAssertionProvider::releaseAnyNameMapping(gss_buffer_t type_id GSSBID_UNUSED,
                                                   gss_any_t input) const
{
    delete ((saml2::Assertion *)input);
}

const char *
BIDGSSSAMLAssertionProvider::prefix(void) const
{
    return "urn:ietf:params:gss:federated-saml-assertion";
}

bool
BIDGSSSAMLAssertionProvider::init(void)
{
    bool ret = false;

    try {
        ret = SAMLConfig::getConfig().init();
    } catch (exception &e) {
    }

    if (ret)
        BIDGSSAttributeContext::registerProvider(ATTR_TYPE_SAML_ASSERTION, createAttrContext);

    return ret;
}

void
BIDGSSSAMLAssertionProvider::finalize(void)
{
    BIDGSSAttributeContext::unregisterProvider(ATTR_TYPE_SAML_ASSERTION);
}

BIDGSSAttributeProvider *
BIDGSSSAMLAssertionProvider::createAttrContext(void)
{
    return new BIDGSSSAMLAssertionProvider;
}

saml2::Assertion *
BIDGSSSAMLAssertionProvider::initAssertion(void)
{
    delete m_assertion;
    m_assertion = saml2::AssertionBuilder::buildAssertion();
    m_authenticated = false;

    return m_assertion;
}

/*
 * BIDGSSSAMLAttributeProvider is for retrieving the underlying attributes.
 */
bool
BIDGSSSAMLAttributeProvider::getAssertion(int *authenticated,
                                          saml2::Assertion **pAssertion,
                                          bool createIfAbsent) const
{
    BIDGSSSAMLAssertionProvider *saml;

    if (authenticated != NULL)
        *authenticated = false;
    if (pAssertion != NULL)
        *pAssertion = NULL;

    saml = static_cast<BIDGSSSAMLAssertionProvider *>
        (m_manager->getProvider(ATTR_TYPE_SAML_ASSERTION));
    if (saml == NULL)
        return false;

    if (authenticated != NULL)
        *authenticated = saml->authenticated();
    if (pAssertion != NULL)
        *pAssertion = saml->getAssertion();

    if (saml->getAssertion() == NULL) {
        if (createIfAbsent) {
            if (authenticated != NULL)
                *authenticated = false;
            if (pAssertion != NULL)
                *pAssertion = saml->initAssertion();
        } else
            return false;
    }

    return true;
}

bool
BIDGSSSAMLAttributeProvider::getAttributeTypes(BIDGSSAttributeIterator addAttribute,
                                               void *data) const
{
    saml2::Assertion *assertion;
    int authenticated;

    if (!getAssertion(&authenticated, &assertion))
        return true;

    /*
     * Note: the first prefix is added by the attribute provider manager
     *
     * From draft-hartman-gss-eap-naming-00:
     *
     *   Each attribute carried in the assertion SHOULD also be a GSS name
     *   attribute.  The name of this attribute has three parts, all separated
     *   by an ASCII space character.  The first part is
     *   urn:ietf:params:gss:federated-saml-attribute.  The second part is the URI for
     *   the SAML attribute name format.  The final part is the name of the
     *   SAML attribute.  If the mechanism performs an additional attribute
     *   query, the retrieved attributes SHOULD be GSS-API name attributes
     *   using the same name syntax.
     */
    /* For each attribute statement, look for an attribute match */
    const vector <saml2::AttributeStatement *> &statements =
        const_cast<const saml2::Assertion *>(assertion)->getAttributeStatements();

    for (vector<saml2::AttributeStatement *>::const_iterator s = statements.begin();
        s != statements.end();
        ++s) {
        const vector<saml2::Attribute*> &attrs =
            const_cast<const saml2::AttributeStatement*>(*s)->getAttributes();

        for (vector<saml2::Attribute*>::const_iterator a = attrs.begin(); a != attrs.end(); ++a) {
            const XMLCh *attributeName, *attributeNameFormat;
            XMLCh space[2] = { ' ', 0 };
            gss_buffer_desc utf8;

            attributeName = (*a)->getName();
            attributeNameFormat = (*a)->getNameFormat();
            if (attributeNameFormat == NULL || attributeNameFormat[0] == '\0')
                attributeNameFormat = saml2::Attribute::UNSPECIFIED;

            XMLCh qualifiedName[XMLString::stringLen(attributeNameFormat) + 1 +
                                XMLString::stringLen(attributeName) + 1];
            XMLString::copyString(qualifiedName, attributeNameFormat);
            XMLString::catString(qualifiedName, space);
            XMLString::catString(qualifiedName, attributeName);

            utf8.value = (void *)toUTF8(qualifiedName);
            utf8.length = strlen((char *)utf8.value);

            if (!addAttribute(m_manager, this, &utf8, data))
                return false;
        }
    }

    return true;
}

static BaseRefVectorOf<XMLCh> *
decomposeAttributeName(const gss_buffer_t attr)
{
    BaseRefVectorOf<XMLCh> *components;
    string str((const char *)attr->value, attr->length);
    auto_ptr_XMLCh qualifiedAttr(str.c_str());

    components = XMLString::tokenizeString(qualifiedAttr.get());

    if (components->size() != 2) {
        delete components;
        components = NULL;
    }

    return components;
}

bool
BIDGSSSAMLAttributeProvider::setAttribute(int complete GSSBID_UNUSED,
                                          const gss_buffer_t attr,
                                          const gss_buffer_t value)
{
    saml2::Assertion *assertion;
    saml2::Attribute *attribute;
    saml2::AttributeValue *attributeValue;
    saml2::AttributeStatement *attributeStatement;

    if (!getAssertion(NULL, &assertion, true))
        return false;

    if (assertion->getAttributeStatements().size() != 0) {
        attributeStatement = assertion->getAttributeStatements().front();
    } else {
        attributeStatement = saml2::AttributeStatementBuilder::buildAttributeStatement();
        assertion->getAttributeStatements().push_back(attributeStatement);
    }

    /* Check the attribute name consists of name format | whsp | name */
    BaseRefVectorOf<XMLCh> *components = decomposeAttributeName(attr);
    if (components == NULL)
        return false;

    attribute = saml2::AttributeBuilder::buildAttribute();
    attribute->setNameFormat(components->elementAt(0));
    attribute->setName(components->elementAt(1));

    attributeValue = saml2::AttributeValueBuilder::buildAttributeValue();
    auto_ptr_XMLCh unistr((char *)value->value, value->length);
    attributeValue->setTextContent(unistr.get());

    attribute->getAttributeValues().push_back(attributeValue);

    GSSBID_ASSERT(attributeStatement != NULL);
    attributeStatement->getAttributes().push_back(attribute);

    delete components;

    return true;
}

bool
BIDGSSSAMLAttributeProvider::deleteAttribute(const gss_buffer_t attr)
{
    saml2::Assertion *assertion;
    bool ret = false;

    if (!getAssertion(NULL, &assertion) ||
        assertion->getAttributeStatements().size() == 0)
        return false;

    /* Check the attribute name consists of name format | whsp | name */
    BaseRefVectorOf<XMLCh> *components = decomposeAttributeName(attr);
    if (components == NULL)
        return false;

    /* For each attribute statement, look for an attribute match */
    const vector<saml2::AttributeStatement *> &statements =
        const_cast<const saml2::Assertion *>(assertion)->getAttributeStatements();

    for (vector<saml2::AttributeStatement *>::const_iterator s = statements.begin();
        s != statements.end();
        ++s) {
        const vector<saml2::Attribute *> &attrs =
            const_cast<const saml2::AttributeStatement *>(*s)->getAttributes();
        ssize_t index = -1, i = 0;

        /* There's got to be an easier way to do this */
        for (vector<saml2::Attribute *>::const_iterator a = attrs.begin();
             a != attrs.end();
             ++a) {
            if (XMLString::equals((*a)->getNameFormat(), components->elementAt(0)) &&
                XMLString::equals((*a)->getName(), components->elementAt(1))) {
                index = i;
                break;
            }
            ++i;
        }
        if (index != -1) {
            (*s)->getAttributes().erase((*s)->getAttributes().begin() + index);
            ret = true;
        }
    }

    delete components;

    return ret;
}

bool
BIDGSSSAMLAttributeProvider::getAttribute(const gss_buffer_t attr,
                                          int *authenticated,
                                          int *complete,
                                          const saml2::Attribute **pAttribute) const
{
    saml2::Assertion *assertion;

    if (authenticated != NULL)
        *authenticated = false;
    if (complete != NULL)
        *complete = true;
    *pAttribute = NULL;

    if (!getAssertion(authenticated, &assertion) ||
        assertion->getAttributeStatements().size() == 0)
        return false;

    /* Check the attribute name consists of name format | whsp | name */
    BaseRefVectorOf<XMLCh> *components = decomposeAttributeName(attr);
    if (components == NULL)
        return false;

    /* For each attribute statement, look for an attribute match */
    const vector <saml2::AttributeStatement *> &statements =
        const_cast<const saml2::Assertion *>(assertion)->getAttributeStatements();
    const saml2::Attribute *ret = NULL;

    for (vector<saml2::AttributeStatement *>::const_iterator s = statements.begin();
        s != statements.end();
        ++s) {
        const vector<saml2::Attribute *> &attrs =
            const_cast<const saml2::AttributeStatement*>(*s)->getAttributes();

        for (vector<saml2::Attribute *>::const_iterator a = attrs.begin(); a != attrs.end(); ++a) {
            const XMLCh *attributeName, *attributeNameFormat;

            attributeName = (*a)->getName();
            attributeNameFormat = (*a)->getNameFormat();
            if (attributeNameFormat == NULL || attributeNameFormat[0] == '\0')
                attributeNameFormat = saml2::Attribute::UNSPECIFIED;

            if (XMLString::equals(attributeNameFormat, components->elementAt(0)) &&
                XMLString::equals(attributeName, components->elementAt(1))) {
                ret = *a;
                break;
            }
        }

        if (ret != NULL)
            break;
    }

    delete components;

    *pAttribute = ret;

    return (ret != NULL);
}

static bool
isBase64EncodedAttributeValueP(const saml2::AttributeValue *av)
{
    const xmltooling::QName *type = av->getSchemaType();

    if (type == NULL)
        return false;

    if (!type->hasNamespaceURI() ||
        !XMLString::equals(type->getNamespaceURI(), xmlconstants::XSD_NS))
        return false;

    if (!type->hasLocalPart() ||
        !XMLString::equals(type->getLocalPart(), base64Binary))
        return false;

    return true;
}

bool
BIDGSSSAMLAttributeProvider::getAttribute(const gss_buffer_t attr,
                                          int *authenticated,
                                          int *complete,
                                          gss_buffer_t value,
                                          gss_buffer_t display_value,
                                          int *more) const
{
    const saml2::Attribute *a;
    const saml2::AttributeValue *av;
    int nvalues, i = *more;

    *more = 0;

    if (!getAttribute(attr, authenticated, complete, &a))
        return false;

    nvalues = a->getAttributeValues().size();

    if (i == -1)
        i = 0;
    if (i >= nvalues)
        return false;
#ifdef __APPLE__
    av = (const saml2::AttributeValue *)((void *)(a->getAttributeValues().at(i)));
#else
    av = dynamic_cast<const saml2::AttributeValue *>(a->getAttributeValues().at(i));
#endif
    if (av != NULL) {
        bool base64Encoded = isBase64EncodedAttributeValueP(av);

        if (value != NULL) {
            char *stringValue = toUTF8(av->getTextContent(), true);
            size_t stringValueLen = strlen(stringValue);

            if (base64Encoded) {
                ssize_t octetLen;

                value->value = GSSBID_MALLOC(stringValueLen);
                if (value->value == NULL) {
                    GSSBID_FREE(stringValue);
                    throw new std::bad_alloc;
                }

                octetLen = base64Decode(stringValue, value->value);
                if (octetLen < 0) {
                    GSSBID_FREE(value->value);
                    GSSBID_FREE(stringValue);
                    value->value = NULL;
                    return false;
                }
                value->length = octetLen;
                GSSBID_FREE(stringValue);
            } else {
                value->value = stringValue;
                value->length = stringValueLen;
            }
        }
        if (display_value != NULL && base64Encoded == false) {
            display_value->value = toUTF8(av->getTextContent(), true);
            display_value->length = strlen((char *)display_value->value);
        }
    }

    if (nvalues > ++i)
        *more = i;

    return true;
}

gss_any_t
BIDGSSSAMLAttributeProvider::mapToAny(int authenticated GSSBID_UNUSED,
                                      gss_buffer_t type_id GSSBID_UNUSED) const
{
    return (gss_any_t)NULL;
}

void
BIDGSSSAMLAttributeProvider::releaseAnyNameMapping(gss_buffer_t type_id GSSBID_UNUSED,
                                                   gss_any_t input GSSBID_UNUSED) const
{
}

const char *
BIDGSSSAMLAttributeProvider::prefix(void) const
{
    return "urn:ietf:params:gss:federated-saml-attribute";
}

bool
BIDGSSSAMLAttributeProvider::init(void)
{
    BIDGSSAttributeContext::registerProvider(ATTR_TYPE_SAML, createAttrContext);
    return true;
}

void
BIDGSSSAMLAttributeProvider::finalize(void)
{
    BIDGSSAttributeContext::unregisterProvider(ATTR_TYPE_SAML);
}

BIDGSSAttributeProvider *
BIDGSSSAMLAttributeProvider::createAttrContext(void)
{
    return new BIDGSSSAMLAttributeProvider;
}

OM_uint32
gssBidSamlAttrProvidersInit(OM_uint32 *minor)
{
    if (!BIDGSSSAMLAssertionProvider::init() ||
        !BIDGSSSAMLAttributeProvider::init()) {
        *minor = GSSBID_SAML_INIT_FAILURE;
        return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}

OM_uint32
gssBidSamlAttrProvidersFinalize(OM_uint32 *minor)
{
    BIDGSSSAMLAttributeProvider::finalize();
    BIDGSSSAMLAssertionProvider::finalize();

    *minor = 0;
    return GSS_S_COMPLETE;
}
