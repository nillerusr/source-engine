//========= Copyright Valve Corporation, All rights reserved. ============//

#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0345 */
/* Compiler settings for msxml2.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __msxml2_h__
#define __msxml2_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IXMLDOMImplementation_FWD_DEFINED__
#define __IXMLDOMImplementation_FWD_DEFINED__
typedef interface IXMLDOMImplementation IXMLDOMImplementation;
#endif 	/* __IXMLDOMImplementation_FWD_DEFINED__ */


#ifndef __IXMLDOMNode_FWD_DEFINED__
#define __IXMLDOMNode_FWD_DEFINED__
typedef interface IXMLDOMNode IXMLDOMNode;
#endif 	/* __IXMLDOMNode_FWD_DEFINED__ */


#ifndef __IXMLDOMDocumentFragment_FWD_DEFINED__
#define __IXMLDOMDocumentFragment_FWD_DEFINED__
typedef interface IXMLDOMDocumentFragment IXMLDOMDocumentFragment;
#endif 	/* __IXMLDOMDocumentFragment_FWD_DEFINED__ */


#ifndef __IXMLDOMDocument_FWD_DEFINED__
#define __IXMLDOMDocument_FWD_DEFINED__
typedef interface IXMLDOMDocument IXMLDOMDocument;
#endif 	/* __IXMLDOMDocument_FWD_DEFINED__ */


#ifndef __IXMLDOMDocument2_FWD_DEFINED__
#define __IXMLDOMDocument2_FWD_DEFINED__
typedef interface IXMLDOMDocument2 IXMLDOMDocument2;
#endif 	/* __IXMLDOMDocument2_FWD_DEFINED__ */


#ifndef __IXMLDOMNodeList_FWD_DEFINED__
#define __IXMLDOMNodeList_FWD_DEFINED__
typedef interface IXMLDOMNodeList IXMLDOMNodeList;
#endif 	/* __IXMLDOMNodeList_FWD_DEFINED__ */


#ifndef __IXMLDOMNamedNodeMap_FWD_DEFINED__
#define __IXMLDOMNamedNodeMap_FWD_DEFINED__
typedef interface IXMLDOMNamedNodeMap IXMLDOMNamedNodeMap;
#endif 	/* __IXMLDOMNamedNodeMap_FWD_DEFINED__ */


#ifndef __IXMLDOMCharacterData_FWD_DEFINED__
#define __IXMLDOMCharacterData_FWD_DEFINED__
typedef interface IXMLDOMCharacterData IXMLDOMCharacterData;
#endif 	/* __IXMLDOMCharacterData_FWD_DEFINED__ */


#ifndef __IXMLDOMAttribute_FWD_DEFINED__
#define __IXMLDOMAttribute_FWD_DEFINED__
typedef interface IXMLDOMAttribute IXMLDOMAttribute;
#endif 	/* __IXMLDOMAttribute_FWD_DEFINED__ */


#ifndef __IXMLDOMElement_FWD_DEFINED__
#define __IXMLDOMElement_FWD_DEFINED__
typedef interface IXMLDOMElement IXMLDOMElement;
#endif 	/* __IXMLDOMElement_FWD_DEFINED__ */


#ifndef __IXMLDOMText_FWD_DEFINED__
#define __IXMLDOMText_FWD_DEFINED__
typedef interface IXMLDOMText IXMLDOMText;
#endif 	/* __IXMLDOMText_FWD_DEFINED__ */


#ifndef __IXMLDOMComment_FWD_DEFINED__
#define __IXMLDOMComment_FWD_DEFINED__
typedef interface IXMLDOMComment IXMLDOMComment;
#endif 	/* __IXMLDOMComment_FWD_DEFINED__ */


#ifndef __IXMLDOMProcessingInstruction_FWD_DEFINED__
#define __IXMLDOMProcessingInstruction_FWD_DEFINED__
typedef interface IXMLDOMProcessingInstruction IXMLDOMProcessingInstruction;
#endif 	/* __IXMLDOMProcessingInstruction_FWD_DEFINED__ */


#ifndef __IXMLDOMCDATASection_FWD_DEFINED__
#define __IXMLDOMCDATASection_FWD_DEFINED__
typedef interface IXMLDOMCDATASection IXMLDOMCDATASection;
#endif 	/* __IXMLDOMCDATASection_FWD_DEFINED__ */


#ifndef __IXMLDOMDocumentType_FWD_DEFINED__
#define __IXMLDOMDocumentType_FWD_DEFINED__
typedef interface IXMLDOMDocumentType IXMLDOMDocumentType;
#endif 	/* __IXMLDOMDocumentType_FWD_DEFINED__ */


#ifndef __IXMLDOMNotation_FWD_DEFINED__
#define __IXMLDOMNotation_FWD_DEFINED__
typedef interface IXMLDOMNotation IXMLDOMNotation;
#endif 	/* __IXMLDOMNotation_FWD_DEFINED__ */


#ifndef __IXMLDOMEntity_FWD_DEFINED__
#define __IXMLDOMEntity_FWD_DEFINED__
typedef interface IXMLDOMEntity IXMLDOMEntity;
#endif 	/* __IXMLDOMEntity_FWD_DEFINED__ */


#ifndef __IXMLDOMEntityReference_FWD_DEFINED__
#define __IXMLDOMEntityReference_FWD_DEFINED__
typedef interface IXMLDOMEntityReference IXMLDOMEntityReference;
#endif 	/* __IXMLDOMEntityReference_FWD_DEFINED__ */


#ifndef __IXMLDOMParseError_FWD_DEFINED__
#define __IXMLDOMParseError_FWD_DEFINED__
typedef interface IXMLDOMParseError IXMLDOMParseError;
#endif 	/* __IXMLDOMParseError_FWD_DEFINED__ */


#ifndef __IXMLDOMSchemaCollection_FWD_DEFINED__
#define __IXMLDOMSchemaCollection_FWD_DEFINED__
typedef interface IXMLDOMSchemaCollection IXMLDOMSchemaCollection;
#endif 	/* __IXMLDOMSchemaCollection_FWD_DEFINED__ */


#ifndef __IXTLRuntime_FWD_DEFINED__
#define __IXTLRuntime_FWD_DEFINED__
typedef interface IXTLRuntime IXTLRuntime;
#endif 	/* __IXTLRuntime_FWD_DEFINED__ */


#ifndef __IXSLTemplate_FWD_DEFINED__
#define __IXSLTemplate_FWD_DEFINED__
typedef interface IXSLTemplate IXSLTemplate;
#endif 	/* __IXSLTemplate_FWD_DEFINED__ */


#ifndef __IXSLProcessor_FWD_DEFINED__
#define __IXSLProcessor_FWD_DEFINED__
typedef interface IXSLProcessor IXSLProcessor;
#endif 	/* __IXSLProcessor_FWD_DEFINED__ */


#ifndef __ISAXXMLReader_FWD_DEFINED__
#define __ISAXXMLReader_FWD_DEFINED__
typedef interface ISAXXMLReader ISAXXMLReader;
#endif 	/* __ISAXXMLReader_FWD_DEFINED__ */


#ifndef __ISAXXMLFilter_FWD_DEFINED__
#define __ISAXXMLFilter_FWD_DEFINED__
typedef interface ISAXXMLFilter ISAXXMLFilter;
#endif 	/* __ISAXXMLFilter_FWD_DEFINED__ */


#ifndef __ISAXLocator_FWD_DEFINED__
#define __ISAXLocator_FWD_DEFINED__
typedef interface ISAXLocator ISAXLocator;
#endif 	/* __ISAXLocator_FWD_DEFINED__ */


#ifndef __ISAXEntityResolver_FWD_DEFINED__
#define __ISAXEntityResolver_FWD_DEFINED__
typedef interface ISAXEntityResolver ISAXEntityResolver;
#endif 	/* __ISAXEntityResolver_FWD_DEFINED__ */


#ifndef __ISAXContentHandler_FWD_DEFINED__
#define __ISAXContentHandler_FWD_DEFINED__
typedef interface ISAXContentHandler ISAXContentHandler;
#endif 	/* __ISAXContentHandler_FWD_DEFINED__ */


#ifndef __ISAXDTDHandler_FWD_DEFINED__
#define __ISAXDTDHandler_FWD_DEFINED__
typedef interface ISAXDTDHandler ISAXDTDHandler;
#endif 	/* __ISAXDTDHandler_FWD_DEFINED__ */


#ifndef __ISAXErrorHandler_FWD_DEFINED__
#define __ISAXErrorHandler_FWD_DEFINED__
typedef interface ISAXErrorHandler ISAXErrorHandler;
#endif 	/* __ISAXErrorHandler_FWD_DEFINED__ */


#ifndef __ISAXLexicalHandler_FWD_DEFINED__
#define __ISAXLexicalHandler_FWD_DEFINED__
typedef interface ISAXLexicalHandler ISAXLexicalHandler;
#endif 	/* __ISAXLexicalHandler_FWD_DEFINED__ */


#ifndef __ISAXDeclHandler_FWD_DEFINED__
#define __ISAXDeclHandler_FWD_DEFINED__
typedef interface ISAXDeclHandler ISAXDeclHandler;
#endif 	/* __ISAXDeclHandler_FWD_DEFINED__ */


#ifndef __ISAXAttributes_FWD_DEFINED__
#define __ISAXAttributes_FWD_DEFINED__
typedef interface ISAXAttributes ISAXAttributes;
#endif 	/* __ISAXAttributes_FWD_DEFINED__ */


#ifndef __IVBSAXXMLReader_FWD_DEFINED__
#define __IVBSAXXMLReader_FWD_DEFINED__
typedef interface IVBSAXXMLReader IVBSAXXMLReader;
#endif 	/* __IVBSAXXMLReader_FWD_DEFINED__ */


#ifndef __IVBSAXXMLFilter_FWD_DEFINED__
#define __IVBSAXXMLFilter_FWD_DEFINED__
typedef interface IVBSAXXMLFilter IVBSAXXMLFilter;
#endif 	/* __IVBSAXXMLFilter_FWD_DEFINED__ */


#ifndef __IVBSAXLocator_FWD_DEFINED__
#define __IVBSAXLocator_FWD_DEFINED__
typedef interface IVBSAXLocator IVBSAXLocator;
#endif 	/* __IVBSAXLocator_FWD_DEFINED__ */


#ifndef __IVBSAXEntityResolver_FWD_DEFINED__
#define __IVBSAXEntityResolver_FWD_DEFINED__
typedef interface IVBSAXEntityResolver IVBSAXEntityResolver;
#endif 	/* __IVBSAXEntityResolver_FWD_DEFINED__ */


#ifndef __IVBSAXContentHandler_FWD_DEFINED__
#define __IVBSAXContentHandler_FWD_DEFINED__
typedef interface IVBSAXContentHandler IVBSAXContentHandler;
#endif 	/* __IVBSAXContentHandler_FWD_DEFINED__ */


#ifndef __IVBSAXDTDHandler_FWD_DEFINED__
#define __IVBSAXDTDHandler_FWD_DEFINED__
typedef interface IVBSAXDTDHandler IVBSAXDTDHandler;
#endif 	/* __IVBSAXDTDHandler_FWD_DEFINED__ */


#ifndef __IVBSAXErrorHandler_FWD_DEFINED__
#define __IVBSAXErrorHandler_FWD_DEFINED__
typedef interface IVBSAXErrorHandler IVBSAXErrorHandler;
#endif 	/* __IVBSAXErrorHandler_FWD_DEFINED__ */


#ifndef __IVBSAXLexicalHandler_FWD_DEFINED__
#define __IVBSAXLexicalHandler_FWD_DEFINED__
typedef interface IVBSAXLexicalHandler IVBSAXLexicalHandler;
#endif 	/* __IVBSAXLexicalHandler_FWD_DEFINED__ */


#ifndef __IVBSAXDeclHandler_FWD_DEFINED__
#define __IVBSAXDeclHandler_FWD_DEFINED__
typedef interface IVBSAXDeclHandler IVBSAXDeclHandler;
#endif 	/* __IVBSAXDeclHandler_FWD_DEFINED__ */


#ifndef __IVBSAXAttributes_FWD_DEFINED__
#define __IVBSAXAttributes_FWD_DEFINED__
typedef interface IVBSAXAttributes IVBSAXAttributes;
#endif 	/* __IVBSAXAttributes_FWD_DEFINED__ */


#ifndef __IMXWriter_FWD_DEFINED__
#define __IMXWriter_FWD_DEFINED__
typedef interface IMXWriter IMXWriter;
#endif 	/* __IMXWriter_FWD_DEFINED__ */


#ifndef __IMXAttributes_FWD_DEFINED__
#define __IMXAttributes_FWD_DEFINED__
typedef interface IMXAttributes IMXAttributes;
#endif 	/* __IMXAttributes_FWD_DEFINED__ */


#ifndef __IMXReaderControl_FWD_DEFINED__
#define __IMXReaderControl_FWD_DEFINED__
typedef interface IMXReaderControl IMXReaderControl;
#endif 	/* __IMXReaderControl_FWD_DEFINED__ */


#ifndef __IMXSchemaDeclHandler_FWD_DEFINED__
#define __IMXSchemaDeclHandler_FWD_DEFINED__
typedef interface IMXSchemaDeclHandler IMXSchemaDeclHandler;
#endif 	/* __IMXSchemaDeclHandler_FWD_DEFINED__ */


#ifndef __IXMLDOMSchemaCollection2_FWD_DEFINED__
#define __IXMLDOMSchemaCollection2_FWD_DEFINED__
typedef interface IXMLDOMSchemaCollection2 IXMLDOMSchemaCollection2;
#endif 	/* __IXMLDOMSchemaCollection2_FWD_DEFINED__ */


#ifndef __ISchemaStringCollection_FWD_DEFINED__
#define __ISchemaStringCollection_FWD_DEFINED__
typedef interface ISchemaStringCollection ISchemaStringCollection;
#endif 	/* __ISchemaStringCollection_FWD_DEFINED__ */


#ifndef __ISchemaItemCollection_FWD_DEFINED__
#define __ISchemaItemCollection_FWD_DEFINED__
typedef interface ISchemaItemCollection ISchemaItemCollection;
#endif 	/* __ISchemaItemCollection_FWD_DEFINED__ */


#ifndef __ISchemaItem_FWD_DEFINED__
#define __ISchemaItem_FWD_DEFINED__
typedef interface ISchemaItem ISchemaItem;
#endif 	/* __ISchemaItem_FWD_DEFINED__ */


#ifndef __ISchema_FWD_DEFINED__
#define __ISchema_FWD_DEFINED__
typedef interface ISchema ISchema;
#endif 	/* __ISchema_FWD_DEFINED__ */


#ifndef __ISchemaParticle_FWD_DEFINED__
#define __ISchemaParticle_FWD_DEFINED__
typedef interface ISchemaParticle ISchemaParticle;
#endif 	/* __ISchemaParticle_FWD_DEFINED__ */


#ifndef __ISchemaAttribute_FWD_DEFINED__
#define __ISchemaAttribute_FWD_DEFINED__
typedef interface ISchemaAttribute ISchemaAttribute;
#endif 	/* __ISchemaAttribute_FWD_DEFINED__ */


#ifndef __ISchemaElement_FWD_DEFINED__
#define __ISchemaElement_FWD_DEFINED__
typedef interface ISchemaElement ISchemaElement;
#endif 	/* __ISchemaElement_FWD_DEFINED__ */


#ifndef __ISchemaType_FWD_DEFINED__
#define __ISchemaType_FWD_DEFINED__
typedef interface ISchemaType ISchemaType;
#endif 	/* __ISchemaType_FWD_DEFINED__ */


#ifndef __ISchemaComplexType_FWD_DEFINED__
#define __ISchemaComplexType_FWD_DEFINED__
typedef interface ISchemaComplexType ISchemaComplexType;
#endif 	/* __ISchemaComplexType_FWD_DEFINED__ */


#ifndef __ISchemaAttributeGroup_FWD_DEFINED__
#define __ISchemaAttributeGroup_FWD_DEFINED__
typedef interface ISchemaAttributeGroup ISchemaAttributeGroup;
#endif 	/* __ISchemaAttributeGroup_FWD_DEFINED__ */


#ifndef __ISchemaModelGroup_FWD_DEFINED__
#define __ISchemaModelGroup_FWD_DEFINED__
typedef interface ISchemaModelGroup ISchemaModelGroup;
#endif 	/* __ISchemaModelGroup_FWD_DEFINED__ */


#ifndef __ISchemaAny_FWD_DEFINED__
#define __ISchemaAny_FWD_DEFINED__
typedef interface ISchemaAny ISchemaAny;
#endif 	/* __ISchemaAny_FWD_DEFINED__ */


#ifndef __ISchemaIdentityConstraint_FWD_DEFINED__
#define __ISchemaIdentityConstraint_FWD_DEFINED__
typedef interface ISchemaIdentityConstraint ISchemaIdentityConstraint;
#endif 	/* __ISchemaIdentityConstraint_FWD_DEFINED__ */


#ifndef __ISchemaNotation_FWD_DEFINED__
#define __ISchemaNotation_FWD_DEFINED__
typedef interface ISchemaNotation ISchemaNotation;
#endif 	/* __ISchemaNotation_FWD_DEFINED__ */


#ifndef __IXMLElementCollection_FWD_DEFINED__
#define __IXMLElementCollection_FWD_DEFINED__
typedef interface IXMLElementCollection IXMLElementCollection;
#endif 	/* __IXMLElementCollection_FWD_DEFINED__ */


#ifndef __IXMLDocument_FWD_DEFINED__
#define __IXMLDocument_FWD_DEFINED__
typedef interface IXMLDocument IXMLDocument;
#endif 	/* __IXMLDocument_FWD_DEFINED__ */


#ifndef __IXMLDocument2_FWD_DEFINED__
#define __IXMLDocument2_FWD_DEFINED__
typedef interface IXMLDocument2 IXMLDocument2;
#endif 	/* __IXMLDocument2_FWD_DEFINED__ */


#ifndef __IXMLElement_FWD_DEFINED__
#define __IXMLElement_FWD_DEFINED__
typedef interface IXMLElement IXMLElement;
#endif 	/* __IXMLElement_FWD_DEFINED__ */


#ifndef __IXMLElement2_FWD_DEFINED__
#define __IXMLElement2_FWD_DEFINED__
typedef interface IXMLElement2 IXMLElement2;
#endif 	/* __IXMLElement2_FWD_DEFINED__ */


#ifndef __IXMLAttribute_FWD_DEFINED__
#define __IXMLAttribute_FWD_DEFINED__
typedef interface IXMLAttribute IXMLAttribute;
#endif 	/* __IXMLAttribute_FWD_DEFINED__ */


#ifndef __IXMLError_FWD_DEFINED__
#define __IXMLError_FWD_DEFINED__
typedef interface IXMLError IXMLError;
#endif 	/* __IXMLError_FWD_DEFINED__ */


#ifndef __IXMLDOMSelection_FWD_DEFINED__
#define __IXMLDOMSelection_FWD_DEFINED__
typedef interface IXMLDOMSelection IXMLDOMSelection;
#endif 	/* __IXMLDOMSelection_FWD_DEFINED__ */


#ifndef __XMLDOMDocumentEvents_FWD_DEFINED__
#define __XMLDOMDocumentEvents_FWD_DEFINED__
typedef interface XMLDOMDocumentEvents XMLDOMDocumentEvents;
#endif 	/* __XMLDOMDocumentEvents_FWD_DEFINED__ */


#ifndef __IDSOControl_FWD_DEFINED__
#define __IDSOControl_FWD_DEFINED__
typedef interface IDSOControl IDSOControl;
#endif 	/* __IDSOControl_FWD_DEFINED__ */


#ifndef __IXMLHTTPRequest_FWD_DEFINED__
#define __IXMLHTTPRequest_FWD_DEFINED__
typedef interface IXMLHTTPRequest IXMLHTTPRequest;
#endif 	/* __IXMLHTTPRequest_FWD_DEFINED__ */


#ifndef __IServerXMLHTTPRequest_FWD_DEFINED__
#define __IServerXMLHTTPRequest_FWD_DEFINED__
typedef interface IServerXMLHTTPRequest IServerXMLHTTPRequest;
#endif 	/* __IServerXMLHTTPRequest_FWD_DEFINED__ */


#ifndef __IServerXMLHTTPRequest2_FWD_DEFINED__
#define __IServerXMLHTTPRequest2_FWD_DEFINED__
typedef interface IServerXMLHTTPRequest2 IServerXMLHTTPRequest2;
#endif 	/* __IServerXMLHTTPRequest2_FWD_DEFINED__ */


#ifndef __IMXNamespacePrefixes_FWD_DEFINED__
#define __IMXNamespacePrefixes_FWD_DEFINED__
typedef interface IMXNamespacePrefixes IMXNamespacePrefixes;
#endif 	/* __IMXNamespacePrefixes_FWD_DEFINED__ */


#ifndef __IVBMXNamespaceManager_FWD_DEFINED__
#define __IVBMXNamespaceManager_FWD_DEFINED__
typedef interface IVBMXNamespaceManager IVBMXNamespaceManager;
#endif 	/* __IVBMXNamespaceManager_FWD_DEFINED__ */


#ifndef __IMXNamespaceManager_FWD_DEFINED__
#define __IMXNamespaceManager_FWD_DEFINED__
typedef interface IMXNamespaceManager IMXNamespaceManager;
#endif 	/* __IMXNamespaceManager_FWD_DEFINED__ */


#ifndef __DOMDocument_FWD_DEFINED__
#define __DOMDocument_FWD_DEFINED__

#ifdef __cplusplus
typedef class DOMDocument DOMDocument;
#else
typedef struct DOMDocument DOMDocument;
#endif /* __cplusplus */

#endif 	/* __DOMDocument_FWD_DEFINED__ */


#ifndef __DOMDocument26_FWD_DEFINED__
#define __DOMDocument26_FWD_DEFINED__

#ifdef __cplusplus
typedef class DOMDocument26 DOMDocument26;
#else
typedef struct DOMDocument26 DOMDocument26;
#endif /* __cplusplus */

#endif 	/* __DOMDocument26_FWD_DEFINED__ */


#ifndef __DOMDocument30_FWD_DEFINED__
#define __DOMDocument30_FWD_DEFINED__

#ifdef __cplusplus
typedef class DOMDocument30 DOMDocument30;
#else
typedef struct DOMDocument30 DOMDocument30;
#endif /* __cplusplus */

#endif 	/* __DOMDocument30_FWD_DEFINED__ */


#ifndef __DOMDocument40_FWD_DEFINED__
#define __DOMDocument40_FWD_DEFINED__

#ifdef __cplusplus
typedef class DOMDocument40 DOMDocument40;
#else
typedef struct DOMDocument40 DOMDocument40;
#endif /* __cplusplus */

#endif 	/* __DOMDocument40_FWD_DEFINED__ */


#ifndef __FreeThreadedDOMDocument_FWD_DEFINED__
#define __FreeThreadedDOMDocument_FWD_DEFINED__

#ifdef __cplusplus
typedef class FreeThreadedDOMDocument FreeThreadedDOMDocument;
#else
typedef struct FreeThreadedDOMDocument FreeThreadedDOMDocument;
#endif /* __cplusplus */

#endif 	/* __FreeThreadedDOMDocument_FWD_DEFINED__ */


#ifndef __FreeThreadedDOMDocument26_FWD_DEFINED__
#define __FreeThreadedDOMDocument26_FWD_DEFINED__

#ifdef __cplusplus
typedef class FreeThreadedDOMDocument26 FreeThreadedDOMDocument26;
#else
typedef struct FreeThreadedDOMDocument26 FreeThreadedDOMDocument26;
#endif /* __cplusplus */

#endif 	/* __FreeThreadedDOMDocument26_FWD_DEFINED__ */


#ifndef __FreeThreadedDOMDocument30_FWD_DEFINED__
#define __FreeThreadedDOMDocument30_FWD_DEFINED__

#ifdef __cplusplus
typedef class FreeThreadedDOMDocument30 FreeThreadedDOMDocument30;
#else
typedef struct FreeThreadedDOMDocument30 FreeThreadedDOMDocument30;
#endif /* __cplusplus */

#endif 	/* __FreeThreadedDOMDocument30_FWD_DEFINED__ */


#ifndef __FreeThreadedDOMDocument40_FWD_DEFINED__
#define __FreeThreadedDOMDocument40_FWD_DEFINED__

#ifdef __cplusplus
typedef class FreeThreadedDOMDocument40 FreeThreadedDOMDocument40;
#else
typedef struct FreeThreadedDOMDocument40 FreeThreadedDOMDocument40;
#endif /* __cplusplus */

#endif 	/* __FreeThreadedDOMDocument40_FWD_DEFINED__ */


#ifndef __XMLSchemaCache_FWD_DEFINED__
#define __XMLSchemaCache_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLSchemaCache XMLSchemaCache;
#else
typedef struct XMLSchemaCache XMLSchemaCache;
#endif /* __cplusplus */

#endif 	/* __XMLSchemaCache_FWD_DEFINED__ */


#ifndef __XMLSchemaCache26_FWD_DEFINED__
#define __XMLSchemaCache26_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLSchemaCache26 XMLSchemaCache26;
#else
typedef struct XMLSchemaCache26 XMLSchemaCache26;
#endif /* __cplusplus */

#endif 	/* __XMLSchemaCache26_FWD_DEFINED__ */


#ifndef __XMLSchemaCache30_FWD_DEFINED__
#define __XMLSchemaCache30_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLSchemaCache30 XMLSchemaCache30;
#else
typedef struct XMLSchemaCache30 XMLSchemaCache30;
#endif /* __cplusplus */

#endif 	/* __XMLSchemaCache30_FWD_DEFINED__ */


#ifndef __XMLSchemaCache40_FWD_DEFINED__
#define __XMLSchemaCache40_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLSchemaCache40 XMLSchemaCache40;
#else
typedef struct XMLSchemaCache40 XMLSchemaCache40;
#endif /* __cplusplus */

#endif 	/* __XMLSchemaCache40_FWD_DEFINED__ */


#ifndef __XSLTemplate_FWD_DEFINED__
#define __XSLTemplate_FWD_DEFINED__

#ifdef __cplusplus
typedef class XSLTemplate XSLTemplate;
#else
typedef struct XSLTemplate XSLTemplate;
#endif /* __cplusplus */

#endif 	/* __XSLTemplate_FWD_DEFINED__ */


#ifndef __XSLTemplate26_FWD_DEFINED__
#define __XSLTemplate26_FWD_DEFINED__

#ifdef __cplusplus
typedef class XSLTemplate26 XSLTemplate26;
#else
typedef struct XSLTemplate26 XSLTemplate26;
#endif /* __cplusplus */

#endif 	/* __XSLTemplate26_FWD_DEFINED__ */


#ifndef __XSLTemplate30_FWD_DEFINED__
#define __XSLTemplate30_FWD_DEFINED__

#ifdef __cplusplus
typedef class XSLTemplate30 XSLTemplate30;
#else
typedef struct XSLTemplate30 XSLTemplate30;
#endif /* __cplusplus */

#endif 	/* __XSLTemplate30_FWD_DEFINED__ */


#ifndef __XSLTemplate40_FWD_DEFINED__
#define __XSLTemplate40_FWD_DEFINED__

#ifdef __cplusplus
typedef class XSLTemplate40 XSLTemplate40;
#else
typedef struct XSLTemplate40 XSLTemplate40;
#endif /* __cplusplus */

#endif 	/* __XSLTemplate40_FWD_DEFINED__ */


#ifndef __DSOControl_FWD_DEFINED__
#define __DSOControl_FWD_DEFINED__

#ifdef __cplusplus
typedef class DSOControl DSOControl;
#else
typedef struct DSOControl DSOControl;
#endif /* __cplusplus */

#endif 	/* __DSOControl_FWD_DEFINED__ */


#ifndef __DSOControl26_FWD_DEFINED__
#define __DSOControl26_FWD_DEFINED__

#ifdef __cplusplus
typedef class DSOControl26 DSOControl26;
#else
typedef struct DSOControl26 DSOControl26;
#endif /* __cplusplus */

#endif 	/* __DSOControl26_FWD_DEFINED__ */


#ifndef __DSOControl30_FWD_DEFINED__
#define __DSOControl30_FWD_DEFINED__

#ifdef __cplusplus
typedef class DSOControl30 DSOControl30;
#else
typedef struct DSOControl30 DSOControl30;
#endif /* __cplusplus */

#endif 	/* __DSOControl30_FWD_DEFINED__ */


#ifndef __DSOControl40_FWD_DEFINED__
#define __DSOControl40_FWD_DEFINED__

#ifdef __cplusplus
typedef class DSOControl40 DSOControl40;
#else
typedef struct DSOControl40 DSOControl40;
#endif /* __cplusplus */

#endif 	/* __DSOControl40_FWD_DEFINED__ */


#ifndef __XMLHTTP_FWD_DEFINED__
#define __XMLHTTP_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLHTTP XMLHTTP;
#else
typedef struct XMLHTTP XMLHTTP;
#endif /* __cplusplus */

#endif 	/* __XMLHTTP_FWD_DEFINED__ */


#ifndef __XMLHTTP26_FWD_DEFINED__
#define __XMLHTTP26_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLHTTP26 XMLHTTP26;
#else
typedef struct XMLHTTP26 XMLHTTP26;
#endif /* __cplusplus */

#endif 	/* __XMLHTTP26_FWD_DEFINED__ */


#ifndef __XMLHTTP30_FWD_DEFINED__
#define __XMLHTTP30_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLHTTP30 XMLHTTP30;
#else
typedef struct XMLHTTP30 XMLHTTP30;
#endif /* __cplusplus */

#endif 	/* __XMLHTTP30_FWD_DEFINED__ */


#ifndef __XMLHTTP40_FWD_DEFINED__
#define __XMLHTTP40_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLHTTP40 XMLHTTP40;
#else
typedef struct XMLHTTP40 XMLHTTP40;
#endif /* __cplusplus */

#endif 	/* __XMLHTTP40_FWD_DEFINED__ */


#ifndef __ServerXMLHTTP_FWD_DEFINED__
#define __ServerXMLHTTP_FWD_DEFINED__

#ifdef __cplusplus
typedef class ServerXMLHTTP ServerXMLHTTP;
#else
typedef struct ServerXMLHTTP ServerXMLHTTP;
#endif /* __cplusplus */

#endif 	/* __ServerXMLHTTP_FWD_DEFINED__ */


#ifndef __ServerXMLHTTP30_FWD_DEFINED__
#define __ServerXMLHTTP30_FWD_DEFINED__

#ifdef __cplusplus
typedef class ServerXMLHTTP30 ServerXMLHTTP30;
#else
typedef struct ServerXMLHTTP30 ServerXMLHTTP30;
#endif /* __cplusplus */

#endif 	/* __ServerXMLHTTP30_FWD_DEFINED__ */


#ifndef __ServerXMLHTTP40_FWD_DEFINED__
#define __ServerXMLHTTP40_FWD_DEFINED__

#ifdef __cplusplus
typedef class ServerXMLHTTP40 ServerXMLHTTP40;
#else
typedef struct ServerXMLHTTP40 ServerXMLHTTP40;
#endif /* __cplusplus */

#endif 	/* __ServerXMLHTTP40_FWD_DEFINED__ */


#ifndef __SAXXMLReader_FWD_DEFINED__
#define __SAXXMLReader_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXXMLReader SAXXMLReader;
#else
typedef struct SAXXMLReader SAXXMLReader;
#endif /* __cplusplus */

#endif 	/* __SAXXMLReader_FWD_DEFINED__ */


#ifndef __SAXXMLReader30_FWD_DEFINED__
#define __SAXXMLReader30_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXXMLReader30 SAXXMLReader30;
#else
typedef struct SAXXMLReader30 SAXXMLReader30;
#endif /* __cplusplus */

#endif 	/* __SAXXMLReader30_FWD_DEFINED__ */


#ifndef __SAXXMLReader40_FWD_DEFINED__
#define __SAXXMLReader40_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXXMLReader40 SAXXMLReader40;
#else
typedef struct SAXXMLReader40 SAXXMLReader40;
#endif /* __cplusplus */

#endif 	/* __SAXXMLReader40_FWD_DEFINED__ */


#ifndef __MXXMLWriter_FWD_DEFINED__
#define __MXXMLWriter_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXXMLWriter MXXMLWriter;
#else
typedef struct MXXMLWriter MXXMLWriter;
#endif /* __cplusplus */

#endif 	/* __MXXMLWriter_FWD_DEFINED__ */


#ifndef __MXXMLWriter30_FWD_DEFINED__
#define __MXXMLWriter30_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXXMLWriter30 MXXMLWriter30;
#else
typedef struct MXXMLWriter30 MXXMLWriter30;
#endif /* __cplusplus */

#endif 	/* __MXXMLWriter30_FWD_DEFINED__ */


#ifndef __MXXMLWriter40_FWD_DEFINED__
#define __MXXMLWriter40_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXXMLWriter40 MXXMLWriter40;
#else
typedef struct MXXMLWriter40 MXXMLWriter40;
#endif /* __cplusplus */

#endif 	/* __MXXMLWriter40_FWD_DEFINED__ */


#ifndef __MXHTMLWriter_FWD_DEFINED__
#define __MXHTMLWriter_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXHTMLWriter MXHTMLWriter;
#else
typedef struct MXHTMLWriter MXHTMLWriter;
#endif /* __cplusplus */

#endif 	/* __MXHTMLWriter_FWD_DEFINED__ */


#ifndef __MXHTMLWriter30_FWD_DEFINED__
#define __MXHTMLWriter30_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXHTMLWriter30 MXHTMLWriter30;
#else
typedef struct MXHTMLWriter30 MXHTMLWriter30;
#endif /* __cplusplus */

#endif 	/* __MXHTMLWriter30_FWD_DEFINED__ */


#ifndef __MXHTMLWriter40_FWD_DEFINED__
#define __MXHTMLWriter40_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXHTMLWriter40 MXHTMLWriter40;
#else
typedef struct MXHTMLWriter40 MXHTMLWriter40;
#endif /* __cplusplus */

#endif 	/* __MXHTMLWriter40_FWD_DEFINED__ */


#ifndef __SAXAttributes_FWD_DEFINED__
#define __SAXAttributes_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXAttributes SAXAttributes;
#else
typedef struct SAXAttributes SAXAttributes;
#endif /* __cplusplus */

#endif 	/* __SAXAttributes_FWD_DEFINED__ */


#ifndef __SAXAttributes30_FWD_DEFINED__
#define __SAXAttributes30_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXAttributes30 SAXAttributes30;
#else
typedef struct SAXAttributes30 SAXAttributes30;
#endif /* __cplusplus */

#endif 	/* __SAXAttributes30_FWD_DEFINED__ */


#ifndef __SAXAttributes40_FWD_DEFINED__
#define __SAXAttributes40_FWD_DEFINED__

#ifdef __cplusplus
typedef class SAXAttributes40 SAXAttributes40;
#else
typedef struct SAXAttributes40 SAXAttributes40;
#endif /* __cplusplus */

#endif 	/* __SAXAttributes40_FWD_DEFINED__ */


#ifndef __MXNamespaceManager_FWD_DEFINED__
#define __MXNamespaceManager_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXNamespaceManager MXNamespaceManager;
#else
typedef struct MXNamespaceManager MXNamespaceManager;
#endif /* __cplusplus */

#endif 	/* __MXNamespaceManager_FWD_DEFINED__ */


#ifndef __MXNamespaceManager40_FWD_DEFINED__
#define __MXNamespaceManager40_FWD_DEFINED__

#ifdef __cplusplus
typedef class MXNamespaceManager40 MXNamespaceManager40;
#else
typedef struct MXNamespaceManager40 MXNamespaceManager40;
#endif /* __cplusplus */

#endif 	/* __MXNamespaceManager40_FWD_DEFINED__ */


#ifndef __XMLDocument_FWD_DEFINED__
#define __XMLDocument_FWD_DEFINED__

#ifdef __cplusplus
typedef class XMLDocument XMLDocument;
#else
typedef struct XMLDocument XMLDocument;
#endif /* __cplusplus */

#endif 	/* __XMLDocument_FWD_DEFINED__ */


/* header files for imported files */
#include "unknwn.h"
#include "objidl.h"
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

/* interface __MIDL_itf_msxml2_0000 */
/* [local] */ 

//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1997-1998.
//
//--------------------------------------------------------------------------
#define DOMDocument DOMDocument2
#define CLSID_DOMDocument CLSID_DOMDocument2

#ifdef __USE_MSXML2_NAMESPACE__
namespace MSXML2 {
#endif
#ifndef __msxml_h__
typedef struct _xml_error
    {
    unsigned int _nLine;
    BSTR _pchBuf;
    unsigned int _cchBuf;
    unsigned int _ich;
    BSTR _pszFound;
    BSTR _pszExpected;
    DWORD _reserved1;
    DWORD _reserved2;
    } 	XML_ERROR;

#endif
#ifndef __ISAXXMLReader_INTERFACE_DEFINED__
#undef __MSXML2_LIBRARY_DEFINED__
#endif


extern RPC_IF_HANDLE __MIDL_itf_msxml2_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_msxml2_0000_v0_0_s_ifspec;


#ifndef __MSXML2_LIBRARY_DEFINED__
#define __MSXML2_LIBRARY_DEFINED__

/* library MSXML2 */
/* [lcid][helpstring][version][uuid] */ 







































































#undef ParseURL
#if !defined(__msxml_h__)
typedef /* [helpstring] */ 
enum tagXMLEMEM_TYPE
    {	XMLELEMTYPE_ELEMENT	= 0,
	XMLELEMTYPE_TEXT	= XMLELEMTYPE_ELEMENT + 1,
	XMLELEMTYPE_COMMENT	= XMLELEMTYPE_TEXT + 1,
	XMLELEMTYPE_DOCUMENT	= XMLELEMTYPE_COMMENT + 1,
	XMLELEMTYPE_DTD	= XMLELEMTYPE_DOCUMENT + 1,
	XMLELEMTYPE_PI	= XMLELEMTYPE_DTD + 1,
	XMLELEMTYPE_OTHER	= XMLELEMTYPE_PI + 1
    } 	XMLELEM_TYPE;

#endif
#if !defined(__msxml_h__) || defined(__IXMLElementNotificationSink_INTERFACE_DEFINED__)
typedef /* [helpstring] */ 
enum tagDOMNodeType
    {	NODE_INVALID	= 0,
	NODE_ELEMENT	= NODE_INVALID + 1,
	NODE_ATTRIBUTE	= NODE_ELEMENT + 1,
	NODE_TEXT	= NODE_ATTRIBUTE + 1,
	NODE_CDATA_SECTION	= NODE_TEXT + 1,
	NODE_ENTITY_REFERENCE	= NODE_CDATA_SECTION + 1,
	NODE_ENTITY	= NODE_ENTITY_REFERENCE + 1,
	NODE_PROCESSING_INSTRUCTION	= NODE_ENTITY + 1,
	NODE_COMMENT	= NODE_PROCESSING_INSTRUCTION + 1,
	NODE_DOCUMENT	= NODE_COMMENT + 1,
	NODE_DOCUMENT_TYPE	= NODE_DOCUMENT + 1,
	NODE_DOCUMENT_FRAGMENT	= NODE_DOCUMENT_TYPE + 1,
	NODE_NOTATION	= NODE_DOCUMENT_FRAGMENT + 1
    } 	DOMNodeType;

#endif
typedef /* [helpstring] */ 
enum _SERVERXMLHTTP_OPTION
    {	SXH_OPTION_URL	= -1,
	SXH_OPTION_URL_CODEPAGE	= SXH_OPTION_URL + 1,
	SXH_OPTION_ESCAPE_PERCENT_IN_URL	= SXH_OPTION_URL_CODEPAGE + 1,
	SXH_OPTION_IGNORE_SERVER_SSL_CERT_ERROR_FLAGS	= SXH_OPTION_ESCAPE_PERCENT_IN_URL + 1,
	SXH_OPTION_SELECT_CLIENT_SSL_CERT	= SXH_OPTION_IGNORE_SERVER_SSL_CERT_ERROR_FLAGS + 1
    } 	SERVERXMLHTTP_OPTION;

typedef /* [helpstring] */ 
enum _SXH_SERVER_CERT_OPTION
    {	SXH_SERVER_CERT_IGNORE_UNKNOWN_CA	= 0x100,
	SXH_SERVER_CERT_IGNORE_WRONG_USAGE	= 0x200,
	SXH_SERVER_CERT_IGNORE_CERT_CN_INVALID	= 0x1000,
	SXH_SERVER_CERT_IGNORE_CERT_DATE_INVALID	= 0x2000,
	SXH_SERVER_CERT_IGNORE_ALL_SERVER_ERRORS	= SXH_SERVER_CERT_IGNORE_UNKNOWN_CA + SXH_SERVER_CERT_IGNORE_WRONG_USAGE + SXH_SERVER_CERT_IGNORE_CERT_CN_INVALID + SXH_SERVER_CERT_IGNORE_CERT_DATE_INVALID
    } 	SXH_SERVER_CERT_OPTION;

typedef /* [helpstring] */ 
enum _SXH_PROXY_SETTING
    {	SXH_PROXY_SET_DEFAULT	= 0,
	SXH_PROXY_SET_PRECONFIG	= 0,
	SXH_PROXY_SET_DIRECT	= 0x1,
	SXH_PROXY_SET_PROXY	= 0x2
    } 	SXH_PROXY_SETTING;

typedef /* [helpstring] */ 
enum _SOMITEMTYPE
    {	SOMITEM_SCHEMA	= 0x1000,
	SOMITEM_ATTRIBUTE	= 0x1001,
	SOMITEM_ATTRIBUTEGROUP	= 0x1002,
	SOMITEM_NOTATION	= 0x1003,
	SOMITEM_IDENTITYCONSTRAINT	= 0x1100,
	SOMITEM_KEY	= 0x1101,
	SOMITEM_KEYREF	= 0x1102,
	SOMITEM_UNIQUE	= 0x1103,
	SOMITEM_ANYTYPE	= 0x2000,
	SOMITEM_DATATYPE	= 0x2100,
	SOMITEM_DATATYPE_ANYTYPE	= 0x2101,
	SOMITEM_DATATYPE_ANYURI	= 0x2102,
	SOMITEM_DATATYPE_BASE64BINARY	= 0x2103,
	SOMITEM_DATATYPE_BOOLEAN	= 0x2104,
	SOMITEM_DATATYPE_BYTE	= 0x2105,
	SOMITEM_DATATYPE_DATE	= 0x2106,
	SOMITEM_DATATYPE_DATETIME	= 0x2107,
	SOMITEM_DATATYPE_DAY	= 0x2108,
	SOMITEM_DATATYPE_DECIMAL	= 0x2109,
	SOMITEM_DATATYPE_DOUBLE	= 0x210a,
	SOMITEM_DATATYPE_DURATION	= 0x210b,
	SOMITEM_DATATYPE_ENTITIES	= 0x210c,
	SOMITEM_DATATYPE_ENTITY	= 0x210d,
	SOMITEM_DATATYPE_FLOAT	= 0x210e,
	SOMITEM_DATATYPE_HEXBINARY	= 0x210f,
	SOMITEM_DATATYPE_ID	= 0x2110,
	SOMITEM_DATATYPE_IDREF	= 0x2111,
	SOMITEM_DATATYPE_IDREFS	= 0x2112,
	SOMITEM_DATATYPE_INT	= 0x2113,
	SOMITEM_DATATYPE_INTEGER	= 0x2114,
	SOMITEM_DATATYPE_LANGUAGE	= 0x2115,
	SOMITEM_DATATYPE_LONG	= 0x2116,
	SOMITEM_DATATYPE_MONTH	= 0x2117,
	SOMITEM_DATATYPE_MONTHDAY	= 0x2118,
	SOMITEM_DATATYPE_NAME	= 0x2119,
	SOMITEM_DATATYPE_NCNAME	= 0x211a,
	SOMITEM_DATATYPE_NEGATIVEINTEGER	= 0x211b,
	SOMITEM_DATATYPE_NMTOKEN	= 0x211c,
	SOMITEM_DATATYPE_NMTOKENS	= 0x211d,
	SOMITEM_DATATYPE_NONNEGATIVEINTEGER	= 0x211e,
	SOMITEM_DATATYPE_NONPOSITIVEINTEGER	= 0x211f,
	SOMITEM_DATATYPE_NORMALIZEDSTRING	= 0x2120,
	SOMITEM_DATATYPE_NOTATION	= 0x2121,
	SOMITEM_DATATYPE_POSITIVEINTEGER	= 0x2122,
	SOMITEM_DATATYPE_QNAME	= 0x2123,
	SOMITEM_DATATYPE_SHORT	= 0x2124,
	SOMITEM_DATATYPE_STRING	= 0x2125,
	SOMITEM_DATATYPE_TIME	= 0x2126,
	SOMITEM_DATATYPE_TOKEN	= 0x2127,
	SOMITEM_DATATYPE_UNSIGNEDBYTE	= 0x2128,
	SOMITEM_DATATYPE_UNSIGNEDINT	= 0x2129,
	SOMITEM_DATATYPE_UNSIGNEDLONG	= 0x212a,
	SOMITEM_DATATYPE_UNSIGNEDSHORT	= 0x212b,
	SOMITEM_DATATYPE_YEAR	= 0x212c,
	SOMITEM_DATATYPE_YEARMONTH	= 0x212d,
	SOMITEM_SIMPLETYPE	= 0x2200,
	SOMITEM_COMPLEXTYPE	= 0x2400,
	SOMITEM_PARTICLE	= 0x4000,
	SOMITEM_ANY	= 0x4001,
	SOMITEM_ANYATTRIBUTE	= 0x4002,
	SOMITEM_ELEMENT	= 0x4003,
	SOMITEM_GROUP	= 0x4100,
	SOMITEM_ALL	= 0x4101,
	SOMITEM_CHOICE	= 0x4102,
	SOMITEM_SEQUENCE	= 0x4103,
	SOMITEM_EMPTYPARTICLE	= 0x4104,
	SOMITEM_NULL	= 0x800,
	SOMITEM_NULL_TYPE	= 0x2800,
	SOMITEM_NULL_ANY	= 0x4801,
	SOMITEM_NULL_ANYATTRIBUTE	= 0x4802,
	SOMITEM_NULL_ELEMENT	= 0x4803
    } 	SOMITEMTYPE;

typedef /* [helpstring] */ 
enum _SCHEMAUSE
    {	SCHEMAUSE_OPTIONAL	= 0,
	SCHEMAUSE_PROHIBITED	= SCHEMAUSE_OPTIONAL + 1,
	SCHEMAUSE_REQUIRED	= SCHEMAUSE_PROHIBITED + 1
    } 	SCHEMAUSE;

typedef /* [helpstring] */ 
enum _SCHEMADERIVATIONMETHOD
    {	SCHEMADERIVATIONMETHOD_EMPTY	= 0,
	SCHEMADERIVATIONMETHOD_SUBSTITUTION	= 0x1,
	SCHEMADERIVATIONMETHOD_EXTENSION	= 0x2,
	SCHEMADERIVATIONMETHOD_RESTRICTION	= 0x4,
	SCHEMADERIVATIONMETHOD_LIST	= 0x8,
	SCHEMADERIVATIONMETHOD_UNION	= 0x10,
	SCHEMADERIVATIONMETHOD_ALL	= 0xff,
	SCHEMADERIVATIONMETHOD_NONE	= 0x100
    } 	SCHEMADERIVATIONMETHOD;

typedef /* [helpstring] */ 
enum _SCHEMACONTENTTYPE
    {	SCHEMACONTENTTYPE_EMPTY	= 0,
	SCHEMACONTENTTYPE_TEXTONLY	= SCHEMACONTENTTYPE_EMPTY + 1,
	SCHEMACONTENTTYPE_ELEMENTONLY	= SCHEMACONTENTTYPE_TEXTONLY + 1,
	SCHEMACONTENTTYPE_MIXED	= SCHEMACONTENTTYPE_ELEMENTONLY + 1
    } 	SCHEMACONTENTTYPE;

typedef /* [helpstring] */ 
enum _SCHEMAPROCESSCONTENTS
    {	SCHEMAPROCESSCONTENTS_NONE	= 0,
	SCHEMAPROCESSCONTENTS_SKIP	= SCHEMAPROCESSCONTENTS_NONE + 1,
	SCHEMAPROCESSCONTENTS_LAX	= SCHEMAPROCESSCONTENTS_SKIP + 1,
	SCHEMAPROCESSCONTENTS_STRICT	= SCHEMAPROCESSCONTENTS_LAX + 1
    } 	SCHEMAPROCESSCONTENTS;

typedef /* [helpstring] */ 
enum _SCHEMAWHITESPACE
    {	SCHEMAWHITESPACE_NONE	= -1,
	SCHEMAWHITESPACE_PRESERVE	= 0,
	SCHEMAWHITESPACE_REPLACE	= 1,
	SCHEMAWHITESPACE_COLLAPSE	= 2
    } 	SCHEMAWHITESPACE;

typedef /* [helpstring] */ 
enum _SCHEMATYPEVARIETY
    {	SCHEMATYPEVARIETY_NONE	= -1,
	SCHEMATYPEVARIETY_ATOMIC	= 0,
	SCHEMATYPEVARIETY_LIST	= 1,
	SCHEMATYPEVARIETY_UNION	= 2
    } 	SCHEMATYPEVARIETY;


EXTERN_C const IID LIBID_MSXML2;

#ifndef __IXMLDOMImplementation_INTERFACE_DEFINED__
#define __IXMLDOMImplementation_INTERFACE_DEFINED__

/* interface IXMLDOMImplementation */
/* [uuid][dual][oleautomation][unique][nonextensible][object][local] */ 


EXTERN_C const IID IID_IXMLDOMImplementation;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8F-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMImplementation : public IDispatch
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE hasFeature( 
            /* [in] */ BSTR feature,
            /* [in] */ BSTR version,
            /* [retval][out] */ VARIANT_BOOL *hasFeature) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMImplementationVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMImplementation * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMImplementation * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMImplementation * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMImplementation * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMImplementation * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMImplementation * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMImplementation * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *hasFeature )( 
            IXMLDOMImplementation * This,
            /* [in] */ BSTR feature,
            /* [in] */ BSTR version,
            /* [retval][out] */ VARIANT_BOOL *hasFeature);
        
        END_INTERFACE
    } IXMLDOMImplementationVtbl;

    interface IXMLDOMImplementation
    {
        CONST_VTBL struct IXMLDOMImplementationVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMImplementation_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMImplementation_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMImplementation_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMImplementation_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMImplementation_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMImplementation_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMImplementation_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMImplementation_hasFeature(This,feature,version,hasFeature)	\
    (This)->lpVtbl -> hasFeature(This,feature,version,hasFeature)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id] */ HRESULT STDMETHODCALLTYPE IXMLDOMImplementation_hasFeature_Proxy( 
    IXMLDOMImplementation * This,
    /* [in] */ BSTR feature,
    /* [in] */ BSTR version,
    /* [retval][out] */ VARIANT_BOOL *hasFeature);


void __RPC_STUB IXMLDOMImplementation_hasFeature_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMImplementation_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMNode_INTERFACE_DEFINED__
#define __IXMLDOMNode_INTERFACE_DEFINED__

/* interface IXMLDOMNode */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMNode;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF80-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMNode : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeName( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeValue( 
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_nodeValue( 
            /* [in] */ VARIANT value) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeType( 
            /* [retval][out] */ DOMNodeType *type) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parentNode( 
            /* [retval][out] */ IXMLDOMNode **parent) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_childNodes( 
            /* [retval][out] */ IXMLDOMNodeList **childList) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_firstChild( 
            /* [retval][out] */ IXMLDOMNode **firstChild) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_lastChild( 
            /* [retval][out] */ IXMLDOMNode **lastChild) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_previousSibling( 
            /* [retval][out] */ IXMLDOMNode **previousSibling) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nextSibling( 
            /* [retval][out] */ IXMLDOMNode **nextSibling) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE insertBefore( 
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE replaceChild( 
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeChild( 
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE appendChild( 
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE hasChildNodes( 
            /* [retval][out] */ VARIANT_BOOL *hasChild) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ownerDocument( 
            /* [retval][out] */ IXMLDOMDocument **DOMDocument) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE cloneNode( 
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeTypeString( 
            /* [out][retval] */ BSTR *nodeType) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_text( 
            /* [out][retval] */ BSTR *text) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_text( 
            /* [in] */ BSTR text) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_specified( 
            /* [retval][out] */ VARIANT_BOOL *isSpecified) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_definition( 
            /* [out][retval] */ IXMLDOMNode **definitionNode) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeTypedValue( 
            /* [out][retval] */ VARIANT *typedValue) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_nodeTypedValue( 
            /* [in] */ VARIANT typedValue) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_dataType( 
            /* [out][retval] */ VARIANT *dataTypeName) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_dataType( 
            /* [in] */ BSTR dataTypeName) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_xml( 
            /* [out][retval] */ BSTR *xmlString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE transformNode( 
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE selectNodes( 
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE selectSingleNode( 
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parsed( 
            /* [out][retval] */ VARIANT_BOOL *isParsed) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_namespaceURI( 
            /* [out][retval] */ BSTR *namespaceURI) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_prefix( 
            /* [out][retval] */ BSTR *prefixString) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_baseName( 
            /* [out][retval] */ BSTR *nameString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE transformNodeToObject( 
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMNodeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMNode * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMNode * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMNode * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMNode * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMNode * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMNode * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMNode * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMNode * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMNode * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMNode * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMNode * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMNode * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMNode * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMNode * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMNode * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMNode * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMNode * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMNode * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMNode * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMNode * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMNode * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMNode * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMNode * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMNode * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMNode * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMNode * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        END_INTERFACE
    } IXMLDOMNodeVtbl;

    interface IXMLDOMNode
    {
        CONST_VTBL struct IXMLDOMNodeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMNode_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMNode_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMNode_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMNode_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMNode_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMNode_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMNode_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMNode_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMNode_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMNode_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMNode_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMNode_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMNode_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMNode_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMNode_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMNode_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMNode_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMNode_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMNode_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMNode_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMNode_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMNode_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMNode_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMNode_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMNode_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMNode_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMNode_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMNode_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMNode_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMNode_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMNode_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMNode_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMNode_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMNode_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMNode_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMNode_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMNode_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMNode_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMNode_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMNode_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMNode_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMNode_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMNode_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nodeName_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ BSTR *name);


void __RPC_STUB IXMLDOMNode_get_nodeName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nodeValue_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IXMLDOMNode_get_nodeValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_put_nodeValue_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ VARIANT value);


void __RPC_STUB IXMLDOMNode_put_nodeValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nodeType_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ DOMNodeType *type);


void __RPC_STUB IXMLDOMNode_get_nodeType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_parentNode_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNode **parent);


void __RPC_STUB IXMLDOMNode_get_parentNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_childNodes_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNodeList **childList);


void __RPC_STUB IXMLDOMNode_get_childNodes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_firstChild_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNode **firstChild);


void __RPC_STUB IXMLDOMNode_get_firstChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_lastChild_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNode **lastChild);


void __RPC_STUB IXMLDOMNode_get_lastChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_previousSibling_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNode **previousSibling);


void __RPC_STUB IXMLDOMNode_get_previousSibling_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nextSibling_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNode **nextSibling);


void __RPC_STUB IXMLDOMNode_get_nextSibling_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_attributes_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);


void __RPC_STUB IXMLDOMNode_get_attributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_insertBefore_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *newChild,
    /* [in] */ VARIANT refChild,
    /* [retval][out] */ IXMLDOMNode **outNewChild);


void __RPC_STUB IXMLDOMNode_insertBefore_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_replaceChild_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *newChild,
    /* [in] */ IXMLDOMNode *oldChild,
    /* [retval][out] */ IXMLDOMNode **outOldChild);


void __RPC_STUB IXMLDOMNode_replaceChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_removeChild_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *childNode,
    /* [retval][out] */ IXMLDOMNode **oldChild);


void __RPC_STUB IXMLDOMNode_removeChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_appendChild_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *newChild,
    /* [retval][out] */ IXMLDOMNode **outNewChild);


void __RPC_STUB IXMLDOMNode_appendChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_hasChildNodes_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ VARIANT_BOOL *hasChild);


void __RPC_STUB IXMLDOMNode_hasChildNodes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_ownerDocument_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ IXMLDOMDocument **DOMDocument);


void __RPC_STUB IXMLDOMNode_get_ownerDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_cloneNode_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ VARIANT_BOOL deep,
    /* [retval][out] */ IXMLDOMNode **cloneRoot);


void __RPC_STUB IXMLDOMNode_cloneNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nodeTypeString_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *nodeType);


void __RPC_STUB IXMLDOMNode_get_nodeTypeString_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_text_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *text);


void __RPC_STUB IXMLDOMNode_get_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_put_text_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ BSTR text);


void __RPC_STUB IXMLDOMNode_put_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_specified_Proxy( 
    IXMLDOMNode * This,
    /* [retval][out] */ VARIANT_BOOL *isSpecified);


void __RPC_STUB IXMLDOMNode_get_specified_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_definition_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ IXMLDOMNode **definitionNode);


void __RPC_STUB IXMLDOMNode_get_definition_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_nodeTypedValue_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ VARIANT *typedValue);


void __RPC_STUB IXMLDOMNode_get_nodeTypedValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_put_nodeTypedValue_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ VARIANT typedValue);


void __RPC_STUB IXMLDOMNode_put_nodeTypedValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_dataType_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ VARIANT *dataTypeName);


void __RPC_STUB IXMLDOMNode_get_dataType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_put_dataType_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ BSTR dataTypeName);


void __RPC_STUB IXMLDOMNode_put_dataType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_xml_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *xmlString);


void __RPC_STUB IXMLDOMNode_get_xml_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_transformNode_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *stylesheet,
    /* [out][retval] */ BSTR *xmlString);


void __RPC_STUB IXMLDOMNode_transformNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_selectNodes_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ BSTR queryString,
    /* [out][retval] */ IXMLDOMNodeList **resultList);


void __RPC_STUB IXMLDOMNode_selectNodes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_selectSingleNode_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ BSTR queryString,
    /* [out][retval] */ IXMLDOMNode **resultNode);


void __RPC_STUB IXMLDOMNode_selectSingleNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_parsed_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ VARIANT_BOOL *isParsed);


void __RPC_STUB IXMLDOMNode_get_parsed_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_namespaceURI_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *namespaceURI);


void __RPC_STUB IXMLDOMNode_get_namespaceURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_prefix_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *prefixString);


void __RPC_STUB IXMLDOMNode_get_prefix_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_get_baseName_Proxy( 
    IXMLDOMNode * This,
    /* [out][retval] */ BSTR *nameString);


void __RPC_STUB IXMLDOMNode_get_baseName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNode_transformNodeToObject_Proxy( 
    IXMLDOMNode * This,
    /* [in] */ IXMLDOMNode *stylesheet,
    /* [in] */ VARIANT outputObject);


void __RPC_STUB IXMLDOMNode_transformNodeToObject_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMNode_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMDocumentFragment_INTERFACE_DEFINED__
#define __IXMLDOMDocumentFragment_INTERFACE_DEFINED__

/* interface IXMLDOMDocumentFragment */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMDocumentFragment;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3efaa413-272f-11d2-836f-0000f87a7782")
    IXMLDOMDocumentFragment : public IXMLDOMNode
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMDocumentFragmentVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMDocumentFragment * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMDocumentFragment * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMDocumentFragment * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMDocumentFragment * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMDocumentFragment * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMDocumentFragment * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        END_INTERFACE
    } IXMLDOMDocumentFragmentVtbl;

    interface IXMLDOMDocumentFragment
    {
        CONST_VTBL struct IXMLDOMDocumentFragmentVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMDocumentFragment_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMDocumentFragment_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMDocumentFragment_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMDocumentFragment_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMDocumentFragment_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMDocumentFragment_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMDocumentFragment_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMDocumentFragment_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMDocumentFragment_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMDocumentFragment_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMDocumentFragment_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMDocumentFragment_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMDocumentFragment_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMDocumentFragment_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMDocumentFragment_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMDocumentFragment_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMDocumentFragment_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMDocumentFragment_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMDocumentFragment_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMDocumentFragment_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMDocumentFragment_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMDocumentFragment_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMDocumentFragment_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMDocumentFragment_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMDocumentFragment_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMDocumentFragment_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMDocumentFragment_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMDocumentFragment_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMDocumentFragment_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMDocumentFragment_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMDocumentFragment_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMDocumentFragment_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMDocumentFragment_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMDocumentFragment_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMDocumentFragment_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMDocumentFragment_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMDocumentFragment_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMDocumentFragment_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMDocumentFragment_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMDocumentFragment_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMDocumentFragment_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMDocumentFragment_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMDocumentFragment_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IXMLDOMDocumentFragment_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMDocument_INTERFACE_DEFINED__
#define __IXMLDOMDocument_INTERFACE_DEFINED__

/* interface IXMLDOMDocument */
/* [hidden][unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMDocument;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF81-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMDocument : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_doctype( 
            /* [retval][out] */ IXMLDOMDocumentType **documentType) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_implementation( 
            /* [retval][out] */ IXMLDOMImplementation **impl) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_documentElement( 
            /* [retval][out] */ IXMLDOMElement **DOMElement) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_documentElement( 
            /* [in] */ IXMLDOMElement *DOMElement) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createElement( 
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMElement **element) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createDocumentFragment( 
            /* [retval][out] */ IXMLDOMDocumentFragment **docFrag) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createTextNode( 
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMText **text) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createComment( 
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMComment **comment) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createCDATASection( 
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMCDATASection **cdata) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createProcessingInstruction( 
            /* [in] */ BSTR target,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMProcessingInstruction **pi) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createAttribute( 
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMAttribute **attribute) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createEntityReference( 
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMEntityReference **entityRef) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getElementsByTagName( 
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMNodeList **resultList) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createNode( 
            /* [in] */ VARIANT Type,
            /* [in] */ BSTR name,
            /* [in] */ BSTR namespaceURI,
            /* [out][retval] */ IXMLDOMNode **node) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE nodeFromID( 
            /* [in] */ BSTR idString,
            /* [out][retval] */ IXMLDOMNode **node) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE load( 
            /* [in] */ VARIANT xmlSource,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [out][retval] */ long *value) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parseError( 
            /* [out][retval] */ IXMLDOMParseError **errorObj) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_url( 
            /* [out][retval] */ BSTR *urlString) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_async( 
            /* [out][retval] */ VARIANT_BOOL *isAsync) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_async( 
            /* [in] */ VARIANT_BOOL isAsync) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE abort( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE loadXML( 
            /* [in] */ BSTR bstrXML,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE save( 
            /* [in] */ VARIANT destination) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_validateOnParse( 
            /* [out][retval] */ VARIANT_BOOL *isValidating) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_validateOnParse( 
            /* [in] */ VARIANT_BOOL isValidating) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_resolveExternals( 
            /* [out][retval] */ VARIANT_BOOL *isResolving) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_resolveExternals( 
            /* [in] */ VARIANT_BOOL isResolving) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_preserveWhiteSpace( 
            /* [out][retval] */ VARIANT_BOOL *isPreserving) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_preserveWhiteSpace( 
            /* [in] */ VARIANT_BOOL isPreserving) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_onreadystatechange( 
            /* [in] */ VARIANT readystatechangeSink) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_ondataavailable( 
            /* [in] */ VARIANT ondataavailableSink) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_ontransformnode( 
            /* [in] */ VARIANT ontransformnodeSink) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMDocumentVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMDocument * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMDocument * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMDocument * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMDocument * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMDocument * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMDocument * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMDocument * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_doctype )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMDocumentType **documentType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_implementation )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMImplementation **impl);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_documentElement )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMElement **DOMElement);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_documentElement )( 
            IXMLDOMDocument * This,
            /* [in] */ IXMLDOMElement *DOMElement);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createElement )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMElement **element);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createDocumentFragment )( 
            IXMLDOMDocument * This,
            /* [retval][out] */ IXMLDOMDocumentFragment **docFrag);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createTextNode )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMText **text);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createComment )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMComment **comment);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createCDATASection )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMCDATASection **cdata);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createProcessingInstruction )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR target,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMProcessingInstruction **pi);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createAttribute )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMAttribute **attribute);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createEntityReference )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMEntityReference **entityRef);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getElementsByTagName )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createNode )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT Type,
            /* [in] */ BSTR name,
            /* [in] */ BSTR namespaceURI,
            /* [out][retval] */ IXMLDOMNode **node);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *nodeFromID )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR idString,
            /* [out][retval] */ IXMLDOMNode **node);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *load )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT xmlSource,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ long *value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parseError )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ IXMLDOMParseError **errorObj);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_url )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ BSTR *urlString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_async )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT_BOOL *isAsync);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_async )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT_BOOL isAsync);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IXMLDOMDocument * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *loadXML )( 
            IXMLDOMDocument * This,
            /* [in] */ BSTR bstrXML,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *save )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT destination);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_validateOnParse )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT_BOOL *isValidating);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_validateOnParse )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT_BOOL isValidating);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_resolveExternals )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT_BOOL *isResolving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_resolveExternals )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT_BOOL isResolving);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_preserveWhiteSpace )( 
            IXMLDOMDocument * This,
            /* [out][retval] */ VARIANT_BOOL *isPreserving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_preserveWhiteSpace )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT_BOOL isPreserving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_onreadystatechange )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT readystatechangeSink);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_ondataavailable )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT ondataavailableSink);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_ontransformnode )( 
            IXMLDOMDocument * This,
            /* [in] */ VARIANT ontransformnodeSink);
        
        END_INTERFACE
    } IXMLDOMDocumentVtbl;

    interface IXMLDOMDocument
    {
        CONST_VTBL struct IXMLDOMDocumentVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMDocument_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMDocument_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMDocument_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMDocument_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMDocument_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMDocument_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMDocument_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMDocument_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMDocument_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMDocument_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMDocument_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMDocument_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMDocument_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMDocument_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMDocument_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMDocument_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMDocument_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMDocument_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMDocument_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMDocument_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMDocument_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMDocument_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMDocument_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMDocument_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMDocument_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMDocument_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMDocument_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMDocument_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMDocument_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMDocument_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMDocument_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMDocument_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMDocument_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMDocument_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMDocument_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMDocument_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMDocument_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMDocument_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMDocument_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMDocument_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMDocument_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMDocument_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMDocument_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMDocument_get_doctype(This,documentType)	\
    (This)->lpVtbl -> get_doctype(This,documentType)

#define IXMLDOMDocument_get_implementation(This,impl)	\
    (This)->lpVtbl -> get_implementation(This,impl)

#define IXMLDOMDocument_get_documentElement(This,DOMElement)	\
    (This)->lpVtbl -> get_documentElement(This,DOMElement)

#define IXMLDOMDocument_putref_documentElement(This,DOMElement)	\
    (This)->lpVtbl -> putref_documentElement(This,DOMElement)

#define IXMLDOMDocument_createElement(This,tagName,element)	\
    (This)->lpVtbl -> createElement(This,tagName,element)

#define IXMLDOMDocument_createDocumentFragment(This,docFrag)	\
    (This)->lpVtbl -> createDocumentFragment(This,docFrag)

#define IXMLDOMDocument_createTextNode(This,data,text)	\
    (This)->lpVtbl -> createTextNode(This,data,text)

#define IXMLDOMDocument_createComment(This,data,comment)	\
    (This)->lpVtbl -> createComment(This,data,comment)

#define IXMLDOMDocument_createCDATASection(This,data,cdata)	\
    (This)->lpVtbl -> createCDATASection(This,data,cdata)

#define IXMLDOMDocument_createProcessingInstruction(This,target,data,pi)	\
    (This)->lpVtbl -> createProcessingInstruction(This,target,data,pi)

#define IXMLDOMDocument_createAttribute(This,name,attribute)	\
    (This)->lpVtbl -> createAttribute(This,name,attribute)

#define IXMLDOMDocument_createEntityReference(This,name,entityRef)	\
    (This)->lpVtbl -> createEntityReference(This,name,entityRef)

#define IXMLDOMDocument_getElementsByTagName(This,tagName,resultList)	\
    (This)->lpVtbl -> getElementsByTagName(This,tagName,resultList)

#define IXMLDOMDocument_createNode(This,Type,name,namespaceURI,node)	\
    (This)->lpVtbl -> createNode(This,Type,name,namespaceURI,node)

#define IXMLDOMDocument_nodeFromID(This,idString,node)	\
    (This)->lpVtbl -> nodeFromID(This,idString,node)

#define IXMLDOMDocument_load(This,xmlSource,isSuccessful)	\
    (This)->lpVtbl -> load(This,xmlSource,isSuccessful)

#define IXMLDOMDocument_get_readyState(This,value)	\
    (This)->lpVtbl -> get_readyState(This,value)

#define IXMLDOMDocument_get_parseError(This,errorObj)	\
    (This)->lpVtbl -> get_parseError(This,errorObj)

#define IXMLDOMDocument_get_url(This,urlString)	\
    (This)->lpVtbl -> get_url(This,urlString)

#define IXMLDOMDocument_get_async(This,isAsync)	\
    (This)->lpVtbl -> get_async(This,isAsync)

#define IXMLDOMDocument_put_async(This,isAsync)	\
    (This)->lpVtbl -> put_async(This,isAsync)

#define IXMLDOMDocument_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IXMLDOMDocument_loadXML(This,bstrXML,isSuccessful)	\
    (This)->lpVtbl -> loadXML(This,bstrXML,isSuccessful)

#define IXMLDOMDocument_save(This,destination)	\
    (This)->lpVtbl -> save(This,destination)

#define IXMLDOMDocument_get_validateOnParse(This,isValidating)	\
    (This)->lpVtbl -> get_validateOnParse(This,isValidating)

#define IXMLDOMDocument_put_validateOnParse(This,isValidating)	\
    (This)->lpVtbl -> put_validateOnParse(This,isValidating)

#define IXMLDOMDocument_get_resolveExternals(This,isResolving)	\
    (This)->lpVtbl -> get_resolveExternals(This,isResolving)

#define IXMLDOMDocument_put_resolveExternals(This,isResolving)	\
    (This)->lpVtbl -> put_resolveExternals(This,isResolving)

#define IXMLDOMDocument_get_preserveWhiteSpace(This,isPreserving)	\
    (This)->lpVtbl -> get_preserveWhiteSpace(This,isPreserving)

#define IXMLDOMDocument_put_preserveWhiteSpace(This,isPreserving)	\
    (This)->lpVtbl -> put_preserveWhiteSpace(This,isPreserving)

#define IXMLDOMDocument_put_onreadystatechange(This,readystatechangeSink)	\
    (This)->lpVtbl -> put_onreadystatechange(This,readystatechangeSink)

#define IXMLDOMDocument_put_ondataavailable(This,ondataavailableSink)	\
    (This)->lpVtbl -> put_ondataavailable(This,ondataavailableSink)

#define IXMLDOMDocument_put_ontransformnode(This,ontransformnodeSink)	\
    (This)->lpVtbl -> put_ontransformnode(This,ontransformnodeSink)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_doctype_Proxy( 
    IXMLDOMDocument * This,
    /* [retval][out] */ IXMLDOMDocumentType **documentType);


void __RPC_STUB IXMLDOMDocument_get_doctype_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_implementation_Proxy( 
    IXMLDOMDocument * This,
    /* [retval][out] */ IXMLDOMImplementation **impl);


void __RPC_STUB IXMLDOMDocument_get_implementation_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_documentElement_Proxy( 
    IXMLDOMDocument * This,
    /* [retval][out] */ IXMLDOMElement **DOMElement);


void __RPC_STUB IXMLDOMDocument_get_documentElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_putref_documentElement_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ IXMLDOMElement *DOMElement);


void __RPC_STUB IXMLDOMDocument_putref_documentElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createElement_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR tagName,
    /* [retval][out] */ IXMLDOMElement **element);


void __RPC_STUB IXMLDOMDocument_createElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createDocumentFragment_Proxy( 
    IXMLDOMDocument * This,
    /* [retval][out] */ IXMLDOMDocumentFragment **docFrag);


void __RPC_STUB IXMLDOMDocument_createDocumentFragment_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createTextNode_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR data,
    /* [retval][out] */ IXMLDOMText **text);


void __RPC_STUB IXMLDOMDocument_createTextNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createComment_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR data,
    /* [retval][out] */ IXMLDOMComment **comment);


void __RPC_STUB IXMLDOMDocument_createComment_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createCDATASection_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR data,
    /* [retval][out] */ IXMLDOMCDATASection **cdata);


void __RPC_STUB IXMLDOMDocument_createCDATASection_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createProcessingInstruction_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR target,
    /* [in] */ BSTR data,
    /* [retval][out] */ IXMLDOMProcessingInstruction **pi);


void __RPC_STUB IXMLDOMDocument_createProcessingInstruction_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createAttribute_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ IXMLDOMAttribute **attribute);


void __RPC_STUB IXMLDOMDocument_createAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createEntityReference_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ IXMLDOMEntityReference **entityRef);


void __RPC_STUB IXMLDOMDocument_createEntityReference_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_getElementsByTagName_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR tagName,
    /* [retval][out] */ IXMLDOMNodeList **resultList);


void __RPC_STUB IXMLDOMDocument_getElementsByTagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_createNode_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT Type,
    /* [in] */ BSTR name,
    /* [in] */ BSTR namespaceURI,
    /* [out][retval] */ IXMLDOMNode **node);


void __RPC_STUB IXMLDOMDocument_createNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_nodeFromID_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR idString,
    /* [out][retval] */ IXMLDOMNode **node);


void __RPC_STUB IXMLDOMDocument_nodeFromID_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_load_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT xmlSource,
    /* [retval][out] */ VARIANT_BOOL *isSuccessful);


void __RPC_STUB IXMLDOMDocument_load_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_readyState_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ long *value);


void __RPC_STUB IXMLDOMDocument_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_parseError_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ IXMLDOMParseError **errorObj);


void __RPC_STUB IXMLDOMDocument_get_parseError_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_url_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ BSTR *urlString);


void __RPC_STUB IXMLDOMDocument_get_url_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_async_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ VARIANT_BOOL *isAsync);


void __RPC_STUB IXMLDOMDocument_get_async_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_async_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT_BOOL isAsync);


void __RPC_STUB IXMLDOMDocument_put_async_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_abort_Proxy( 
    IXMLDOMDocument * This);


void __RPC_STUB IXMLDOMDocument_abort_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_loadXML_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ BSTR bstrXML,
    /* [retval][out] */ VARIANT_BOOL *isSuccessful);


void __RPC_STUB IXMLDOMDocument_loadXML_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_save_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT destination);


void __RPC_STUB IXMLDOMDocument_save_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_validateOnParse_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ VARIANT_BOOL *isValidating);


void __RPC_STUB IXMLDOMDocument_get_validateOnParse_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_validateOnParse_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT_BOOL isValidating);


void __RPC_STUB IXMLDOMDocument_put_validateOnParse_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_resolveExternals_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ VARIANT_BOOL *isResolving);


void __RPC_STUB IXMLDOMDocument_get_resolveExternals_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_resolveExternals_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT_BOOL isResolving);


void __RPC_STUB IXMLDOMDocument_put_resolveExternals_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_get_preserveWhiteSpace_Proxy( 
    IXMLDOMDocument * This,
    /* [out][retval] */ VARIANT_BOOL *isPreserving);


void __RPC_STUB IXMLDOMDocument_get_preserveWhiteSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_preserveWhiteSpace_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT_BOOL isPreserving);


void __RPC_STUB IXMLDOMDocument_put_preserveWhiteSpace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_onreadystatechange_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT readystatechangeSink);


void __RPC_STUB IXMLDOMDocument_put_onreadystatechange_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_ondataavailable_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT ondataavailableSink);


void __RPC_STUB IXMLDOMDocument_put_ondataavailable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument_put_ontransformnode_Proxy( 
    IXMLDOMDocument * This,
    /* [in] */ VARIANT ontransformnodeSink);


void __RPC_STUB IXMLDOMDocument_put_ontransformnode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMDocument_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMDocument2_INTERFACE_DEFINED__
#define __IXMLDOMDocument2_INTERFACE_DEFINED__

/* interface IXMLDOMDocument2 */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMDocument2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF95-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMDocument2 : public IXMLDOMDocument
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_namespaces( 
            /* [retval][out] */ IXMLDOMSchemaCollection **namespaceCollection) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_schemas( 
            /* [retval][out] */ VARIANT *otherCollection) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_schemas( 
            /* [in] */ VARIANT otherCollection) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE validate( 
            /* [out][retval] */ IXMLDOMParseError **errorObj) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setProperty( 
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getProperty( 
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMDocument2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMDocument2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMDocument2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMDocument2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMDocument2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMDocument2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMDocument2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMDocument2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_doctype )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMDocumentType **documentType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_implementation )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMImplementation **impl);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_documentElement )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMElement **DOMElement);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_documentElement )( 
            IXMLDOMDocument2 * This,
            /* [in] */ IXMLDOMElement *DOMElement);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createElement )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMElement **element);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createDocumentFragment )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMDocumentFragment **docFrag);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createTextNode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMText **text);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createComment )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMComment **comment);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createCDATASection )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMCDATASection **cdata);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createProcessingInstruction )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR target,
            /* [in] */ BSTR data,
            /* [retval][out] */ IXMLDOMProcessingInstruction **pi);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createAttribute )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMAttribute **attribute);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createEntityReference )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMEntityReference **entityRef);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getElementsByTagName )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createNode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT Type,
            /* [in] */ BSTR name,
            /* [in] */ BSTR namespaceURI,
            /* [out][retval] */ IXMLDOMNode **node);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *nodeFromID )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR idString,
            /* [out][retval] */ IXMLDOMNode **node);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *load )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT xmlSource,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ long *value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parseError )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ IXMLDOMParseError **errorObj);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_url )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ BSTR *urlString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_async )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *isAsync);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_async )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT_BOOL isAsync);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IXMLDOMDocument2 * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *loadXML )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR bstrXML,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *save )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT destination);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_validateOnParse )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *isValidating);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_validateOnParse )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT_BOOL isValidating);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_resolveExternals )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *isResolving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_resolveExternals )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT_BOOL isResolving);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_preserveWhiteSpace )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *isPreserving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_preserveWhiteSpace )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT_BOOL isPreserving);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_onreadystatechange )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT readystatechangeSink);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_ondataavailable )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT ondataavailableSink);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_ontransformnode )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT ontransformnodeSink);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaces )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ IXMLDOMSchemaCollection **namespaceCollection);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_schemas )( 
            IXMLDOMDocument2 * This,
            /* [retval][out] */ VARIANT *otherCollection);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_schemas )( 
            IXMLDOMDocument2 * This,
            /* [in] */ VARIANT otherCollection);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *validate )( 
            IXMLDOMDocument2 * This,
            /* [out][retval] */ IXMLDOMParseError **errorObj);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setProperty )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getProperty )( 
            IXMLDOMDocument2 * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value);
        
        END_INTERFACE
    } IXMLDOMDocument2Vtbl;

    interface IXMLDOMDocument2
    {
        CONST_VTBL struct IXMLDOMDocument2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMDocument2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMDocument2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMDocument2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMDocument2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMDocument2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMDocument2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMDocument2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMDocument2_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMDocument2_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMDocument2_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMDocument2_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMDocument2_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMDocument2_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMDocument2_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMDocument2_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMDocument2_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMDocument2_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMDocument2_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMDocument2_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMDocument2_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMDocument2_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMDocument2_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMDocument2_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMDocument2_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMDocument2_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMDocument2_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMDocument2_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMDocument2_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMDocument2_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMDocument2_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMDocument2_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMDocument2_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMDocument2_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMDocument2_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMDocument2_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMDocument2_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMDocument2_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMDocument2_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMDocument2_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMDocument2_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMDocument2_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMDocument2_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMDocument2_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMDocument2_get_doctype(This,documentType)	\
    (This)->lpVtbl -> get_doctype(This,documentType)

#define IXMLDOMDocument2_get_implementation(This,impl)	\
    (This)->lpVtbl -> get_implementation(This,impl)

#define IXMLDOMDocument2_get_documentElement(This,DOMElement)	\
    (This)->lpVtbl -> get_documentElement(This,DOMElement)

#define IXMLDOMDocument2_putref_documentElement(This,DOMElement)	\
    (This)->lpVtbl -> putref_documentElement(This,DOMElement)

#define IXMLDOMDocument2_createElement(This,tagName,element)	\
    (This)->lpVtbl -> createElement(This,tagName,element)

#define IXMLDOMDocument2_createDocumentFragment(This,docFrag)	\
    (This)->lpVtbl -> createDocumentFragment(This,docFrag)

#define IXMLDOMDocument2_createTextNode(This,data,text)	\
    (This)->lpVtbl -> createTextNode(This,data,text)

#define IXMLDOMDocument2_createComment(This,data,comment)	\
    (This)->lpVtbl -> createComment(This,data,comment)

#define IXMLDOMDocument2_createCDATASection(This,data,cdata)	\
    (This)->lpVtbl -> createCDATASection(This,data,cdata)

#define IXMLDOMDocument2_createProcessingInstruction(This,target,data,pi)	\
    (This)->lpVtbl -> createProcessingInstruction(This,target,data,pi)

#define IXMLDOMDocument2_createAttribute(This,name,attribute)	\
    (This)->lpVtbl -> createAttribute(This,name,attribute)

#define IXMLDOMDocument2_createEntityReference(This,name,entityRef)	\
    (This)->lpVtbl -> createEntityReference(This,name,entityRef)

#define IXMLDOMDocument2_getElementsByTagName(This,tagName,resultList)	\
    (This)->lpVtbl -> getElementsByTagName(This,tagName,resultList)

#define IXMLDOMDocument2_createNode(This,Type,name,namespaceURI,node)	\
    (This)->lpVtbl -> createNode(This,Type,name,namespaceURI,node)

#define IXMLDOMDocument2_nodeFromID(This,idString,node)	\
    (This)->lpVtbl -> nodeFromID(This,idString,node)

#define IXMLDOMDocument2_load(This,xmlSource,isSuccessful)	\
    (This)->lpVtbl -> load(This,xmlSource,isSuccessful)

#define IXMLDOMDocument2_get_readyState(This,value)	\
    (This)->lpVtbl -> get_readyState(This,value)

#define IXMLDOMDocument2_get_parseError(This,errorObj)	\
    (This)->lpVtbl -> get_parseError(This,errorObj)

#define IXMLDOMDocument2_get_url(This,urlString)	\
    (This)->lpVtbl -> get_url(This,urlString)

#define IXMLDOMDocument2_get_async(This,isAsync)	\
    (This)->lpVtbl -> get_async(This,isAsync)

#define IXMLDOMDocument2_put_async(This,isAsync)	\
    (This)->lpVtbl -> put_async(This,isAsync)

#define IXMLDOMDocument2_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IXMLDOMDocument2_loadXML(This,bstrXML,isSuccessful)	\
    (This)->lpVtbl -> loadXML(This,bstrXML,isSuccessful)

#define IXMLDOMDocument2_save(This,destination)	\
    (This)->lpVtbl -> save(This,destination)

#define IXMLDOMDocument2_get_validateOnParse(This,isValidating)	\
    (This)->lpVtbl -> get_validateOnParse(This,isValidating)

#define IXMLDOMDocument2_put_validateOnParse(This,isValidating)	\
    (This)->lpVtbl -> put_validateOnParse(This,isValidating)

#define IXMLDOMDocument2_get_resolveExternals(This,isResolving)	\
    (This)->lpVtbl -> get_resolveExternals(This,isResolving)

#define IXMLDOMDocument2_put_resolveExternals(This,isResolving)	\
    (This)->lpVtbl -> put_resolveExternals(This,isResolving)

#define IXMLDOMDocument2_get_preserveWhiteSpace(This,isPreserving)	\
    (This)->lpVtbl -> get_preserveWhiteSpace(This,isPreserving)

#define IXMLDOMDocument2_put_preserveWhiteSpace(This,isPreserving)	\
    (This)->lpVtbl -> put_preserveWhiteSpace(This,isPreserving)

#define IXMLDOMDocument2_put_onreadystatechange(This,readystatechangeSink)	\
    (This)->lpVtbl -> put_onreadystatechange(This,readystatechangeSink)

#define IXMLDOMDocument2_put_ondataavailable(This,ondataavailableSink)	\
    (This)->lpVtbl -> put_ondataavailable(This,ondataavailableSink)

#define IXMLDOMDocument2_put_ontransformnode(This,ontransformnodeSink)	\
    (This)->lpVtbl -> put_ontransformnode(This,ontransformnodeSink)


#define IXMLDOMDocument2_get_namespaces(This,namespaceCollection)	\
    (This)->lpVtbl -> get_namespaces(This,namespaceCollection)

#define IXMLDOMDocument2_get_schemas(This,otherCollection)	\
    (This)->lpVtbl -> get_schemas(This,otherCollection)

#define IXMLDOMDocument2_putref_schemas(This,otherCollection)	\
    (This)->lpVtbl -> putref_schemas(This,otherCollection)

#define IXMLDOMDocument2_validate(This,errorObj)	\
    (This)->lpVtbl -> validate(This,errorObj)

#define IXMLDOMDocument2_setProperty(This,name,value)	\
    (This)->lpVtbl -> setProperty(This,name,value)

#define IXMLDOMDocument2_getProperty(This,name,value)	\
    (This)->lpVtbl -> getProperty(This,name,value)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_get_namespaces_Proxy( 
    IXMLDOMDocument2 * This,
    /* [retval][out] */ IXMLDOMSchemaCollection **namespaceCollection);


void __RPC_STUB IXMLDOMDocument2_get_namespaces_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_get_schemas_Proxy( 
    IXMLDOMDocument2 * This,
    /* [retval][out] */ VARIANT *otherCollection);


void __RPC_STUB IXMLDOMDocument2_get_schemas_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_putref_schemas_Proxy( 
    IXMLDOMDocument2 * This,
    /* [in] */ VARIANT otherCollection);


void __RPC_STUB IXMLDOMDocument2_putref_schemas_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_validate_Proxy( 
    IXMLDOMDocument2 * This,
    /* [out][retval] */ IXMLDOMParseError **errorObj);


void __RPC_STUB IXMLDOMDocument2_validate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_setProperty_Proxy( 
    IXMLDOMDocument2 * This,
    /* [in] */ BSTR name,
    /* [in] */ VARIANT value);


void __RPC_STUB IXMLDOMDocument2_setProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocument2_getProperty_Proxy( 
    IXMLDOMDocument2 * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IXMLDOMDocument2_getProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMDocument2_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMNodeList_INTERFACE_DEFINED__
#define __IXMLDOMNodeList_INTERFACE_DEFINED__

/* interface IXMLDOMNodeList */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMNodeList;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF82-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMNodeList : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_item( 
            /* [in] */ long index,
            /* [retval][out] */ IXMLDOMNode **listItem) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *listLength) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE nextNode( 
            /* [retval][out] */ IXMLDOMNode **nextItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE reset( void) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [out][retval] */ IUnknown **ppUnk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMNodeListVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMNodeList * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMNodeList * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMNodeList * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMNodeList * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMNodeList * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMNodeList * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMNodeList * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            IXMLDOMNodeList * This,
            /* [in] */ long index,
            /* [retval][out] */ IXMLDOMNode **listItem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMNodeList * This,
            /* [retval][out] */ long *listLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *nextNode )( 
            IXMLDOMNodeList * This,
            /* [retval][out] */ IXMLDOMNode **nextItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *reset )( 
            IXMLDOMNodeList * This);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLDOMNodeList * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        END_INTERFACE
    } IXMLDOMNodeListVtbl;

    interface IXMLDOMNodeList
    {
        CONST_VTBL struct IXMLDOMNodeListVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMNodeList_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMNodeList_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMNodeList_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMNodeList_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMNodeList_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMNodeList_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMNodeList_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMNodeList_get_item(This,index,listItem)	\
    (This)->lpVtbl -> get_item(This,index,listItem)

#define IXMLDOMNodeList_get_length(This,listLength)	\
    (This)->lpVtbl -> get_length(This,listLength)

#define IXMLDOMNodeList_nextNode(This,nextItem)	\
    (This)->lpVtbl -> nextNode(This,nextItem)

#define IXMLDOMNodeList_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IXMLDOMNodeList_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNodeList_get_item_Proxy( 
    IXMLDOMNodeList * This,
    /* [in] */ long index,
    /* [retval][out] */ IXMLDOMNode **listItem);


void __RPC_STUB IXMLDOMNodeList_get_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNodeList_get_length_Proxy( 
    IXMLDOMNodeList * This,
    /* [retval][out] */ long *listLength);


void __RPC_STUB IXMLDOMNodeList_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNodeList_nextNode_Proxy( 
    IXMLDOMNodeList * This,
    /* [retval][out] */ IXMLDOMNode **nextItem);


void __RPC_STUB IXMLDOMNodeList_nextNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNodeList_reset_Proxy( 
    IXMLDOMNodeList * This);


void __RPC_STUB IXMLDOMNodeList_reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNodeList_get__newEnum_Proxy( 
    IXMLDOMNodeList * This,
    /* [out][retval] */ IUnknown **ppUnk);


void __RPC_STUB IXMLDOMNodeList_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMNodeList_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMNamedNodeMap_INTERFACE_DEFINED__
#define __IXMLDOMNamedNodeMap_INTERFACE_DEFINED__

/* interface IXMLDOMNamedNodeMap */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMNamedNodeMap;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF83-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMNamedNodeMap : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getNamedItem( 
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMNode **namedItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setNamedItem( 
            /* [in] */ IXMLDOMNode *newItem,
            /* [retval][out] */ IXMLDOMNode **nameItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeNamedItem( 
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMNode **namedItem) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_item( 
            /* [in] */ long index,
            /* [retval][out] */ IXMLDOMNode **listItem) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *listLength) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getQualifiedItem( 
            /* [in] */ BSTR baseName,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **qualifiedItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeQualifiedItem( 
            /* [in] */ BSTR baseName,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **qualifiedItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE nextNode( 
            /* [retval][out] */ IXMLDOMNode **nextItem) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE reset( void) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [out][retval] */ IUnknown **ppUnk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMNamedNodeMapVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMNamedNodeMap * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMNamedNodeMap * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMNamedNodeMap * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getNamedItem )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMNode **namedItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setNamedItem )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ IXMLDOMNode *newItem,
            /* [retval][out] */ IXMLDOMNode **nameItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeNamedItem )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMNode **namedItem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ long index,
            /* [retval][out] */ IXMLDOMNode **listItem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMNamedNodeMap * This,
            /* [retval][out] */ long *listLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getQualifiedItem )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ BSTR baseName,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **qualifiedItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeQualifiedItem )( 
            IXMLDOMNamedNodeMap * This,
            /* [in] */ BSTR baseName,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **qualifiedItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *nextNode )( 
            IXMLDOMNamedNodeMap * This,
            /* [retval][out] */ IXMLDOMNode **nextItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *reset )( 
            IXMLDOMNamedNodeMap * This);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLDOMNamedNodeMap * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        END_INTERFACE
    } IXMLDOMNamedNodeMapVtbl;

    interface IXMLDOMNamedNodeMap
    {
        CONST_VTBL struct IXMLDOMNamedNodeMapVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMNamedNodeMap_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMNamedNodeMap_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMNamedNodeMap_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMNamedNodeMap_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMNamedNodeMap_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMNamedNodeMap_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMNamedNodeMap_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMNamedNodeMap_getNamedItem(This,name,namedItem)	\
    (This)->lpVtbl -> getNamedItem(This,name,namedItem)

#define IXMLDOMNamedNodeMap_setNamedItem(This,newItem,nameItem)	\
    (This)->lpVtbl -> setNamedItem(This,newItem,nameItem)

#define IXMLDOMNamedNodeMap_removeNamedItem(This,name,namedItem)	\
    (This)->lpVtbl -> removeNamedItem(This,name,namedItem)

#define IXMLDOMNamedNodeMap_get_item(This,index,listItem)	\
    (This)->lpVtbl -> get_item(This,index,listItem)

#define IXMLDOMNamedNodeMap_get_length(This,listLength)	\
    (This)->lpVtbl -> get_length(This,listLength)

#define IXMLDOMNamedNodeMap_getQualifiedItem(This,baseName,namespaceURI,qualifiedItem)	\
    (This)->lpVtbl -> getQualifiedItem(This,baseName,namespaceURI,qualifiedItem)

#define IXMLDOMNamedNodeMap_removeQualifiedItem(This,baseName,namespaceURI,qualifiedItem)	\
    (This)->lpVtbl -> removeQualifiedItem(This,baseName,namespaceURI,qualifiedItem)

#define IXMLDOMNamedNodeMap_nextNode(This,nextItem)	\
    (This)->lpVtbl -> nextNode(This,nextItem)

#define IXMLDOMNamedNodeMap_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IXMLDOMNamedNodeMap_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_getNamedItem_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ IXMLDOMNode **namedItem);


void __RPC_STUB IXMLDOMNamedNodeMap_getNamedItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_setNamedItem_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ IXMLDOMNode *newItem,
    /* [retval][out] */ IXMLDOMNode **nameItem);


void __RPC_STUB IXMLDOMNamedNodeMap_setNamedItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_removeNamedItem_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ IXMLDOMNode **namedItem);


void __RPC_STUB IXMLDOMNamedNodeMap_removeNamedItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_get_item_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ long index,
    /* [retval][out] */ IXMLDOMNode **listItem);


void __RPC_STUB IXMLDOMNamedNodeMap_get_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_get_length_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [retval][out] */ long *listLength);


void __RPC_STUB IXMLDOMNamedNodeMap_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_getQualifiedItem_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ BSTR baseName,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ IXMLDOMNode **qualifiedItem);


void __RPC_STUB IXMLDOMNamedNodeMap_getQualifiedItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_removeQualifiedItem_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [in] */ BSTR baseName,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ IXMLDOMNode **qualifiedItem);


void __RPC_STUB IXMLDOMNamedNodeMap_removeQualifiedItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_nextNode_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [retval][out] */ IXMLDOMNode **nextItem);


void __RPC_STUB IXMLDOMNamedNodeMap_nextNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_reset_Proxy( 
    IXMLDOMNamedNodeMap * This);


void __RPC_STUB IXMLDOMNamedNodeMap_reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNamedNodeMap_get__newEnum_Proxy( 
    IXMLDOMNamedNodeMap * This,
    /* [out][retval] */ IUnknown **ppUnk);


void __RPC_STUB IXMLDOMNamedNodeMap_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMNamedNodeMap_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMCharacterData_INTERFACE_DEFINED__
#define __IXMLDOMCharacterData_INTERFACE_DEFINED__

/* interface IXMLDOMCharacterData */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMCharacterData;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF84-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMCharacterData : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_data( 
            /* [retval][out] */ BSTR *data) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_data( 
            /* [in] */ BSTR data) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *dataLength) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE substringData( 
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [retval][out] */ BSTR *data) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE appendData( 
            /* [in] */ BSTR data) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE insertData( 
            /* [in] */ long offset,
            /* [in] */ BSTR data) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE deleteData( 
            /* [in] */ long offset,
            /* [in] */ long count) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE replaceData( 
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [in] */ BSTR data) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMCharacterDataVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMCharacterData * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMCharacterData * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMCharacterData * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMCharacterData * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMCharacterData * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMCharacterData * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMCharacterData * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMCharacterData * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMCharacterData * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMCharacterData * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMCharacterData * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMCharacterData * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_data )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_data )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMCharacterData * This,
            /* [retval][out] */ long *dataLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *substringData )( 
            IXMLDOMCharacterData * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendData )( 
            IXMLDOMCharacterData * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertData )( 
            IXMLDOMCharacterData * This,
            /* [in] */ long offset,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *deleteData )( 
            IXMLDOMCharacterData * This,
            /* [in] */ long offset,
            /* [in] */ long count);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceData )( 
            IXMLDOMCharacterData * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [in] */ BSTR data);
        
        END_INTERFACE
    } IXMLDOMCharacterDataVtbl;

    interface IXMLDOMCharacterData
    {
        CONST_VTBL struct IXMLDOMCharacterDataVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMCharacterData_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMCharacterData_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMCharacterData_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMCharacterData_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMCharacterData_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMCharacterData_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMCharacterData_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMCharacterData_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMCharacterData_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMCharacterData_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMCharacterData_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMCharacterData_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMCharacterData_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMCharacterData_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMCharacterData_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMCharacterData_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMCharacterData_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMCharacterData_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMCharacterData_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMCharacterData_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMCharacterData_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMCharacterData_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMCharacterData_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMCharacterData_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMCharacterData_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMCharacterData_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMCharacterData_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMCharacterData_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMCharacterData_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMCharacterData_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMCharacterData_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMCharacterData_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMCharacterData_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMCharacterData_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMCharacterData_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMCharacterData_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMCharacterData_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMCharacterData_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMCharacterData_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMCharacterData_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMCharacterData_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMCharacterData_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMCharacterData_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMCharacterData_get_data(This,data)	\
    (This)->lpVtbl -> get_data(This,data)

#define IXMLDOMCharacterData_put_data(This,data)	\
    (This)->lpVtbl -> put_data(This,data)

#define IXMLDOMCharacterData_get_length(This,dataLength)	\
    (This)->lpVtbl -> get_length(This,dataLength)

#define IXMLDOMCharacterData_substringData(This,offset,count,data)	\
    (This)->lpVtbl -> substringData(This,offset,count,data)

#define IXMLDOMCharacterData_appendData(This,data)	\
    (This)->lpVtbl -> appendData(This,data)

#define IXMLDOMCharacterData_insertData(This,offset,data)	\
    (This)->lpVtbl -> insertData(This,offset,data)

#define IXMLDOMCharacterData_deleteData(This,offset,count)	\
    (This)->lpVtbl -> deleteData(This,offset,count)

#define IXMLDOMCharacterData_replaceData(This,offset,count,data)	\
    (This)->lpVtbl -> replaceData(This,offset,count,data)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_get_data_Proxy( 
    IXMLDOMCharacterData * This,
    /* [retval][out] */ BSTR *data);


void __RPC_STUB IXMLDOMCharacterData_get_data_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_put_data_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ BSTR data);


void __RPC_STUB IXMLDOMCharacterData_put_data_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_get_length_Proxy( 
    IXMLDOMCharacterData * This,
    /* [retval][out] */ long *dataLength);


void __RPC_STUB IXMLDOMCharacterData_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_substringData_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ long offset,
    /* [in] */ long count,
    /* [retval][out] */ BSTR *data);


void __RPC_STUB IXMLDOMCharacterData_substringData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_appendData_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ BSTR data);


void __RPC_STUB IXMLDOMCharacterData_appendData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_insertData_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ long offset,
    /* [in] */ BSTR data);


void __RPC_STUB IXMLDOMCharacterData_insertData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_deleteData_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ long offset,
    /* [in] */ long count);


void __RPC_STUB IXMLDOMCharacterData_deleteData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMCharacterData_replaceData_Proxy( 
    IXMLDOMCharacterData * This,
    /* [in] */ long offset,
    /* [in] */ long count,
    /* [in] */ BSTR data);


void __RPC_STUB IXMLDOMCharacterData_replaceData_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMCharacterData_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMAttribute_INTERFACE_DEFINED__
#define __IXMLDOMAttribute_INTERFACE_DEFINED__

/* interface IXMLDOMAttribute */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMAttribute;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF85-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMAttribute : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
            /* [retval][out] */ BSTR *attributeName) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_value( 
            /* [retval][out] */ VARIANT *attributeValue) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_value( 
            /* [in] */ VARIANT attributeValue) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMAttributeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMAttribute * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMAttribute * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMAttribute * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMAttribute * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMAttribute * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMAttribute * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMAttribute * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMAttribute * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMAttribute * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMAttribute * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMAttribute * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMAttribute * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMAttribute * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMAttribute * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMAttribute * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMAttribute * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ BSTR *attributeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_value )( 
            IXMLDOMAttribute * This,
            /* [retval][out] */ VARIANT *attributeValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_value )( 
            IXMLDOMAttribute * This,
            /* [in] */ VARIANT attributeValue);
        
        END_INTERFACE
    } IXMLDOMAttributeVtbl;

    interface IXMLDOMAttribute
    {
        CONST_VTBL struct IXMLDOMAttributeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMAttribute_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMAttribute_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMAttribute_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMAttribute_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMAttribute_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMAttribute_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMAttribute_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMAttribute_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMAttribute_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMAttribute_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMAttribute_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMAttribute_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMAttribute_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMAttribute_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMAttribute_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMAttribute_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMAttribute_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMAttribute_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMAttribute_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMAttribute_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMAttribute_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMAttribute_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMAttribute_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMAttribute_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMAttribute_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMAttribute_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMAttribute_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMAttribute_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMAttribute_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMAttribute_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMAttribute_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMAttribute_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMAttribute_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMAttribute_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMAttribute_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMAttribute_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMAttribute_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMAttribute_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMAttribute_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMAttribute_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMAttribute_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMAttribute_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMAttribute_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMAttribute_get_name(This,attributeName)	\
    (This)->lpVtbl -> get_name(This,attributeName)

#define IXMLDOMAttribute_get_value(This,attributeValue)	\
    (This)->lpVtbl -> get_value(This,attributeValue)

#define IXMLDOMAttribute_put_value(This,attributeValue)	\
    (This)->lpVtbl -> put_value(This,attributeValue)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMAttribute_get_name_Proxy( 
    IXMLDOMAttribute * This,
    /* [retval][out] */ BSTR *attributeName);


void __RPC_STUB IXMLDOMAttribute_get_name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMAttribute_get_value_Proxy( 
    IXMLDOMAttribute * This,
    /* [retval][out] */ VARIANT *attributeValue);


void __RPC_STUB IXMLDOMAttribute_get_value_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMAttribute_put_value_Proxy( 
    IXMLDOMAttribute * This,
    /* [in] */ VARIANT attributeValue);


void __RPC_STUB IXMLDOMAttribute_put_value_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMAttribute_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMElement_INTERFACE_DEFINED__
#define __IXMLDOMElement_INTERFACE_DEFINED__

/* interface IXMLDOMElement */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMElement;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF86-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMElement : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_tagName( 
            /* [retval][out] */ BSTR *tagName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getAttribute( 
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttribute( 
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAttribute( 
            /* [in] */ BSTR name) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getAttributeNode( 
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttributeNode( 
            /* [in] */ IXMLDOMAttribute *DOMAttribute,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAttributeNode( 
            /* [in] */ IXMLDOMAttribute *DOMAttribute,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getElementsByTagName( 
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMNodeList **resultList) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE normalize( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMElementVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMElement * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMElement * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMElement * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMElement * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMElement * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMElement * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMElement * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMElement * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMElement * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMElement * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMElement * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMElement * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMElement * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMElement * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMElement * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMElement * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMElement * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMElement * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMElement * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMElement * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMElement * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_tagName )( 
            IXMLDOMElement * This,
            /* [retval][out] */ BSTR *tagName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAttribute )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttribute )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAttribute )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR name);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAttributeNode )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttributeNode )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMAttribute *DOMAttribute,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAttributeNode )( 
            IXMLDOMElement * This,
            /* [in] */ IXMLDOMAttribute *DOMAttribute,
            /* [retval][out] */ IXMLDOMAttribute **attributeNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getElementsByTagName )( 
            IXMLDOMElement * This,
            /* [in] */ BSTR tagName,
            /* [retval][out] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *normalize )( 
            IXMLDOMElement * This);
        
        END_INTERFACE
    } IXMLDOMElementVtbl;

    interface IXMLDOMElement
    {
        CONST_VTBL struct IXMLDOMElementVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMElement_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMElement_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMElement_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMElement_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMElement_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMElement_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMElement_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMElement_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMElement_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMElement_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMElement_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMElement_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMElement_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMElement_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMElement_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMElement_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMElement_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMElement_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMElement_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMElement_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMElement_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMElement_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMElement_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMElement_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMElement_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMElement_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMElement_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMElement_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMElement_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMElement_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMElement_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMElement_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMElement_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMElement_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMElement_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMElement_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMElement_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMElement_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMElement_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMElement_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMElement_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMElement_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMElement_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMElement_get_tagName(This,tagName)	\
    (This)->lpVtbl -> get_tagName(This,tagName)

#define IXMLDOMElement_getAttribute(This,name,value)	\
    (This)->lpVtbl -> getAttribute(This,name,value)

#define IXMLDOMElement_setAttribute(This,name,value)	\
    (This)->lpVtbl -> setAttribute(This,name,value)

#define IXMLDOMElement_removeAttribute(This,name)	\
    (This)->lpVtbl -> removeAttribute(This,name)

#define IXMLDOMElement_getAttributeNode(This,name,attributeNode)	\
    (This)->lpVtbl -> getAttributeNode(This,name,attributeNode)

#define IXMLDOMElement_setAttributeNode(This,DOMAttribute,attributeNode)	\
    (This)->lpVtbl -> setAttributeNode(This,DOMAttribute,attributeNode)

#define IXMLDOMElement_removeAttributeNode(This,DOMAttribute,attributeNode)	\
    (This)->lpVtbl -> removeAttributeNode(This,DOMAttribute,attributeNode)

#define IXMLDOMElement_getElementsByTagName(This,tagName,resultList)	\
    (This)->lpVtbl -> getElementsByTagName(This,tagName,resultList)

#define IXMLDOMElement_normalize(This)	\
    (This)->lpVtbl -> normalize(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_get_tagName_Proxy( 
    IXMLDOMElement * This,
    /* [retval][out] */ BSTR *tagName);


void __RPC_STUB IXMLDOMElement_get_tagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_getAttribute_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IXMLDOMElement_getAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_setAttribute_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ BSTR name,
    /* [in] */ VARIANT value);


void __RPC_STUB IXMLDOMElement_setAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_removeAttribute_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ BSTR name);


void __RPC_STUB IXMLDOMElement_removeAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_getAttributeNode_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ IXMLDOMAttribute **attributeNode);


void __RPC_STUB IXMLDOMElement_getAttributeNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_setAttributeNode_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ IXMLDOMAttribute *DOMAttribute,
    /* [retval][out] */ IXMLDOMAttribute **attributeNode);


void __RPC_STUB IXMLDOMElement_setAttributeNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_removeAttributeNode_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ IXMLDOMAttribute *DOMAttribute,
    /* [retval][out] */ IXMLDOMAttribute **attributeNode);


void __RPC_STUB IXMLDOMElement_removeAttributeNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_getElementsByTagName_Proxy( 
    IXMLDOMElement * This,
    /* [in] */ BSTR tagName,
    /* [retval][out] */ IXMLDOMNodeList **resultList);


void __RPC_STUB IXMLDOMElement_getElementsByTagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMElement_normalize_Proxy( 
    IXMLDOMElement * This);


void __RPC_STUB IXMLDOMElement_normalize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMElement_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMText_INTERFACE_DEFINED__
#define __IXMLDOMText_INTERFACE_DEFINED__

/* interface IXMLDOMText */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMText;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF87-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMText : public IXMLDOMCharacterData
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE splitText( 
            /* [in] */ long offset,
            /* [retval][out] */ IXMLDOMText **rightHandTextNode) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMTextVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMText * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMText * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMText * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMText * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMText * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMText * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMText * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMText * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMText * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMText * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMText * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMText * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMText * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMText * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMText * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMText * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMText * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMText * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMText * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMText * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMText * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMText * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMText * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMText * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMText * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMText * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_data )( 
            IXMLDOMText * This,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_data )( 
            IXMLDOMText * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMText * This,
            /* [retval][out] */ long *dataLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *substringData )( 
            IXMLDOMText * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendData )( 
            IXMLDOMText * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertData )( 
            IXMLDOMText * This,
            /* [in] */ long offset,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *deleteData )( 
            IXMLDOMText * This,
            /* [in] */ long offset,
            /* [in] */ long count);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceData )( 
            IXMLDOMText * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *splitText )( 
            IXMLDOMText * This,
            /* [in] */ long offset,
            /* [retval][out] */ IXMLDOMText **rightHandTextNode);
        
        END_INTERFACE
    } IXMLDOMTextVtbl;

    interface IXMLDOMText
    {
        CONST_VTBL struct IXMLDOMTextVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMText_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMText_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMText_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMText_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMText_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMText_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMText_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMText_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMText_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMText_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMText_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMText_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMText_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMText_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMText_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMText_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMText_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMText_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMText_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMText_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMText_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMText_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMText_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMText_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMText_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMText_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMText_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMText_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMText_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMText_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMText_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMText_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMText_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMText_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMText_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMText_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMText_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMText_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMText_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMText_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMText_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMText_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMText_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMText_get_data(This,data)	\
    (This)->lpVtbl -> get_data(This,data)

#define IXMLDOMText_put_data(This,data)	\
    (This)->lpVtbl -> put_data(This,data)

#define IXMLDOMText_get_length(This,dataLength)	\
    (This)->lpVtbl -> get_length(This,dataLength)

#define IXMLDOMText_substringData(This,offset,count,data)	\
    (This)->lpVtbl -> substringData(This,offset,count,data)

#define IXMLDOMText_appendData(This,data)	\
    (This)->lpVtbl -> appendData(This,data)

#define IXMLDOMText_insertData(This,offset,data)	\
    (This)->lpVtbl -> insertData(This,offset,data)

#define IXMLDOMText_deleteData(This,offset,count)	\
    (This)->lpVtbl -> deleteData(This,offset,count)

#define IXMLDOMText_replaceData(This,offset,count,data)	\
    (This)->lpVtbl -> replaceData(This,offset,count,data)


#define IXMLDOMText_splitText(This,offset,rightHandTextNode)	\
    (This)->lpVtbl -> splitText(This,offset,rightHandTextNode)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMText_splitText_Proxy( 
    IXMLDOMText * This,
    /* [in] */ long offset,
    /* [retval][out] */ IXMLDOMText **rightHandTextNode);


void __RPC_STUB IXMLDOMText_splitText_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMText_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMComment_INTERFACE_DEFINED__
#define __IXMLDOMComment_INTERFACE_DEFINED__

/* interface IXMLDOMComment */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMComment;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF88-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMComment : public IXMLDOMCharacterData
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMCommentVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMComment * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMComment * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMComment * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMComment * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMComment * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMComment * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMComment * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMComment * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMComment * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMComment * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMComment * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMComment * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMComment * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMComment * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMComment * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMComment * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMComment * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMComment * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMComment * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMComment * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMComment * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMComment * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_data )( 
            IXMLDOMComment * This,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_data )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMComment * This,
            /* [retval][out] */ long *dataLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *substringData )( 
            IXMLDOMComment * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendData )( 
            IXMLDOMComment * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertData )( 
            IXMLDOMComment * This,
            /* [in] */ long offset,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *deleteData )( 
            IXMLDOMComment * This,
            /* [in] */ long offset,
            /* [in] */ long count);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceData )( 
            IXMLDOMComment * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [in] */ BSTR data);
        
        END_INTERFACE
    } IXMLDOMCommentVtbl;

    interface IXMLDOMComment
    {
        CONST_VTBL struct IXMLDOMCommentVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMComment_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMComment_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMComment_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMComment_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMComment_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMComment_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMComment_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMComment_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMComment_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMComment_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMComment_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMComment_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMComment_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMComment_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMComment_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMComment_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMComment_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMComment_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMComment_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMComment_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMComment_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMComment_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMComment_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMComment_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMComment_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMComment_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMComment_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMComment_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMComment_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMComment_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMComment_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMComment_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMComment_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMComment_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMComment_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMComment_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMComment_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMComment_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMComment_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMComment_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMComment_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMComment_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMComment_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMComment_get_data(This,data)	\
    (This)->lpVtbl -> get_data(This,data)

#define IXMLDOMComment_put_data(This,data)	\
    (This)->lpVtbl -> put_data(This,data)

#define IXMLDOMComment_get_length(This,dataLength)	\
    (This)->lpVtbl -> get_length(This,dataLength)

#define IXMLDOMComment_substringData(This,offset,count,data)	\
    (This)->lpVtbl -> substringData(This,offset,count,data)

#define IXMLDOMComment_appendData(This,data)	\
    (This)->lpVtbl -> appendData(This,data)

#define IXMLDOMComment_insertData(This,offset,data)	\
    (This)->lpVtbl -> insertData(This,offset,data)

#define IXMLDOMComment_deleteData(This,offset,count)	\
    (This)->lpVtbl -> deleteData(This,offset,count)

#define IXMLDOMComment_replaceData(This,offset,count,data)	\
    (This)->lpVtbl -> replaceData(This,offset,count,data)


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IXMLDOMComment_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMProcessingInstruction_INTERFACE_DEFINED__
#define __IXMLDOMProcessingInstruction_INTERFACE_DEFINED__

/* interface IXMLDOMProcessingInstruction */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMProcessingInstruction;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF89-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMProcessingInstruction : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_target( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_data( 
            /* [retval][out] */ BSTR *value) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_data( 
            /* [in] */ BSTR value) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMProcessingInstructionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMProcessingInstruction * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMProcessingInstruction * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMProcessingInstruction * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMProcessingInstruction * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_target )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_data )( 
            IXMLDOMProcessingInstruction * This,
            /* [retval][out] */ BSTR *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_data )( 
            IXMLDOMProcessingInstruction * This,
            /* [in] */ BSTR value);
        
        END_INTERFACE
    } IXMLDOMProcessingInstructionVtbl;

    interface IXMLDOMProcessingInstruction
    {
        CONST_VTBL struct IXMLDOMProcessingInstructionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMProcessingInstruction_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMProcessingInstruction_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMProcessingInstruction_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMProcessingInstruction_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMProcessingInstruction_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMProcessingInstruction_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMProcessingInstruction_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMProcessingInstruction_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMProcessingInstruction_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMProcessingInstruction_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMProcessingInstruction_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMProcessingInstruction_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMProcessingInstruction_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMProcessingInstruction_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMProcessingInstruction_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMProcessingInstruction_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMProcessingInstruction_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMProcessingInstruction_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMProcessingInstruction_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMProcessingInstruction_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMProcessingInstruction_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMProcessingInstruction_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMProcessingInstruction_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMProcessingInstruction_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMProcessingInstruction_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMProcessingInstruction_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMProcessingInstruction_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMProcessingInstruction_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMProcessingInstruction_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMProcessingInstruction_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMProcessingInstruction_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMProcessingInstruction_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMProcessingInstruction_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMProcessingInstruction_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMProcessingInstruction_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMProcessingInstruction_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMProcessingInstruction_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMProcessingInstruction_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMProcessingInstruction_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMProcessingInstruction_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMProcessingInstruction_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMProcessingInstruction_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMProcessingInstruction_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMProcessingInstruction_get_target(This,name)	\
    (This)->lpVtbl -> get_target(This,name)

#define IXMLDOMProcessingInstruction_get_data(This,value)	\
    (This)->lpVtbl -> get_data(This,value)

#define IXMLDOMProcessingInstruction_put_data(This,value)	\
    (This)->lpVtbl -> put_data(This,value)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMProcessingInstruction_get_target_Proxy( 
    IXMLDOMProcessingInstruction * This,
    /* [retval][out] */ BSTR *name);


void __RPC_STUB IXMLDOMProcessingInstruction_get_target_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMProcessingInstruction_get_data_Proxy( 
    IXMLDOMProcessingInstruction * This,
    /* [retval][out] */ BSTR *value);


void __RPC_STUB IXMLDOMProcessingInstruction_get_data_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMProcessingInstruction_put_data_Proxy( 
    IXMLDOMProcessingInstruction * This,
    /* [in] */ BSTR value);


void __RPC_STUB IXMLDOMProcessingInstruction_put_data_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMProcessingInstruction_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMCDATASection_INTERFACE_DEFINED__
#define __IXMLDOMCDATASection_INTERFACE_DEFINED__

/* interface IXMLDOMCDATASection */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMCDATASection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8A-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMCDATASection : public IXMLDOMText
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMCDATASectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMCDATASection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMCDATASection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMCDATASection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMCDATASection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMCDATASection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMCDATASection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMCDATASection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMCDATASection * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMCDATASection * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMCDATASection * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMCDATASection * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMCDATASection * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_data )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_data )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMCDATASection * This,
            /* [retval][out] */ long *dataLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *substringData )( 
            IXMLDOMCDATASection * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [retval][out] */ BSTR *data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendData )( 
            IXMLDOMCDATASection * This,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertData )( 
            IXMLDOMCDATASection * This,
            /* [in] */ long offset,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *deleteData )( 
            IXMLDOMCDATASection * This,
            /* [in] */ long offset,
            /* [in] */ long count);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceData )( 
            IXMLDOMCDATASection * This,
            /* [in] */ long offset,
            /* [in] */ long count,
            /* [in] */ BSTR data);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *splitText )( 
            IXMLDOMCDATASection * This,
            /* [in] */ long offset,
            /* [retval][out] */ IXMLDOMText **rightHandTextNode);
        
        END_INTERFACE
    } IXMLDOMCDATASectionVtbl;

    interface IXMLDOMCDATASection
    {
        CONST_VTBL struct IXMLDOMCDATASectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMCDATASection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMCDATASection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMCDATASection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMCDATASection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMCDATASection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMCDATASection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMCDATASection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMCDATASection_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMCDATASection_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMCDATASection_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMCDATASection_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMCDATASection_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMCDATASection_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMCDATASection_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMCDATASection_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMCDATASection_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMCDATASection_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMCDATASection_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMCDATASection_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMCDATASection_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMCDATASection_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMCDATASection_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMCDATASection_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMCDATASection_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMCDATASection_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMCDATASection_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMCDATASection_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMCDATASection_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMCDATASection_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMCDATASection_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMCDATASection_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMCDATASection_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMCDATASection_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMCDATASection_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMCDATASection_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMCDATASection_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMCDATASection_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMCDATASection_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMCDATASection_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMCDATASection_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMCDATASection_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMCDATASection_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMCDATASection_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMCDATASection_get_data(This,data)	\
    (This)->lpVtbl -> get_data(This,data)

#define IXMLDOMCDATASection_put_data(This,data)	\
    (This)->lpVtbl -> put_data(This,data)

#define IXMLDOMCDATASection_get_length(This,dataLength)	\
    (This)->lpVtbl -> get_length(This,dataLength)

#define IXMLDOMCDATASection_substringData(This,offset,count,data)	\
    (This)->lpVtbl -> substringData(This,offset,count,data)

#define IXMLDOMCDATASection_appendData(This,data)	\
    (This)->lpVtbl -> appendData(This,data)

#define IXMLDOMCDATASection_insertData(This,offset,data)	\
    (This)->lpVtbl -> insertData(This,offset,data)

#define IXMLDOMCDATASection_deleteData(This,offset,count)	\
    (This)->lpVtbl -> deleteData(This,offset,count)

#define IXMLDOMCDATASection_replaceData(This,offset,count,data)	\
    (This)->lpVtbl -> replaceData(This,offset,count,data)


#define IXMLDOMCDATASection_splitText(This,offset,rightHandTextNode)	\
    (This)->lpVtbl -> splitText(This,offset,rightHandTextNode)


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IXMLDOMCDATASection_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMDocumentType_INTERFACE_DEFINED__
#define __IXMLDOMDocumentType_INTERFACE_DEFINED__

/* interface IXMLDOMDocumentType */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMDocumentType;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8B-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMDocumentType : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
            /* [retval][out] */ BSTR *rootName) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_entities( 
            /* [retval][out] */ IXMLDOMNamedNodeMap **entityMap) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_notations( 
            /* [retval][out] */ IXMLDOMNamedNodeMap **notationMap) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMDocumentTypeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMDocumentType * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMDocumentType * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMDocumentType * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMDocumentType * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMDocumentType * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMDocumentType * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMDocumentType * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMDocumentType * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMDocumentType * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMDocumentType * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMDocumentType * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMDocumentType * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMDocumentType * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMDocumentType * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMDocumentType * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMDocumentType * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ BSTR *rootName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_entities )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **entityMap);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_notations )( 
            IXMLDOMDocumentType * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **notationMap);
        
        END_INTERFACE
    } IXMLDOMDocumentTypeVtbl;

    interface IXMLDOMDocumentType
    {
        CONST_VTBL struct IXMLDOMDocumentTypeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMDocumentType_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMDocumentType_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMDocumentType_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMDocumentType_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMDocumentType_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMDocumentType_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMDocumentType_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMDocumentType_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMDocumentType_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMDocumentType_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMDocumentType_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMDocumentType_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMDocumentType_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMDocumentType_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMDocumentType_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMDocumentType_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMDocumentType_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMDocumentType_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMDocumentType_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMDocumentType_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMDocumentType_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMDocumentType_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMDocumentType_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMDocumentType_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMDocumentType_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMDocumentType_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMDocumentType_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMDocumentType_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMDocumentType_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMDocumentType_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMDocumentType_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMDocumentType_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMDocumentType_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMDocumentType_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMDocumentType_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMDocumentType_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMDocumentType_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMDocumentType_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMDocumentType_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMDocumentType_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMDocumentType_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMDocumentType_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMDocumentType_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMDocumentType_get_name(This,rootName)	\
    (This)->lpVtbl -> get_name(This,rootName)

#define IXMLDOMDocumentType_get_entities(This,entityMap)	\
    (This)->lpVtbl -> get_entities(This,entityMap)

#define IXMLDOMDocumentType_get_notations(This,notationMap)	\
    (This)->lpVtbl -> get_notations(This,notationMap)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocumentType_get_name_Proxy( 
    IXMLDOMDocumentType * This,
    /* [retval][out] */ BSTR *rootName);


void __RPC_STUB IXMLDOMDocumentType_get_name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocumentType_get_entities_Proxy( 
    IXMLDOMDocumentType * This,
    /* [retval][out] */ IXMLDOMNamedNodeMap **entityMap);


void __RPC_STUB IXMLDOMDocumentType_get_entities_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMDocumentType_get_notations_Proxy( 
    IXMLDOMDocumentType * This,
    /* [retval][out] */ IXMLDOMNamedNodeMap **notationMap);


void __RPC_STUB IXMLDOMDocumentType_get_notations_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMDocumentType_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMNotation_INTERFACE_DEFINED__
#define __IXMLDOMNotation_INTERFACE_DEFINED__

/* interface IXMLDOMNotation */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMNotation;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8C-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMNotation : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_publicId( 
            /* [retval][out] */ VARIANT *publicID) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_systemId( 
            /* [retval][out] */ VARIANT *systemID) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMNotationVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMNotation * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMNotation * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMNotation * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMNotation * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMNotation * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMNotation * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMNotation * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMNotation * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMNotation * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMNotation * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMNotation * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMNotation * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMNotation * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMNotation * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMNotation * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMNotation * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_publicId )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ VARIANT *publicID);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_systemId )( 
            IXMLDOMNotation * This,
            /* [retval][out] */ VARIANT *systemID);
        
        END_INTERFACE
    } IXMLDOMNotationVtbl;

    interface IXMLDOMNotation
    {
        CONST_VTBL struct IXMLDOMNotationVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMNotation_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMNotation_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMNotation_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMNotation_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMNotation_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMNotation_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMNotation_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMNotation_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMNotation_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMNotation_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMNotation_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMNotation_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMNotation_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMNotation_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMNotation_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMNotation_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMNotation_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMNotation_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMNotation_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMNotation_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMNotation_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMNotation_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMNotation_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMNotation_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMNotation_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMNotation_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMNotation_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMNotation_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMNotation_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMNotation_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMNotation_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMNotation_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMNotation_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMNotation_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMNotation_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMNotation_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMNotation_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMNotation_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMNotation_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMNotation_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMNotation_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMNotation_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMNotation_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMNotation_get_publicId(This,publicID)	\
    (This)->lpVtbl -> get_publicId(This,publicID)

#define IXMLDOMNotation_get_systemId(This,systemID)	\
    (This)->lpVtbl -> get_systemId(This,systemID)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNotation_get_publicId_Proxy( 
    IXMLDOMNotation * This,
    /* [retval][out] */ VARIANT *publicID);


void __RPC_STUB IXMLDOMNotation_get_publicId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMNotation_get_systemId_Proxy( 
    IXMLDOMNotation * This,
    /* [retval][out] */ VARIANT *systemID);


void __RPC_STUB IXMLDOMNotation_get_systemId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMNotation_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMEntity_INTERFACE_DEFINED__
#define __IXMLDOMEntity_INTERFACE_DEFINED__

/* interface IXMLDOMEntity */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMEntity;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8D-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMEntity : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_publicId( 
            /* [retval][out] */ VARIANT *publicID) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_systemId( 
            /* [retval][out] */ VARIANT *systemID) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_notationName( 
            /* [retval][out] */ BSTR *name) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMEntityVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMEntity * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMEntity * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMEntity * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMEntity * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMEntity * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMEntity * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMEntity * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMEntity * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMEntity * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMEntity * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMEntity * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMEntity * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMEntity * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMEntity * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMEntity * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMEntity * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_publicId )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ VARIANT *publicID);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_systemId )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ VARIANT *systemID);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_notationName )( 
            IXMLDOMEntity * This,
            /* [retval][out] */ BSTR *name);
        
        END_INTERFACE
    } IXMLDOMEntityVtbl;

    interface IXMLDOMEntity
    {
        CONST_VTBL struct IXMLDOMEntityVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMEntity_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMEntity_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMEntity_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMEntity_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMEntity_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMEntity_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMEntity_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMEntity_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMEntity_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMEntity_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMEntity_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMEntity_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMEntity_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMEntity_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMEntity_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMEntity_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMEntity_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMEntity_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMEntity_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMEntity_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMEntity_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMEntity_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMEntity_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMEntity_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMEntity_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMEntity_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMEntity_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMEntity_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMEntity_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMEntity_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMEntity_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMEntity_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMEntity_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMEntity_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMEntity_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMEntity_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMEntity_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMEntity_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMEntity_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMEntity_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMEntity_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMEntity_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMEntity_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXMLDOMEntity_get_publicId(This,publicID)	\
    (This)->lpVtbl -> get_publicId(This,publicID)

#define IXMLDOMEntity_get_systemId(This,systemID)	\
    (This)->lpVtbl -> get_systemId(This,systemID)

#define IXMLDOMEntity_get_notationName(This,name)	\
    (This)->lpVtbl -> get_notationName(This,name)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMEntity_get_publicId_Proxy( 
    IXMLDOMEntity * This,
    /* [retval][out] */ VARIANT *publicID);


void __RPC_STUB IXMLDOMEntity_get_publicId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMEntity_get_systemId_Proxy( 
    IXMLDOMEntity * This,
    /* [retval][out] */ VARIANT *systemID);


void __RPC_STUB IXMLDOMEntity_get_systemId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMEntity_get_notationName_Proxy( 
    IXMLDOMEntity * This,
    /* [retval][out] */ BSTR *name);


void __RPC_STUB IXMLDOMEntity_get_notationName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMEntity_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMEntityReference_INTERFACE_DEFINED__
#define __IXMLDOMEntityReference_INTERFACE_DEFINED__

/* interface IXMLDOMEntityReference */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMEntityReference;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF8E-7B36-11d2-B20E-00C04F983E60")
    IXMLDOMEntityReference : public IXMLDOMNode
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMEntityReferenceVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMEntityReference * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMEntityReference * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMEntityReference * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMEntityReference * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMEntityReference * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMEntityReference * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMEntityReference * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXMLDOMEntityReference * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXMLDOMEntityReference * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLDOMEntityReference * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXMLDOMEntityReference * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXMLDOMEntityReference * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXMLDOMEntityReference * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXMLDOMEntityReference * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXMLDOMEntityReference * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXMLDOMEntityReference * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXMLDOMEntityReference * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        END_INTERFACE
    } IXMLDOMEntityReferenceVtbl;

    interface IXMLDOMEntityReference
    {
        CONST_VTBL struct IXMLDOMEntityReferenceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMEntityReference_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMEntityReference_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMEntityReference_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMEntityReference_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMEntityReference_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMEntityReference_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMEntityReference_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMEntityReference_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXMLDOMEntityReference_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXMLDOMEntityReference_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXMLDOMEntityReference_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXMLDOMEntityReference_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXMLDOMEntityReference_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXMLDOMEntityReference_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXMLDOMEntityReference_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXMLDOMEntityReference_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXMLDOMEntityReference_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXMLDOMEntityReference_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXMLDOMEntityReference_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXMLDOMEntityReference_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXMLDOMEntityReference_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXMLDOMEntityReference_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXMLDOMEntityReference_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXMLDOMEntityReference_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXMLDOMEntityReference_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXMLDOMEntityReference_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXMLDOMEntityReference_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXMLDOMEntityReference_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXMLDOMEntityReference_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXMLDOMEntityReference_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXMLDOMEntityReference_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXMLDOMEntityReference_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXMLDOMEntityReference_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXMLDOMEntityReference_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXMLDOMEntityReference_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXMLDOMEntityReference_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXMLDOMEntityReference_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXMLDOMEntityReference_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXMLDOMEntityReference_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXMLDOMEntityReference_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXMLDOMEntityReference_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXMLDOMEntityReference_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXMLDOMEntityReference_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IXMLDOMEntityReference_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMParseError_INTERFACE_DEFINED__
#define __IXMLDOMParseError_INTERFACE_DEFINED__

/* interface IXMLDOMParseError */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMParseError;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3efaa426-272f-11d2-836f-0000f87a7782")
    IXMLDOMParseError : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_errorCode( 
            /* [out][retval] */ long *errorCode) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_url( 
            /* [out][retval] */ BSTR *urlString) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_reason( 
            /* [out][retval] */ BSTR *reasonString) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_srcText( 
            /* [out][retval] */ BSTR *sourceString) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_line( 
            /* [out][retval] */ long *lineNumber) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_linepos( 
            /* [out][retval] */ long *linePosition) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_filepos( 
            /* [out][retval] */ long *filePosition) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMParseErrorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMParseError * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMParseError * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMParseError * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMParseError * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMParseError * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMParseError * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMParseError * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_errorCode )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ long *errorCode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_url )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ BSTR *urlString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_reason )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ BSTR *reasonString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_srcText )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ BSTR *sourceString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_line )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ long *lineNumber);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_linepos )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ long *linePosition);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_filepos )( 
            IXMLDOMParseError * This,
            /* [out][retval] */ long *filePosition);
        
        END_INTERFACE
    } IXMLDOMParseErrorVtbl;

    interface IXMLDOMParseError
    {
        CONST_VTBL struct IXMLDOMParseErrorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMParseError_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMParseError_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMParseError_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMParseError_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMParseError_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMParseError_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMParseError_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMParseError_get_errorCode(This,errorCode)	\
    (This)->lpVtbl -> get_errorCode(This,errorCode)

#define IXMLDOMParseError_get_url(This,urlString)	\
    (This)->lpVtbl -> get_url(This,urlString)

#define IXMLDOMParseError_get_reason(This,reasonString)	\
    (This)->lpVtbl -> get_reason(This,reasonString)

#define IXMLDOMParseError_get_srcText(This,sourceString)	\
    (This)->lpVtbl -> get_srcText(This,sourceString)

#define IXMLDOMParseError_get_line(This,lineNumber)	\
    (This)->lpVtbl -> get_line(This,lineNumber)

#define IXMLDOMParseError_get_linepos(This,linePosition)	\
    (This)->lpVtbl -> get_linepos(This,linePosition)

#define IXMLDOMParseError_get_filepos(This,filePosition)	\
    (This)->lpVtbl -> get_filepos(This,filePosition)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_errorCode_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ long *errorCode);


void __RPC_STUB IXMLDOMParseError_get_errorCode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_url_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ BSTR *urlString);


void __RPC_STUB IXMLDOMParseError_get_url_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_reason_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ BSTR *reasonString);


void __RPC_STUB IXMLDOMParseError_get_reason_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_srcText_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ BSTR *sourceString);


void __RPC_STUB IXMLDOMParseError_get_srcText_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_line_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ long *lineNumber);


void __RPC_STUB IXMLDOMParseError_get_line_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_linepos_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ long *linePosition);


void __RPC_STUB IXMLDOMParseError_get_linepos_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMParseError_get_filepos_Proxy( 
    IXMLDOMParseError * This,
    /* [out][retval] */ long *filePosition);


void __RPC_STUB IXMLDOMParseError_get_filepos_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMParseError_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMSchemaCollection_INTERFACE_DEFINED__
#define __IXMLDOMSchemaCollection_INTERFACE_DEFINED__

/* interface IXMLDOMSchemaCollection */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMSchemaCollection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("373984c8-b845-449b-91e7-45ac83036ade")
    IXMLDOMSchemaCollection : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE add( 
            /* [in] */ BSTR namespaceURI,
            /* [in] */ VARIANT var) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE get( 
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **schemaNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE remove( 
            /* [in] */ BSTR namespaceURI) = 0;
        
        virtual /* [propget][helpstring][id] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *length) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_namespaceURI( 
            /* [in] */ long index,
            /* [retval][out] */ BSTR *length) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addCollection( 
            /* [in] */ IXMLDOMSchemaCollection *otherCollection) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [out][retval] */ IUnknown **ppUnk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMSchemaCollectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMSchemaCollection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMSchemaCollection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMSchemaCollection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *add )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ BSTR namespaceURI,
            /* [in] */ VARIANT var);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *get )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **schemaNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *remove )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ BSTR namespaceURI);
        
        /* [propget][helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMSchemaCollection * This,
            /* [retval][out] */ long *length);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ long index,
            /* [retval][out] */ BSTR *length);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addCollection )( 
            IXMLDOMSchemaCollection * This,
            /* [in] */ IXMLDOMSchemaCollection *otherCollection);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLDOMSchemaCollection * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        END_INTERFACE
    } IXMLDOMSchemaCollectionVtbl;

    interface IXMLDOMSchemaCollection
    {
        CONST_VTBL struct IXMLDOMSchemaCollectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMSchemaCollection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMSchemaCollection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMSchemaCollection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMSchemaCollection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMSchemaCollection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMSchemaCollection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMSchemaCollection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMSchemaCollection_add(This,namespaceURI,var)	\
    (This)->lpVtbl -> add(This,namespaceURI,var)

#define IXMLDOMSchemaCollection_get(This,namespaceURI,schemaNode)	\
    (This)->lpVtbl -> get(This,namespaceURI,schemaNode)

#define IXMLDOMSchemaCollection_remove(This,namespaceURI)	\
    (This)->lpVtbl -> remove(This,namespaceURI)

#define IXMLDOMSchemaCollection_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define IXMLDOMSchemaCollection_get_namespaceURI(This,index,length)	\
    (This)->lpVtbl -> get_namespaceURI(This,index,length)

#define IXMLDOMSchemaCollection_addCollection(This,otherCollection)	\
    (This)->lpVtbl -> addCollection(This,otherCollection)

#define IXMLDOMSchemaCollection_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_add_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [in] */ BSTR namespaceURI,
    /* [in] */ VARIANT var);


void __RPC_STUB IXMLDOMSchemaCollection_add_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_get_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ IXMLDOMNode **schemaNode);


void __RPC_STUB IXMLDOMSchemaCollection_get_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_remove_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [in] */ BSTR namespaceURI);


void __RPC_STUB IXMLDOMSchemaCollection_remove_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_get_length_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [retval][out] */ long *length);


void __RPC_STUB IXMLDOMSchemaCollection_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_get_namespaceURI_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [in] */ long index,
    /* [retval][out] */ BSTR *length);


void __RPC_STUB IXMLDOMSchemaCollection_get_namespaceURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_addCollection_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [in] */ IXMLDOMSchemaCollection *otherCollection);


void __RPC_STUB IXMLDOMSchemaCollection_addCollection_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection_get__newEnum_Proxy( 
    IXMLDOMSchemaCollection * This,
    /* [out][retval] */ IUnknown **ppUnk);


void __RPC_STUB IXMLDOMSchemaCollection_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMSchemaCollection_INTERFACE_DEFINED__ */


#ifndef __IXTLRuntime_INTERFACE_DEFINED__
#define __IXTLRuntime_INTERFACE_DEFINED__

/* interface IXTLRuntime */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXTLRuntime;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3efaa425-272f-11d2-836f-0000f87a7782")
    IXTLRuntime : public IXMLDOMNode
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE uniqueID( 
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pID) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE depth( 
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pDepth) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE childNumber( 
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ancestorChildNumber( 
            /* [in] */ BSTR bstrNodeName,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE absoluteChildNumber( 
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE formatIndex( 
            /* [in] */ long lIndex,
            /* [in] */ BSTR bstrFormat,
            /* [retval][out] */ BSTR *pbstrFormattedString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE formatNumber( 
            /* [in] */ double dblNumber,
            /* [in] */ BSTR bstrFormat,
            /* [retval][out] */ BSTR *pbstrFormattedString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE formatDate( 
            /* [in] */ VARIANT varDate,
            /* [in] */ BSTR bstrFormat,
            /* [optional][in] */ VARIANT varDestLocale,
            /* [retval][out] */ BSTR *pbstrFormattedString) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE formatTime( 
            /* [in] */ VARIANT varTime,
            /* [in] */ BSTR bstrFormat,
            /* [optional][in] */ VARIANT varDestLocale,
            /* [retval][out] */ BSTR *pbstrFormattedString) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXTLRuntimeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXTLRuntime * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXTLRuntime * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXTLRuntime * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXTLRuntime * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXTLRuntime * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXTLRuntime * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXTLRuntime * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeName )( 
            IXTLRuntime * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeValue )( 
            IXTLRuntime * This,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeValue )( 
            IXTLRuntime * This,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeType )( 
            IXTLRuntime * This,
            /* [retval][out] */ DOMNodeType *type);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parentNode )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNode **parent);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_childNodes )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNodeList **childList);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_firstChild )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNode **firstChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lastChild )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNode **lastChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_previousSibling )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNode **previousSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nextSibling )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNode **nextSibling);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMNamedNodeMap **attributeMap);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *insertBefore )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ VARIANT refChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *replaceChild )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [in] */ IXMLDOMNode *oldChild,
            /* [retval][out] */ IXMLDOMNode **outOldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *childNode,
            /* [retval][out] */ IXMLDOMNode **oldChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *appendChild )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *newChild,
            /* [retval][out] */ IXMLDOMNode **outNewChild);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *hasChildNodes )( 
            IXTLRuntime * This,
            /* [retval][out] */ VARIANT_BOOL *hasChild);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerDocument )( 
            IXTLRuntime * This,
            /* [retval][out] */ IXMLDOMDocument **DOMDocument);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *cloneNode )( 
            IXTLRuntime * This,
            /* [in] */ VARIANT_BOOL deep,
            /* [retval][out] */ IXMLDOMNode **cloneRoot);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypeString )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *nodeType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *text);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXTLRuntime * This,
            /* [in] */ BSTR text);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_specified )( 
            IXTLRuntime * This,
            /* [retval][out] */ VARIANT_BOOL *isSpecified);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_definition )( 
            IXTLRuntime * This,
            /* [out][retval] */ IXMLDOMNode **definitionNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_nodeTypedValue )( 
            IXTLRuntime * This,
            /* [out][retval] */ VARIANT *typedValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_nodeTypedValue )( 
            IXTLRuntime * This,
            /* [in] */ VARIANT typedValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dataType )( 
            IXTLRuntime * This,
            /* [out][retval] */ VARIANT *dataTypeName);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_dataType )( 
            IXTLRuntime * This,
            /* [in] */ BSTR dataTypeName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_xml )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNode )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [out][retval] */ BSTR *xmlString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectNodes )( 
            IXTLRuntime * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNodeList **resultList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *selectSingleNode )( 
            IXTLRuntime * This,
            /* [in] */ BSTR queryString,
            /* [out][retval] */ IXMLDOMNode **resultNode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parsed )( 
            IXTLRuntime * This,
            /* [out][retval] */ VARIANT_BOOL *isParsed);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_prefix )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *prefixString);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseName )( 
            IXTLRuntime * This,
            /* [out][retval] */ BSTR *nameString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transformNodeToObject )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *stylesheet,
            /* [in] */ VARIANT outputObject);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *uniqueID )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pID);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *depth )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pDepth);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *childNumber )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *ancestorChildNumber )( 
            IXTLRuntime * This,
            /* [in] */ BSTR bstrNodeName,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *absoluteChildNumber )( 
            IXTLRuntime * This,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ long *pNumber);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *formatIndex )( 
            IXTLRuntime * This,
            /* [in] */ long lIndex,
            /* [in] */ BSTR bstrFormat,
            /* [retval][out] */ BSTR *pbstrFormattedString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *formatNumber )( 
            IXTLRuntime * This,
            /* [in] */ double dblNumber,
            /* [in] */ BSTR bstrFormat,
            /* [retval][out] */ BSTR *pbstrFormattedString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *formatDate )( 
            IXTLRuntime * This,
            /* [in] */ VARIANT varDate,
            /* [in] */ BSTR bstrFormat,
            /* [optional][in] */ VARIANT varDestLocale,
            /* [retval][out] */ BSTR *pbstrFormattedString);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *formatTime )( 
            IXTLRuntime * This,
            /* [in] */ VARIANT varTime,
            /* [in] */ BSTR bstrFormat,
            /* [optional][in] */ VARIANT varDestLocale,
            /* [retval][out] */ BSTR *pbstrFormattedString);
        
        END_INTERFACE
    } IXTLRuntimeVtbl;

    interface IXTLRuntime
    {
        CONST_VTBL struct IXTLRuntimeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXTLRuntime_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXTLRuntime_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXTLRuntime_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXTLRuntime_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXTLRuntime_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXTLRuntime_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXTLRuntime_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXTLRuntime_get_nodeName(This,name)	\
    (This)->lpVtbl -> get_nodeName(This,name)

#define IXTLRuntime_get_nodeValue(This,value)	\
    (This)->lpVtbl -> get_nodeValue(This,value)

#define IXTLRuntime_put_nodeValue(This,value)	\
    (This)->lpVtbl -> put_nodeValue(This,value)

#define IXTLRuntime_get_nodeType(This,type)	\
    (This)->lpVtbl -> get_nodeType(This,type)

#define IXTLRuntime_get_parentNode(This,parent)	\
    (This)->lpVtbl -> get_parentNode(This,parent)

#define IXTLRuntime_get_childNodes(This,childList)	\
    (This)->lpVtbl -> get_childNodes(This,childList)

#define IXTLRuntime_get_firstChild(This,firstChild)	\
    (This)->lpVtbl -> get_firstChild(This,firstChild)

#define IXTLRuntime_get_lastChild(This,lastChild)	\
    (This)->lpVtbl -> get_lastChild(This,lastChild)

#define IXTLRuntime_get_previousSibling(This,previousSibling)	\
    (This)->lpVtbl -> get_previousSibling(This,previousSibling)

#define IXTLRuntime_get_nextSibling(This,nextSibling)	\
    (This)->lpVtbl -> get_nextSibling(This,nextSibling)

#define IXTLRuntime_get_attributes(This,attributeMap)	\
    (This)->lpVtbl -> get_attributes(This,attributeMap)

#define IXTLRuntime_insertBefore(This,newChild,refChild,outNewChild)	\
    (This)->lpVtbl -> insertBefore(This,newChild,refChild,outNewChild)

#define IXTLRuntime_replaceChild(This,newChild,oldChild,outOldChild)	\
    (This)->lpVtbl -> replaceChild(This,newChild,oldChild,outOldChild)

#define IXTLRuntime_removeChild(This,childNode,oldChild)	\
    (This)->lpVtbl -> removeChild(This,childNode,oldChild)

#define IXTLRuntime_appendChild(This,newChild,outNewChild)	\
    (This)->lpVtbl -> appendChild(This,newChild,outNewChild)

#define IXTLRuntime_hasChildNodes(This,hasChild)	\
    (This)->lpVtbl -> hasChildNodes(This,hasChild)

#define IXTLRuntime_get_ownerDocument(This,DOMDocument)	\
    (This)->lpVtbl -> get_ownerDocument(This,DOMDocument)

#define IXTLRuntime_cloneNode(This,deep,cloneRoot)	\
    (This)->lpVtbl -> cloneNode(This,deep,cloneRoot)

#define IXTLRuntime_get_nodeTypeString(This,nodeType)	\
    (This)->lpVtbl -> get_nodeTypeString(This,nodeType)

#define IXTLRuntime_get_text(This,text)	\
    (This)->lpVtbl -> get_text(This,text)

#define IXTLRuntime_put_text(This,text)	\
    (This)->lpVtbl -> put_text(This,text)

#define IXTLRuntime_get_specified(This,isSpecified)	\
    (This)->lpVtbl -> get_specified(This,isSpecified)

#define IXTLRuntime_get_definition(This,definitionNode)	\
    (This)->lpVtbl -> get_definition(This,definitionNode)

#define IXTLRuntime_get_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> get_nodeTypedValue(This,typedValue)

#define IXTLRuntime_put_nodeTypedValue(This,typedValue)	\
    (This)->lpVtbl -> put_nodeTypedValue(This,typedValue)

#define IXTLRuntime_get_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> get_dataType(This,dataTypeName)

#define IXTLRuntime_put_dataType(This,dataTypeName)	\
    (This)->lpVtbl -> put_dataType(This,dataTypeName)

#define IXTLRuntime_get_xml(This,xmlString)	\
    (This)->lpVtbl -> get_xml(This,xmlString)

#define IXTLRuntime_transformNode(This,stylesheet,xmlString)	\
    (This)->lpVtbl -> transformNode(This,stylesheet,xmlString)

#define IXTLRuntime_selectNodes(This,queryString,resultList)	\
    (This)->lpVtbl -> selectNodes(This,queryString,resultList)

#define IXTLRuntime_selectSingleNode(This,queryString,resultNode)	\
    (This)->lpVtbl -> selectSingleNode(This,queryString,resultNode)

#define IXTLRuntime_get_parsed(This,isParsed)	\
    (This)->lpVtbl -> get_parsed(This,isParsed)

#define IXTLRuntime_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define IXTLRuntime_get_prefix(This,prefixString)	\
    (This)->lpVtbl -> get_prefix(This,prefixString)

#define IXTLRuntime_get_baseName(This,nameString)	\
    (This)->lpVtbl -> get_baseName(This,nameString)

#define IXTLRuntime_transformNodeToObject(This,stylesheet,outputObject)	\
    (This)->lpVtbl -> transformNodeToObject(This,stylesheet,outputObject)


#define IXTLRuntime_uniqueID(This,pNode,pID)	\
    (This)->lpVtbl -> uniqueID(This,pNode,pID)

#define IXTLRuntime_depth(This,pNode,pDepth)	\
    (This)->lpVtbl -> depth(This,pNode,pDepth)

#define IXTLRuntime_childNumber(This,pNode,pNumber)	\
    (This)->lpVtbl -> childNumber(This,pNode,pNumber)

#define IXTLRuntime_ancestorChildNumber(This,bstrNodeName,pNode,pNumber)	\
    (This)->lpVtbl -> ancestorChildNumber(This,bstrNodeName,pNode,pNumber)

#define IXTLRuntime_absoluteChildNumber(This,pNode,pNumber)	\
    (This)->lpVtbl -> absoluteChildNumber(This,pNode,pNumber)

#define IXTLRuntime_formatIndex(This,lIndex,bstrFormat,pbstrFormattedString)	\
    (This)->lpVtbl -> formatIndex(This,lIndex,bstrFormat,pbstrFormattedString)

#define IXTLRuntime_formatNumber(This,dblNumber,bstrFormat,pbstrFormattedString)	\
    (This)->lpVtbl -> formatNumber(This,dblNumber,bstrFormat,pbstrFormattedString)

#define IXTLRuntime_formatDate(This,varDate,bstrFormat,varDestLocale,pbstrFormattedString)	\
    (This)->lpVtbl -> formatDate(This,varDate,bstrFormat,varDestLocale,pbstrFormattedString)

#define IXTLRuntime_formatTime(This,varTime,bstrFormat,varDestLocale,pbstrFormattedString)	\
    (This)->lpVtbl -> formatTime(This,varTime,bstrFormat,varDestLocale,pbstrFormattedString)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_uniqueID_Proxy( 
    IXTLRuntime * This,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ long *pID);


void __RPC_STUB IXTLRuntime_uniqueID_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_depth_Proxy( 
    IXTLRuntime * This,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ long *pDepth);


void __RPC_STUB IXTLRuntime_depth_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_childNumber_Proxy( 
    IXTLRuntime * This,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ long *pNumber);


void __RPC_STUB IXTLRuntime_childNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_ancestorChildNumber_Proxy( 
    IXTLRuntime * This,
    /* [in] */ BSTR bstrNodeName,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ long *pNumber);


void __RPC_STUB IXTLRuntime_ancestorChildNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_absoluteChildNumber_Proxy( 
    IXTLRuntime * This,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ long *pNumber);


void __RPC_STUB IXTLRuntime_absoluteChildNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_formatIndex_Proxy( 
    IXTLRuntime * This,
    /* [in] */ long lIndex,
    /* [in] */ BSTR bstrFormat,
    /* [retval][out] */ BSTR *pbstrFormattedString);


void __RPC_STUB IXTLRuntime_formatIndex_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_formatNumber_Proxy( 
    IXTLRuntime * This,
    /* [in] */ double dblNumber,
    /* [in] */ BSTR bstrFormat,
    /* [retval][out] */ BSTR *pbstrFormattedString);


void __RPC_STUB IXTLRuntime_formatNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_formatDate_Proxy( 
    IXTLRuntime * This,
    /* [in] */ VARIANT varDate,
    /* [in] */ BSTR bstrFormat,
    /* [optional][in] */ VARIANT varDestLocale,
    /* [retval][out] */ BSTR *pbstrFormattedString);


void __RPC_STUB IXTLRuntime_formatDate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXTLRuntime_formatTime_Proxy( 
    IXTLRuntime * This,
    /* [in] */ VARIANT varTime,
    /* [in] */ BSTR bstrFormat,
    /* [optional][in] */ VARIANT varDestLocale,
    /* [retval][out] */ BSTR *pbstrFormattedString);


void __RPC_STUB IXTLRuntime_formatTime_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXTLRuntime_INTERFACE_DEFINED__ */


#ifndef __IXSLTemplate_INTERFACE_DEFINED__
#define __IXSLTemplate_INTERFACE_DEFINED__

/* interface IXSLTemplate */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXSLTemplate;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF93-7B36-11d2-B20E-00C04F983E60")
    IXSLTemplate : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_stylesheet( 
            /* [in] */ IXMLDOMNode *stylesheet) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_stylesheet( 
            /* [retval][out] */ IXMLDOMNode **stylesheet) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createProcessor( 
            /* [retval][out] */ IXSLProcessor **ppProcessor) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXSLTemplateVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXSLTemplate * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXSLTemplate * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXSLTemplate * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXSLTemplate * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXSLTemplate * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXSLTemplate * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXSLTemplate * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_stylesheet )( 
            IXSLTemplate * This,
            /* [in] */ IXMLDOMNode *stylesheet);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_stylesheet )( 
            IXSLTemplate * This,
            /* [retval][out] */ IXMLDOMNode **stylesheet);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createProcessor )( 
            IXSLTemplate * This,
            /* [retval][out] */ IXSLProcessor **ppProcessor);
        
        END_INTERFACE
    } IXSLTemplateVtbl;

    interface IXSLTemplate
    {
        CONST_VTBL struct IXSLTemplateVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXSLTemplate_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXSLTemplate_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXSLTemplate_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXSLTemplate_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXSLTemplate_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXSLTemplate_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXSLTemplate_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXSLTemplate_putref_stylesheet(This,stylesheet)	\
    (This)->lpVtbl -> putref_stylesheet(This,stylesheet)

#define IXSLTemplate_get_stylesheet(This,stylesheet)	\
    (This)->lpVtbl -> get_stylesheet(This,stylesheet)

#define IXSLTemplate_createProcessor(This,ppProcessor)	\
    (This)->lpVtbl -> createProcessor(This,ppProcessor)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IXSLTemplate_putref_stylesheet_Proxy( 
    IXSLTemplate * This,
    /* [in] */ IXMLDOMNode *stylesheet);


void __RPC_STUB IXSLTemplate_putref_stylesheet_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLTemplate_get_stylesheet_Proxy( 
    IXSLTemplate * This,
    /* [retval][out] */ IXMLDOMNode **stylesheet);


void __RPC_STUB IXSLTemplate_get_stylesheet_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLTemplate_createProcessor_Proxy( 
    IXSLTemplate * This,
    /* [retval][out] */ IXSLProcessor **ppProcessor);


void __RPC_STUB IXSLTemplate_createProcessor_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXSLTemplate_INTERFACE_DEFINED__ */


#ifndef __IXSLProcessor_INTERFACE_DEFINED__
#define __IXSLProcessor_INTERFACE_DEFINED__

/* interface IXSLProcessor */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXSLProcessor;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2933BF92-7B36-11d2-B20E-00C04F983E60")
    IXSLProcessor : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_input( 
            /* [in] */ VARIANT var) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_input( 
            /* [retval][out] */ VARIANT *pVar) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ownerTemplate( 
            /* [retval][out] */ IXSLTemplate **ppTemplate) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setStartMode( 
            /* [in] */ BSTR mode,
            /* [defaultvalue][in] */ BSTR namespaceURI = L"") = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_startMode( 
            /* [retval][out] */ BSTR *mode) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_startModeURI( 
            /* [retval][out] */ BSTR *namespaceURI) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_output( 
            /* [in] */ VARIANT output) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_output( 
            /* [retval][out] */ VARIANT *pOutput) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE transform( 
            /* [retval][out] */ VARIANT_BOOL *pDone) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE reset( void) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [retval][out] */ long *pReadyState) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addParameter( 
            /* [in] */ BSTR baseName,
            /* [in] */ VARIANT parameter,
            /* [defaultvalue][in] */ BSTR namespaceURI = L"") = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addObject( 
            /* [in] */ IDispatch *obj,
            /* [in] */ BSTR namespaceURI) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_stylesheet( 
            /* [retval][out] */ IXMLDOMNode **stylesheet) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXSLProcessorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXSLProcessor * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXSLProcessor * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXSLProcessor * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXSLProcessor * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXSLProcessor * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXSLProcessor * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXSLProcessor * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_input )( 
            IXSLProcessor * This,
            /* [in] */ VARIANT var);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_input )( 
            IXSLProcessor * This,
            /* [retval][out] */ VARIANT *pVar);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ownerTemplate )( 
            IXSLProcessor * This,
            /* [retval][out] */ IXSLTemplate **ppTemplate);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setStartMode )( 
            IXSLProcessor * This,
            /* [in] */ BSTR mode,
            /* [defaultvalue][in] */ BSTR namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_startMode )( 
            IXSLProcessor * This,
            /* [retval][out] */ BSTR *mode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_startModeURI )( 
            IXSLProcessor * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_output )( 
            IXSLProcessor * This,
            /* [in] */ VARIANT output);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_output )( 
            IXSLProcessor * This,
            /* [retval][out] */ VARIANT *pOutput);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *transform )( 
            IXSLProcessor * This,
            /* [retval][out] */ VARIANT_BOOL *pDone);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *reset )( 
            IXSLProcessor * This);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXSLProcessor * This,
            /* [retval][out] */ long *pReadyState);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addParameter )( 
            IXSLProcessor * This,
            /* [in] */ BSTR baseName,
            /* [in] */ VARIANT parameter,
            /* [defaultvalue][in] */ BSTR namespaceURI);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addObject )( 
            IXSLProcessor * This,
            /* [in] */ IDispatch *obj,
            /* [in] */ BSTR namespaceURI);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_stylesheet )( 
            IXSLProcessor * This,
            /* [retval][out] */ IXMLDOMNode **stylesheet);
        
        END_INTERFACE
    } IXSLProcessorVtbl;

    interface IXSLProcessor
    {
        CONST_VTBL struct IXSLProcessorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXSLProcessor_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXSLProcessor_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXSLProcessor_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXSLProcessor_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXSLProcessor_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXSLProcessor_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXSLProcessor_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXSLProcessor_put_input(This,var)	\
    (This)->lpVtbl -> put_input(This,var)

#define IXSLProcessor_get_input(This,pVar)	\
    (This)->lpVtbl -> get_input(This,pVar)

#define IXSLProcessor_get_ownerTemplate(This,ppTemplate)	\
    (This)->lpVtbl -> get_ownerTemplate(This,ppTemplate)

#define IXSLProcessor_setStartMode(This,mode,namespaceURI)	\
    (This)->lpVtbl -> setStartMode(This,mode,namespaceURI)

#define IXSLProcessor_get_startMode(This,mode)	\
    (This)->lpVtbl -> get_startMode(This,mode)

#define IXSLProcessor_get_startModeURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_startModeURI(This,namespaceURI)

#define IXSLProcessor_put_output(This,output)	\
    (This)->lpVtbl -> put_output(This,output)

#define IXSLProcessor_get_output(This,pOutput)	\
    (This)->lpVtbl -> get_output(This,pOutput)

#define IXSLProcessor_transform(This,pDone)	\
    (This)->lpVtbl -> transform(This,pDone)

#define IXSLProcessor_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IXSLProcessor_get_readyState(This,pReadyState)	\
    (This)->lpVtbl -> get_readyState(This,pReadyState)

#define IXSLProcessor_addParameter(This,baseName,parameter,namespaceURI)	\
    (This)->lpVtbl -> addParameter(This,baseName,parameter,namespaceURI)

#define IXSLProcessor_addObject(This,obj,namespaceURI)	\
    (This)->lpVtbl -> addObject(This,obj,namespaceURI)

#define IXSLProcessor_get_stylesheet(This,stylesheet)	\
    (This)->lpVtbl -> get_stylesheet(This,stylesheet)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_put_input_Proxy( 
    IXSLProcessor * This,
    /* [in] */ VARIANT var);


void __RPC_STUB IXSLProcessor_put_input_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_input_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ VARIANT *pVar);


void __RPC_STUB IXSLProcessor_get_input_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_ownerTemplate_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ IXSLTemplate **ppTemplate);


void __RPC_STUB IXSLProcessor_get_ownerTemplate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_setStartMode_Proxy( 
    IXSLProcessor * This,
    /* [in] */ BSTR mode,
    /* [defaultvalue][in] */ BSTR namespaceURI);


void __RPC_STUB IXSLProcessor_setStartMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_startMode_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ BSTR *mode);


void __RPC_STUB IXSLProcessor_get_startMode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_startModeURI_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ BSTR *namespaceURI);


void __RPC_STUB IXSLProcessor_get_startModeURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_put_output_Proxy( 
    IXSLProcessor * This,
    /* [in] */ VARIANT output);


void __RPC_STUB IXSLProcessor_put_output_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_output_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ VARIANT *pOutput);


void __RPC_STUB IXSLProcessor_get_output_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_transform_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ VARIANT_BOOL *pDone);


void __RPC_STUB IXSLProcessor_transform_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_reset_Proxy( 
    IXSLProcessor * This);


void __RPC_STUB IXSLProcessor_reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_readyState_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ long *pReadyState);


void __RPC_STUB IXSLProcessor_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_addParameter_Proxy( 
    IXSLProcessor * This,
    /* [in] */ BSTR baseName,
    /* [in] */ VARIANT parameter,
    /* [defaultvalue][in] */ BSTR namespaceURI);


void __RPC_STUB IXSLProcessor_addParameter_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_addObject_Proxy( 
    IXSLProcessor * This,
    /* [in] */ IDispatch *obj,
    /* [in] */ BSTR namespaceURI);


void __RPC_STUB IXSLProcessor_addObject_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXSLProcessor_get_stylesheet_Proxy( 
    IXSLProcessor * This,
    /* [retval][out] */ IXMLDOMNode **stylesheet);


void __RPC_STUB IXSLProcessor_get_stylesheet_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXSLProcessor_INTERFACE_DEFINED__ */


#ifndef __ISAXXMLReader_INTERFACE_DEFINED__
#define __ISAXXMLReader_INTERFACE_DEFINED__

/* interface ISAXXMLReader */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXXMLReader;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("a4f96ed0-f829-476e-81c0-cdc7bd2a0802")
    ISAXXMLReader : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE getFeature( 
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT_BOOL *pvfValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putFeature( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT_BOOL vfValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getProperty( 
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT *pvarValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putProperty( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT varValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getEntityResolver( 
            /* [retval][out] */ ISAXEntityResolver **ppResolver) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putEntityResolver( 
            /* [in] */ ISAXEntityResolver *pResolver) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getContentHandler( 
            /* [retval][out] */ ISAXContentHandler **ppHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putContentHandler( 
            /* [in] */ ISAXContentHandler *pHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getDTDHandler( 
            /* [retval][out] */ ISAXDTDHandler **ppHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putDTDHandler( 
            /* [in] */ ISAXDTDHandler *pHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getErrorHandler( 
            /* [retval][out] */ ISAXErrorHandler **ppHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putErrorHandler( 
            /* [in] */ ISAXErrorHandler *pHandler) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getBaseURL( 
            /* [retval][out] */ const wchar_t **ppwchBaseUrl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putBaseURL( 
            /* [in] */ const wchar_t *pwchBaseUrl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getSecureBaseURL( 
            /* [retval][out] */ const wchar_t **ppwchSecureBaseUrl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putSecureBaseURL( 
            /* [in] */ const wchar_t *pwchSecureBaseUrl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE parse( 
            /* [in] */ VARIANT varInput) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE parseURL( 
            /* [in] */ const wchar_t *pwchUrl) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXXMLReaderVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXXMLReader * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXXMLReader * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXXMLReader * This);
        
        HRESULT ( STDMETHODCALLTYPE *getFeature )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT_BOOL *pvfValue);
        
        HRESULT ( STDMETHODCALLTYPE *putFeature )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT_BOOL vfValue);
        
        HRESULT ( STDMETHODCALLTYPE *getProperty )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT *pvarValue);
        
        HRESULT ( STDMETHODCALLTYPE *putProperty )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT varValue);
        
        HRESULT ( STDMETHODCALLTYPE *getEntityResolver )( 
            ISAXXMLReader * This,
            /* [retval][out] */ ISAXEntityResolver **ppResolver);
        
        HRESULT ( STDMETHODCALLTYPE *putEntityResolver )( 
            ISAXXMLReader * This,
            /* [in] */ ISAXEntityResolver *pResolver);
        
        HRESULT ( STDMETHODCALLTYPE *getContentHandler )( 
            ISAXXMLReader * This,
            /* [retval][out] */ ISAXContentHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putContentHandler )( 
            ISAXXMLReader * This,
            /* [in] */ ISAXContentHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getDTDHandler )( 
            ISAXXMLReader * This,
            /* [retval][out] */ ISAXDTDHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putDTDHandler )( 
            ISAXXMLReader * This,
            /* [in] */ ISAXDTDHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getErrorHandler )( 
            ISAXXMLReader * This,
            /* [retval][out] */ ISAXErrorHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putErrorHandler )( 
            ISAXXMLReader * This,
            /* [in] */ ISAXErrorHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getBaseURL )( 
            ISAXXMLReader * This,
            /* [retval][out] */ const wchar_t **ppwchBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *putBaseURL )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *getSecureBaseURL )( 
            ISAXXMLReader * This,
            /* [retval][out] */ const wchar_t **ppwchSecureBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *putSecureBaseURL )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchSecureBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *parse )( 
            ISAXXMLReader * This,
            /* [in] */ VARIANT varInput);
        
        HRESULT ( STDMETHODCALLTYPE *parseURL )( 
            ISAXXMLReader * This,
            /* [in] */ const wchar_t *pwchUrl);
        
        END_INTERFACE
    } ISAXXMLReaderVtbl;

    interface ISAXXMLReader
    {
        CONST_VTBL struct ISAXXMLReaderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXXMLReader_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXXMLReader_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXXMLReader_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXXMLReader_getFeature(This,pwchName,pvfValue)	\
    (This)->lpVtbl -> getFeature(This,pwchName,pvfValue)

#define ISAXXMLReader_putFeature(This,pwchName,vfValue)	\
    (This)->lpVtbl -> putFeature(This,pwchName,vfValue)

#define ISAXXMLReader_getProperty(This,pwchName,pvarValue)	\
    (This)->lpVtbl -> getProperty(This,pwchName,pvarValue)

#define ISAXXMLReader_putProperty(This,pwchName,varValue)	\
    (This)->lpVtbl -> putProperty(This,pwchName,varValue)

#define ISAXXMLReader_getEntityResolver(This,ppResolver)	\
    (This)->lpVtbl -> getEntityResolver(This,ppResolver)

#define ISAXXMLReader_putEntityResolver(This,pResolver)	\
    (This)->lpVtbl -> putEntityResolver(This,pResolver)

#define ISAXXMLReader_getContentHandler(This,ppHandler)	\
    (This)->lpVtbl -> getContentHandler(This,ppHandler)

#define ISAXXMLReader_putContentHandler(This,pHandler)	\
    (This)->lpVtbl -> putContentHandler(This,pHandler)

#define ISAXXMLReader_getDTDHandler(This,ppHandler)	\
    (This)->lpVtbl -> getDTDHandler(This,ppHandler)

#define ISAXXMLReader_putDTDHandler(This,pHandler)	\
    (This)->lpVtbl -> putDTDHandler(This,pHandler)

#define ISAXXMLReader_getErrorHandler(This,ppHandler)	\
    (This)->lpVtbl -> getErrorHandler(This,ppHandler)

#define ISAXXMLReader_putErrorHandler(This,pHandler)	\
    (This)->lpVtbl -> putErrorHandler(This,pHandler)

#define ISAXXMLReader_getBaseURL(This,ppwchBaseUrl)	\
    (This)->lpVtbl -> getBaseURL(This,ppwchBaseUrl)

#define ISAXXMLReader_putBaseURL(This,pwchBaseUrl)	\
    (This)->lpVtbl -> putBaseURL(This,pwchBaseUrl)

#define ISAXXMLReader_getSecureBaseURL(This,ppwchSecureBaseUrl)	\
    (This)->lpVtbl -> getSecureBaseURL(This,ppwchSecureBaseUrl)

#define ISAXXMLReader_putSecureBaseURL(This,pwchSecureBaseUrl)	\
    (This)->lpVtbl -> putSecureBaseURL(This,pwchSecureBaseUrl)

#define ISAXXMLReader_parse(This,varInput)	\
    (This)->lpVtbl -> parse(This,varInput)

#define ISAXXMLReader_parseURL(This,pwchUrl)	\
    (This)->lpVtbl -> parseURL(This,pwchUrl)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXXMLReader_getFeature_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchName,
    /* [retval][out] */ VARIANT_BOOL *pvfValue);


void __RPC_STUB ISAXXMLReader_getFeature_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putFeature_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ VARIANT_BOOL vfValue);


void __RPC_STUB ISAXXMLReader_putFeature_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getProperty_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchName,
    /* [retval][out] */ VARIANT *pvarValue);


void __RPC_STUB ISAXXMLReader_getProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putProperty_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ VARIANT varValue);


void __RPC_STUB ISAXXMLReader_putProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getEntityResolver_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ ISAXEntityResolver **ppResolver);


void __RPC_STUB ISAXXMLReader_getEntityResolver_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putEntityResolver_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ ISAXEntityResolver *pResolver);


void __RPC_STUB ISAXXMLReader_putEntityResolver_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getContentHandler_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ ISAXContentHandler **ppHandler);


void __RPC_STUB ISAXXMLReader_getContentHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putContentHandler_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ ISAXContentHandler *pHandler);


void __RPC_STUB ISAXXMLReader_putContentHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getDTDHandler_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ ISAXDTDHandler **ppHandler);


void __RPC_STUB ISAXXMLReader_getDTDHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putDTDHandler_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ ISAXDTDHandler *pHandler);


void __RPC_STUB ISAXXMLReader_putDTDHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getErrorHandler_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ ISAXErrorHandler **ppHandler);


void __RPC_STUB ISAXXMLReader_getErrorHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putErrorHandler_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ ISAXErrorHandler *pHandler);


void __RPC_STUB ISAXXMLReader_putErrorHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getBaseURL_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ const wchar_t **ppwchBaseUrl);


void __RPC_STUB ISAXXMLReader_getBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putBaseURL_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchBaseUrl);


void __RPC_STUB ISAXXMLReader_putBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_getSecureBaseURL_Proxy( 
    ISAXXMLReader * This,
    /* [retval][out] */ const wchar_t **ppwchSecureBaseUrl);


void __RPC_STUB ISAXXMLReader_getSecureBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_putSecureBaseURL_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchSecureBaseUrl);


void __RPC_STUB ISAXXMLReader_putSecureBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_parse_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ VARIANT varInput);


void __RPC_STUB ISAXXMLReader_parse_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLReader_parseURL_Proxy( 
    ISAXXMLReader * This,
    /* [in] */ const wchar_t *pwchUrl);


void __RPC_STUB ISAXXMLReader_parseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXXMLReader_INTERFACE_DEFINED__ */


#ifndef __ISAXXMLFilter_INTERFACE_DEFINED__
#define __ISAXXMLFilter_INTERFACE_DEFINED__

/* interface ISAXXMLFilter */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXXMLFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("70409222-ca09-4475-acb8-40312fe8d145")
    ISAXXMLFilter : public ISAXXMLReader
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE getParent( 
            /* [retval][out] */ ISAXXMLReader **ppReader) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE putParent( 
            /* [in] */ ISAXXMLReader *pReader) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXXMLFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXXMLFilter * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXXMLFilter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXXMLFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *getFeature )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT_BOOL *pvfValue);
        
        HRESULT ( STDMETHODCALLTYPE *putFeature )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT_BOOL vfValue);
        
        HRESULT ( STDMETHODCALLTYPE *getProperty )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchName,
            /* [retval][out] */ VARIANT *pvarValue);
        
        HRESULT ( STDMETHODCALLTYPE *putProperty )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ VARIANT varValue);
        
        HRESULT ( STDMETHODCALLTYPE *getEntityResolver )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ ISAXEntityResolver **ppResolver);
        
        HRESULT ( STDMETHODCALLTYPE *putEntityResolver )( 
            ISAXXMLFilter * This,
            /* [in] */ ISAXEntityResolver *pResolver);
        
        HRESULT ( STDMETHODCALLTYPE *getContentHandler )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ ISAXContentHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putContentHandler )( 
            ISAXXMLFilter * This,
            /* [in] */ ISAXContentHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getDTDHandler )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ ISAXDTDHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putDTDHandler )( 
            ISAXXMLFilter * This,
            /* [in] */ ISAXDTDHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getErrorHandler )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ ISAXErrorHandler **ppHandler);
        
        HRESULT ( STDMETHODCALLTYPE *putErrorHandler )( 
            ISAXXMLFilter * This,
            /* [in] */ ISAXErrorHandler *pHandler);
        
        HRESULT ( STDMETHODCALLTYPE *getBaseURL )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ const wchar_t **ppwchBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *putBaseURL )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *getSecureBaseURL )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ const wchar_t **ppwchSecureBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *putSecureBaseURL )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchSecureBaseUrl);
        
        HRESULT ( STDMETHODCALLTYPE *parse )( 
            ISAXXMLFilter * This,
            /* [in] */ VARIANT varInput);
        
        HRESULT ( STDMETHODCALLTYPE *parseURL )( 
            ISAXXMLFilter * This,
            /* [in] */ const wchar_t *pwchUrl);
        
        HRESULT ( STDMETHODCALLTYPE *getParent )( 
            ISAXXMLFilter * This,
            /* [retval][out] */ ISAXXMLReader **ppReader);
        
        HRESULT ( STDMETHODCALLTYPE *putParent )( 
            ISAXXMLFilter * This,
            /* [in] */ ISAXXMLReader *pReader);
        
        END_INTERFACE
    } ISAXXMLFilterVtbl;

    interface ISAXXMLFilter
    {
        CONST_VTBL struct ISAXXMLFilterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXXMLFilter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXXMLFilter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXXMLFilter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXXMLFilter_getFeature(This,pwchName,pvfValue)	\
    (This)->lpVtbl -> getFeature(This,pwchName,pvfValue)

#define ISAXXMLFilter_putFeature(This,pwchName,vfValue)	\
    (This)->lpVtbl -> putFeature(This,pwchName,vfValue)

#define ISAXXMLFilter_getProperty(This,pwchName,pvarValue)	\
    (This)->lpVtbl -> getProperty(This,pwchName,pvarValue)

#define ISAXXMLFilter_putProperty(This,pwchName,varValue)	\
    (This)->lpVtbl -> putProperty(This,pwchName,varValue)

#define ISAXXMLFilter_getEntityResolver(This,ppResolver)	\
    (This)->lpVtbl -> getEntityResolver(This,ppResolver)

#define ISAXXMLFilter_putEntityResolver(This,pResolver)	\
    (This)->lpVtbl -> putEntityResolver(This,pResolver)

#define ISAXXMLFilter_getContentHandler(This,ppHandler)	\
    (This)->lpVtbl -> getContentHandler(This,ppHandler)

#define ISAXXMLFilter_putContentHandler(This,pHandler)	\
    (This)->lpVtbl -> putContentHandler(This,pHandler)

#define ISAXXMLFilter_getDTDHandler(This,ppHandler)	\
    (This)->lpVtbl -> getDTDHandler(This,ppHandler)

#define ISAXXMLFilter_putDTDHandler(This,pHandler)	\
    (This)->lpVtbl -> putDTDHandler(This,pHandler)

#define ISAXXMLFilter_getErrorHandler(This,ppHandler)	\
    (This)->lpVtbl -> getErrorHandler(This,ppHandler)

#define ISAXXMLFilter_putErrorHandler(This,pHandler)	\
    (This)->lpVtbl -> putErrorHandler(This,pHandler)

#define ISAXXMLFilter_getBaseURL(This,ppwchBaseUrl)	\
    (This)->lpVtbl -> getBaseURL(This,ppwchBaseUrl)

#define ISAXXMLFilter_putBaseURL(This,pwchBaseUrl)	\
    (This)->lpVtbl -> putBaseURL(This,pwchBaseUrl)

#define ISAXXMLFilter_getSecureBaseURL(This,ppwchSecureBaseUrl)	\
    (This)->lpVtbl -> getSecureBaseURL(This,ppwchSecureBaseUrl)

#define ISAXXMLFilter_putSecureBaseURL(This,pwchSecureBaseUrl)	\
    (This)->lpVtbl -> putSecureBaseURL(This,pwchSecureBaseUrl)

#define ISAXXMLFilter_parse(This,varInput)	\
    (This)->lpVtbl -> parse(This,varInput)

#define ISAXXMLFilter_parseURL(This,pwchUrl)	\
    (This)->lpVtbl -> parseURL(This,pwchUrl)


#define ISAXXMLFilter_getParent(This,ppReader)	\
    (This)->lpVtbl -> getParent(This,ppReader)

#define ISAXXMLFilter_putParent(This,pReader)	\
    (This)->lpVtbl -> putParent(This,pReader)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXXMLFilter_getParent_Proxy( 
    ISAXXMLFilter * This,
    /* [retval][out] */ ISAXXMLReader **ppReader);


void __RPC_STUB ISAXXMLFilter_getParent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXXMLFilter_putParent_Proxy( 
    ISAXXMLFilter * This,
    /* [in] */ ISAXXMLReader *pReader);


void __RPC_STUB ISAXXMLFilter_putParent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXXMLFilter_INTERFACE_DEFINED__ */


#ifndef __ISAXLocator_INTERFACE_DEFINED__
#define __ISAXLocator_INTERFACE_DEFINED__

/* interface ISAXLocator */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXLocator;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("9b7e472a-0de4-4640-bff3-84d38a051c31")
    ISAXLocator : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE getColumnNumber( 
            /* [retval][out] */ int *pnColumn) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getLineNumber( 
            /* [retval][out] */ int *pnLine) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getPublicId( 
            /* [retval][out] */ const wchar_t **ppwchPublicId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getSystemId( 
            /* [retval][out] */ const wchar_t **ppwchSystemId) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXLocatorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXLocator * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXLocator * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXLocator * This);
        
        HRESULT ( STDMETHODCALLTYPE *getColumnNumber )( 
            ISAXLocator * This,
            /* [retval][out] */ int *pnColumn);
        
        HRESULT ( STDMETHODCALLTYPE *getLineNumber )( 
            ISAXLocator * This,
            /* [retval][out] */ int *pnLine);
        
        HRESULT ( STDMETHODCALLTYPE *getPublicId )( 
            ISAXLocator * This,
            /* [retval][out] */ const wchar_t **ppwchPublicId);
        
        HRESULT ( STDMETHODCALLTYPE *getSystemId )( 
            ISAXLocator * This,
            /* [retval][out] */ const wchar_t **ppwchSystemId);
        
        END_INTERFACE
    } ISAXLocatorVtbl;

    interface ISAXLocator
    {
        CONST_VTBL struct ISAXLocatorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXLocator_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXLocator_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXLocator_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXLocator_getColumnNumber(This,pnColumn)	\
    (This)->lpVtbl -> getColumnNumber(This,pnColumn)

#define ISAXLocator_getLineNumber(This,pnLine)	\
    (This)->lpVtbl -> getLineNumber(This,pnLine)

#define ISAXLocator_getPublicId(This,ppwchPublicId)	\
    (This)->lpVtbl -> getPublicId(This,ppwchPublicId)

#define ISAXLocator_getSystemId(This,ppwchSystemId)	\
    (This)->lpVtbl -> getSystemId(This,ppwchSystemId)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXLocator_getColumnNumber_Proxy( 
    ISAXLocator * This,
    /* [retval][out] */ int *pnColumn);


void __RPC_STUB ISAXLocator_getColumnNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLocator_getLineNumber_Proxy( 
    ISAXLocator * This,
    /* [retval][out] */ int *pnLine);


void __RPC_STUB ISAXLocator_getLineNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLocator_getPublicId_Proxy( 
    ISAXLocator * This,
    /* [retval][out] */ const wchar_t **ppwchPublicId);


void __RPC_STUB ISAXLocator_getPublicId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLocator_getSystemId_Proxy( 
    ISAXLocator * This,
    /* [retval][out] */ const wchar_t **ppwchSystemId);


void __RPC_STUB ISAXLocator_getSystemId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXLocator_INTERFACE_DEFINED__ */


#ifndef __ISAXEntityResolver_INTERFACE_DEFINED__
#define __ISAXEntityResolver_INTERFACE_DEFINED__

/* interface ISAXEntityResolver */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXEntityResolver;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("99bca7bd-e8c4-4d5f-a0cf-6d907901ff07")
    ISAXEntityResolver : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE resolveEntity( 
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [retval][out] */ VARIANT *pvarInput) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXEntityResolverVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXEntityResolver * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXEntityResolver * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXEntityResolver * This);
        
        HRESULT ( STDMETHODCALLTYPE *resolveEntity )( 
            ISAXEntityResolver * This,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [retval][out] */ VARIANT *pvarInput);
        
        END_INTERFACE
    } ISAXEntityResolverVtbl;

    interface ISAXEntityResolver
    {
        CONST_VTBL struct ISAXEntityResolverVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXEntityResolver_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXEntityResolver_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXEntityResolver_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXEntityResolver_resolveEntity(This,pwchPublicId,pwchSystemId,pvarInput)	\
    (This)->lpVtbl -> resolveEntity(This,pwchPublicId,pwchSystemId,pvarInput)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXEntityResolver_resolveEntity_Proxy( 
    ISAXEntityResolver * This,
    /* [in] */ const wchar_t *pwchPublicId,
    /* [in] */ const wchar_t *pwchSystemId,
    /* [retval][out] */ VARIANT *pvarInput);


void __RPC_STUB ISAXEntityResolver_resolveEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXEntityResolver_INTERFACE_DEFINED__ */


#ifndef __ISAXContentHandler_INTERFACE_DEFINED__
#define __ISAXContentHandler_INTERFACE_DEFINED__

/* interface ISAXContentHandler */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXContentHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1545cdfa-9e4e-4497-a8a4-2bf7d0112c44")
    ISAXContentHandler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE putDocumentLocator( 
            /* [in] */ ISAXLocator *pLocator) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE startDocument( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endDocument( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE startPrefixMapping( 
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ int cchPrefix,
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endPrefixMapping( 
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ int cchPrefix) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE startElement( 
            /* [in] */ const wchar_t *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [in] */ ISAXAttributes *pAttributes) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endElement( 
            /* [in] */ const wchar_t *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE characters( 
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE processingInstruction( 
            /* [in] */ const wchar_t *pwchTarget,
            /* [in] */ int cchTarget,
            /* [in] */ const wchar_t *pwchData,
            /* [in] */ int cchData) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE skippedEntity( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXContentHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXContentHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXContentHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXContentHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *putDocumentLocator )( 
            ISAXContentHandler * This,
            /* [in] */ ISAXLocator *pLocator);
        
        HRESULT ( STDMETHODCALLTYPE *startDocument )( 
            ISAXContentHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *endDocument )( 
            ISAXContentHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *startPrefixMapping )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ int cchPrefix,
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri);
        
        HRESULT ( STDMETHODCALLTYPE *endPrefixMapping )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ int cchPrefix);
        
        HRESULT ( STDMETHODCALLTYPE *startElement )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [in] */ ISAXAttributes *pAttributes);
        
        HRESULT ( STDMETHODCALLTYPE *endElement )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchNamespaceUri,
            /* [in] */ int cchNamespaceUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName);
        
        HRESULT ( STDMETHODCALLTYPE *characters )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars);
        
        HRESULT ( STDMETHODCALLTYPE *ignorableWhitespace )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars);
        
        HRESULT ( STDMETHODCALLTYPE *processingInstruction )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchTarget,
            /* [in] */ int cchTarget,
            /* [in] */ const wchar_t *pwchData,
            /* [in] */ int cchData);
        
        HRESULT ( STDMETHODCALLTYPE *skippedEntity )( 
            ISAXContentHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName);
        
        END_INTERFACE
    } ISAXContentHandlerVtbl;

    interface ISAXContentHandler
    {
        CONST_VTBL struct ISAXContentHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXContentHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXContentHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXContentHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXContentHandler_putDocumentLocator(This,pLocator)	\
    (This)->lpVtbl -> putDocumentLocator(This,pLocator)

#define ISAXContentHandler_startDocument(This)	\
    (This)->lpVtbl -> startDocument(This)

#define ISAXContentHandler_endDocument(This)	\
    (This)->lpVtbl -> endDocument(This)

#define ISAXContentHandler_startPrefixMapping(This,pwchPrefix,cchPrefix,pwchUri,cchUri)	\
    (This)->lpVtbl -> startPrefixMapping(This,pwchPrefix,cchPrefix,pwchUri,cchUri)

#define ISAXContentHandler_endPrefixMapping(This,pwchPrefix,cchPrefix)	\
    (This)->lpVtbl -> endPrefixMapping(This,pwchPrefix,cchPrefix)

#define ISAXContentHandler_startElement(This,pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName,pAttributes)	\
    (This)->lpVtbl -> startElement(This,pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName,pAttributes)

#define ISAXContentHandler_endElement(This,pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName)	\
    (This)->lpVtbl -> endElement(This,pwchNamespaceUri,cchNamespaceUri,pwchLocalName,cchLocalName,pwchQName,cchQName)

#define ISAXContentHandler_characters(This,pwchChars,cchChars)	\
    (This)->lpVtbl -> characters(This,pwchChars,cchChars)

#define ISAXContentHandler_ignorableWhitespace(This,pwchChars,cchChars)	\
    (This)->lpVtbl -> ignorableWhitespace(This,pwchChars,cchChars)

#define ISAXContentHandler_processingInstruction(This,pwchTarget,cchTarget,pwchData,cchData)	\
    (This)->lpVtbl -> processingInstruction(This,pwchTarget,cchTarget,pwchData,cchData)

#define ISAXContentHandler_skippedEntity(This,pwchName,cchName)	\
    (This)->lpVtbl -> skippedEntity(This,pwchName,cchName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXContentHandler_putDocumentLocator_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ ISAXLocator *pLocator);


void __RPC_STUB ISAXContentHandler_putDocumentLocator_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_startDocument_Proxy( 
    ISAXContentHandler * This);


void __RPC_STUB ISAXContentHandler_startDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_endDocument_Proxy( 
    ISAXContentHandler * This);


void __RPC_STUB ISAXContentHandler_endDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_startPrefixMapping_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchPrefix,
    /* [in] */ int cchPrefix,
    /* [in] */ const wchar_t *pwchUri,
    /* [in] */ int cchUri);


void __RPC_STUB ISAXContentHandler_startPrefixMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_endPrefixMapping_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchPrefix,
    /* [in] */ int cchPrefix);


void __RPC_STUB ISAXContentHandler_endPrefixMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_startElement_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes *pAttributes);


void __RPC_STUB ISAXContentHandler_startElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_endElement_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName);


void __RPC_STUB ISAXContentHandler_endElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_characters_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchChars,
    /* [in] */ int cchChars);


void __RPC_STUB ISAXContentHandler_characters_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_ignorableWhitespace_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchChars,
    /* [in] */ int cchChars);


void __RPC_STUB ISAXContentHandler_ignorableWhitespace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_processingInstruction_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchTarget,
    /* [in] */ int cchTarget,
    /* [in] */ const wchar_t *pwchData,
    /* [in] */ int cchData);


void __RPC_STUB ISAXContentHandler_processingInstruction_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXContentHandler_skippedEntity_Proxy( 
    ISAXContentHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName);


void __RPC_STUB ISAXContentHandler_skippedEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXContentHandler_INTERFACE_DEFINED__ */


#ifndef __ISAXDTDHandler_INTERFACE_DEFINED__
#define __ISAXDTDHandler_INTERFACE_DEFINED__

/* interface ISAXDTDHandler */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXDTDHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("e15c1baf-afb3-4d60-8c36-19a8c45defed")
    ISAXDTDHandler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE notationDecl( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE unparsedEntityDecl( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId,
            /* [in] */ const wchar_t *pwchNotationName,
            /* [in] */ int cchNotationName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXDTDHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXDTDHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXDTDHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXDTDHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *notationDecl )( 
            ISAXDTDHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId);
        
        HRESULT ( STDMETHODCALLTYPE *unparsedEntityDecl )( 
            ISAXDTDHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId,
            /* [in] */ const wchar_t *pwchNotationName,
            /* [in] */ int cchNotationName);
        
        END_INTERFACE
    } ISAXDTDHandlerVtbl;

    interface ISAXDTDHandler
    {
        CONST_VTBL struct ISAXDTDHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXDTDHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXDTDHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXDTDHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXDTDHandler_notationDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)	\
    (This)->lpVtbl -> notationDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)

#define ISAXDTDHandler_unparsedEntityDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId,pwchNotationName,cchNotationName)	\
    (This)->lpVtbl -> unparsedEntityDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId,pwchNotationName,cchNotationName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXDTDHandler_notationDecl_Proxy( 
    ISAXDTDHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchPublicId,
    /* [in] */ int cchPublicId,
    /* [in] */ const wchar_t *pwchSystemId,
    /* [in] */ int cchSystemId);


void __RPC_STUB ISAXDTDHandler_notationDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXDTDHandler_unparsedEntityDecl_Proxy( 
    ISAXDTDHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchPublicId,
    /* [in] */ int cchPublicId,
    /* [in] */ const wchar_t *pwchSystemId,
    /* [in] */ int cchSystemId,
    /* [in] */ const wchar_t *pwchNotationName,
    /* [in] */ int cchNotationName);


void __RPC_STUB ISAXDTDHandler_unparsedEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXDTDHandler_INTERFACE_DEFINED__ */


#ifndef __ISAXErrorHandler_INTERFACE_DEFINED__
#define __ISAXErrorHandler_INTERFACE_DEFINED__

/* interface ISAXErrorHandler */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXErrorHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("a60511c4-ccf5-479e-98a3-dc8dc545b7d0")
    ISAXErrorHandler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE error( 
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE fatalError( 
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ignorableWarning( 
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXErrorHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXErrorHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXErrorHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXErrorHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *error )( 
            ISAXErrorHandler * This,
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode);
        
        HRESULT ( STDMETHODCALLTYPE *fatalError )( 
            ISAXErrorHandler * This,
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode);
        
        HRESULT ( STDMETHODCALLTYPE *ignorableWarning )( 
            ISAXErrorHandler * This,
            /* [in] */ ISAXLocator *pLocator,
            /* [in] */ const wchar_t *pwchErrorMessage,
            /* [in] */ HRESULT hrErrorCode);
        
        END_INTERFACE
    } ISAXErrorHandlerVtbl;

    interface ISAXErrorHandler
    {
        CONST_VTBL struct ISAXErrorHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXErrorHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXErrorHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXErrorHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXErrorHandler_error(This,pLocator,pwchErrorMessage,hrErrorCode)	\
    (This)->lpVtbl -> error(This,pLocator,pwchErrorMessage,hrErrorCode)

#define ISAXErrorHandler_fatalError(This,pLocator,pwchErrorMessage,hrErrorCode)	\
    (This)->lpVtbl -> fatalError(This,pLocator,pwchErrorMessage,hrErrorCode)

#define ISAXErrorHandler_ignorableWarning(This,pLocator,pwchErrorMessage,hrErrorCode)	\
    (This)->lpVtbl -> ignorableWarning(This,pLocator,pwchErrorMessage,hrErrorCode)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXErrorHandler_error_Proxy( 
    ISAXErrorHandler * This,
    /* [in] */ ISAXLocator *pLocator,
    /* [in] */ const wchar_t *pwchErrorMessage,
    /* [in] */ HRESULT hrErrorCode);


void __RPC_STUB ISAXErrorHandler_error_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXErrorHandler_fatalError_Proxy( 
    ISAXErrorHandler * This,
    /* [in] */ ISAXLocator *pLocator,
    /* [in] */ const wchar_t *pwchErrorMessage,
    /* [in] */ HRESULT hrErrorCode);


void __RPC_STUB ISAXErrorHandler_fatalError_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXErrorHandler_ignorableWarning_Proxy( 
    ISAXErrorHandler * This,
    /* [in] */ ISAXLocator *pLocator,
    /* [in] */ const wchar_t *pwchErrorMessage,
    /* [in] */ HRESULT hrErrorCode);


void __RPC_STUB ISAXErrorHandler_ignorableWarning_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXErrorHandler_INTERFACE_DEFINED__ */


#ifndef __ISAXLexicalHandler_INTERFACE_DEFINED__
#define __ISAXLexicalHandler_INTERFACE_DEFINED__

/* interface ISAXLexicalHandler */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXLexicalHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7f85d5f5-47a8-4497-bda5-84ba04819ea6")
    ISAXLexicalHandler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE startDTD( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endDTD( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE startEntity( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endEntity( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE startCDATA( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE endCDATA( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE comment( 
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXLexicalHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXLexicalHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXLexicalHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXLexicalHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *startDTD )( 
            ISAXLexicalHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId);
        
        HRESULT ( STDMETHODCALLTYPE *endDTD )( 
            ISAXLexicalHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *startEntity )( 
            ISAXLexicalHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName);
        
        HRESULT ( STDMETHODCALLTYPE *endEntity )( 
            ISAXLexicalHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName);
        
        HRESULT ( STDMETHODCALLTYPE *startCDATA )( 
            ISAXLexicalHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *endCDATA )( 
            ISAXLexicalHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *comment )( 
            ISAXLexicalHandler * This,
            /* [in] */ const wchar_t *pwchChars,
            /* [in] */ int cchChars);
        
        END_INTERFACE
    } ISAXLexicalHandlerVtbl;

    interface ISAXLexicalHandler
    {
        CONST_VTBL struct ISAXLexicalHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXLexicalHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXLexicalHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXLexicalHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXLexicalHandler_startDTD(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)	\
    (This)->lpVtbl -> startDTD(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)

#define ISAXLexicalHandler_endDTD(This)	\
    (This)->lpVtbl -> endDTD(This)

#define ISAXLexicalHandler_startEntity(This,pwchName,cchName)	\
    (This)->lpVtbl -> startEntity(This,pwchName,cchName)

#define ISAXLexicalHandler_endEntity(This,pwchName,cchName)	\
    (This)->lpVtbl -> endEntity(This,pwchName,cchName)

#define ISAXLexicalHandler_startCDATA(This)	\
    (This)->lpVtbl -> startCDATA(This)

#define ISAXLexicalHandler_endCDATA(This)	\
    (This)->lpVtbl -> endCDATA(This)

#define ISAXLexicalHandler_comment(This,pwchChars,cchChars)	\
    (This)->lpVtbl -> comment(This,pwchChars,cchChars)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_startDTD_Proxy( 
    ISAXLexicalHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchPublicId,
    /* [in] */ int cchPublicId,
    /* [in] */ const wchar_t *pwchSystemId,
    /* [in] */ int cchSystemId);


void __RPC_STUB ISAXLexicalHandler_startDTD_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_endDTD_Proxy( 
    ISAXLexicalHandler * This);


void __RPC_STUB ISAXLexicalHandler_endDTD_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_startEntity_Proxy( 
    ISAXLexicalHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName);


void __RPC_STUB ISAXLexicalHandler_startEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_endEntity_Proxy( 
    ISAXLexicalHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName);


void __RPC_STUB ISAXLexicalHandler_endEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_startCDATA_Proxy( 
    ISAXLexicalHandler * This);


void __RPC_STUB ISAXLexicalHandler_startCDATA_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_endCDATA_Proxy( 
    ISAXLexicalHandler * This);


void __RPC_STUB ISAXLexicalHandler_endCDATA_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXLexicalHandler_comment_Proxy( 
    ISAXLexicalHandler * This,
    /* [in] */ const wchar_t *pwchChars,
    /* [in] */ int cchChars);


void __RPC_STUB ISAXLexicalHandler_comment_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXLexicalHandler_INTERFACE_DEFINED__ */


#ifndef __ISAXDeclHandler_INTERFACE_DEFINED__
#define __ISAXDeclHandler_INTERFACE_DEFINED__

/* interface ISAXDeclHandler */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXDeclHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("862629ac-771a-47b2-8337-4e6843c1be90")
    ISAXDeclHandler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE elementDecl( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchModel,
            /* [in] */ int cchModel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE attributeDecl( 
            /* [in] */ const wchar_t *pwchElementName,
            /* [in] */ int cchElementName,
            /* [in] */ const wchar_t *pwchAttributeName,
            /* [in] */ int cchAttributeName,
            /* [in] */ const wchar_t *pwchType,
            /* [in] */ int cchType,
            /* [in] */ const wchar_t *pwchValueDefault,
            /* [in] */ int cchValueDefault,
            /* [in] */ const wchar_t *pwchValue,
            /* [in] */ int cchValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE internalEntityDecl( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchValue,
            /* [in] */ int cchValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE externalEntityDecl( 
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXDeclHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXDeclHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXDeclHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXDeclHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *elementDecl )( 
            ISAXDeclHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchModel,
            /* [in] */ int cchModel);
        
        HRESULT ( STDMETHODCALLTYPE *attributeDecl )( 
            ISAXDeclHandler * This,
            /* [in] */ const wchar_t *pwchElementName,
            /* [in] */ int cchElementName,
            /* [in] */ const wchar_t *pwchAttributeName,
            /* [in] */ int cchAttributeName,
            /* [in] */ const wchar_t *pwchType,
            /* [in] */ int cchType,
            /* [in] */ const wchar_t *pwchValueDefault,
            /* [in] */ int cchValueDefault,
            /* [in] */ const wchar_t *pwchValue,
            /* [in] */ int cchValue);
        
        HRESULT ( STDMETHODCALLTYPE *internalEntityDecl )( 
            ISAXDeclHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchValue,
            /* [in] */ int cchValue);
        
        HRESULT ( STDMETHODCALLTYPE *externalEntityDecl )( 
            ISAXDeclHandler * This,
            /* [in] */ const wchar_t *pwchName,
            /* [in] */ int cchName,
            /* [in] */ const wchar_t *pwchPublicId,
            /* [in] */ int cchPublicId,
            /* [in] */ const wchar_t *pwchSystemId,
            /* [in] */ int cchSystemId);
        
        END_INTERFACE
    } ISAXDeclHandlerVtbl;

    interface ISAXDeclHandler
    {
        CONST_VTBL struct ISAXDeclHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXDeclHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXDeclHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXDeclHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXDeclHandler_elementDecl(This,pwchName,cchName,pwchModel,cchModel)	\
    (This)->lpVtbl -> elementDecl(This,pwchName,cchName,pwchModel,cchModel)

#define ISAXDeclHandler_attributeDecl(This,pwchElementName,cchElementName,pwchAttributeName,cchAttributeName,pwchType,cchType,pwchValueDefault,cchValueDefault,pwchValue,cchValue)	\
    (This)->lpVtbl -> attributeDecl(This,pwchElementName,cchElementName,pwchAttributeName,cchAttributeName,pwchType,cchType,pwchValueDefault,cchValueDefault,pwchValue,cchValue)

#define ISAXDeclHandler_internalEntityDecl(This,pwchName,cchName,pwchValue,cchValue)	\
    (This)->lpVtbl -> internalEntityDecl(This,pwchName,cchName,pwchValue,cchValue)

#define ISAXDeclHandler_externalEntityDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)	\
    (This)->lpVtbl -> externalEntityDecl(This,pwchName,cchName,pwchPublicId,cchPublicId,pwchSystemId,cchSystemId)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXDeclHandler_elementDecl_Proxy( 
    ISAXDeclHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchModel,
    /* [in] */ int cchModel);


void __RPC_STUB ISAXDeclHandler_elementDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXDeclHandler_attributeDecl_Proxy( 
    ISAXDeclHandler * This,
    /* [in] */ const wchar_t *pwchElementName,
    /* [in] */ int cchElementName,
    /* [in] */ const wchar_t *pwchAttributeName,
    /* [in] */ int cchAttributeName,
    /* [in] */ const wchar_t *pwchType,
    /* [in] */ int cchType,
    /* [in] */ const wchar_t *pwchValueDefault,
    /* [in] */ int cchValueDefault,
    /* [in] */ const wchar_t *pwchValue,
    /* [in] */ int cchValue);


void __RPC_STUB ISAXDeclHandler_attributeDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXDeclHandler_internalEntityDecl_Proxy( 
    ISAXDeclHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchValue,
    /* [in] */ int cchValue);


void __RPC_STUB ISAXDeclHandler_internalEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXDeclHandler_externalEntityDecl_Proxy( 
    ISAXDeclHandler * This,
    /* [in] */ const wchar_t *pwchName,
    /* [in] */ int cchName,
    /* [in] */ const wchar_t *pwchPublicId,
    /* [in] */ int cchPublicId,
    /* [in] */ const wchar_t *pwchSystemId,
    /* [in] */ int cchSystemId);


void __RPC_STUB ISAXDeclHandler_externalEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXDeclHandler_INTERFACE_DEFINED__ */


#ifndef __ISAXAttributes_INTERFACE_DEFINED__
#define __ISAXAttributes_INTERFACE_DEFINED__

/* interface ISAXAttributes */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_ISAXAttributes;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f078abe1-45d2-4832-91ea-4466ce2f25c9")
    ISAXAttributes : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE getLength( 
            /* [retval][out] */ int *pnLength) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getURI( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchUri,
            /* [out] */ int *pcchUri) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getLocalName( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchLocalName,
            /* [out] */ int *pcchLocalName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getQName( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchQName,
            /* [out] */ int *pcchQName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getName( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchUri,
            /* [out] */ int *pcchUri,
            /* [out] */ const wchar_t **ppwchLocalName,
            /* [out] */ int *pcchLocalName,
            /* [out] */ const wchar_t **ppwchQName,
            /* [out] */ int *pcchQName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getIndexFromName( 
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [retval][out] */ int *pnIndex) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getIndexFromQName( 
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [retval][out] */ int *pnIndex) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getType( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getTypeFromName( 
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getTypeFromQName( 
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getValue( 
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getValueFromName( 
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getValueFromQName( 
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISAXAttributesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISAXAttributes * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISAXAttributes * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISAXAttributes * This);
        
        HRESULT ( STDMETHODCALLTYPE *getLength )( 
            ISAXAttributes * This,
            /* [retval][out] */ int *pnLength);
        
        HRESULT ( STDMETHODCALLTYPE *getURI )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchUri,
            /* [out] */ int *pcchUri);
        
        HRESULT ( STDMETHODCALLTYPE *getLocalName )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchLocalName,
            /* [out] */ int *pcchLocalName);
        
        HRESULT ( STDMETHODCALLTYPE *getQName )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchQName,
            /* [out] */ int *pcchQName);
        
        HRESULT ( STDMETHODCALLTYPE *getName )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchUri,
            /* [out] */ int *pcchUri,
            /* [out] */ const wchar_t **ppwchLocalName,
            /* [out] */ int *pcchLocalName,
            /* [out] */ const wchar_t **ppwchQName,
            /* [out] */ int *pcchQName);
        
        HRESULT ( STDMETHODCALLTYPE *getIndexFromName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [retval][out] */ int *pnIndex);
        
        HRESULT ( STDMETHODCALLTYPE *getIndexFromQName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [retval][out] */ int *pnIndex);
        
        HRESULT ( STDMETHODCALLTYPE *getType )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType);
        
        HRESULT ( STDMETHODCALLTYPE *getTypeFromName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType);
        
        HRESULT ( STDMETHODCALLTYPE *getTypeFromQName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [out] */ const wchar_t **ppwchType,
            /* [out] */ int *pcchType);
        
        HRESULT ( STDMETHODCALLTYPE *getValue )( 
            ISAXAttributes * This,
            /* [in] */ int nIndex,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue);
        
        HRESULT ( STDMETHODCALLTYPE *getValueFromName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchUri,
            /* [in] */ int cchUri,
            /* [in] */ const wchar_t *pwchLocalName,
            /* [in] */ int cchLocalName,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue);
        
        HRESULT ( STDMETHODCALLTYPE *getValueFromQName )( 
            ISAXAttributes * This,
            /* [in] */ const wchar_t *pwchQName,
            /* [in] */ int cchQName,
            /* [out] */ const wchar_t **ppwchValue,
            /* [out] */ int *pcchValue);
        
        END_INTERFACE
    } ISAXAttributesVtbl;

    interface ISAXAttributes
    {
        CONST_VTBL struct ISAXAttributesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISAXAttributes_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISAXAttributes_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISAXAttributes_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISAXAttributes_getLength(This,pnLength)	\
    (This)->lpVtbl -> getLength(This,pnLength)

#define ISAXAttributes_getURI(This,nIndex,ppwchUri,pcchUri)	\
    (This)->lpVtbl -> getURI(This,nIndex,ppwchUri,pcchUri)

#define ISAXAttributes_getLocalName(This,nIndex,ppwchLocalName,pcchLocalName)	\
    (This)->lpVtbl -> getLocalName(This,nIndex,ppwchLocalName,pcchLocalName)

#define ISAXAttributes_getQName(This,nIndex,ppwchQName,pcchQName)	\
    (This)->lpVtbl -> getQName(This,nIndex,ppwchQName,pcchQName)

#define ISAXAttributes_getName(This,nIndex,ppwchUri,pcchUri,ppwchLocalName,pcchLocalName,ppwchQName,pcchQName)	\
    (This)->lpVtbl -> getName(This,nIndex,ppwchUri,pcchUri,ppwchLocalName,pcchLocalName,ppwchQName,pcchQName)

#define ISAXAttributes_getIndexFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,pnIndex)	\
    (This)->lpVtbl -> getIndexFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,pnIndex)

#define ISAXAttributes_getIndexFromQName(This,pwchQName,cchQName,pnIndex)	\
    (This)->lpVtbl -> getIndexFromQName(This,pwchQName,cchQName,pnIndex)

#define ISAXAttributes_getType(This,nIndex,ppwchType,pcchType)	\
    (This)->lpVtbl -> getType(This,nIndex,ppwchType,pcchType)

#define ISAXAttributes_getTypeFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,ppwchType,pcchType)	\
    (This)->lpVtbl -> getTypeFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,ppwchType,pcchType)

#define ISAXAttributes_getTypeFromQName(This,pwchQName,cchQName,ppwchType,pcchType)	\
    (This)->lpVtbl -> getTypeFromQName(This,pwchQName,cchQName,ppwchType,pcchType)

#define ISAXAttributes_getValue(This,nIndex,ppwchValue,pcchValue)	\
    (This)->lpVtbl -> getValue(This,nIndex,ppwchValue,pcchValue)

#define ISAXAttributes_getValueFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,ppwchValue,pcchValue)	\
    (This)->lpVtbl -> getValueFromName(This,pwchUri,cchUri,pwchLocalName,cchLocalName,ppwchValue,pcchValue)

#define ISAXAttributes_getValueFromQName(This,pwchQName,cchQName,ppwchValue,pcchValue)	\
    (This)->lpVtbl -> getValueFromQName(This,pwchQName,cchQName,ppwchValue,pcchValue)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE ISAXAttributes_getLength_Proxy( 
    ISAXAttributes * This,
    /* [retval][out] */ int *pnLength);


void __RPC_STUB ISAXAttributes_getLength_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getURI_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchUri,
    /* [out] */ int *pcchUri);


void __RPC_STUB ISAXAttributes_getURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getLocalName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchLocalName,
    /* [out] */ int *pcchLocalName);


void __RPC_STUB ISAXAttributes_getLocalName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getQName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchQName,
    /* [out] */ int *pcchQName);


void __RPC_STUB ISAXAttributes_getQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchUri,
    /* [out] */ int *pcchUri,
    /* [out] */ const wchar_t **ppwchLocalName,
    /* [out] */ int *pcchLocalName,
    /* [out] */ const wchar_t **ppwchQName,
    /* [out] */ int *pcchQName);


void __RPC_STUB ISAXAttributes_getName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getIndexFromName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchUri,
    /* [in] */ int cchUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [retval][out] */ int *pnIndex);


void __RPC_STUB ISAXAttributes_getIndexFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getIndexFromQName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName,
    /* [retval][out] */ int *pnIndex);


void __RPC_STUB ISAXAttributes_getIndexFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getType_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchType,
    /* [out] */ int *pcchType);


void __RPC_STUB ISAXAttributes_getType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getTypeFromName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchUri,
    /* [in] */ int cchUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [out] */ const wchar_t **ppwchType,
    /* [out] */ int *pcchType);


void __RPC_STUB ISAXAttributes_getTypeFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getTypeFromQName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName,
    /* [out] */ const wchar_t **ppwchType,
    /* [out] */ int *pcchType);


void __RPC_STUB ISAXAttributes_getTypeFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getValue_Proxy( 
    ISAXAttributes * This,
    /* [in] */ int nIndex,
    /* [out] */ const wchar_t **ppwchValue,
    /* [out] */ int *pcchValue);


void __RPC_STUB ISAXAttributes_getValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getValueFromName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchUri,
    /* [in] */ int cchUri,
    /* [in] */ const wchar_t *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [out] */ const wchar_t **ppwchValue,
    /* [out] */ int *pcchValue);


void __RPC_STUB ISAXAttributes_getValueFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE ISAXAttributes_getValueFromQName_Proxy( 
    ISAXAttributes * This,
    /* [in] */ const wchar_t *pwchQName,
    /* [in] */ int cchQName,
    /* [out] */ const wchar_t **ppwchValue,
    /* [out] */ int *pcchValue);


void __RPC_STUB ISAXAttributes_getValueFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISAXAttributes_INTERFACE_DEFINED__ */


#ifndef __IVBSAXXMLReader_INTERFACE_DEFINED__
#define __IVBSAXXMLReader_INTERFACE_DEFINED__

/* interface IVBSAXXMLReader */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXXMLReader;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8c033caa-6cd6-4f73-b728-4531af74945f")
    IVBSAXXMLReader : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getFeature( 
            /* [in] */ BSTR strName,
            /* [retval][out] */ VARIANT_BOOL *fValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE putFeature( 
            /* [in] */ BSTR strName,
            /* [in] */ VARIANT_BOOL fValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getProperty( 
            /* [in] */ BSTR strName,
            /* [retval][out] */ VARIANT *varValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE putProperty( 
            /* [in] */ BSTR strName,
            /* [in] */ VARIANT varValue) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_entityResolver( 
            /* [retval][out] */ IVBSAXEntityResolver **oResolver) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_entityResolver( 
            /* [in] */ IVBSAXEntityResolver *oResolver) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_contentHandler( 
            /* [retval][out] */ IVBSAXContentHandler **oHandler) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_contentHandler( 
            /* [in] */ IVBSAXContentHandler *oHandler) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_dtdHandler( 
            /* [retval][out] */ IVBSAXDTDHandler **oHandler) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_dtdHandler( 
            /* [in] */ IVBSAXDTDHandler *oHandler) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_errorHandler( 
            /* [retval][out] */ IVBSAXErrorHandler **oHandler) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_errorHandler( 
            /* [in] */ IVBSAXErrorHandler *oHandler) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_baseURL( 
            /* [retval][out] */ BSTR *strBaseURL) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_baseURL( 
            /* [in] */ BSTR strBaseURL) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_secureBaseURL( 
            /* [retval][out] */ BSTR *strSecureBaseURL) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_secureBaseURL( 
            /* [in] */ BSTR strSecureBaseURL) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE parse( 
            /* [in] */ VARIANT varInput) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE parseURL( 
            /* [in] */ BSTR strURL) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXXMLReaderVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXXMLReader * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXXMLReader * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXXMLReader * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXXMLReader * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXXMLReader * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXXMLReader * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXXMLReader * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getFeature )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strName,
            /* [retval][out] */ VARIANT_BOOL *fValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *putFeature )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strName,
            /* [in] */ VARIANT_BOOL fValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getProperty )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strName,
            /* [retval][out] */ VARIANT *varValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *putProperty )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strName,
            /* [in] */ VARIANT varValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_entityResolver )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ IVBSAXEntityResolver **oResolver);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_entityResolver )( 
            IVBSAXXMLReader * This,
            /* [in] */ IVBSAXEntityResolver *oResolver);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_contentHandler )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ IVBSAXContentHandler **oHandler);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_contentHandler )( 
            IVBSAXXMLReader * This,
            /* [in] */ IVBSAXContentHandler *oHandler);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dtdHandler )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ IVBSAXDTDHandler **oHandler);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_dtdHandler )( 
            IVBSAXXMLReader * This,
            /* [in] */ IVBSAXDTDHandler *oHandler);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_errorHandler )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ IVBSAXErrorHandler **oHandler);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_errorHandler )( 
            IVBSAXXMLReader * This,
            /* [in] */ IVBSAXErrorHandler *oHandler);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_baseURL )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ BSTR *strBaseURL);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_baseURL )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strBaseURL);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_secureBaseURL )( 
            IVBSAXXMLReader * This,
            /* [retval][out] */ BSTR *strSecureBaseURL);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_secureBaseURL )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strSecureBaseURL);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *parse )( 
            IVBSAXXMLReader * This,
            /* [in] */ VARIANT varInput);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *parseURL )( 
            IVBSAXXMLReader * This,
            /* [in] */ BSTR strURL);
        
        END_INTERFACE
    } IVBSAXXMLReaderVtbl;

    interface IVBSAXXMLReader
    {
        CONST_VTBL struct IVBSAXXMLReaderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXXMLReader_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXXMLReader_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXXMLReader_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXXMLReader_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXXMLReader_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXXMLReader_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXXMLReader_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXXMLReader_getFeature(This,strName,fValue)	\
    (This)->lpVtbl -> getFeature(This,strName,fValue)

#define IVBSAXXMLReader_putFeature(This,strName,fValue)	\
    (This)->lpVtbl -> putFeature(This,strName,fValue)

#define IVBSAXXMLReader_getProperty(This,strName,varValue)	\
    (This)->lpVtbl -> getProperty(This,strName,varValue)

#define IVBSAXXMLReader_putProperty(This,strName,varValue)	\
    (This)->lpVtbl -> putProperty(This,strName,varValue)

#define IVBSAXXMLReader_get_entityResolver(This,oResolver)	\
    (This)->lpVtbl -> get_entityResolver(This,oResolver)

#define IVBSAXXMLReader_putref_entityResolver(This,oResolver)	\
    (This)->lpVtbl -> putref_entityResolver(This,oResolver)

#define IVBSAXXMLReader_get_contentHandler(This,oHandler)	\
    (This)->lpVtbl -> get_contentHandler(This,oHandler)

#define IVBSAXXMLReader_putref_contentHandler(This,oHandler)	\
    (This)->lpVtbl -> putref_contentHandler(This,oHandler)

#define IVBSAXXMLReader_get_dtdHandler(This,oHandler)	\
    (This)->lpVtbl -> get_dtdHandler(This,oHandler)

#define IVBSAXXMLReader_putref_dtdHandler(This,oHandler)	\
    (This)->lpVtbl -> putref_dtdHandler(This,oHandler)

#define IVBSAXXMLReader_get_errorHandler(This,oHandler)	\
    (This)->lpVtbl -> get_errorHandler(This,oHandler)

#define IVBSAXXMLReader_putref_errorHandler(This,oHandler)	\
    (This)->lpVtbl -> putref_errorHandler(This,oHandler)

#define IVBSAXXMLReader_get_baseURL(This,strBaseURL)	\
    (This)->lpVtbl -> get_baseURL(This,strBaseURL)

#define IVBSAXXMLReader_put_baseURL(This,strBaseURL)	\
    (This)->lpVtbl -> put_baseURL(This,strBaseURL)

#define IVBSAXXMLReader_get_secureBaseURL(This,strSecureBaseURL)	\
    (This)->lpVtbl -> get_secureBaseURL(This,strSecureBaseURL)

#define IVBSAXXMLReader_put_secureBaseURL(This,strSecureBaseURL)	\
    (This)->lpVtbl -> put_secureBaseURL(This,strSecureBaseURL)

#define IVBSAXXMLReader_parse(This,varInput)	\
    (This)->lpVtbl -> parse(This,varInput)

#define IVBSAXXMLReader_parseURL(This,strURL)	\
    (This)->lpVtbl -> parseURL(This,strURL)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_getFeature_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strName,
    /* [retval][out] */ VARIANT_BOOL *fValue);


void __RPC_STUB IVBSAXXMLReader_getFeature_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putFeature_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strName,
    /* [in] */ VARIANT_BOOL fValue);


void __RPC_STUB IVBSAXXMLReader_putFeature_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_getProperty_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strName,
    /* [retval][out] */ VARIANT *varValue);


void __RPC_STUB IVBSAXXMLReader_getProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putProperty_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strName,
    /* [in] */ VARIANT varValue);


void __RPC_STUB IVBSAXXMLReader_putProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_entityResolver_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ IVBSAXEntityResolver **oResolver);


void __RPC_STUB IVBSAXXMLReader_get_entityResolver_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putref_entityResolver_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ IVBSAXEntityResolver *oResolver);


void __RPC_STUB IVBSAXXMLReader_putref_entityResolver_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_contentHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ IVBSAXContentHandler **oHandler);


void __RPC_STUB IVBSAXXMLReader_get_contentHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putref_contentHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ IVBSAXContentHandler *oHandler);


void __RPC_STUB IVBSAXXMLReader_putref_contentHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_dtdHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ IVBSAXDTDHandler **oHandler);


void __RPC_STUB IVBSAXXMLReader_get_dtdHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putref_dtdHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ IVBSAXDTDHandler *oHandler);


void __RPC_STUB IVBSAXXMLReader_putref_dtdHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_errorHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ IVBSAXErrorHandler **oHandler);


void __RPC_STUB IVBSAXXMLReader_get_errorHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_putref_errorHandler_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ IVBSAXErrorHandler *oHandler);


void __RPC_STUB IVBSAXXMLReader_putref_errorHandler_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_baseURL_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ BSTR *strBaseURL);


void __RPC_STUB IVBSAXXMLReader_get_baseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_put_baseURL_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strBaseURL);


void __RPC_STUB IVBSAXXMLReader_put_baseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_get_secureBaseURL_Proxy( 
    IVBSAXXMLReader * This,
    /* [retval][out] */ BSTR *strSecureBaseURL);


void __RPC_STUB IVBSAXXMLReader_get_secureBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_put_secureBaseURL_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strSecureBaseURL);


void __RPC_STUB IVBSAXXMLReader_put_secureBaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_parse_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ VARIANT varInput);


void __RPC_STUB IVBSAXXMLReader_parse_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLReader_parseURL_Proxy( 
    IVBSAXXMLReader * This,
    /* [in] */ BSTR strURL);


void __RPC_STUB IVBSAXXMLReader_parseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXXMLReader_INTERFACE_DEFINED__ */


#ifndef __IVBSAXXMLFilter_INTERFACE_DEFINED__
#define __IVBSAXXMLFilter_INTERFACE_DEFINED__

/* interface IVBSAXXMLFilter */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXXMLFilter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1299eb1b-5b88-433e-82de-82ca75ad4e04")
    IVBSAXXMLFilter : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parent( 
            /* [retval][out] */ IVBSAXXMLReader **oReader) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_parent( 
            /* [in] */ IVBSAXXMLReader *oReader) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXXMLFilterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXXMLFilter * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXXMLFilter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXXMLFilter * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXXMLFilter * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXXMLFilter * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXXMLFilter * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXXMLFilter * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parent )( 
            IVBSAXXMLFilter * This,
            /* [retval][out] */ IVBSAXXMLReader **oReader);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_parent )( 
            IVBSAXXMLFilter * This,
            /* [in] */ IVBSAXXMLReader *oReader);
        
        END_INTERFACE
    } IVBSAXXMLFilterVtbl;

    interface IVBSAXXMLFilter
    {
        CONST_VTBL struct IVBSAXXMLFilterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXXMLFilter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXXMLFilter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXXMLFilter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXXMLFilter_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXXMLFilter_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXXMLFilter_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXXMLFilter_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXXMLFilter_get_parent(This,oReader)	\
    (This)->lpVtbl -> get_parent(This,oReader)

#define IVBSAXXMLFilter_putref_parent(This,oReader)	\
    (This)->lpVtbl -> putref_parent(This,oReader)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLFilter_get_parent_Proxy( 
    IVBSAXXMLFilter * This,
    /* [retval][out] */ IVBSAXXMLReader **oReader);


void __RPC_STUB IVBSAXXMLFilter_get_parent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXXMLFilter_putref_parent_Proxy( 
    IVBSAXXMLFilter * This,
    /* [in] */ IVBSAXXMLReader *oReader);


void __RPC_STUB IVBSAXXMLFilter_putref_parent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXXMLFilter_INTERFACE_DEFINED__ */


#ifndef __IVBSAXLocator_INTERFACE_DEFINED__
#define __IVBSAXLocator_INTERFACE_DEFINED__

/* interface IVBSAXLocator */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXLocator;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("796e7ac5-5aa2-4eff-acad-3faaf01a3288")
    IVBSAXLocator : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_columnNumber( 
            /* [retval][out] */ int *nColumn) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_lineNumber( 
            /* [retval][out] */ int *nLine) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_publicId( 
            /* [retval][out] */ BSTR *strPublicId) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_systemId( 
            /* [retval][out] */ BSTR *strSystemId) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXLocatorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXLocator * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXLocator * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXLocator * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXLocator * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXLocator * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXLocator * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXLocator * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_columnNumber )( 
            IVBSAXLocator * This,
            /* [retval][out] */ int *nColumn);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_lineNumber )( 
            IVBSAXLocator * This,
            /* [retval][out] */ int *nLine);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_publicId )( 
            IVBSAXLocator * This,
            /* [retval][out] */ BSTR *strPublicId);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_systemId )( 
            IVBSAXLocator * This,
            /* [retval][out] */ BSTR *strSystemId);
        
        END_INTERFACE
    } IVBSAXLocatorVtbl;

    interface IVBSAXLocator
    {
        CONST_VTBL struct IVBSAXLocatorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXLocator_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXLocator_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXLocator_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXLocator_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXLocator_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXLocator_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXLocator_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXLocator_get_columnNumber(This,nColumn)	\
    (This)->lpVtbl -> get_columnNumber(This,nColumn)

#define IVBSAXLocator_get_lineNumber(This,nLine)	\
    (This)->lpVtbl -> get_lineNumber(This,nLine)

#define IVBSAXLocator_get_publicId(This,strPublicId)	\
    (This)->lpVtbl -> get_publicId(This,strPublicId)

#define IVBSAXLocator_get_systemId(This,strSystemId)	\
    (This)->lpVtbl -> get_systemId(This,strSystemId)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXLocator_get_columnNumber_Proxy( 
    IVBSAXLocator * This,
    /* [retval][out] */ int *nColumn);


void __RPC_STUB IVBSAXLocator_get_columnNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXLocator_get_lineNumber_Proxy( 
    IVBSAXLocator * This,
    /* [retval][out] */ int *nLine);


void __RPC_STUB IVBSAXLocator_get_lineNumber_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXLocator_get_publicId_Proxy( 
    IVBSAXLocator * This,
    /* [retval][out] */ BSTR *strPublicId);


void __RPC_STUB IVBSAXLocator_get_publicId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXLocator_get_systemId_Proxy( 
    IVBSAXLocator * This,
    /* [retval][out] */ BSTR *strSystemId);


void __RPC_STUB IVBSAXLocator_get_systemId_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXLocator_INTERFACE_DEFINED__ */


#ifndef __IVBSAXEntityResolver_INTERFACE_DEFINED__
#define __IVBSAXEntityResolver_INTERFACE_DEFINED__

/* interface IVBSAXEntityResolver */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXEntityResolver;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("0c05d096-f45b-4aca-ad1a-aa0bc25518dc")
    IVBSAXEntityResolver : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE resolveEntity( 
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId,
            /* [retval][out] */ VARIANT *varInput) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXEntityResolverVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXEntityResolver * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXEntityResolver * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXEntityResolver * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXEntityResolver * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXEntityResolver * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXEntityResolver * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXEntityResolver * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *resolveEntity )( 
            IVBSAXEntityResolver * This,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId,
            /* [retval][out] */ VARIANT *varInput);
        
        END_INTERFACE
    } IVBSAXEntityResolverVtbl;

    interface IVBSAXEntityResolver
    {
        CONST_VTBL struct IVBSAXEntityResolverVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXEntityResolver_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXEntityResolver_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXEntityResolver_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXEntityResolver_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXEntityResolver_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXEntityResolver_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXEntityResolver_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXEntityResolver_resolveEntity(This,strPublicId,strSystemId,varInput)	\
    (This)->lpVtbl -> resolveEntity(This,strPublicId,strSystemId,varInput)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXEntityResolver_resolveEntity_Proxy( 
    IVBSAXEntityResolver * This,
    /* [out][in] */ BSTR *strPublicId,
    /* [out][in] */ BSTR *strSystemId,
    /* [retval][out] */ VARIANT *varInput);


void __RPC_STUB IVBSAXEntityResolver_resolveEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXEntityResolver_INTERFACE_DEFINED__ */


#ifndef __IVBSAXContentHandler_INTERFACE_DEFINED__
#define __IVBSAXContentHandler_INTERFACE_DEFINED__

/* interface IVBSAXContentHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXContentHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2ed7290a-4dd5-4b46-bb26-4e4155e77faa")
    IVBSAXContentHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_documentLocator( 
            /* [in] */ IVBSAXLocator *oLocator) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startDocument( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endDocument( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startPrefixMapping( 
            /* [out][in] */ BSTR *strPrefix,
            /* [out][in] */ BSTR *strURI) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endPrefixMapping( 
            /* [out][in] */ BSTR *strPrefix) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startElement( 
            /* [out][in] */ BSTR *strNamespaceURI,
            /* [out][in] */ BSTR *strLocalName,
            /* [out][in] */ BSTR *strQName,
            /* [in] */ IVBSAXAttributes *oAttributes) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endElement( 
            /* [out][in] */ BSTR *strNamespaceURI,
            /* [out][in] */ BSTR *strLocalName,
            /* [out][in] */ BSTR *strQName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE characters( 
            /* [out][in] */ BSTR *strChars) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
            /* [out][in] */ BSTR *strChars) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE processingInstruction( 
            /* [out][in] */ BSTR *strTarget,
            /* [out][in] */ BSTR *strData) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE skippedEntity( 
            /* [out][in] */ BSTR *strName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXContentHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXContentHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXContentHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXContentHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXContentHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXContentHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXContentHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXContentHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_documentLocator )( 
            IVBSAXContentHandler * This,
            /* [in] */ IVBSAXLocator *oLocator);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startDocument )( 
            IVBSAXContentHandler * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endDocument )( 
            IVBSAXContentHandler * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startPrefixMapping )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strPrefix,
            /* [out][in] */ BSTR *strURI);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endPrefixMapping )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strPrefix);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startElement )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strNamespaceURI,
            /* [out][in] */ BSTR *strLocalName,
            /* [out][in] */ BSTR *strQName,
            /* [in] */ IVBSAXAttributes *oAttributes);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endElement )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strNamespaceURI,
            /* [out][in] */ BSTR *strLocalName,
            /* [out][in] */ BSTR *strQName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *characters )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strChars);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *ignorableWhitespace )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strChars);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *processingInstruction )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strTarget,
            /* [out][in] */ BSTR *strData);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *skippedEntity )( 
            IVBSAXContentHandler * This,
            /* [out][in] */ BSTR *strName);
        
        END_INTERFACE
    } IVBSAXContentHandlerVtbl;

    interface IVBSAXContentHandler
    {
        CONST_VTBL struct IVBSAXContentHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXContentHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXContentHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXContentHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXContentHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXContentHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXContentHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXContentHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXContentHandler_putref_documentLocator(This,oLocator)	\
    (This)->lpVtbl -> putref_documentLocator(This,oLocator)

#define IVBSAXContentHandler_startDocument(This)	\
    (This)->lpVtbl -> startDocument(This)

#define IVBSAXContentHandler_endDocument(This)	\
    (This)->lpVtbl -> endDocument(This)

#define IVBSAXContentHandler_startPrefixMapping(This,strPrefix,strURI)	\
    (This)->lpVtbl -> startPrefixMapping(This,strPrefix,strURI)

#define IVBSAXContentHandler_endPrefixMapping(This,strPrefix)	\
    (This)->lpVtbl -> endPrefixMapping(This,strPrefix)

#define IVBSAXContentHandler_startElement(This,strNamespaceURI,strLocalName,strQName,oAttributes)	\
    (This)->lpVtbl -> startElement(This,strNamespaceURI,strLocalName,strQName,oAttributes)

#define IVBSAXContentHandler_endElement(This,strNamespaceURI,strLocalName,strQName)	\
    (This)->lpVtbl -> endElement(This,strNamespaceURI,strLocalName,strQName)

#define IVBSAXContentHandler_characters(This,strChars)	\
    (This)->lpVtbl -> characters(This,strChars)

#define IVBSAXContentHandler_ignorableWhitespace(This,strChars)	\
    (This)->lpVtbl -> ignorableWhitespace(This,strChars)

#define IVBSAXContentHandler_processingInstruction(This,strTarget,strData)	\
    (This)->lpVtbl -> processingInstruction(This,strTarget,strData)

#define IVBSAXContentHandler_skippedEntity(This,strName)	\
    (This)->lpVtbl -> skippedEntity(This,strName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_putref_documentLocator_Proxy( 
    IVBSAXContentHandler * This,
    /* [in] */ IVBSAXLocator *oLocator);


void __RPC_STUB IVBSAXContentHandler_putref_documentLocator_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_startDocument_Proxy( 
    IVBSAXContentHandler * This);


void __RPC_STUB IVBSAXContentHandler_startDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_endDocument_Proxy( 
    IVBSAXContentHandler * This);


void __RPC_STUB IVBSAXContentHandler_endDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_startPrefixMapping_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strPrefix,
    /* [out][in] */ BSTR *strURI);


void __RPC_STUB IVBSAXContentHandler_startPrefixMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_endPrefixMapping_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strPrefix);


void __RPC_STUB IVBSAXContentHandler_endPrefixMapping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_startElement_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strNamespaceURI,
    /* [out][in] */ BSTR *strLocalName,
    /* [out][in] */ BSTR *strQName,
    /* [in] */ IVBSAXAttributes *oAttributes);


void __RPC_STUB IVBSAXContentHandler_startElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_endElement_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strNamespaceURI,
    /* [out][in] */ BSTR *strLocalName,
    /* [out][in] */ BSTR *strQName);


void __RPC_STUB IVBSAXContentHandler_endElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_characters_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strChars);


void __RPC_STUB IVBSAXContentHandler_characters_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_ignorableWhitespace_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strChars);


void __RPC_STUB IVBSAXContentHandler_ignorableWhitespace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_processingInstruction_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strTarget,
    /* [out][in] */ BSTR *strData);


void __RPC_STUB IVBSAXContentHandler_processingInstruction_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXContentHandler_skippedEntity_Proxy( 
    IVBSAXContentHandler * This,
    /* [out][in] */ BSTR *strName);


void __RPC_STUB IVBSAXContentHandler_skippedEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXContentHandler_INTERFACE_DEFINED__ */


#ifndef __IVBSAXDTDHandler_INTERFACE_DEFINED__
#define __IVBSAXDTDHandler_INTERFACE_DEFINED__

/* interface IVBSAXDTDHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXDTDHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("24fb3297-302d-4620-ba39-3a732d850558")
    IVBSAXDTDHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE notationDecl( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE unparsedEntityDecl( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId,
            /* [out][in] */ BSTR *strNotationName) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXDTDHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXDTDHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXDTDHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXDTDHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXDTDHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXDTDHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXDTDHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXDTDHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *notationDecl )( 
            IVBSAXDTDHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *unparsedEntityDecl )( 
            IVBSAXDTDHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId,
            /* [out][in] */ BSTR *strNotationName);
        
        END_INTERFACE
    } IVBSAXDTDHandlerVtbl;

    interface IVBSAXDTDHandler
    {
        CONST_VTBL struct IVBSAXDTDHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXDTDHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXDTDHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXDTDHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXDTDHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXDTDHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXDTDHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXDTDHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXDTDHandler_notationDecl(This,strName,strPublicId,strSystemId)	\
    (This)->lpVtbl -> notationDecl(This,strName,strPublicId,strSystemId)

#define IVBSAXDTDHandler_unparsedEntityDecl(This,strName,strPublicId,strSystemId,strNotationName)	\
    (This)->lpVtbl -> unparsedEntityDecl(This,strName,strPublicId,strSystemId,strNotationName)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDTDHandler_notationDecl_Proxy( 
    IVBSAXDTDHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strPublicId,
    /* [out][in] */ BSTR *strSystemId);


void __RPC_STUB IVBSAXDTDHandler_notationDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDTDHandler_unparsedEntityDecl_Proxy( 
    IVBSAXDTDHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strPublicId,
    /* [out][in] */ BSTR *strSystemId,
    /* [out][in] */ BSTR *strNotationName);


void __RPC_STUB IVBSAXDTDHandler_unparsedEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXDTDHandler_INTERFACE_DEFINED__ */


#ifndef __IVBSAXErrorHandler_INTERFACE_DEFINED__
#define __IVBSAXErrorHandler_INTERFACE_DEFINED__

/* interface IVBSAXErrorHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXErrorHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("d963d3fe-173c-4862-9095-b92f66995f52")
    IVBSAXErrorHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE error( 
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE fatalError( 
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ignorableWarning( 
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXErrorHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXErrorHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXErrorHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXErrorHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXErrorHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXErrorHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXErrorHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXErrorHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *error )( 
            IVBSAXErrorHandler * This,
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *fatalError )( 
            IVBSAXErrorHandler * This,
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *ignorableWarning )( 
            IVBSAXErrorHandler * This,
            /* [in] */ IVBSAXLocator *oLocator,
            /* [out][in] */ BSTR *strErrorMessage,
            /* [in] */ long nErrorCode);
        
        END_INTERFACE
    } IVBSAXErrorHandlerVtbl;

    interface IVBSAXErrorHandler
    {
        CONST_VTBL struct IVBSAXErrorHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXErrorHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXErrorHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXErrorHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXErrorHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXErrorHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXErrorHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXErrorHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXErrorHandler_error(This,oLocator,strErrorMessage,nErrorCode)	\
    (This)->lpVtbl -> error(This,oLocator,strErrorMessage,nErrorCode)

#define IVBSAXErrorHandler_fatalError(This,oLocator,strErrorMessage,nErrorCode)	\
    (This)->lpVtbl -> fatalError(This,oLocator,strErrorMessage,nErrorCode)

#define IVBSAXErrorHandler_ignorableWarning(This,oLocator,strErrorMessage,nErrorCode)	\
    (This)->lpVtbl -> ignorableWarning(This,oLocator,strErrorMessage,nErrorCode)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXErrorHandler_error_Proxy( 
    IVBSAXErrorHandler * This,
    /* [in] */ IVBSAXLocator *oLocator,
    /* [out][in] */ BSTR *strErrorMessage,
    /* [in] */ long nErrorCode);


void __RPC_STUB IVBSAXErrorHandler_error_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXErrorHandler_fatalError_Proxy( 
    IVBSAXErrorHandler * This,
    /* [in] */ IVBSAXLocator *oLocator,
    /* [out][in] */ BSTR *strErrorMessage,
    /* [in] */ long nErrorCode);


void __RPC_STUB IVBSAXErrorHandler_fatalError_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXErrorHandler_ignorableWarning_Proxy( 
    IVBSAXErrorHandler * This,
    /* [in] */ IVBSAXLocator *oLocator,
    /* [out][in] */ BSTR *strErrorMessage,
    /* [in] */ long nErrorCode);


void __RPC_STUB IVBSAXErrorHandler_ignorableWarning_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXErrorHandler_INTERFACE_DEFINED__ */


#ifndef __IVBSAXLexicalHandler_INTERFACE_DEFINED__
#define __IVBSAXLexicalHandler_INTERFACE_DEFINED__

/* interface IVBSAXLexicalHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXLexicalHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("032aac35-8c0e-4d9d-979f-e3b702935576")
    IVBSAXLexicalHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startDTD( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endDTD( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startEntity( 
            /* [out][in] */ BSTR *strName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endEntity( 
            /* [out][in] */ BSTR *strName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE startCDATA( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE endCDATA( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE comment( 
            /* [out][in] */ BSTR *strChars) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXLexicalHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXLexicalHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXLexicalHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXLexicalHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXLexicalHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXLexicalHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXLexicalHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXLexicalHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startDTD )( 
            IVBSAXLexicalHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endDTD )( 
            IVBSAXLexicalHandler * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startEntity )( 
            IVBSAXLexicalHandler * This,
            /* [out][in] */ BSTR *strName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endEntity )( 
            IVBSAXLexicalHandler * This,
            /* [out][in] */ BSTR *strName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *startCDATA )( 
            IVBSAXLexicalHandler * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *endCDATA )( 
            IVBSAXLexicalHandler * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *comment )( 
            IVBSAXLexicalHandler * This,
            /* [out][in] */ BSTR *strChars);
        
        END_INTERFACE
    } IVBSAXLexicalHandlerVtbl;

    interface IVBSAXLexicalHandler
    {
        CONST_VTBL struct IVBSAXLexicalHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXLexicalHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXLexicalHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXLexicalHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXLexicalHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXLexicalHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXLexicalHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXLexicalHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXLexicalHandler_startDTD(This,strName,strPublicId,strSystemId)	\
    (This)->lpVtbl -> startDTD(This,strName,strPublicId,strSystemId)

#define IVBSAXLexicalHandler_endDTD(This)	\
    (This)->lpVtbl -> endDTD(This)

#define IVBSAXLexicalHandler_startEntity(This,strName)	\
    (This)->lpVtbl -> startEntity(This,strName)

#define IVBSAXLexicalHandler_endEntity(This,strName)	\
    (This)->lpVtbl -> endEntity(This,strName)

#define IVBSAXLexicalHandler_startCDATA(This)	\
    (This)->lpVtbl -> startCDATA(This)

#define IVBSAXLexicalHandler_endCDATA(This)	\
    (This)->lpVtbl -> endCDATA(This)

#define IVBSAXLexicalHandler_comment(This,strChars)	\
    (This)->lpVtbl -> comment(This,strChars)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_startDTD_Proxy( 
    IVBSAXLexicalHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strPublicId,
    /* [out][in] */ BSTR *strSystemId);


void __RPC_STUB IVBSAXLexicalHandler_startDTD_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_endDTD_Proxy( 
    IVBSAXLexicalHandler * This);


void __RPC_STUB IVBSAXLexicalHandler_endDTD_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_startEntity_Proxy( 
    IVBSAXLexicalHandler * This,
    /* [out][in] */ BSTR *strName);


void __RPC_STUB IVBSAXLexicalHandler_startEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_endEntity_Proxy( 
    IVBSAXLexicalHandler * This,
    /* [out][in] */ BSTR *strName);


void __RPC_STUB IVBSAXLexicalHandler_endEntity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_startCDATA_Proxy( 
    IVBSAXLexicalHandler * This);


void __RPC_STUB IVBSAXLexicalHandler_startCDATA_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_endCDATA_Proxy( 
    IVBSAXLexicalHandler * This);


void __RPC_STUB IVBSAXLexicalHandler_endCDATA_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXLexicalHandler_comment_Proxy( 
    IVBSAXLexicalHandler * This,
    /* [out][in] */ BSTR *strChars);


void __RPC_STUB IVBSAXLexicalHandler_comment_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXLexicalHandler_INTERFACE_DEFINED__ */


#ifndef __IVBSAXDeclHandler_INTERFACE_DEFINED__
#define __IVBSAXDeclHandler_INTERFACE_DEFINED__

/* interface IVBSAXDeclHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXDeclHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("e8917260-7579-4be1-b5dd-7afbfa6f077b")
    IVBSAXDeclHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE elementDecl( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strModel) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE attributeDecl( 
            /* [out][in] */ BSTR *strElementName,
            /* [out][in] */ BSTR *strAttributeName,
            /* [out][in] */ BSTR *strType,
            /* [out][in] */ BSTR *strValueDefault,
            /* [out][in] */ BSTR *strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE internalEntityDecl( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE externalEntityDecl( 
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXDeclHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXDeclHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXDeclHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXDeclHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXDeclHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXDeclHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXDeclHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXDeclHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *elementDecl )( 
            IVBSAXDeclHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strModel);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *attributeDecl )( 
            IVBSAXDeclHandler * This,
            /* [out][in] */ BSTR *strElementName,
            /* [out][in] */ BSTR *strAttributeName,
            /* [out][in] */ BSTR *strType,
            /* [out][in] */ BSTR *strValueDefault,
            /* [out][in] */ BSTR *strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *internalEntityDecl )( 
            IVBSAXDeclHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *externalEntityDecl )( 
            IVBSAXDeclHandler * This,
            /* [out][in] */ BSTR *strName,
            /* [out][in] */ BSTR *strPublicId,
            /* [out][in] */ BSTR *strSystemId);
        
        END_INTERFACE
    } IVBSAXDeclHandlerVtbl;

    interface IVBSAXDeclHandler
    {
        CONST_VTBL struct IVBSAXDeclHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXDeclHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXDeclHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXDeclHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXDeclHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXDeclHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXDeclHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXDeclHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXDeclHandler_elementDecl(This,strName,strModel)	\
    (This)->lpVtbl -> elementDecl(This,strName,strModel)

#define IVBSAXDeclHandler_attributeDecl(This,strElementName,strAttributeName,strType,strValueDefault,strValue)	\
    (This)->lpVtbl -> attributeDecl(This,strElementName,strAttributeName,strType,strValueDefault,strValue)

#define IVBSAXDeclHandler_internalEntityDecl(This,strName,strValue)	\
    (This)->lpVtbl -> internalEntityDecl(This,strName,strValue)

#define IVBSAXDeclHandler_externalEntityDecl(This,strName,strPublicId,strSystemId)	\
    (This)->lpVtbl -> externalEntityDecl(This,strName,strPublicId,strSystemId)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDeclHandler_elementDecl_Proxy( 
    IVBSAXDeclHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strModel);


void __RPC_STUB IVBSAXDeclHandler_elementDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDeclHandler_attributeDecl_Proxy( 
    IVBSAXDeclHandler * This,
    /* [out][in] */ BSTR *strElementName,
    /* [out][in] */ BSTR *strAttributeName,
    /* [out][in] */ BSTR *strType,
    /* [out][in] */ BSTR *strValueDefault,
    /* [out][in] */ BSTR *strValue);


void __RPC_STUB IVBSAXDeclHandler_attributeDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDeclHandler_internalEntityDecl_Proxy( 
    IVBSAXDeclHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strValue);


void __RPC_STUB IVBSAXDeclHandler_internalEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXDeclHandler_externalEntityDecl_Proxy( 
    IVBSAXDeclHandler * This,
    /* [out][in] */ BSTR *strName,
    /* [out][in] */ BSTR *strPublicId,
    /* [out][in] */ BSTR *strSystemId);


void __RPC_STUB IVBSAXDeclHandler_externalEntityDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXDeclHandler_INTERFACE_DEFINED__ */


#ifndef __IVBSAXAttributes_INTERFACE_DEFINED__
#define __IVBSAXAttributes_INTERFACE_DEFINED__

/* interface IVBSAXAttributes */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IVBSAXAttributes;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("10dc0586-132b-4cac-8bb3-db00ac8b7ee0")
    IVBSAXAttributes : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ int *nLength) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getURI( 
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strURI) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getLocalName( 
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strLocalName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getQName( 
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strQName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getIndexFromName( 
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ int *nIndex) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getIndexFromQName( 
            /* [in] */ BSTR strQName,
            /* [retval][out] */ int *nIndex) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getType( 
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strType) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getTypeFromName( 
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ BSTR *strType) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getTypeFromQName( 
            /* [in] */ BSTR strQName,
            /* [retval][out] */ BSTR *strType) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getValue( 
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getValueFromName( 
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ BSTR *strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getValueFromQName( 
            /* [in] */ BSTR strQName,
            /* [retval][out] */ BSTR *strValue) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBSAXAttributesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBSAXAttributes * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBSAXAttributes * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBSAXAttributes * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBSAXAttributes * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBSAXAttributes * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBSAXAttributes * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBSAXAttributes * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IVBSAXAttributes * This,
            /* [retval][out] */ int *nLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getURI )( 
            IVBSAXAttributes * This,
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strURI);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getLocalName )( 
            IVBSAXAttributes * This,
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strLocalName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getQName )( 
            IVBSAXAttributes * This,
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strQName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getIndexFromName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ int *nIndex);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getIndexFromQName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strQName,
            /* [retval][out] */ int *nIndex);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getType )( 
            IVBSAXAttributes * This,
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strType);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getTypeFromName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ BSTR *strType);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getTypeFromQName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strQName,
            /* [retval][out] */ BSTR *strType);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getValue )( 
            IVBSAXAttributes * This,
            /* [in] */ int nIndex,
            /* [retval][out] */ BSTR *strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getValueFromName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [retval][out] */ BSTR *strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getValueFromQName )( 
            IVBSAXAttributes * This,
            /* [in] */ BSTR strQName,
            /* [retval][out] */ BSTR *strValue);
        
        END_INTERFACE
    } IVBSAXAttributesVtbl;

    interface IVBSAXAttributes
    {
        CONST_VTBL struct IVBSAXAttributesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBSAXAttributes_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBSAXAttributes_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBSAXAttributes_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBSAXAttributes_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBSAXAttributes_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBSAXAttributes_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBSAXAttributes_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBSAXAttributes_get_length(This,nLength)	\
    (This)->lpVtbl -> get_length(This,nLength)

#define IVBSAXAttributes_getURI(This,nIndex,strURI)	\
    (This)->lpVtbl -> getURI(This,nIndex,strURI)

#define IVBSAXAttributes_getLocalName(This,nIndex,strLocalName)	\
    (This)->lpVtbl -> getLocalName(This,nIndex,strLocalName)

#define IVBSAXAttributes_getQName(This,nIndex,strQName)	\
    (This)->lpVtbl -> getQName(This,nIndex,strQName)

#define IVBSAXAttributes_getIndexFromName(This,strURI,strLocalName,nIndex)	\
    (This)->lpVtbl -> getIndexFromName(This,strURI,strLocalName,nIndex)

#define IVBSAXAttributes_getIndexFromQName(This,strQName,nIndex)	\
    (This)->lpVtbl -> getIndexFromQName(This,strQName,nIndex)

#define IVBSAXAttributes_getType(This,nIndex,strType)	\
    (This)->lpVtbl -> getType(This,nIndex,strType)

#define IVBSAXAttributes_getTypeFromName(This,strURI,strLocalName,strType)	\
    (This)->lpVtbl -> getTypeFromName(This,strURI,strLocalName,strType)

#define IVBSAXAttributes_getTypeFromQName(This,strQName,strType)	\
    (This)->lpVtbl -> getTypeFromQName(This,strQName,strType)

#define IVBSAXAttributes_getValue(This,nIndex,strValue)	\
    (This)->lpVtbl -> getValue(This,nIndex,strValue)

#define IVBSAXAttributes_getValueFromName(This,strURI,strLocalName,strValue)	\
    (This)->lpVtbl -> getValueFromName(This,strURI,strLocalName,strValue)

#define IVBSAXAttributes_getValueFromQName(This,strQName,strValue)	\
    (This)->lpVtbl -> getValueFromQName(This,strQName,strValue)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_get_length_Proxy( 
    IVBSAXAttributes * This,
    /* [retval][out] */ int *nLength);


void __RPC_STUB IVBSAXAttributes_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getURI_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ int nIndex,
    /* [retval][out] */ BSTR *strURI);


void __RPC_STUB IVBSAXAttributes_getURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getLocalName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ int nIndex,
    /* [retval][out] */ BSTR *strLocalName);


void __RPC_STUB IVBSAXAttributes_getLocalName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getQName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ int nIndex,
    /* [retval][out] */ BSTR *strQName);


void __RPC_STUB IVBSAXAttributes_getQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getIndexFromName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strURI,
    /* [in] */ BSTR strLocalName,
    /* [retval][out] */ int *nIndex);


void __RPC_STUB IVBSAXAttributes_getIndexFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getIndexFromQName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strQName,
    /* [retval][out] */ int *nIndex);


void __RPC_STUB IVBSAXAttributes_getIndexFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getType_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ int nIndex,
    /* [retval][out] */ BSTR *strType);


void __RPC_STUB IVBSAXAttributes_getType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getTypeFromName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strURI,
    /* [in] */ BSTR strLocalName,
    /* [retval][out] */ BSTR *strType);


void __RPC_STUB IVBSAXAttributes_getTypeFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getTypeFromQName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strQName,
    /* [retval][out] */ BSTR *strType);


void __RPC_STUB IVBSAXAttributes_getTypeFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getValue_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ int nIndex,
    /* [retval][out] */ BSTR *strValue);


void __RPC_STUB IVBSAXAttributes_getValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getValueFromName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strURI,
    /* [in] */ BSTR strLocalName,
    /* [retval][out] */ BSTR *strValue);


void __RPC_STUB IVBSAXAttributes_getValueFromName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IVBSAXAttributes_getValueFromQName_Proxy( 
    IVBSAXAttributes * This,
    /* [in] */ BSTR strQName,
    /* [retval][out] */ BSTR *strValue);


void __RPC_STUB IVBSAXAttributes_getValueFromQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBSAXAttributes_INTERFACE_DEFINED__ */


#ifndef __IMXWriter_INTERFACE_DEFINED__
#define __IMXWriter_INTERFACE_DEFINED__

/* interface IMXWriter */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IMXWriter;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4d7ff4ba-1565-4ea8-94e1-6e724a46f98d")
    IMXWriter : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_output( 
            /* [in] */ VARIANT varDestination) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_output( 
            /* [retval][out] */ VARIANT *varDestination) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_encoding( 
            /* [in] */ BSTR strEncoding) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_encoding( 
            /* [retval][out] */ BSTR *strEncoding) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_byteOrderMark( 
            /* [in] */ VARIANT_BOOL fWriteByteOrderMark) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_byteOrderMark( 
            /* [retval][out] */ VARIANT_BOOL *fWriteByteOrderMark) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_indent( 
            /* [in] */ VARIANT_BOOL fIndentMode) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_indent( 
            /* [retval][out] */ VARIANT_BOOL *fIndentMode) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_standalone( 
            /* [in] */ VARIANT_BOOL fValue) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_standalone( 
            /* [retval][out] */ VARIANT_BOOL *fValue) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_omitXMLDeclaration( 
            /* [in] */ VARIANT_BOOL fValue) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_omitXMLDeclaration( 
            /* [retval][out] */ VARIANT_BOOL *fValue) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_version( 
            /* [in] */ BSTR strVersion) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [retval][out] */ BSTR *strVersion) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_disableOutputEscaping( 
            /* [in] */ VARIANT_BOOL fValue) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_disableOutputEscaping( 
            /* [retval][out] */ VARIANT_BOOL *fValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE flush( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXWriterVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXWriter * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXWriter * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXWriter * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMXWriter * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMXWriter * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMXWriter * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMXWriter * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_output )( 
            IMXWriter * This,
            /* [in] */ VARIANT varDestination);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_output )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT *varDestination);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_encoding )( 
            IMXWriter * This,
            /* [in] */ BSTR strEncoding);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_encoding )( 
            IMXWriter * This,
            /* [retval][out] */ BSTR *strEncoding);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_byteOrderMark )( 
            IMXWriter * This,
            /* [in] */ VARIANT_BOOL fWriteByteOrderMark);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_byteOrderMark )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT_BOOL *fWriteByteOrderMark);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_indent )( 
            IMXWriter * This,
            /* [in] */ VARIANT_BOOL fIndentMode);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_indent )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT_BOOL *fIndentMode);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_standalone )( 
            IMXWriter * This,
            /* [in] */ VARIANT_BOOL fValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_standalone )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT_BOOL *fValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_omitXMLDeclaration )( 
            IMXWriter * This,
            /* [in] */ VARIANT_BOOL fValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_omitXMLDeclaration )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT_BOOL *fValue);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_version )( 
            IMXWriter * This,
            /* [in] */ BSTR strVersion);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            IMXWriter * This,
            /* [retval][out] */ BSTR *strVersion);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_disableOutputEscaping )( 
            IMXWriter * This,
            /* [in] */ VARIANT_BOOL fValue);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_disableOutputEscaping )( 
            IMXWriter * This,
            /* [retval][out] */ VARIANT_BOOL *fValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *flush )( 
            IMXWriter * This);
        
        END_INTERFACE
    } IMXWriterVtbl;

    interface IMXWriter
    {
        CONST_VTBL struct IMXWriterVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXWriter_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXWriter_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXWriter_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXWriter_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMXWriter_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMXWriter_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMXWriter_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMXWriter_put_output(This,varDestination)	\
    (This)->lpVtbl -> put_output(This,varDestination)

#define IMXWriter_get_output(This,varDestination)	\
    (This)->lpVtbl -> get_output(This,varDestination)

#define IMXWriter_put_encoding(This,strEncoding)	\
    (This)->lpVtbl -> put_encoding(This,strEncoding)

#define IMXWriter_get_encoding(This,strEncoding)	\
    (This)->lpVtbl -> get_encoding(This,strEncoding)

#define IMXWriter_put_byteOrderMark(This,fWriteByteOrderMark)	\
    (This)->lpVtbl -> put_byteOrderMark(This,fWriteByteOrderMark)

#define IMXWriter_get_byteOrderMark(This,fWriteByteOrderMark)	\
    (This)->lpVtbl -> get_byteOrderMark(This,fWriteByteOrderMark)

#define IMXWriter_put_indent(This,fIndentMode)	\
    (This)->lpVtbl -> put_indent(This,fIndentMode)

#define IMXWriter_get_indent(This,fIndentMode)	\
    (This)->lpVtbl -> get_indent(This,fIndentMode)

#define IMXWriter_put_standalone(This,fValue)	\
    (This)->lpVtbl -> put_standalone(This,fValue)

#define IMXWriter_get_standalone(This,fValue)	\
    (This)->lpVtbl -> get_standalone(This,fValue)

#define IMXWriter_put_omitXMLDeclaration(This,fValue)	\
    (This)->lpVtbl -> put_omitXMLDeclaration(This,fValue)

#define IMXWriter_get_omitXMLDeclaration(This,fValue)	\
    (This)->lpVtbl -> get_omitXMLDeclaration(This,fValue)

#define IMXWriter_put_version(This,strVersion)	\
    (This)->lpVtbl -> put_version(This,strVersion)

#define IMXWriter_get_version(This,strVersion)	\
    (This)->lpVtbl -> get_version(This,strVersion)

#define IMXWriter_put_disableOutputEscaping(This,fValue)	\
    (This)->lpVtbl -> put_disableOutputEscaping(This,fValue)

#define IMXWriter_get_disableOutputEscaping(This,fValue)	\
    (This)->lpVtbl -> get_disableOutputEscaping(This,fValue)

#define IMXWriter_flush(This)	\
    (This)->lpVtbl -> flush(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_output_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT varDestination);


void __RPC_STUB IMXWriter_put_output_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_output_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT *varDestination);


void __RPC_STUB IMXWriter_get_output_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_encoding_Proxy( 
    IMXWriter * This,
    /* [in] */ BSTR strEncoding);


void __RPC_STUB IMXWriter_put_encoding_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_encoding_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ BSTR *strEncoding);


void __RPC_STUB IMXWriter_get_encoding_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_byteOrderMark_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT_BOOL fWriteByteOrderMark);


void __RPC_STUB IMXWriter_put_byteOrderMark_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_byteOrderMark_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT_BOOL *fWriteByteOrderMark);


void __RPC_STUB IMXWriter_get_byteOrderMark_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_indent_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT_BOOL fIndentMode);


void __RPC_STUB IMXWriter_put_indent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_indent_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT_BOOL *fIndentMode);


void __RPC_STUB IMXWriter_get_indent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_standalone_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT_BOOL fValue);


void __RPC_STUB IMXWriter_put_standalone_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_standalone_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT_BOOL *fValue);


void __RPC_STUB IMXWriter_get_standalone_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_omitXMLDeclaration_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT_BOOL fValue);


void __RPC_STUB IMXWriter_put_omitXMLDeclaration_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_omitXMLDeclaration_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT_BOOL *fValue);


void __RPC_STUB IMXWriter_get_omitXMLDeclaration_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_version_Proxy( 
    IMXWriter * This,
    /* [in] */ BSTR strVersion);


void __RPC_STUB IMXWriter_put_version_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_version_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ BSTR *strVersion);


void __RPC_STUB IMXWriter_get_version_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IMXWriter_put_disableOutputEscaping_Proxy( 
    IMXWriter * This,
    /* [in] */ VARIANT_BOOL fValue);


void __RPC_STUB IMXWriter_put_disableOutputEscaping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IMXWriter_get_disableOutputEscaping_Proxy( 
    IMXWriter * This,
    /* [retval][out] */ VARIANT_BOOL *fValue);


void __RPC_STUB IMXWriter_get_disableOutputEscaping_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXWriter_flush_Proxy( 
    IMXWriter * This);


void __RPC_STUB IMXWriter_flush_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXWriter_INTERFACE_DEFINED__ */


#ifndef __IMXAttributes_INTERFACE_DEFINED__
#define __IMXAttributes_INTERFACE_DEFINED__

/* interface IMXAttributes */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IMXAttributes;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("f10d27cc-3ec0-415c-8ed8-77ab1c5e7262")
    IMXAttributes : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addAttribute( 
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [in] */ BSTR strQName,
            /* [in] */ BSTR strType,
            /* [in] */ BSTR strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addAttributeFromIndex( 
            /* [in] */ VARIANT varAtts,
            /* [in] */ int nIndex) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE clear( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAttribute( 
            /* [in] */ int nIndex) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttribute( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [in] */ BSTR strQName,
            /* [in] */ BSTR strType,
            /* [in] */ BSTR strValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttributes( 
            /* [in] */ VARIANT varAtts) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setLocalName( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strLocalName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setQName( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strQName) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setType( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strType) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setURI( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strURI) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setValue( 
            /* [in] */ int nIndex,
            /* [in] */ BSTR strValue) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXAttributesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXAttributes * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXAttributes * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXAttributes * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMXAttributes * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMXAttributes * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMXAttributes * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMXAttributes * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addAttribute )( 
            IMXAttributes * This,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [in] */ BSTR strQName,
            /* [in] */ BSTR strType,
            /* [in] */ BSTR strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addAttributeFromIndex )( 
            IMXAttributes * This,
            /* [in] */ VARIANT varAtts,
            /* [in] */ int nIndex);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *clear )( 
            IMXAttributes * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAttribute )( 
            IMXAttributes * This,
            /* [in] */ int nIndex);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttribute )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strURI,
            /* [in] */ BSTR strLocalName,
            /* [in] */ BSTR strQName,
            /* [in] */ BSTR strType,
            /* [in] */ BSTR strValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttributes )( 
            IMXAttributes * This,
            /* [in] */ VARIANT varAtts);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setLocalName )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strLocalName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setQName )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strQName);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setType )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strType);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setURI )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strURI);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setValue )( 
            IMXAttributes * This,
            /* [in] */ int nIndex,
            /* [in] */ BSTR strValue);
        
        END_INTERFACE
    } IMXAttributesVtbl;

    interface IMXAttributes
    {
        CONST_VTBL struct IMXAttributesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXAttributes_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXAttributes_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXAttributes_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXAttributes_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMXAttributes_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMXAttributes_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMXAttributes_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMXAttributes_addAttribute(This,strURI,strLocalName,strQName,strType,strValue)	\
    (This)->lpVtbl -> addAttribute(This,strURI,strLocalName,strQName,strType,strValue)

#define IMXAttributes_addAttributeFromIndex(This,varAtts,nIndex)	\
    (This)->lpVtbl -> addAttributeFromIndex(This,varAtts,nIndex)

#define IMXAttributes_clear(This)	\
    (This)->lpVtbl -> clear(This)

#define IMXAttributes_removeAttribute(This,nIndex)	\
    (This)->lpVtbl -> removeAttribute(This,nIndex)

#define IMXAttributes_setAttribute(This,nIndex,strURI,strLocalName,strQName,strType,strValue)	\
    (This)->lpVtbl -> setAttribute(This,nIndex,strURI,strLocalName,strQName,strType,strValue)

#define IMXAttributes_setAttributes(This,varAtts)	\
    (This)->lpVtbl -> setAttributes(This,varAtts)

#define IMXAttributes_setLocalName(This,nIndex,strLocalName)	\
    (This)->lpVtbl -> setLocalName(This,nIndex,strLocalName)

#define IMXAttributes_setQName(This,nIndex,strQName)	\
    (This)->lpVtbl -> setQName(This,nIndex,strQName)

#define IMXAttributes_setType(This,nIndex,strType)	\
    (This)->lpVtbl -> setType(This,nIndex,strType)

#define IMXAttributes_setURI(This,nIndex,strURI)	\
    (This)->lpVtbl -> setURI(This,nIndex,strURI)

#define IMXAttributes_setValue(This,nIndex,strValue)	\
    (This)->lpVtbl -> setValue(This,nIndex,strValue)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_addAttribute_Proxy( 
    IMXAttributes * This,
    /* [in] */ BSTR strURI,
    /* [in] */ BSTR strLocalName,
    /* [in] */ BSTR strQName,
    /* [in] */ BSTR strType,
    /* [in] */ BSTR strValue);


void __RPC_STUB IMXAttributes_addAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_addAttributeFromIndex_Proxy( 
    IMXAttributes * This,
    /* [in] */ VARIANT varAtts,
    /* [in] */ int nIndex);


void __RPC_STUB IMXAttributes_addAttributeFromIndex_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_clear_Proxy( 
    IMXAttributes * This);


void __RPC_STUB IMXAttributes_clear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_removeAttribute_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex);


void __RPC_STUB IMXAttributes_removeAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setAttribute_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strURI,
    /* [in] */ BSTR strLocalName,
    /* [in] */ BSTR strQName,
    /* [in] */ BSTR strType,
    /* [in] */ BSTR strValue);


void __RPC_STUB IMXAttributes_setAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setAttributes_Proxy( 
    IMXAttributes * This,
    /* [in] */ VARIANT varAtts);


void __RPC_STUB IMXAttributes_setAttributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setLocalName_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strLocalName);


void __RPC_STUB IMXAttributes_setLocalName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setQName_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strQName);


void __RPC_STUB IMXAttributes_setQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setType_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strType);


void __RPC_STUB IMXAttributes_setType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setURI_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strURI);


void __RPC_STUB IMXAttributes_setURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXAttributes_setValue_Proxy( 
    IMXAttributes * This,
    /* [in] */ int nIndex,
    /* [in] */ BSTR strValue);


void __RPC_STUB IMXAttributes_setValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXAttributes_INTERFACE_DEFINED__ */


#ifndef __IMXReaderControl_INTERFACE_DEFINED__
#define __IMXReaderControl_INTERFACE_DEFINED__

/* interface IMXReaderControl */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IMXReaderControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("808f4e35-8d5a-4fbe-8466-33a41279ed30")
    IMXReaderControl : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE abort( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE resume( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE suspend( void) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXReaderControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXReaderControl * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXReaderControl * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXReaderControl * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMXReaderControl * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMXReaderControl * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMXReaderControl * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMXReaderControl * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IMXReaderControl * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *resume )( 
            IMXReaderControl * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *suspend )( 
            IMXReaderControl * This);
        
        END_INTERFACE
    } IMXReaderControlVtbl;

    interface IMXReaderControl
    {
        CONST_VTBL struct IMXReaderControlVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXReaderControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXReaderControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXReaderControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXReaderControl_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMXReaderControl_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMXReaderControl_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMXReaderControl_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMXReaderControl_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IMXReaderControl_resume(This)	\
    (This)->lpVtbl -> resume(This)

#define IMXReaderControl_suspend(This)	\
    (This)->lpVtbl -> suspend(This)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXReaderControl_abort_Proxy( 
    IMXReaderControl * This);


void __RPC_STUB IMXReaderControl_abort_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXReaderControl_resume_Proxy( 
    IMXReaderControl * This);


void __RPC_STUB IMXReaderControl_resume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXReaderControl_suspend_Proxy( 
    IMXReaderControl * This);


void __RPC_STUB IMXReaderControl_suspend_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXReaderControl_INTERFACE_DEFINED__ */


#ifndef __IMXSchemaDeclHandler_INTERFACE_DEFINED__
#define __IMXSchemaDeclHandler_INTERFACE_DEFINED__

/* interface IMXSchemaDeclHandler */
/* [unique][helpstring][uuid][nonextensible][oleautomation][dual][local][object] */ 


EXTERN_C const IID IID_IMXSchemaDeclHandler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("fa4bb38c-faf9-4cca-9302-d1dd0fe520db")
    IMXSchemaDeclHandler : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE schemaElementDecl( 
            /* [in] */ ISchemaElement *oSchemaElement) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXSchemaDeclHandlerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXSchemaDeclHandler * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXSchemaDeclHandler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXSchemaDeclHandler * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMXSchemaDeclHandler * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMXSchemaDeclHandler * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMXSchemaDeclHandler * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMXSchemaDeclHandler * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *schemaElementDecl )( 
            IMXSchemaDeclHandler * This,
            /* [in] */ ISchemaElement *oSchemaElement);
        
        END_INTERFACE
    } IMXSchemaDeclHandlerVtbl;

    interface IMXSchemaDeclHandler
    {
        CONST_VTBL struct IMXSchemaDeclHandlerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXSchemaDeclHandler_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXSchemaDeclHandler_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXSchemaDeclHandler_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXSchemaDeclHandler_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMXSchemaDeclHandler_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMXSchemaDeclHandler_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMXSchemaDeclHandler_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMXSchemaDeclHandler_schemaElementDecl(This,oSchemaElement)	\
    (This)->lpVtbl -> schemaElementDecl(This,oSchemaElement)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IMXSchemaDeclHandler_schemaElementDecl_Proxy( 
    IMXSchemaDeclHandler * This,
    /* [in] */ ISchemaElement *oSchemaElement);


void __RPC_STUB IMXSchemaDeclHandler_schemaElementDecl_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXSchemaDeclHandler_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMSchemaCollection2_INTERFACE_DEFINED__
#define __IXMLDOMSchemaCollection2_INTERFACE_DEFINED__

/* interface IXMLDOMSchemaCollection2 */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMSchemaCollection2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b0-dd1b-4664-9a50-c2f40f4bd79a")
    IXMLDOMSchemaCollection2 : public IXMLDOMSchemaCollection
    {
    public:
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE validate( void) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_validateOnLoad( 
            /* [in] */ VARIANT_BOOL validateOnLoad) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_validateOnLoad( 
            /* [retval][out] */ VARIANT_BOOL *validateOnLoad) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getSchema( 
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ ISchema **schema) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getDeclaration( 
            /* [in] */ IXMLDOMNode *node,
            /* [retval][out] */ ISchemaItem **item) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMSchemaCollection2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMSchemaCollection2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMSchemaCollection2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMSchemaCollection2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *add )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ BSTR namespaceURI,
            /* [in] */ VARIANT var);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *get )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IXMLDOMNode **schemaNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *remove )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ BSTR namespaceURI);
        
        /* [propget][helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMSchemaCollection2 * This,
            /* [retval][out] */ long *length);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ long index,
            /* [retval][out] */ BSTR *length);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addCollection )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ IXMLDOMSchemaCollection *otherCollection);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLDOMSchemaCollection2 * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *validate )( 
            IXMLDOMSchemaCollection2 * This);
        
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_validateOnLoad )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ VARIANT_BOOL validateOnLoad);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_validateOnLoad )( 
            IXMLDOMSchemaCollection2 * This,
            /* [retval][out] */ VARIANT_BOOL *validateOnLoad);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getSchema )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ ISchema **schema);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getDeclaration )( 
            IXMLDOMSchemaCollection2 * This,
            /* [in] */ IXMLDOMNode *node,
            /* [retval][out] */ ISchemaItem **item);
        
        END_INTERFACE
    } IXMLDOMSchemaCollection2Vtbl;

    interface IXMLDOMSchemaCollection2
    {
        CONST_VTBL struct IXMLDOMSchemaCollection2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMSchemaCollection2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMSchemaCollection2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMSchemaCollection2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMSchemaCollection2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMSchemaCollection2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMSchemaCollection2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMSchemaCollection2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMSchemaCollection2_add(This,namespaceURI,var)	\
    (This)->lpVtbl -> add(This,namespaceURI,var)

#define IXMLDOMSchemaCollection2_get(This,namespaceURI,schemaNode)	\
    (This)->lpVtbl -> get(This,namespaceURI,schemaNode)

#define IXMLDOMSchemaCollection2_remove(This,namespaceURI)	\
    (This)->lpVtbl -> remove(This,namespaceURI)

#define IXMLDOMSchemaCollection2_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define IXMLDOMSchemaCollection2_get_namespaceURI(This,index,length)	\
    (This)->lpVtbl -> get_namespaceURI(This,index,length)

#define IXMLDOMSchemaCollection2_addCollection(This,otherCollection)	\
    (This)->lpVtbl -> addCollection(This,otherCollection)

#define IXMLDOMSchemaCollection2_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)


#define IXMLDOMSchemaCollection2_validate(This)	\
    (This)->lpVtbl -> validate(This)

#define IXMLDOMSchemaCollection2_put_validateOnLoad(This,validateOnLoad)	\
    (This)->lpVtbl -> put_validateOnLoad(This,validateOnLoad)

#define IXMLDOMSchemaCollection2_get_validateOnLoad(This,validateOnLoad)	\
    (This)->lpVtbl -> get_validateOnLoad(This,validateOnLoad)

#define IXMLDOMSchemaCollection2_getSchema(This,namespaceURI,schema)	\
    (This)->lpVtbl -> getSchema(This,namespaceURI,schema)

#define IXMLDOMSchemaCollection2_getDeclaration(This,node,item)	\
    (This)->lpVtbl -> getDeclaration(This,node,item)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection2_validate_Proxy( 
    IXMLDOMSchemaCollection2 * This);


void __RPC_STUB IXMLDOMSchemaCollection2_validate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propput][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection2_put_validateOnLoad_Proxy( 
    IXMLDOMSchemaCollection2 * This,
    /* [in] */ VARIANT_BOOL validateOnLoad);


void __RPC_STUB IXMLDOMSchemaCollection2_put_validateOnLoad_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection2_get_validateOnLoad_Proxy( 
    IXMLDOMSchemaCollection2 * This,
    /* [retval][out] */ VARIANT_BOOL *validateOnLoad);


void __RPC_STUB IXMLDOMSchemaCollection2_get_validateOnLoad_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection2_getSchema_Proxy( 
    IXMLDOMSchemaCollection2 * This,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ ISchema **schema);


void __RPC_STUB IXMLDOMSchemaCollection2_getSchema_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSchemaCollection2_getDeclaration_Proxy( 
    IXMLDOMSchemaCollection2 * This,
    /* [in] */ IXMLDOMNode *node,
    /* [retval][out] */ ISchemaItem **item);


void __RPC_STUB IXMLDOMSchemaCollection2_getDeclaration_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMSchemaCollection2_INTERFACE_DEFINED__ */


#ifndef __ISchemaStringCollection_INTERFACE_DEFINED__
#define __ISchemaStringCollection_INTERFACE_DEFINED__

/* interface ISchemaStringCollection */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaStringCollection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b1-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaStringCollection : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_item( 
            /* [in] */ long index,
            /* [retval][out] */ BSTR *bstr) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *length) = 0;
        
        virtual /* [propget][restricted][hidden][id] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [retval][out] */ IUnknown **ppunk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaStringCollectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaStringCollection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaStringCollection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaStringCollection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaStringCollection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaStringCollection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaStringCollection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaStringCollection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            ISchemaStringCollection * This,
            /* [in] */ long index,
            /* [retval][out] */ BSTR *bstr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            ISchemaStringCollection * This,
            /* [retval][out] */ long *length);
        
        /* [propget][restricted][hidden][id] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            ISchemaStringCollection * This,
            /* [retval][out] */ IUnknown **ppunk);
        
        END_INTERFACE
    } ISchemaStringCollectionVtbl;

    interface ISchemaStringCollection
    {
        CONST_VTBL struct ISchemaStringCollectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaStringCollection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaStringCollection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaStringCollection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaStringCollection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaStringCollection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaStringCollection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaStringCollection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaStringCollection_get_item(This,index,bstr)	\
    (This)->lpVtbl -> get_item(This,index,bstr)

#define ISchemaStringCollection_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define ISchemaStringCollection_get__newEnum(This,ppunk)	\
    (This)->lpVtbl -> get__newEnum(This,ppunk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaStringCollection_get_item_Proxy( 
    ISchemaStringCollection * This,
    /* [in] */ long index,
    /* [retval][out] */ BSTR *bstr);


void __RPC_STUB ISchemaStringCollection_get_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaStringCollection_get_length_Proxy( 
    ISchemaStringCollection * This,
    /* [retval][out] */ long *length);


void __RPC_STUB ISchemaStringCollection_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][restricted][hidden][id] */ HRESULT STDMETHODCALLTYPE ISchemaStringCollection_get__newEnum_Proxy( 
    ISchemaStringCollection * This,
    /* [retval][out] */ IUnknown **ppunk);


void __RPC_STUB ISchemaStringCollection_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaStringCollection_INTERFACE_DEFINED__ */


#ifndef __ISchemaItemCollection_INTERFACE_DEFINED__
#define __ISchemaItemCollection_INTERFACE_DEFINED__

/* interface ISchemaItemCollection */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaItemCollection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b2-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaItemCollection : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_item( 
            /* [in] */ long index,
            /* [retval][out] */ ISchemaItem **item) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE itemByName( 
            /* [in] */ BSTR name,
            /* [retval][out] */ ISchemaItem **item) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE itemByQName( 
            /* [in] */ BSTR name,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ ISchemaItem **item) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *length) = 0;
        
        virtual /* [propget][restricted][hidden][id] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [retval][out] */ IUnknown **ppunk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaItemCollectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaItemCollection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaItemCollection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaItemCollection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaItemCollection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaItemCollection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaItemCollection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaItemCollection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            ISchemaItemCollection * This,
            /* [in] */ long index,
            /* [retval][out] */ ISchemaItem **item);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *itemByName )( 
            ISchemaItemCollection * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ ISchemaItem **item);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *itemByQName )( 
            ISchemaItemCollection * This,
            /* [in] */ BSTR name,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ ISchemaItem **item);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            ISchemaItemCollection * This,
            /* [retval][out] */ long *length);
        
        /* [propget][restricted][hidden][id] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            ISchemaItemCollection * This,
            /* [retval][out] */ IUnknown **ppunk);
        
        END_INTERFACE
    } ISchemaItemCollectionVtbl;

    interface ISchemaItemCollection
    {
        CONST_VTBL struct ISchemaItemCollectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaItemCollection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaItemCollection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaItemCollection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaItemCollection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaItemCollection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaItemCollection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaItemCollection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaItemCollection_get_item(This,index,item)	\
    (This)->lpVtbl -> get_item(This,index,item)

#define ISchemaItemCollection_itemByName(This,name,item)	\
    (This)->lpVtbl -> itemByName(This,name,item)

#define ISchemaItemCollection_itemByQName(This,name,namespaceURI,item)	\
    (This)->lpVtbl -> itemByQName(This,name,namespaceURI,item)

#define ISchemaItemCollection_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define ISchemaItemCollection_get__newEnum(This,ppunk)	\
    (This)->lpVtbl -> get__newEnum(This,ppunk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItemCollection_get_item_Proxy( 
    ISchemaItemCollection * This,
    /* [in] */ long index,
    /* [retval][out] */ ISchemaItem **item);


void __RPC_STUB ISchemaItemCollection_get_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE ISchemaItemCollection_itemByName_Proxy( 
    ISchemaItemCollection * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ ISchemaItem **item);


void __RPC_STUB ISchemaItemCollection_itemByName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE ISchemaItemCollection_itemByQName_Proxy( 
    ISchemaItemCollection * This,
    /* [in] */ BSTR name,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ ISchemaItem **item);


void __RPC_STUB ISchemaItemCollection_itemByQName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItemCollection_get_length_Proxy( 
    ISchemaItemCollection * This,
    /* [retval][out] */ long *length);


void __RPC_STUB ISchemaItemCollection_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][restricted][hidden][id] */ HRESULT STDMETHODCALLTYPE ISchemaItemCollection_get__newEnum_Proxy( 
    ISchemaItemCollection * This,
    /* [retval][out] */ IUnknown **ppunk);


void __RPC_STUB ISchemaItemCollection_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaItemCollection_INTERFACE_DEFINED__ */


#ifndef __ISchemaItem_INTERFACE_DEFINED__
#define __ISchemaItem_INTERFACE_DEFINED__

/* interface ISchemaItem */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaItem;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b3-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaItem : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_name( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_namespaceURI( 
            /* [retval][out] */ BSTR *namespaceURI) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_schema( 
            /* [retval][out] */ ISchema **schema) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_id( 
            /* [retval][out] */ BSTR *id) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_itemType( 
            /* [retval][out] */ SOMITEMTYPE *itemType) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_unhandledAttributes( 
            /* [retval][out] */ IVBSAXAttributes **attributes) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE writeAnnotation( 
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaItemVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaItem * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaItem * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaItem * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaItem * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaItem * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaItem * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaItem * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaItem * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaItem * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaItem * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaItem * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaItem * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaItem * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaItem * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        END_INTERFACE
    } ISchemaItemVtbl;

    interface ISchemaItem
    {
        CONST_VTBL struct ISchemaItemVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaItem_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaItem_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaItem_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaItem_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaItem_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaItem_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaItem_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaItem_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaItem_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaItem_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaItem_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaItem_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaItem_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaItem_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_name_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ BSTR *name);


void __RPC_STUB ISchemaItem_get_name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_namespaceURI_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ BSTR *namespaceURI);


void __RPC_STUB ISchemaItem_get_namespaceURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_schema_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ ISchema **schema);


void __RPC_STUB ISchemaItem_get_schema_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_id_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ BSTR *id);


void __RPC_STUB ISchemaItem_get_id_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_itemType_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ SOMITEMTYPE *itemType);


void __RPC_STUB ISchemaItem_get_itemType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_get_unhandledAttributes_Proxy( 
    ISchemaItem * This,
    /* [retval][out] */ IVBSAXAttributes **attributes);


void __RPC_STUB ISchemaItem_get_unhandledAttributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE ISchemaItem_writeAnnotation_Proxy( 
    ISchemaItem * This,
    /* [in] */ IUnknown *annotationSink,
    /* [retval][out] */ VARIANT_BOOL *isWritten);


void __RPC_STUB ISchemaItem_writeAnnotation_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaItem_INTERFACE_DEFINED__ */


#ifndef __ISchema_INTERFACE_DEFINED__
#define __ISchema_INTERFACE_DEFINED__

/* interface ISchema */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchema;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b4-dd1b-4664-9a50-c2f40f4bd79a")
    ISchema : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_targetNamespace( 
            /* [retval][out] */ BSTR *targetNamespace) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [retval][out] */ BSTR *version) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_types( 
            /* [retval][out] */ ISchemaItemCollection **types) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_elements( 
            /* [retval][out] */ ISchemaItemCollection **elements) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [retval][out] */ ISchemaItemCollection **attributes) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_attributeGroups( 
            /* [retval][out] */ ISchemaItemCollection **attributeGroups) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_modelGroups( 
            /* [retval][out] */ ISchemaItemCollection **modelGroups) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_notations( 
            /* [retval][out] */ ISchemaItemCollection **notations) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_schemaLocations( 
            /* [retval][out] */ ISchemaStringCollection **schemaLocations) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchema * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchema * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchema * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchema * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchema * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchema * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchema * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchema * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchema * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchema * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchema * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchema * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchema * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchema * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_targetNamespace )( 
            ISchema * This,
            /* [retval][out] */ BSTR *targetNamespace);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            ISchema * This,
            /* [retval][out] */ BSTR *version);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_types )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **types);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_elements )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **elements);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **attributes);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_attributeGroups )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **attributeGroups);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_modelGroups )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **modelGroups);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_notations )( 
            ISchema * This,
            /* [retval][out] */ ISchemaItemCollection **notations);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schemaLocations )( 
            ISchema * This,
            /* [retval][out] */ ISchemaStringCollection **schemaLocations);
        
        END_INTERFACE
    } ISchemaVtbl;

    interface ISchema
    {
        CONST_VTBL struct ISchemaVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchema_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchema_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchema_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchema_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchema_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchema_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchema_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchema_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchema_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchema_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchema_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchema_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchema_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchema_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchema_get_targetNamespace(This,targetNamespace)	\
    (This)->lpVtbl -> get_targetNamespace(This,targetNamespace)

#define ISchema_get_version(This,version)	\
    (This)->lpVtbl -> get_version(This,version)

#define ISchema_get_types(This,types)	\
    (This)->lpVtbl -> get_types(This,types)

#define ISchema_get_elements(This,elements)	\
    (This)->lpVtbl -> get_elements(This,elements)

#define ISchema_get_attributes(This,attributes)	\
    (This)->lpVtbl -> get_attributes(This,attributes)

#define ISchema_get_attributeGroups(This,attributeGroups)	\
    (This)->lpVtbl -> get_attributeGroups(This,attributeGroups)

#define ISchema_get_modelGroups(This,modelGroups)	\
    (This)->lpVtbl -> get_modelGroups(This,modelGroups)

#define ISchema_get_notations(This,notations)	\
    (This)->lpVtbl -> get_notations(This,notations)

#define ISchema_get_schemaLocations(This,schemaLocations)	\
    (This)->lpVtbl -> get_schemaLocations(This,schemaLocations)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_targetNamespace_Proxy( 
    ISchema * This,
    /* [retval][out] */ BSTR *targetNamespace);


void __RPC_STUB ISchema_get_targetNamespace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_version_Proxy( 
    ISchema * This,
    /* [retval][out] */ BSTR *version);


void __RPC_STUB ISchema_get_version_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_types_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **types);


void __RPC_STUB ISchema_get_types_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_elements_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **elements);


void __RPC_STUB ISchema_get_elements_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_attributes_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **attributes);


void __RPC_STUB ISchema_get_attributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_attributeGroups_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **attributeGroups);


void __RPC_STUB ISchema_get_attributeGroups_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_modelGroups_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **modelGroups);


void __RPC_STUB ISchema_get_modelGroups_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_notations_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaItemCollection **notations);


void __RPC_STUB ISchema_get_notations_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchema_get_schemaLocations_Proxy( 
    ISchema * This,
    /* [retval][out] */ ISchemaStringCollection **schemaLocations);


void __RPC_STUB ISchema_get_schemaLocations_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchema_INTERFACE_DEFINED__ */


#ifndef __ISchemaParticle_INTERFACE_DEFINED__
#define __ISchemaParticle_INTERFACE_DEFINED__

/* interface ISchemaParticle */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaParticle;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b5-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaParticle : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_minOccurs( 
            /* [retval][out] */ VARIANT *minOccurs) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_maxOccurs( 
            /* [retval][out] */ VARIANT *maxOccurs) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaParticleVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaParticle * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaParticle * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaParticle * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaParticle * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaParticle * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaParticle * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaParticle * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaParticle * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaParticle * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaParticle * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaParticle * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaParticle * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaParticle * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaParticle * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minOccurs )( 
            ISchemaParticle * This,
            /* [retval][out] */ VARIANT *minOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxOccurs )( 
            ISchemaParticle * This,
            /* [retval][out] */ VARIANT *maxOccurs);
        
        END_INTERFACE
    } ISchemaParticleVtbl;

    interface ISchemaParticle
    {
        CONST_VTBL struct ISchemaParticleVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaParticle_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaParticle_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaParticle_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaParticle_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaParticle_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaParticle_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaParticle_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaParticle_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaParticle_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaParticle_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaParticle_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaParticle_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaParticle_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaParticle_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaParticle_get_minOccurs(This,minOccurs)	\
    (This)->lpVtbl -> get_minOccurs(This,minOccurs)

#define ISchemaParticle_get_maxOccurs(This,maxOccurs)	\
    (This)->lpVtbl -> get_maxOccurs(This,maxOccurs)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaParticle_get_minOccurs_Proxy( 
    ISchemaParticle * This,
    /* [retval][out] */ VARIANT *minOccurs);


void __RPC_STUB ISchemaParticle_get_minOccurs_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaParticle_get_maxOccurs_Proxy( 
    ISchemaParticle * This,
    /* [retval][out] */ VARIANT *maxOccurs);


void __RPC_STUB ISchemaParticle_get_maxOccurs_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaParticle_INTERFACE_DEFINED__ */


#ifndef __ISchemaAttribute_INTERFACE_DEFINED__
#define __ISchemaAttribute_INTERFACE_DEFINED__

/* interface ISchemaAttribute */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaAttribute;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b6-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaAttribute : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_type( 
            /* [retval][out] */ ISchemaType **type) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_scope( 
            /* [retval][out] */ ISchemaComplexType **scope) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_defaultValue( 
            /* [retval][out] */ BSTR *defaultValue) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_fixedValue( 
            /* [retval][out] */ BSTR *fixedValue) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_use( 
            /* [retval][out] */ SCHEMAUSE *use) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_isReference( 
            /* [retval][out] */ VARIANT_BOOL *reference) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaAttributeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaAttribute * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaAttribute * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaAttribute * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaAttribute * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaAttribute * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaAttribute * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaAttribute * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaAttribute * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaAttribute * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaAttribute * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaAttribute * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaAttribute * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaAttribute * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaAttribute * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_type )( 
            ISchemaAttribute * This,
            /* [retval][out] */ ISchemaType **type);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_scope )( 
            ISchemaAttribute * This,
            /* [retval][out] */ ISchemaComplexType **scope);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_defaultValue )( 
            ISchemaAttribute * This,
            /* [retval][out] */ BSTR *defaultValue);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_fixedValue )( 
            ISchemaAttribute * This,
            /* [retval][out] */ BSTR *fixedValue);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_use )( 
            ISchemaAttribute * This,
            /* [retval][out] */ SCHEMAUSE *use);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_isReference )( 
            ISchemaAttribute * This,
            /* [retval][out] */ VARIANT_BOOL *reference);
        
        END_INTERFACE
    } ISchemaAttributeVtbl;

    interface ISchemaAttribute
    {
        CONST_VTBL struct ISchemaAttributeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaAttribute_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaAttribute_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaAttribute_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaAttribute_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaAttribute_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaAttribute_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaAttribute_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaAttribute_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaAttribute_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaAttribute_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaAttribute_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaAttribute_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaAttribute_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaAttribute_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaAttribute_get_type(This,type)	\
    (This)->lpVtbl -> get_type(This,type)

#define ISchemaAttribute_get_scope(This,scope)	\
    (This)->lpVtbl -> get_scope(This,scope)

#define ISchemaAttribute_get_defaultValue(This,defaultValue)	\
    (This)->lpVtbl -> get_defaultValue(This,defaultValue)

#define ISchemaAttribute_get_fixedValue(This,fixedValue)	\
    (This)->lpVtbl -> get_fixedValue(This,fixedValue)

#define ISchemaAttribute_get_use(This,use)	\
    (This)->lpVtbl -> get_use(This,use)

#define ISchemaAttribute_get_isReference(This,reference)	\
    (This)->lpVtbl -> get_isReference(This,reference)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_type_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ ISchemaType **type);


void __RPC_STUB ISchemaAttribute_get_type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_scope_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ ISchemaComplexType **scope);


void __RPC_STUB ISchemaAttribute_get_scope_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_defaultValue_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ BSTR *defaultValue);


void __RPC_STUB ISchemaAttribute_get_defaultValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_fixedValue_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ BSTR *fixedValue);


void __RPC_STUB ISchemaAttribute_get_fixedValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_use_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ SCHEMAUSE *use);


void __RPC_STUB ISchemaAttribute_get_use_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttribute_get_isReference_Proxy( 
    ISchemaAttribute * This,
    /* [retval][out] */ VARIANT_BOOL *reference);


void __RPC_STUB ISchemaAttribute_get_isReference_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaAttribute_INTERFACE_DEFINED__ */


#ifndef __ISchemaElement_INTERFACE_DEFINED__
#define __ISchemaElement_INTERFACE_DEFINED__

/* interface ISchemaElement */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaElement;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b7-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaElement : public ISchemaParticle
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_type( 
            /* [retval][out] */ ISchemaType **type) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_scope( 
            /* [retval][out] */ ISchemaComplexType **scope) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_defaultValue( 
            /* [retval][out] */ BSTR *defaultValue) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_fixedValue( 
            /* [retval][out] */ BSTR *fixedValue) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_isNillable( 
            /* [retval][out] */ VARIANT_BOOL *nillable) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_identityConstraints( 
            /* [retval][out] */ ISchemaItemCollection **constraints) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_substitutionGroup( 
            /* [retval][out] */ ISchemaElement **element) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_substitutionGroupExclusions( 
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *exclusions) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_disallowedSubstitutions( 
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *disallowed) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_isAbstract( 
            /* [retval][out] */ VARIANT_BOOL *abstract) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_isReference( 
            /* [retval][out] */ VARIANT_BOOL *reference) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaElementVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaElement * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaElement * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaElement * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaElement * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaElement * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaElement * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaElement * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaElement * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaElement * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaElement * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaElement * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaElement * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaElement * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaElement * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minOccurs )( 
            ISchemaElement * This,
            /* [retval][out] */ VARIANT *minOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxOccurs )( 
            ISchemaElement * This,
            /* [retval][out] */ VARIANT *maxOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_type )( 
            ISchemaElement * This,
            /* [retval][out] */ ISchemaType **type);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_scope )( 
            ISchemaElement * This,
            /* [retval][out] */ ISchemaComplexType **scope);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_defaultValue )( 
            ISchemaElement * This,
            /* [retval][out] */ BSTR *defaultValue);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_fixedValue )( 
            ISchemaElement * This,
            /* [retval][out] */ BSTR *fixedValue);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_isNillable )( 
            ISchemaElement * This,
            /* [retval][out] */ VARIANT_BOOL *nillable);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_identityConstraints )( 
            ISchemaElement * This,
            /* [retval][out] */ ISchemaItemCollection **constraints);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_substitutionGroup )( 
            ISchemaElement * This,
            /* [retval][out] */ ISchemaElement **element);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_substitutionGroupExclusions )( 
            ISchemaElement * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *exclusions);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_disallowedSubstitutions )( 
            ISchemaElement * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *disallowed);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_isAbstract )( 
            ISchemaElement * This,
            /* [retval][out] */ VARIANT_BOOL *abstract);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_isReference )( 
            ISchemaElement * This,
            /* [retval][out] */ VARIANT_BOOL *reference);
        
        END_INTERFACE
    } ISchemaElementVtbl;

    interface ISchemaElement
    {
        CONST_VTBL struct ISchemaElementVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaElement_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaElement_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaElement_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaElement_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaElement_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaElement_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaElement_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaElement_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaElement_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaElement_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaElement_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaElement_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaElement_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaElement_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaElement_get_minOccurs(This,minOccurs)	\
    (This)->lpVtbl -> get_minOccurs(This,minOccurs)

#define ISchemaElement_get_maxOccurs(This,maxOccurs)	\
    (This)->lpVtbl -> get_maxOccurs(This,maxOccurs)


#define ISchemaElement_get_type(This,type)	\
    (This)->lpVtbl -> get_type(This,type)

#define ISchemaElement_get_scope(This,scope)	\
    (This)->lpVtbl -> get_scope(This,scope)

#define ISchemaElement_get_defaultValue(This,defaultValue)	\
    (This)->lpVtbl -> get_defaultValue(This,defaultValue)

#define ISchemaElement_get_fixedValue(This,fixedValue)	\
    (This)->lpVtbl -> get_fixedValue(This,fixedValue)

#define ISchemaElement_get_isNillable(This,nillable)	\
    (This)->lpVtbl -> get_isNillable(This,nillable)

#define ISchemaElement_get_identityConstraints(This,constraints)	\
    (This)->lpVtbl -> get_identityConstraints(This,constraints)

#define ISchemaElement_get_substitutionGroup(This,element)	\
    (This)->lpVtbl -> get_substitutionGroup(This,element)

#define ISchemaElement_get_substitutionGroupExclusions(This,exclusions)	\
    (This)->lpVtbl -> get_substitutionGroupExclusions(This,exclusions)

#define ISchemaElement_get_disallowedSubstitutions(This,disallowed)	\
    (This)->lpVtbl -> get_disallowedSubstitutions(This,disallowed)

#define ISchemaElement_get_isAbstract(This,abstract)	\
    (This)->lpVtbl -> get_isAbstract(This,abstract)

#define ISchemaElement_get_isReference(This,reference)	\
    (This)->lpVtbl -> get_isReference(This,reference)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_type_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ ISchemaType **type);


void __RPC_STUB ISchemaElement_get_type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_scope_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ ISchemaComplexType **scope);


void __RPC_STUB ISchemaElement_get_scope_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_defaultValue_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ BSTR *defaultValue);


void __RPC_STUB ISchemaElement_get_defaultValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_fixedValue_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ BSTR *fixedValue);


void __RPC_STUB ISchemaElement_get_fixedValue_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_isNillable_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ VARIANT_BOOL *nillable);


void __RPC_STUB ISchemaElement_get_isNillable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_identityConstraints_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ ISchemaItemCollection **constraints);


void __RPC_STUB ISchemaElement_get_identityConstraints_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_substitutionGroup_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ ISchemaElement **element);


void __RPC_STUB ISchemaElement_get_substitutionGroup_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_substitutionGroupExclusions_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ SCHEMADERIVATIONMETHOD *exclusions);


void __RPC_STUB ISchemaElement_get_substitutionGroupExclusions_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_disallowedSubstitutions_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ SCHEMADERIVATIONMETHOD *disallowed);


void __RPC_STUB ISchemaElement_get_disallowedSubstitutions_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_isAbstract_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ VARIANT_BOOL *abstract);


void __RPC_STUB ISchemaElement_get_isAbstract_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaElement_get_isReference_Proxy( 
    ISchemaElement * This,
    /* [retval][out] */ VARIANT_BOOL *reference);


void __RPC_STUB ISchemaElement_get_isReference_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaElement_INTERFACE_DEFINED__ */


#ifndef __ISchemaType_INTERFACE_DEFINED__
#define __ISchemaType_INTERFACE_DEFINED__

/* interface ISchemaType */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaType;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b8-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaType : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_baseTypes( 
            /* [retval][out] */ ISchemaItemCollection **baseTypes) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_final( 
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *final) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_variety( 
            /* [retval][out] */ SCHEMATYPEVARIETY *variety) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_derivedBy( 
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *derivedBy) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE isValid( 
            /* [in] */ BSTR data,
            /* [retval][out] */ VARIANT_BOOL *valid) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_minExclusive( 
            /* [retval][out] */ BSTR *minExclusive) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_minInclusive( 
            /* [retval][out] */ BSTR *minInclusive) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_maxExclusive( 
            /* [retval][out] */ BSTR *maxExclusive) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_maxInclusive( 
            /* [retval][out] */ BSTR *maxInclusive) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_totalDigits( 
            /* [retval][out] */ VARIANT *totalDigits) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_fractionDigits( 
            /* [retval][out] */ VARIANT *fractionDigits) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ VARIANT *length) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_minLength( 
            /* [retval][out] */ VARIANT *minLength) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_maxLength( 
            /* [retval][out] */ VARIANT *maxLength) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_enumeration( 
            /* [retval][out] */ ISchemaStringCollection **enumeration) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_whitespace( 
            /* [retval][out] */ SCHEMAWHITESPACE *whitespace) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_patterns( 
            /* [retval][out] */ ISchemaStringCollection **patterns) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaTypeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaType * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaType * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaType * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaType * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaType * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaType * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaType * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaType * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaType * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaType * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaType * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_baseTypes )( 
            ISchemaType * This,
            /* [retval][out] */ ISchemaItemCollection **baseTypes);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_final )( 
            ISchemaType * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *final);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_variety )( 
            ISchemaType * This,
            /* [retval][out] */ SCHEMATYPEVARIETY *variety);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_derivedBy )( 
            ISchemaType * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *derivedBy);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *isValid )( 
            ISchemaType * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ VARIANT_BOOL *valid);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minExclusive )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *minExclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minInclusive )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *minInclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxExclusive )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *maxExclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxInclusive )( 
            ISchemaType * This,
            /* [retval][out] */ BSTR *maxInclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_totalDigits )( 
            ISchemaType * This,
            /* [retval][out] */ VARIANT *totalDigits);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_fractionDigits )( 
            ISchemaType * This,
            /* [retval][out] */ VARIANT *fractionDigits);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            ISchemaType * This,
            /* [retval][out] */ VARIANT *length);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minLength )( 
            ISchemaType * This,
            /* [retval][out] */ VARIANT *minLength);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxLength )( 
            ISchemaType * This,
            /* [retval][out] */ VARIANT *maxLength);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_enumeration )( 
            ISchemaType * This,
            /* [retval][out] */ ISchemaStringCollection **enumeration);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_whitespace )( 
            ISchemaType * This,
            /* [retval][out] */ SCHEMAWHITESPACE *whitespace);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_patterns )( 
            ISchemaType * This,
            /* [retval][out] */ ISchemaStringCollection **patterns);
        
        END_INTERFACE
    } ISchemaTypeVtbl;

    interface ISchemaType
    {
        CONST_VTBL struct ISchemaTypeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaType_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaType_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaType_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaType_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaType_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaType_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaType_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaType_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaType_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaType_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaType_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaType_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaType_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaType_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaType_get_baseTypes(This,baseTypes)	\
    (This)->lpVtbl -> get_baseTypes(This,baseTypes)

#define ISchemaType_get_final(This,final)	\
    (This)->lpVtbl -> get_final(This,final)

#define ISchemaType_get_variety(This,variety)	\
    (This)->lpVtbl -> get_variety(This,variety)

#define ISchemaType_get_derivedBy(This,derivedBy)	\
    (This)->lpVtbl -> get_derivedBy(This,derivedBy)

#define ISchemaType_isValid(This,data,valid)	\
    (This)->lpVtbl -> isValid(This,data,valid)

#define ISchemaType_get_minExclusive(This,minExclusive)	\
    (This)->lpVtbl -> get_minExclusive(This,minExclusive)

#define ISchemaType_get_minInclusive(This,minInclusive)	\
    (This)->lpVtbl -> get_minInclusive(This,minInclusive)

#define ISchemaType_get_maxExclusive(This,maxExclusive)	\
    (This)->lpVtbl -> get_maxExclusive(This,maxExclusive)

#define ISchemaType_get_maxInclusive(This,maxInclusive)	\
    (This)->lpVtbl -> get_maxInclusive(This,maxInclusive)

#define ISchemaType_get_totalDigits(This,totalDigits)	\
    (This)->lpVtbl -> get_totalDigits(This,totalDigits)

#define ISchemaType_get_fractionDigits(This,fractionDigits)	\
    (This)->lpVtbl -> get_fractionDigits(This,fractionDigits)

#define ISchemaType_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define ISchemaType_get_minLength(This,minLength)	\
    (This)->lpVtbl -> get_minLength(This,minLength)

#define ISchemaType_get_maxLength(This,maxLength)	\
    (This)->lpVtbl -> get_maxLength(This,maxLength)

#define ISchemaType_get_enumeration(This,enumeration)	\
    (This)->lpVtbl -> get_enumeration(This,enumeration)

#define ISchemaType_get_whitespace(This,whitespace)	\
    (This)->lpVtbl -> get_whitespace(This,whitespace)

#define ISchemaType_get_patterns(This,patterns)	\
    (This)->lpVtbl -> get_patterns(This,patterns)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_baseTypes_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ ISchemaItemCollection **baseTypes);


void __RPC_STUB ISchemaType_get_baseTypes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_final_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ SCHEMADERIVATIONMETHOD *final);


void __RPC_STUB ISchemaType_get_final_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_variety_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ SCHEMATYPEVARIETY *variety);


void __RPC_STUB ISchemaType_get_variety_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_derivedBy_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ SCHEMADERIVATIONMETHOD *derivedBy);


void __RPC_STUB ISchemaType_get_derivedBy_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE ISchemaType_isValid_Proxy( 
    ISchemaType * This,
    /* [in] */ BSTR data,
    /* [retval][out] */ VARIANT_BOOL *valid);


void __RPC_STUB ISchemaType_isValid_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_minExclusive_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ BSTR *minExclusive);


void __RPC_STUB ISchemaType_get_minExclusive_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_minInclusive_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ BSTR *minInclusive);


void __RPC_STUB ISchemaType_get_minInclusive_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_maxExclusive_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ BSTR *maxExclusive);


void __RPC_STUB ISchemaType_get_maxExclusive_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_maxInclusive_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ BSTR *maxInclusive);


void __RPC_STUB ISchemaType_get_maxInclusive_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_totalDigits_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ VARIANT *totalDigits);


void __RPC_STUB ISchemaType_get_totalDigits_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_fractionDigits_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ VARIANT *fractionDigits);


void __RPC_STUB ISchemaType_get_fractionDigits_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_length_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ VARIANT *length);


void __RPC_STUB ISchemaType_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_minLength_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ VARIANT *minLength);


void __RPC_STUB ISchemaType_get_minLength_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_maxLength_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ VARIANT *maxLength);


void __RPC_STUB ISchemaType_get_maxLength_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_enumeration_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ ISchemaStringCollection **enumeration);


void __RPC_STUB ISchemaType_get_enumeration_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_whitespace_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ SCHEMAWHITESPACE *whitespace);


void __RPC_STUB ISchemaType_get_whitespace_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaType_get_patterns_Proxy( 
    ISchemaType * This,
    /* [retval][out] */ ISchemaStringCollection **patterns);


void __RPC_STUB ISchemaType_get_patterns_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaType_INTERFACE_DEFINED__ */


#ifndef __ISchemaComplexType_INTERFACE_DEFINED__
#define __ISchemaComplexType_INTERFACE_DEFINED__

/* interface ISchemaComplexType */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaComplexType;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08b9-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaComplexType : public ISchemaType
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_isAbstract( 
            /* [retval][out] */ VARIANT_BOOL *abstract) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_anyAttribute( 
            /* [retval][out] */ ISchemaAny **anyAttribute) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [retval][out] */ ISchemaItemCollection **attributes) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_contentType( 
            /* [retval][out] */ SCHEMACONTENTTYPE *contentType) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_contentModel( 
            /* [retval][out] */ ISchemaModelGroup **contentModel) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_prohibitedSubstitutions( 
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *prohibited) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaComplexTypeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaComplexType * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaComplexType * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaComplexType * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaComplexType * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaComplexType * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaComplexType * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaComplexType * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaComplexType * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaComplexType * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_baseTypes )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaItemCollection **baseTypes);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_final )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *final);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_variety )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMATYPEVARIETY *variety);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_derivedBy )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *derivedBy);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *isValid )( 
            ISchemaComplexType * This,
            /* [in] */ BSTR data,
            /* [retval][out] */ VARIANT_BOOL *valid);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minExclusive )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *minExclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minInclusive )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *minInclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxExclusive )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *maxExclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxInclusive )( 
            ISchemaComplexType * This,
            /* [retval][out] */ BSTR *maxInclusive);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_totalDigits )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT *totalDigits);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_fractionDigits )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT *fractionDigits);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT *length);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minLength )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT *minLength);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxLength )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT *maxLength);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_enumeration )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaStringCollection **enumeration);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_whitespace )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMAWHITESPACE *whitespace);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_patterns )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaStringCollection **patterns);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_isAbstract )( 
            ISchemaComplexType * This,
            /* [retval][out] */ VARIANT_BOOL *abstract);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_anyAttribute )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaAny **anyAttribute);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaItemCollection **attributes);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_contentType )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMACONTENTTYPE *contentType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_contentModel )( 
            ISchemaComplexType * This,
            /* [retval][out] */ ISchemaModelGroup **contentModel);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_prohibitedSubstitutions )( 
            ISchemaComplexType * This,
            /* [retval][out] */ SCHEMADERIVATIONMETHOD *prohibited);
        
        END_INTERFACE
    } ISchemaComplexTypeVtbl;

    interface ISchemaComplexType
    {
        CONST_VTBL struct ISchemaComplexTypeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaComplexType_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaComplexType_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaComplexType_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaComplexType_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaComplexType_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaComplexType_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaComplexType_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaComplexType_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaComplexType_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaComplexType_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaComplexType_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaComplexType_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaComplexType_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaComplexType_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaComplexType_get_baseTypes(This,baseTypes)	\
    (This)->lpVtbl -> get_baseTypes(This,baseTypes)

#define ISchemaComplexType_get_final(This,final)	\
    (This)->lpVtbl -> get_final(This,final)

#define ISchemaComplexType_get_variety(This,variety)	\
    (This)->lpVtbl -> get_variety(This,variety)

#define ISchemaComplexType_get_derivedBy(This,derivedBy)	\
    (This)->lpVtbl -> get_derivedBy(This,derivedBy)

#define ISchemaComplexType_isValid(This,data,valid)	\
    (This)->lpVtbl -> isValid(This,data,valid)

#define ISchemaComplexType_get_minExclusive(This,minExclusive)	\
    (This)->lpVtbl -> get_minExclusive(This,minExclusive)

#define ISchemaComplexType_get_minInclusive(This,minInclusive)	\
    (This)->lpVtbl -> get_minInclusive(This,minInclusive)

#define ISchemaComplexType_get_maxExclusive(This,maxExclusive)	\
    (This)->lpVtbl -> get_maxExclusive(This,maxExclusive)

#define ISchemaComplexType_get_maxInclusive(This,maxInclusive)	\
    (This)->lpVtbl -> get_maxInclusive(This,maxInclusive)

#define ISchemaComplexType_get_totalDigits(This,totalDigits)	\
    (This)->lpVtbl -> get_totalDigits(This,totalDigits)

#define ISchemaComplexType_get_fractionDigits(This,fractionDigits)	\
    (This)->lpVtbl -> get_fractionDigits(This,fractionDigits)

#define ISchemaComplexType_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define ISchemaComplexType_get_minLength(This,minLength)	\
    (This)->lpVtbl -> get_minLength(This,minLength)

#define ISchemaComplexType_get_maxLength(This,maxLength)	\
    (This)->lpVtbl -> get_maxLength(This,maxLength)

#define ISchemaComplexType_get_enumeration(This,enumeration)	\
    (This)->lpVtbl -> get_enumeration(This,enumeration)

#define ISchemaComplexType_get_whitespace(This,whitespace)	\
    (This)->lpVtbl -> get_whitespace(This,whitespace)

#define ISchemaComplexType_get_patterns(This,patterns)	\
    (This)->lpVtbl -> get_patterns(This,patterns)


#define ISchemaComplexType_get_isAbstract(This,abstract)	\
    (This)->lpVtbl -> get_isAbstract(This,abstract)

#define ISchemaComplexType_get_anyAttribute(This,anyAttribute)	\
    (This)->lpVtbl -> get_anyAttribute(This,anyAttribute)

#define ISchemaComplexType_get_attributes(This,attributes)	\
    (This)->lpVtbl -> get_attributes(This,attributes)

#define ISchemaComplexType_get_contentType(This,contentType)	\
    (This)->lpVtbl -> get_contentType(This,contentType)

#define ISchemaComplexType_get_contentModel(This,contentModel)	\
    (This)->lpVtbl -> get_contentModel(This,contentModel)

#define ISchemaComplexType_get_prohibitedSubstitutions(This,prohibited)	\
    (This)->lpVtbl -> get_prohibitedSubstitutions(This,prohibited)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_isAbstract_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ VARIANT_BOOL *abstract);


void __RPC_STUB ISchemaComplexType_get_isAbstract_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_anyAttribute_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ ISchemaAny **anyAttribute);


void __RPC_STUB ISchemaComplexType_get_anyAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_attributes_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ ISchemaItemCollection **attributes);


void __RPC_STUB ISchemaComplexType_get_attributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_contentType_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ SCHEMACONTENTTYPE *contentType);


void __RPC_STUB ISchemaComplexType_get_contentType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_contentModel_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ ISchemaModelGroup **contentModel);


void __RPC_STUB ISchemaComplexType_get_contentModel_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaComplexType_get_prohibitedSubstitutions_Proxy( 
    ISchemaComplexType * This,
    /* [retval][out] */ SCHEMADERIVATIONMETHOD *prohibited);


void __RPC_STUB ISchemaComplexType_get_prohibitedSubstitutions_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaComplexType_INTERFACE_DEFINED__ */


#ifndef __ISchemaAttributeGroup_INTERFACE_DEFINED__
#define __ISchemaAttributeGroup_INTERFACE_DEFINED__

/* interface ISchemaAttributeGroup */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaAttributeGroup;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08ba-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaAttributeGroup : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_anyAttribute( 
            /* [retval][out] */ ISchemaAny **anyAttribute) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [retval][out] */ ISchemaItemCollection **attributes) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaAttributeGroupVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaAttributeGroup * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaAttributeGroup * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaAttributeGroup * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaAttributeGroup * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaAttributeGroup * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaAttributeGroup * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaAttributeGroup * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaAttributeGroup * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_anyAttribute )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ ISchemaAny **anyAttribute);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            ISchemaAttributeGroup * This,
            /* [retval][out] */ ISchemaItemCollection **attributes);
        
        END_INTERFACE
    } ISchemaAttributeGroupVtbl;

    interface ISchemaAttributeGroup
    {
        CONST_VTBL struct ISchemaAttributeGroupVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaAttributeGroup_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaAttributeGroup_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaAttributeGroup_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaAttributeGroup_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaAttributeGroup_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaAttributeGroup_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaAttributeGroup_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaAttributeGroup_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaAttributeGroup_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaAttributeGroup_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaAttributeGroup_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaAttributeGroup_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaAttributeGroup_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaAttributeGroup_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaAttributeGroup_get_anyAttribute(This,anyAttribute)	\
    (This)->lpVtbl -> get_anyAttribute(This,anyAttribute)

#define ISchemaAttributeGroup_get_attributes(This,attributes)	\
    (This)->lpVtbl -> get_attributes(This,attributes)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttributeGroup_get_anyAttribute_Proxy( 
    ISchemaAttributeGroup * This,
    /* [retval][out] */ ISchemaAny **anyAttribute);


void __RPC_STUB ISchemaAttributeGroup_get_anyAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAttributeGroup_get_attributes_Proxy( 
    ISchemaAttributeGroup * This,
    /* [retval][out] */ ISchemaItemCollection **attributes);


void __RPC_STUB ISchemaAttributeGroup_get_attributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaAttributeGroup_INTERFACE_DEFINED__ */


#ifndef __ISchemaModelGroup_INTERFACE_DEFINED__
#define __ISchemaModelGroup_INTERFACE_DEFINED__

/* interface ISchemaModelGroup */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaModelGroup;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08bb-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaModelGroup : public ISchemaParticle
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_particles( 
            /* [retval][out] */ ISchemaItemCollection **particles) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaModelGroupVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaModelGroup * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaModelGroup * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaModelGroup * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaModelGroup * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaModelGroup * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaModelGroup * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaModelGroup * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaModelGroup * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minOccurs )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ VARIANT *minOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxOccurs )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ VARIANT *maxOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_particles )( 
            ISchemaModelGroup * This,
            /* [retval][out] */ ISchemaItemCollection **particles);
        
        END_INTERFACE
    } ISchemaModelGroupVtbl;

    interface ISchemaModelGroup
    {
        CONST_VTBL struct ISchemaModelGroupVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaModelGroup_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaModelGroup_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaModelGroup_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaModelGroup_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaModelGroup_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaModelGroup_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaModelGroup_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaModelGroup_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaModelGroup_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaModelGroup_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaModelGroup_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaModelGroup_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaModelGroup_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaModelGroup_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaModelGroup_get_minOccurs(This,minOccurs)	\
    (This)->lpVtbl -> get_minOccurs(This,minOccurs)

#define ISchemaModelGroup_get_maxOccurs(This,maxOccurs)	\
    (This)->lpVtbl -> get_maxOccurs(This,maxOccurs)


#define ISchemaModelGroup_get_particles(This,particles)	\
    (This)->lpVtbl -> get_particles(This,particles)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaModelGroup_get_particles_Proxy( 
    ISchemaModelGroup * This,
    /* [retval][out] */ ISchemaItemCollection **particles);


void __RPC_STUB ISchemaModelGroup_get_particles_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaModelGroup_INTERFACE_DEFINED__ */


#ifndef __ISchemaAny_INTERFACE_DEFINED__
#define __ISchemaAny_INTERFACE_DEFINED__

/* interface ISchemaAny */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaAny;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08bc-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaAny : public ISchemaParticle
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_namespaces( 
            /* [retval][out] */ ISchemaStringCollection **namespaces) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_processContents( 
            /* [retval][out] */ SCHEMAPROCESSCONTENTS *processContents) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaAnyVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaAny * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaAny * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaAny * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaAny * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaAny * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaAny * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaAny * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaAny * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaAny * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaAny * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaAny * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaAny * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaAny * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaAny * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_minOccurs )( 
            ISchemaAny * This,
            /* [retval][out] */ VARIANT *minOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_maxOccurs )( 
            ISchemaAny * This,
            /* [retval][out] */ VARIANT *maxOccurs);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaces )( 
            ISchemaAny * This,
            /* [retval][out] */ ISchemaStringCollection **namespaces);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_processContents )( 
            ISchemaAny * This,
            /* [retval][out] */ SCHEMAPROCESSCONTENTS *processContents);
        
        END_INTERFACE
    } ISchemaAnyVtbl;

    interface ISchemaAny
    {
        CONST_VTBL struct ISchemaAnyVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaAny_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaAny_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaAny_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaAny_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaAny_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaAny_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaAny_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaAny_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaAny_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaAny_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaAny_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaAny_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaAny_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaAny_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaAny_get_minOccurs(This,minOccurs)	\
    (This)->lpVtbl -> get_minOccurs(This,minOccurs)

#define ISchemaAny_get_maxOccurs(This,maxOccurs)	\
    (This)->lpVtbl -> get_maxOccurs(This,maxOccurs)


#define ISchemaAny_get_namespaces(This,namespaces)	\
    (This)->lpVtbl -> get_namespaces(This,namespaces)

#define ISchemaAny_get_processContents(This,processContents)	\
    (This)->lpVtbl -> get_processContents(This,processContents)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAny_get_namespaces_Proxy( 
    ISchemaAny * This,
    /* [retval][out] */ ISchemaStringCollection **namespaces);


void __RPC_STUB ISchemaAny_get_namespaces_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaAny_get_processContents_Proxy( 
    ISchemaAny * This,
    /* [retval][out] */ SCHEMAPROCESSCONTENTS *processContents);


void __RPC_STUB ISchemaAny_get_processContents_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaAny_INTERFACE_DEFINED__ */


#ifndef __ISchemaIdentityConstraint_INTERFACE_DEFINED__
#define __ISchemaIdentityConstraint_INTERFACE_DEFINED__

/* interface ISchemaIdentityConstraint */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaIdentityConstraint;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08bd-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaIdentityConstraint : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_selector( 
            /* [retval][out] */ BSTR *selector) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_fields( 
            /* [retval][out] */ ISchemaStringCollection **fields) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_referencedKey( 
            /* [retval][out] */ ISchemaIdentityConstraint **key) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaIdentityConstraintVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaIdentityConstraint * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaIdentityConstraint * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaIdentityConstraint * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaIdentityConstraint * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaIdentityConstraint * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaIdentityConstraint * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaIdentityConstraint * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaIdentityConstraint * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_selector )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ BSTR *selector);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_fields )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ ISchemaStringCollection **fields);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_referencedKey )( 
            ISchemaIdentityConstraint * This,
            /* [retval][out] */ ISchemaIdentityConstraint **key);
        
        END_INTERFACE
    } ISchemaIdentityConstraintVtbl;

    interface ISchemaIdentityConstraint
    {
        CONST_VTBL struct ISchemaIdentityConstraintVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaIdentityConstraint_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaIdentityConstraint_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaIdentityConstraint_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaIdentityConstraint_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaIdentityConstraint_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaIdentityConstraint_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaIdentityConstraint_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaIdentityConstraint_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaIdentityConstraint_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaIdentityConstraint_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaIdentityConstraint_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaIdentityConstraint_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaIdentityConstraint_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaIdentityConstraint_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaIdentityConstraint_get_selector(This,selector)	\
    (This)->lpVtbl -> get_selector(This,selector)

#define ISchemaIdentityConstraint_get_fields(This,fields)	\
    (This)->lpVtbl -> get_fields(This,fields)

#define ISchemaIdentityConstraint_get_referencedKey(This,key)	\
    (This)->lpVtbl -> get_referencedKey(This,key)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaIdentityConstraint_get_selector_Proxy( 
    ISchemaIdentityConstraint * This,
    /* [retval][out] */ BSTR *selector);


void __RPC_STUB ISchemaIdentityConstraint_get_selector_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaIdentityConstraint_get_fields_Proxy( 
    ISchemaIdentityConstraint * This,
    /* [retval][out] */ ISchemaStringCollection **fields);


void __RPC_STUB ISchemaIdentityConstraint_get_fields_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaIdentityConstraint_get_referencedKey_Proxy( 
    ISchemaIdentityConstraint * This,
    /* [retval][out] */ ISchemaIdentityConstraint **key);


void __RPC_STUB ISchemaIdentityConstraint_get_referencedKey_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaIdentityConstraint_INTERFACE_DEFINED__ */


#ifndef __ISchemaNotation_INTERFACE_DEFINED__
#define __ISchemaNotation_INTERFACE_DEFINED__

/* interface ISchemaNotation */
/* [unique][helpstring][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_ISchemaNotation;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("50ea08be-dd1b-4664-9a50-c2f40f4bd79a")
    ISchemaNotation : public ISchemaItem
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_systemIdentifier( 
            /* [retval][out] */ BSTR *uri) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_publicIdentifier( 
            /* [retval][out] */ BSTR *uri) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISchemaNotationVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISchemaNotation * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISchemaNotation * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISchemaNotation * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISchemaNotation * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISchemaNotation * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISchemaNotation * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISchemaNotation * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            ISchemaNotation * This,
            /* [retval][out] */ BSTR *name);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_namespaceURI )( 
            ISchemaNotation * This,
            /* [retval][out] */ BSTR *namespaceURI);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_schema )( 
            ISchemaNotation * This,
            /* [retval][out] */ ISchema **schema);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_id )( 
            ISchemaNotation * This,
            /* [retval][out] */ BSTR *id);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_itemType )( 
            ISchemaNotation * This,
            /* [retval][out] */ SOMITEMTYPE *itemType);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_unhandledAttributes )( 
            ISchemaNotation * This,
            /* [retval][out] */ IVBSAXAttributes **attributes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *writeAnnotation )( 
            ISchemaNotation * This,
            /* [in] */ IUnknown *annotationSink,
            /* [retval][out] */ VARIANT_BOOL *isWritten);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_systemIdentifier )( 
            ISchemaNotation * This,
            /* [retval][out] */ BSTR *uri);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_publicIdentifier )( 
            ISchemaNotation * This,
            /* [retval][out] */ BSTR *uri);
        
        END_INTERFACE
    } ISchemaNotationVtbl;

    interface ISchemaNotation
    {
        CONST_VTBL struct ISchemaNotationVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISchemaNotation_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISchemaNotation_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISchemaNotation_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISchemaNotation_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISchemaNotation_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISchemaNotation_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISchemaNotation_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISchemaNotation_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define ISchemaNotation_get_namespaceURI(This,namespaceURI)	\
    (This)->lpVtbl -> get_namespaceURI(This,namespaceURI)

#define ISchemaNotation_get_schema(This,schema)	\
    (This)->lpVtbl -> get_schema(This,schema)

#define ISchemaNotation_get_id(This,id)	\
    (This)->lpVtbl -> get_id(This,id)

#define ISchemaNotation_get_itemType(This,itemType)	\
    (This)->lpVtbl -> get_itemType(This,itemType)

#define ISchemaNotation_get_unhandledAttributes(This,attributes)	\
    (This)->lpVtbl -> get_unhandledAttributes(This,attributes)

#define ISchemaNotation_writeAnnotation(This,annotationSink,isWritten)	\
    (This)->lpVtbl -> writeAnnotation(This,annotationSink,isWritten)


#define ISchemaNotation_get_systemIdentifier(This,uri)	\
    (This)->lpVtbl -> get_systemIdentifier(This,uri)

#define ISchemaNotation_get_publicIdentifier(This,uri)	\
    (This)->lpVtbl -> get_publicIdentifier(This,uri)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaNotation_get_systemIdentifier_Proxy( 
    ISchemaNotation * This,
    /* [retval][out] */ BSTR *uri);


void __RPC_STUB ISchemaNotation_get_systemIdentifier_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [propget][id] */ HRESULT STDMETHODCALLTYPE ISchemaNotation_get_publicIdentifier_Proxy( 
    ISchemaNotation * This,
    /* [retval][out] */ BSTR *uri);


void __RPC_STUB ISchemaNotation_get_publicIdentifier_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISchemaNotation_INTERFACE_DEFINED__ */


#ifndef __IXMLElementCollection_INTERFACE_DEFINED__
#define __IXMLElementCollection_INTERFACE_DEFINED__

/* interface IXMLElementCollection */
/* [helpstring][hidden][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLElementCollection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("65725580-9B5D-11d0-9BFE-00C04FC99C8E")
    IXMLElementCollection : public IDispatch
    {
    public:
        virtual /* [id][hidden][restricted][propput] */ HRESULT STDMETHODCALLTYPE put_length( 
            /* [in] */ long v) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [out][retval] */ long *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [out][retval] */ IUnknown **ppUnk) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE item( 
            /* [in][optional] */ VARIANT var1,
            /* [in][optional] */ VARIANT var2,
            /* [out][retval] */ IDispatch **ppDisp) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLElementCollectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLElementCollection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLElementCollection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLElementCollection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLElementCollection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLElementCollection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLElementCollection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLElementCollection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [id][hidden][restricted][propput] */ HRESULT ( STDMETHODCALLTYPE *put_length )( 
            IXMLElementCollection * This,
            /* [in] */ long v);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLElementCollection * This,
            /* [out][retval] */ long *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLElementCollection * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *item )( 
            IXMLElementCollection * This,
            /* [in][optional] */ VARIANT var1,
            /* [in][optional] */ VARIANT var2,
            /* [out][retval] */ IDispatch **ppDisp);
        
        END_INTERFACE
    } IXMLElementCollectionVtbl;

    interface IXMLElementCollection
    {
        CONST_VTBL struct IXMLElementCollectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLElementCollection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLElementCollection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLElementCollection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLElementCollection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLElementCollection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLElementCollection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLElementCollection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLElementCollection_put_length(This,v)	\
    (This)->lpVtbl -> put_length(This,v)

#define IXMLElementCollection_get_length(This,p)	\
    (This)->lpVtbl -> get_length(This,p)

#define IXMLElementCollection_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)

#define IXMLElementCollection_item(This,var1,var2,ppDisp)	\
    (This)->lpVtbl -> item(This,var1,var2,ppDisp)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id][hidden][restricted][propput] */ HRESULT STDMETHODCALLTYPE IXMLElementCollection_put_length_Proxy( 
    IXMLElementCollection * This,
    /* [in] */ long v);


void __RPC_STUB IXMLElementCollection_put_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElementCollection_get_length_Proxy( 
    IXMLElementCollection * This,
    /* [out][retval] */ long *p);


void __RPC_STUB IXMLElementCollection_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLElementCollection_get__newEnum_Proxy( 
    IXMLElementCollection * This,
    /* [out][retval] */ IUnknown **ppUnk);


void __RPC_STUB IXMLElementCollection_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElementCollection_item_Proxy( 
    IXMLElementCollection * This,
    /* [in][optional] */ VARIANT var1,
    /* [in][optional] */ VARIANT var2,
    /* [out][retval] */ IDispatch **ppDisp);


void __RPC_STUB IXMLElementCollection_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLElementCollection_INTERFACE_DEFINED__ */


#ifndef __IXMLDocument_INTERFACE_DEFINED__
#define __IXMLDocument_INTERFACE_DEFINED__

/* interface IXMLDocument */
/* [helpstring][hidden][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDocument;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("F52E2B61-18A1-11d1-B105-00805F49916B")
    IXMLDocument : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_root( 
            /* [out][retval] */ IXMLElement **p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileSize( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileModifiedDate( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileUpdatedDate( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_URL( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_URL( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [out][retval] */ long *pl) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_charset( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_charset( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_doctype( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_dtdURL( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createElement( 
            /* [in] */ VARIANT vType,
            /* [in][optional] */ VARIANT var1,
            /* [out][retval] */ IXMLElement **ppElem) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDocumentVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDocument * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDocument * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDocument * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDocument * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDocument * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDocument * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDocument * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_root )( 
            IXMLDocument * This,
            /* [out][retval] */ IXMLElement **p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileSize )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileModifiedDate )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileUpdatedDate )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_URL )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_URL )( 
            IXMLDocument * This,
            /* [in] */ BSTR p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_mimeType )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXMLDocument * This,
            /* [out][retval] */ long *pl);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_charset )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_charset )( 
            IXMLDocument * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_doctype )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dtdURL )( 
            IXMLDocument * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createElement )( 
            IXMLDocument * This,
            /* [in] */ VARIANT vType,
            /* [in][optional] */ VARIANT var1,
            /* [out][retval] */ IXMLElement **ppElem);
        
        END_INTERFACE
    } IXMLDocumentVtbl;

    interface IXMLDocument
    {
        CONST_VTBL struct IXMLDocumentVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDocument_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDocument_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDocument_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDocument_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDocument_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDocument_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDocument_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDocument_get_root(This,p)	\
    (This)->lpVtbl -> get_root(This,p)

#define IXMLDocument_get_fileSize(This,p)	\
    (This)->lpVtbl -> get_fileSize(This,p)

#define IXMLDocument_get_fileModifiedDate(This,p)	\
    (This)->lpVtbl -> get_fileModifiedDate(This,p)

#define IXMLDocument_get_fileUpdatedDate(This,p)	\
    (This)->lpVtbl -> get_fileUpdatedDate(This,p)

#define IXMLDocument_get_URL(This,p)	\
    (This)->lpVtbl -> get_URL(This,p)

#define IXMLDocument_put_URL(This,p)	\
    (This)->lpVtbl -> put_URL(This,p)

#define IXMLDocument_get_mimeType(This,p)	\
    (This)->lpVtbl -> get_mimeType(This,p)

#define IXMLDocument_get_readyState(This,pl)	\
    (This)->lpVtbl -> get_readyState(This,pl)

#define IXMLDocument_get_charset(This,p)	\
    (This)->lpVtbl -> get_charset(This,p)

#define IXMLDocument_put_charset(This,p)	\
    (This)->lpVtbl -> put_charset(This,p)

#define IXMLDocument_get_version(This,p)	\
    (This)->lpVtbl -> get_version(This,p)

#define IXMLDocument_get_doctype(This,p)	\
    (This)->lpVtbl -> get_doctype(This,p)

#define IXMLDocument_get_dtdURL(This,p)	\
    (This)->lpVtbl -> get_dtdURL(This,p)

#define IXMLDocument_createElement(This,vType,var1,ppElem)	\
    (This)->lpVtbl -> createElement(This,vType,var1,ppElem)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_root_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ IXMLElement **p);


void __RPC_STUB IXMLDocument_get_root_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_fileSize_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_fileSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_fileModifiedDate_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_fileModifiedDate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_fileUpdatedDate_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_fileUpdatedDate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_URL_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_URL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDocument_put_URL_Proxy( 
    IXMLDocument * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLDocument_put_URL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_mimeType_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_mimeType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_readyState_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ long *pl);


void __RPC_STUB IXMLDocument_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_charset_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_charset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDocument_put_charset_Proxy( 
    IXMLDocument * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLDocument_put_charset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_version_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_version_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_doctype_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_doctype_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument_get_dtdURL_Proxy( 
    IXMLDocument * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument_get_dtdURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDocument_createElement_Proxy( 
    IXMLDocument * This,
    /* [in] */ VARIANT vType,
    /* [in][optional] */ VARIANT var1,
    /* [out][retval] */ IXMLElement **ppElem);


void __RPC_STUB IXMLDocument_createElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDocument_INTERFACE_DEFINED__ */


#ifndef __IXMLDocument2_INTERFACE_DEFINED__
#define __IXMLDocument2_INTERFACE_DEFINED__

/* interface IXMLDocument2 */
/* [hidden][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDocument2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2B8DE2FE-8D2D-11d1-B2FC-00C04FD915A9")
    IXMLDocument2 : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_root( 
            /* [out][retval] */ IXMLElement2 **p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileSize( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileModifiedDate( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_fileUpdatedDate( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_URL( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_URL( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [out][retval] */ long *pl) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_charset( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_charset( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_version( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_doctype( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get_dtdURL( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE createElement( 
            /* [in] */ VARIANT vType,
            /* [in][optional] */ VARIANT var1,
            /* [out][retval] */ IXMLElement2 **ppElem) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_async( 
            /* [out][retval] */ VARIANT_BOOL *pf) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_async( 
            /* [in] */ VARIANT_BOOL f) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDocument2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDocument2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDocument2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDocument2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDocument2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDocument2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDocument2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDocument2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_root )( 
            IXMLDocument2 * This,
            /* [out][retval] */ IXMLElement2 **p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileSize )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileModifiedDate )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fileUpdatedDate )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_URL )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_URL )( 
            IXMLDocument2 * This,
            /* [in] */ BSTR p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_mimeType )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXMLDocument2 * This,
            /* [out][retval] */ long *pl);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_charset )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_charset )( 
            IXMLDocument2 * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_version )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_doctype )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get_dtdURL )( 
            IXMLDocument2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *createElement )( 
            IXMLDocument2 * This,
            /* [in] */ VARIANT vType,
            /* [in][optional] */ VARIANT var1,
            /* [out][retval] */ IXMLElement2 **ppElem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_async )( 
            IXMLDocument2 * This,
            /* [out][retval] */ VARIANT_BOOL *pf);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_async )( 
            IXMLDocument2 * This,
            /* [in] */ VARIANT_BOOL f);
        
        END_INTERFACE
    } IXMLDocument2Vtbl;

    interface IXMLDocument2
    {
        CONST_VTBL struct IXMLDocument2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDocument2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDocument2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDocument2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDocument2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDocument2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDocument2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDocument2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDocument2_get_root(This,p)	\
    (This)->lpVtbl -> get_root(This,p)

#define IXMLDocument2_get_fileSize(This,p)	\
    (This)->lpVtbl -> get_fileSize(This,p)

#define IXMLDocument2_get_fileModifiedDate(This,p)	\
    (This)->lpVtbl -> get_fileModifiedDate(This,p)

#define IXMLDocument2_get_fileUpdatedDate(This,p)	\
    (This)->lpVtbl -> get_fileUpdatedDate(This,p)

#define IXMLDocument2_get_URL(This,p)	\
    (This)->lpVtbl -> get_URL(This,p)

#define IXMLDocument2_put_URL(This,p)	\
    (This)->lpVtbl -> put_URL(This,p)

#define IXMLDocument2_get_mimeType(This,p)	\
    (This)->lpVtbl -> get_mimeType(This,p)

#define IXMLDocument2_get_readyState(This,pl)	\
    (This)->lpVtbl -> get_readyState(This,pl)

#define IXMLDocument2_get_charset(This,p)	\
    (This)->lpVtbl -> get_charset(This,p)

#define IXMLDocument2_put_charset(This,p)	\
    (This)->lpVtbl -> put_charset(This,p)

#define IXMLDocument2_get_version(This,p)	\
    (This)->lpVtbl -> get_version(This,p)

#define IXMLDocument2_get_doctype(This,p)	\
    (This)->lpVtbl -> get_doctype(This,p)

#define IXMLDocument2_get_dtdURL(This,p)	\
    (This)->lpVtbl -> get_dtdURL(This,p)

#define IXMLDocument2_createElement(This,vType,var1,ppElem)	\
    (This)->lpVtbl -> createElement(This,vType,var1,ppElem)

#define IXMLDocument2_get_async(This,pf)	\
    (This)->lpVtbl -> get_async(This,pf)

#define IXMLDocument2_put_async(This,f)	\
    (This)->lpVtbl -> put_async(This,f)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_root_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ IXMLElement2 **p);


void __RPC_STUB IXMLDocument2_get_root_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_fileSize_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_fileSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_fileModifiedDate_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_fileModifiedDate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_fileUpdatedDate_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_fileUpdatedDate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_URL_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_URL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_put_URL_Proxy( 
    IXMLDocument2 * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLDocument2_put_URL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_mimeType_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_mimeType_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_readyState_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ long *pl);


void __RPC_STUB IXMLDocument2_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_charset_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_charset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_put_charset_Proxy( 
    IXMLDocument2 * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLDocument2_put_charset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_version_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_version_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_doctype_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_doctype_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_dtdURL_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLDocument2_get_dtdURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_createElement_Proxy( 
    IXMLDocument2 * This,
    /* [in] */ VARIANT vType,
    /* [in][optional] */ VARIANT var1,
    /* [out][retval] */ IXMLElement2 **ppElem);


void __RPC_STUB IXMLDocument2_createElement_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_get_async_Proxy( 
    IXMLDocument2 * This,
    /* [out][retval] */ VARIANT_BOOL *pf);


void __RPC_STUB IXMLDocument2_get_async_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDocument2_put_async_Proxy( 
    IXMLDocument2 * This,
    /* [in] */ VARIANT_BOOL f);


void __RPC_STUB IXMLDocument2_put_async_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDocument2_INTERFACE_DEFINED__ */


#ifndef __IXMLElement_INTERFACE_DEFINED__
#define __IXMLElement_INTERFACE_DEFINED__

/* interface IXMLElement */
/* [helpstring][hidden][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLElement;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("3F7F31AC-E15F-11d0-9C25-00C04FC99C8E")
    IXMLElement : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_tagName( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_tagName( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parent( 
            /* [out][retval] */ IXMLElement **ppParent) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttribute( 
            /* [in] */ BSTR strPropertyName,
            /* [in] */ VARIANT PropertyValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getAttribute( 
            /* [in] */ BSTR strPropertyName,
            /* [out][retval] */ VARIANT *PropertyValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAttribute( 
            /* [in] */ BSTR strPropertyName) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_children( 
            /* [out][retval] */ IXMLElementCollection **pp) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_type( 
            /* [out][retval] */ long *plType) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_text( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_text( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addChild( 
            /* [in] */ IXMLElement *pChildElem,
            long lIndex,
            long lReserved) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeChild( 
            /* [in] */ IXMLElement *pChildElem) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLElementVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLElement * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLElement * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLElement * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLElement * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLElement * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLElement * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLElement * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_tagName )( 
            IXMLElement * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_tagName )( 
            IXMLElement * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parent )( 
            IXMLElement * This,
            /* [out][retval] */ IXMLElement **ppParent);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttribute )( 
            IXMLElement * This,
            /* [in] */ BSTR strPropertyName,
            /* [in] */ VARIANT PropertyValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAttribute )( 
            IXMLElement * This,
            /* [in] */ BSTR strPropertyName,
            /* [out][retval] */ VARIANT *PropertyValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAttribute )( 
            IXMLElement * This,
            /* [in] */ BSTR strPropertyName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_children )( 
            IXMLElement * This,
            /* [out][retval] */ IXMLElementCollection **pp);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_type )( 
            IXMLElement * This,
            /* [out][retval] */ long *plType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLElement * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLElement * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addChild )( 
            IXMLElement * This,
            /* [in] */ IXMLElement *pChildElem,
            long lIndex,
            long lReserved);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLElement * This,
            /* [in] */ IXMLElement *pChildElem);
        
        END_INTERFACE
    } IXMLElementVtbl;

    interface IXMLElement
    {
        CONST_VTBL struct IXMLElementVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLElement_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLElement_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLElement_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLElement_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLElement_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLElement_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLElement_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLElement_get_tagName(This,p)	\
    (This)->lpVtbl -> get_tagName(This,p)

#define IXMLElement_put_tagName(This,p)	\
    (This)->lpVtbl -> put_tagName(This,p)

#define IXMLElement_get_parent(This,ppParent)	\
    (This)->lpVtbl -> get_parent(This,ppParent)

#define IXMLElement_setAttribute(This,strPropertyName,PropertyValue)	\
    (This)->lpVtbl -> setAttribute(This,strPropertyName,PropertyValue)

#define IXMLElement_getAttribute(This,strPropertyName,PropertyValue)	\
    (This)->lpVtbl -> getAttribute(This,strPropertyName,PropertyValue)

#define IXMLElement_removeAttribute(This,strPropertyName)	\
    (This)->lpVtbl -> removeAttribute(This,strPropertyName)

#define IXMLElement_get_children(This,pp)	\
    (This)->lpVtbl -> get_children(This,pp)

#define IXMLElement_get_type(This,plType)	\
    (This)->lpVtbl -> get_type(This,plType)

#define IXMLElement_get_text(This,p)	\
    (This)->lpVtbl -> get_text(This,p)

#define IXMLElement_put_text(This,p)	\
    (This)->lpVtbl -> put_text(This,p)

#define IXMLElement_addChild(This,pChildElem,lIndex,lReserved)	\
    (This)->lpVtbl -> addChild(This,pChildElem,lIndex,lReserved)

#define IXMLElement_removeChild(This,pChildElem)	\
    (This)->lpVtbl -> removeChild(This,pChildElem)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement_get_tagName_Proxy( 
    IXMLElement * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLElement_get_tagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLElement_put_tagName_Proxy( 
    IXMLElement * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLElement_put_tagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement_get_parent_Proxy( 
    IXMLElement * This,
    /* [out][retval] */ IXMLElement **ppParent);


void __RPC_STUB IXMLElement_get_parent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement_setAttribute_Proxy( 
    IXMLElement * This,
    /* [in] */ BSTR strPropertyName,
    /* [in] */ VARIANT PropertyValue);


void __RPC_STUB IXMLElement_setAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement_getAttribute_Proxy( 
    IXMLElement * This,
    /* [in] */ BSTR strPropertyName,
    /* [out][retval] */ VARIANT *PropertyValue);


void __RPC_STUB IXMLElement_getAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement_removeAttribute_Proxy( 
    IXMLElement * This,
    /* [in] */ BSTR strPropertyName);


void __RPC_STUB IXMLElement_removeAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement_get_children_Proxy( 
    IXMLElement * This,
    /* [out][retval] */ IXMLElementCollection **pp);


void __RPC_STUB IXMLElement_get_children_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement_get_type_Proxy( 
    IXMLElement * This,
    /* [out][retval] */ long *plType);


void __RPC_STUB IXMLElement_get_type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement_get_text_Proxy( 
    IXMLElement * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLElement_get_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLElement_put_text_Proxy( 
    IXMLElement * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLElement_put_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement_addChild_Proxy( 
    IXMLElement * This,
    /* [in] */ IXMLElement *pChildElem,
    long lIndex,
    long lReserved);


void __RPC_STUB IXMLElement_addChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement_removeChild_Proxy( 
    IXMLElement * This,
    /* [in] */ IXMLElement *pChildElem);


void __RPC_STUB IXMLElement_removeChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLElement_INTERFACE_DEFINED__ */


#ifndef __IXMLElement2_INTERFACE_DEFINED__
#define __IXMLElement2_INTERFACE_DEFINED__

/* interface IXMLElement2 */
/* [helpstring][hidden][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLElement2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2B8DE2FF-8D2D-11d1-B2FC-00C04FD915A9")
    IXMLElement2 : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_tagName( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_tagName( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_parent( 
            /* [out][retval] */ IXMLElement2 **ppParent) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setAttribute( 
            /* [in] */ BSTR strPropertyName,
            /* [in] */ VARIANT PropertyValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getAttribute( 
            /* [in] */ BSTR strPropertyName,
            /* [out][retval] */ VARIANT *PropertyValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAttribute( 
            /* [in] */ BSTR strPropertyName) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_children( 
            /* [out][retval] */ IXMLElementCollection **pp) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_type( 
            /* [out][retval] */ long *plType) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_text( 
            /* [out][retval] */ BSTR *p) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_text( 
            /* [in] */ BSTR p) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE addChild( 
            /* [in] */ IXMLElement2 *pChildElem,
            long lIndex,
            long lReserved) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeChild( 
            /* [in] */ IXMLElement2 *pChildElem) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_attributes( 
            /* [out][retval] */ IXMLElementCollection **pp) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLElement2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLElement2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLElement2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLElement2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLElement2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLElement2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLElement2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLElement2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_tagName )( 
            IXMLElement2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_tagName )( 
            IXMLElement2 * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_parent )( 
            IXMLElement2 * This,
            /* [out][retval] */ IXMLElement2 **ppParent);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setAttribute )( 
            IXMLElement2 * This,
            /* [in] */ BSTR strPropertyName,
            /* [in] */ VARIANT PropertyValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAttribute )( 
            IXMLElement2 * This,
            /* [in] */ BSTR strPropertyName,
            /* [out][retval] */ VARIANT *PropertyValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAttribute )( 
            IXMLElement2 * This,
            /* [in] */ BSTR strPropertyName);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_children )( 
            IXMLElement2 * This,
            /* [out][retval] */ IXMLElementCollection **pp);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_type )( 
            IXMLElement2 * This,
            /* [out][retval] */ long *plType);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_text )( 
            IXMLElement2 * This,
            /* [out][retval] */ BSTR *p);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_text )( 
            IXMLElement2 * This,
            /* [in] */ BSTR p);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *addChild )( 
            IXMLElement2 * This,
            /* [in] */ IXMLElement2 *pChildElem,
            long lIndex,
            long lReserved);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeChild )( 
            IXMLElement2 * This,
            /* [in] */ IXMLElement2 *pChildElem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_attributes )( 
            IXMLElement2 * This,
            /* [out][retval] */ IXMLElementCollection **pp);
        
        END_INTERFACE
    } IXMLElement2Vtbl;

    interface IXMLElement2
    {
        CONST_VTBL struct IXMLElement2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLElement2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLElement2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLElement2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLElement2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLElement2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLElement2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLElement2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLElement2_get_tagName(This,p)	\
    (This)->lpVtbl -> get_tagName(This,p)

#define IXMLElement2_put_tagName(This,p)	\
    (This)->lpVtbl -> put_tagName(This,p)

#define IXMLElement2_get_parent(This,ppParent)	\
    (This)->lpVtbl -> get_parent(This,ppParent)

#define IXMLElement2_setAttribute(This,strPropertyName,PropertyValue)	\
    (This)->lpVtbl -> setAttribute(This,strPropertyName,PropertyValue)

#define IXMLElement2_getAttribute(This,strPropertyName,PropertyValue)	\
    (This)->lpVtbl -> getAttribute(This,strPropertyName,PropertyValue)

#define IXMLElement2_removeAttribute(This,strPropertyName)	\
    (This)->lpVtbl -> removeAttribute(This,strPropertyName)

#define IXMLElement2_get_children(This,pp)	\
    (This)->lpVtbl -> get_children(This,pp)

#define IXMLElement2_get_type(This,plType)	\
    (This)->lpVtbl -> get_type(This,plType)

#define IXMLElement2_get_text(This,p)	\
    (This)->lpVtbl -> get_text(This,p)

#define IXMLElement2_put_text(This,p)	\
    (This)->lpVtbl -> put_text(This,p)

#define IXMLElement2_addChild(This,pChildElem,lIndex,lReserved)	\
    (This)->lpVtbl -> addChild(This,pChildElem,lIndex,lReserved)

#define IXMLElement2_removeChild(This,pChildElem)	\
    (This)->lpVtbl -> removeChild(This,pChildElem)

#define IXMLElement2_get_attributes(This,pp)	\
    (This)->lpVtbl -> get_attributes(This,pp)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_tagName_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLElement2_get_tagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLElement2_put_tagName_Proxy( 
    IXMLElement2 * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLElement2_put_tagName_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_parent_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ IXMLElement2 **ppParent);


void __RPC_STUB IXMLElement2_get_parent_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement2_setAttribute_Proxy( 
    IXMLElement2 * This,
    /* [in] */ BSTR strPropertyName,
    /* [in] */ VARIANT PropertyValue);


void __RPC_STUB IXMLElement2_setAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement2_getAttribute_Proxy( 
    IXMLElement2 * This,
    /* [in] */ BSTR strPropertyName,
    /* [out][retval] */ VARIANT *PropertyValue);


void __RPC_STUB IXMLElement2_getAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement2_removeAttribute_Proxy( 
    IXMLElement2 * This,
    /* [in] */ BSTR strPropertyName);


void __RPC_STUB IXMLElement2_removeAttribute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_children_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ IXMLElementCollection **pp);


void __RPC_STUB IXMLElement2_get_children_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_type_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ long *plType);


void __RPC_STUB IXMLElement2_get_type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_text_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ BSTR *p);


void __RPC_STUB IXMLElement2_get_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLElement2_put_text_Proxy( 
    IXMLElement2 * This,
    /* [in] */ BSTR p);


void __RPC_STUB IXMLElement2_put_text_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement2_addChild_Proxy( 
    IXMLElement2 * This,
    /* [in] */ IXMLElement2 *pChildElem,
    long lIndex,
    long lReserved);


void __RPC_STUB IXMLElement2_addChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLElement2_removeChild_Proxy( 
    IXMLElement2 * This,
    /* [in] */ IXMLElement2 *pChildElem);


void __RPC_STUB IXMLElement2_removeChild_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLElement2_get_attributes_Proxy( 
    IXMLElement2 * This,
    /* [out][retval] */ IXMLElementCollection **pp);


void __RPC_STUB IXMLElement2_get_attributes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLElement2_INTERFACE_DEFINED__ */


#ifndef __IXMLAttribute_INTERFACE_DEFINED__
#define __IXMLAttribute_INTERFACE_DEFINED__

/* interface IXMLAttribute */
/* [helpstring][hidden][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLAttribute;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D4D4A0FC-3B73-11d1-B2B4-00C04FB92596")
    IXMLAttribute : public IDispatch
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
            /* [out][retval] */ BSTR *n) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_value( 
            /* [out][retval] */ BSTR *v) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLAttributeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLAttribute * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLAttribute * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLAttribute * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLAttribute * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLAttribute * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLAttribute * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLAttribute * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            IXMLAttribute * This,
            /* [out][retval] */ BSTR *n);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_value )( 
            IXMLAttribute * This,
            /* [out][retval] */ BSTR *v);
        
        END_INTERFACE
    } IXMLAttributeVtbl;

    interface IXMLAttribute
    {
        CONST_VTBL struct IXMLAttributeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLAttribute_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLAttribute_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLAttribute_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLAttribute_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLAttribute_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLAttribute_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLAttribute_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLAttribute_get_name(This,n)	\
    (This)->lpVtbl -> get_name(This,n)

#define IXMLAttribute_get_value(This,v)	\
    (This)->lpVtbl -> get_value(This,v)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLAttribute_get_name_Proxy( 
    IXMLAttribute * This,
    /* [out][retval] */ BSTR *n);


void __RPC_STUB IXMLAttribute_get_name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLAttribute_get_value_Proxy( 
    IXMLAttribute * This,
    /* [out][retval] */ BSTR *v);


void __RPC_STUB IXMLAttribute_get_value_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLAttribute_INTERFACE_DEFINED__ */


#ifndef __IXMLError_INTERFACE_DEFINED__
#define __IXMLError_INTERFACE_DEFINED__

/* interface IXMLError */
/* [helpstring][hidden][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLError;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("948C5AD3-C58D-11d0-9C0B-00C04FC99C8E")
    IXMLError : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetErrorInfo( 
            XML_ERROR *pErrorReturn) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLErrorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLError * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLError * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLError * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetErrorInfo )( 
            IXMLError * This,
            XML_ERROR *pErrorReturn);
        
        END_INTERFACE
    } IXMLErrorVtbl;

    interface IXMLError
    {
        CONST_VTBL struct IXMLErrorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLError_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLError_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLError_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLError_GetErrorInfo(This,pErrorReturn)	\
    (This)->lpVtbl -> GetErrorInfo(This,pErrorReturn)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IXMLError_GetErrorInfo_Proxy( 
    IXMLError * This,
    XML_ERROR *pErrorReturn);


void __RPC_STUB IXMLError_GetErrorInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLError_INTERFACE_DEFINED__ */


#ifndef __IXMLDOMSelection_INTERFACE_DEFINED__
#define __IXMLDOMSelection_INTERFACE_DEFINED__

/* interface IXMLDOMSelection */
/* [unique][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IXMLDOMSelection;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("AA634FC7-5888-44a7-A257-3A47150D3A0E")
    IXMLDOMSelection : public IXMLDOMNodeList
    {
    public:
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_expr( 
            /* [retval][out] */ BSTR *expression) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_expr( 
            /* [in] */ BSTR expression) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_context( 
            /* [retval][out] */ IXMLDOMNode **ppNode) = 0;
        
        virtual /* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE putref_context( 
            /* [in] */ IXMLDOMNode *pNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE peekNode( 
            /* [retval][out] */ IXMLDOMNode **ppNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE matches( 
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ IXMLDOMNode **ppNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeNext( 
            /* [retval][out] */ IXMLDOMNode **ppNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE removeAll( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE clone( 
            /* [retval][out] */ IXMLDOMSelection **ppNode) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getProperty( 
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setProperty( 
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLDOMSelectionVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLDOMSelection * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLDOMSelection * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLDOMSelection * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLDOMSelection * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLDOMSelection * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLDOMSelection * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLDOMSelection * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            IXMLDOMSelection * This,
            /* [in] */ long index,
            /* [retval][out] */ IXMLDOMNode **listItem);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ long *listLength);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *nextNode )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ IXMLDOMNode **nextItem);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *reset )( 
            IXMLDOMSelection * This);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IXMLDOMSelection * This,
            /* [out][retval] */ IUnknown **ppUnk);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_expr )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ BSTR *expression);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_expr )( 
            IXMLDOMSelection * This,
            /* [in] */ BSTR expression);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_context )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ IXMLDOMNode **ppNode);
        
        /* [helpstring][id][propputref] */ HRESULT ( STDMETHODCALLTYPE *putref_context )( 
            IXMLDOMSelection * This,
            /* [in] */ IXMLDOMNode *pNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *peekNode )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ IXMLDOMNode **ppNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *matches )( 
            IXMLDOMSelection * This,
            /* [in] */ IXMLDOMNode *pNode,
            /* [retval][out] */ IXMLDOMNode **ppNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeNext )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ IXMLDOMNode **ppNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *removeAll )( 
            IXMLDOMSelection * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *clone )( 
            IXMLDOMSelection * This,
            /* [retval][out] */ IXMLDOMSelection **ppNode);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getProperty )( 
            IXMLDOMSelection * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setProperty )( 
            IXMLDOMSelection * This,
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value);
        
        END_INTERFACE
    } IXMLDOMSelectionVtbl;

    interface IXMLDOMSelection
    {
        CONST_VTBL struct IXMLDOMSelectionVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLDOMSelection_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLDOMSelection_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLDOMSelection_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLDOMSelection_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLDOMSelection_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLDOMSelection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLDOMSelection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLDOMSelection_get_item(This,index,listItem)	\
    (This)->lpVtbl -> get_item(This,index,listItem)

#define IXMLDOMSelection_get_length(This,listLength)	\
    (This)->lpVtbl -> get_length(This,listLength)

#define IXMLDOMSelection_nextNode(This,nextItem)	\
    (This)->lpVtbl -> nextNode(This,nextItem)

#define IXMLDOMSelection_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IXMLDOMSelection_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)


#define IXMLDOMSelection_get_expr(This,expression)	\
    (This)->lpVtbl -> get_expr(This,expression)

#define IXMLDOMSelection_put_expr(This,expression)	\
    (This)->lpVtbl -> put_expr(This,expression)

#define IXMLDOMSelection_get_context(This,ppNode)	\
    (This)->lpVtbl -> get_context(This,ppNode)

#define IXMLDOMSelection_putref_context(This,pNode)	\
    (This)->lpVtbl -> putref_context(This,pNode)

#define IXMLDOMSelection_peekNode(This,ppNode)	\
    (This)->lpVtbl -> peekNode(This,ppNode)

#define IXMLDOMSelection_matches(This,pNode,ppNode)	\
    (This)->lpVtbl -> matches(This,pNode,ppNode)

#define IXMLDOMSelection_removeNext(This,ppNode)	\
    (This)->lpVtbl -> removeNext(This,ppNode)

#define IXMLDOMSelection_removeAll(This)	\
    (This)->lpVtbl -> removeAll(This)

#define IXMLDOMSelection_clone(This,ppNode)	\
    (This)->lpVtbl -> clone(This,ppNode)

#define IXMLDOMSelection_getProperty(This,name,value)	\
    (This)->lpVtbl -> getProperty(This,name,value)

#define IXMLDOMSelection_setProperty(This,name,value)	\
    (This)->lpVtbl -> setProperty(This,name,value)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_get_expr_Proxy( 
    IXMLDOMSelection * This,
    /* [retval][out] */ BSTR *expression);


void __RPC_STUB IXMLDOMSelection_get_expr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_put_expr_Proxy( 
    IXMLDOMSelection * This,
    /* [in] */ BSTR expression);


void __RPC_STUB IXMLDOMSelection_put_expr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_get_context_Proxy( 
    IXMLDOMSelection * This,
    /* [retval][out] */ IXMLDOMNode **ppNode);


void __RPC_STUB IXMLDOMSelection_get_context_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propputref] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_putref_context_Proxy( 
    IXMLDOMSelection * This,
    /* [in] */ IXMLDOMNode *pNode);


void __RPC_STUB IXMLDOMSelection_putref_context_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_peekNode_Proxy( 
    IXMLDOMSelection * This,
    /* [retval][out] */ IXMLDOMNode **ppNode);


void __RPC_STUB IXMLDOMSelection_peekNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_matches_Proxy( 
    IXMLDOMSelection * This,
    /* [in] */ IXMLDOMNode *pNode,
    /* [retval][out] */ IXMLDOMNode **ppNode);


void __RPC_STUB IXMLDOMSelection_matches_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_removeNext_Proxy( 
    IXMLDOMSelection * This,
    /* [retval][out] */ IXMLDOMNode **ppNode);


void __RPC_STUB IXMLDOMSelection_removeNext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_removeAll_Proxy( 
    IXMLDOMSelection * This);


void __RPC_STUB IXMLDOMSelection_removeAll_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_clone_Proxy( 
    IXMLDOMSelection * This,
    /* [retval][out] */ IXMLDOMSelection **ppNode);


void __RPC_STUB IXMLDOMSelection_clone_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_getProperty_Proxy( 
    IXMLDOMSelection * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IXMLDOMSelection_getProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLDOMSelection_setProperty_Proxy( 
    IXMLDOMSelection * This,
    /* [in] */ BSTR name,
    /* [in] */ VARIANT value);


void __RPC_STUB IXMLDOMSelection_setProperty_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLDOMSelection_INTERFACE_DEFINED__ */


#ifndef __XMLDOMDocumentEvents_DISPINTERFACE_DEFINED__
#define __XMLDOMDocumentEvents_DISPINTERFACE_DEFINED__

/* dispinterface XMLDOMDocumentEvents */
/* [uuid][hidden] */ 


EXTERN_C const IID DIID_XMLDOMDocumentEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("3efaa427-272f-11d2-836f-0000f87a7782")
    XMLDOMDocumentEvents : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct XMLDOMDocumentEventsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            XMLDOMDocumentEvents * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            XMLDOMDocumentEvents * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            XMLDOMDocumentEvents * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            XMLDOMDocumentEvents * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            XMLDOMDocumentEvents * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            XMLDOMDocumentEvents * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            XMLDOMDocumentEvents * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        END_INTERFACE
    } XMLDOMDocumentEventsVtbl;

    interface XMLDOMDocumentEvents
    {
        CONST_VTBL struct XMLDOMDocumentEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define XMLDOMDocumentEvents_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define XMLDOMDocumentEvents_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define XMLDOMDocumentEvents_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define XMLDOMDocumentEvents_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define XMLDOMDocumentEvents_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define XMLDOMDocumentEvents_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define XMLDOMDocumentEvents_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __XMLDOMDocumentEvents_DISPINTERFACE_DEFINED__ */


#ifndef __IDSOControl_INTERFACE_DEFINED__
#define __IDSOControl_INTERFACE_DEFINED__

/* interface IDSOControl */
/* [unique][helpstring][hidden][nonextensible][oleautomation][dual][uuid][object][local] */ 


EXTERN_C const IID IID_IDSOControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("310afa62-0575-11d2-9ca9-0060b0ec3d39")
    IDSOControl : public IDispatch
    {
    public:
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_XMLDocument( 
            /* [retval][out] */ IXMLDOMDocument **ppDoc) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_XMLDocument( 
            /* [in] */ IXMLDOMDocument *ppDoc) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_JavaDSOCompatible( 
            /* [retval][out] */ BOOL *fJavaDSOCompatible) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_JavaDSOCompatible( 
            /* [in] */ BOOL fJavaDSOCompatible) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [retval][out] */ long *state) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IDSOControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDSOControl * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDSOControl * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDSOControl * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IDSOControl * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IDSOControl * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IDSOControl * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IDSOControl * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_XMLDocument )( 
            IDSOControl * This,
            /* [retval][out] */ IXMLDOMDocument **ppDoc);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_XMLDocument )( 
            IDSOControl * This,
            /* [in] */ IXMLDOMDocument *ppDoc);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_JavaDSOCompatible )( 
            IDSOControl * This,
            /* [retval][out] */ BOOL *fJavaDSOCompatible);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_JavaDSOCompatible )( 
            IDSOControl * This,
            /* [in] */ BOOL fJavaDSOCompatible);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IDSOControl * This,
            /* [retval][out] */ long *state);
        
        END_INTERFACE
    } IDSOControlVtbl;

    interface IDSOControl
    {
        CONST_VTBL struct IDSOControlVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDSOControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IDSOControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IDSOControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IDSOControl_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IDSOControl_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IDSOControl_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IDSOControl_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IDSOControl_get_XMLDocument(This,ppDoc)	\
    (This)->lpVtbl -> get_XMLDocument(This,ppDoc)

#define IDSOControl_put_XMLDocument(This,ppDoc)	\
    (This)->lpVtbl -> put_XMLDocument(This,ppDoc)

#define IDSOControl_get_JavaDSOCompatible(This,fJavaDSOCompatible)	\
    (This)->lpVtbl -> get_JavaDSOCompatible(This,fJavaDSOCompatible)

#define IDSOControl_put_JavaDSOCompatible(This,fJavaDSOCompatible)	\
    (This)->lpVtbl -> put_JavaDSOCompatible(This,fJavaDSOCompatible)

#define IDSOControl_get_readyState(This,state)	\
    (This)->lpVtbl -> get_readyState(This,state)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id][propget] */ HRESULT STDMETHODCALLTYPE IDSOControl_get_XMLDocument_Proxy( 
    IDSOControl * This,
    /* [retval][out] */ IXMLDOMDocument **ppDoc);


void __RPC_STUB IDSOControl_get_XMLDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propput] */ HRESULT STDMETHODCALLTYPE IDSOControl_put_XMLDocument_Proxy( 
    IDSOControl * This,
    /* [in] */ IXMLDOMDocument *ppDoc);


void __RPC_STUB IDSOControl_put_XMLDocument_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propget] */ HRESULT STDMETHODCALLTYPE IDSOControl_get_JavaDSOCompatible_Proxy( 
    IDSOControl * This,
    /* [retval][out] */ BOOL *fJavaDSOCompatible);


void __RPC_STUB IDSOControl_get_JavaDSOCompatible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propput] */ HRESULT STDMETHODCALLTYPE IDSOControl_put_JavaDSOCompatible_Proxy( 
    IDSOControl * This,
    /* [in] */ BOOL fJavaDSOCompatible);


void __RPC_STUB IDSOControl_put_JavaDSOCompatible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propget] */ HRESULT STDMETHODCALLTYPE IDSOControl_get_readyState_Proxy( 
    IDSOControl * This,
    /* [retval][out] */ long *state);


void __RPC_STUB IDSOControl_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IDSOControl_INTERFACE_DEFINED__ */


#ifndef __IXMLHTTPRequest_INTERFACE_DEFINED__
#define __IXMLHTTPRequest_INTERFACE_DEFINED__

/* interface IXMLHTTPRequest */
/* [unique][helpstring][oleautomation][dual][uuid][object] */ 


EXTERN_C const IID IID_IXMLHTTPRequest;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("ED8C108D-4349-11D2-91A4-00C04F7969E8")
    IXMLHTTPRequest : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE open( 
            /* [in] */ BSTR bstrMethod,
            /* [in] */ BSTR bstrUrl,
            /* [optional][in] */ VARIANT varAsync,
            /* [optional][in] */ VARIANT bstrUser,
            /* [optional][in] */ VARIANT bstrPassword) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setRequestHeader( 
            /* [in] */ BSTR bstrHeader,
            /* [in] */ BSTR bstrValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getResponseHeader( 
            /* [in] */ BSTR bstrHeader,
            /* [retval][out] */ BSTR *pbstrValue) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getAllResponseHeaders( 
            /* [retval][out] */ BSTR *pbstrHeaders) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE send( 
            /* [optional][in] */ VARIANT varBody) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE abort( void) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_status( 
            /* [retval][out] */ long *plStatus) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_statusText( 
            /* [retval][out] */ BSTR *pbstrStatus) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_responseXML( 
            /* [retval][out] */ IDispatch **ppBody) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_responseText( 
            /* [retval][out] */ BSTR *pbstrBody) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_responseBody( 
            /* [retval][out] */ VARIANT *pvarBody) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_responseStream( 
            /* [retval][out] */ VARIANT *pvarBody) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
            /* [retval][out] */ long *plState) = 0;
        
        virtual /* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE put_onreadystatechange( 
            /* [in] */ IDispatch *pReadyStateSink) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IXMLHTTPRequestVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IXMLHTTPRequest * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IXMLHTTPRequest * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IXMLHTTPRequest * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IXMLHTTPRequest * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IXMLHTTPRequest * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IXMLHTTPRequest * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IXMLHTTPRequest * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *open )( 
            IXMLHTTPRequest * This,
            /* [in] */ BSTR bstrMethod,
            /* [in] */ BSTR bstrUrl,
            /* [optional][in] */ VARIANT varAsync,
            /* [optional][in] */ VARIANT bstrUser,
            /* [optional][in] */ VARIANT bstrPassword);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setRequestHeader )( 
            IXMLHTTPRequest * This,
            /* [in] */ BSTR bstrHeader,
            /* [in] */ BSTR bstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getResponseHeader )( 
            IXMLHTTPRequest * This,
            /* [in] */ BSTR bstrHeader,
            /* [retval][out] */ BSTR *pbstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAllResponseHeaders )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrHeaders);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *send )( 
            IXMLHTTPRequest * This,
            /* [optional][in] */ VARIANT varBody);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IXMLHTTPRequest * This);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ long *plStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_statusText )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseXML )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ IDispatch **ppBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseText )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseBody )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseStream )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IXMLHTTPRequest * This,
            /* [retval][out] */ long *plState);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_onreadystatechange )( 
            IXMLHTTPRequest * This,
            /* [in] */ IDispatch *pReadyStateSink);
        
        END_INTERFACE
    } IXMLHTTPRequestVtbl;

    interface IXMLHTTPRequest
    {
        CONST_VTBL struct IXMLHTTPRequestVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXMLHTTPRequest_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IXMLHTTPRequest_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IXMLHTTPRequest_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IXMLHTTPRequest_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IXMLHTTPRequest_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IXMLHTTPRequest_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IXMLHTTPRequest_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IXMLHTTPRequest_open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)	\
    (This)->lpVtbl -> open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)

#define IXMLHTTPRequest_setRequestHeader(This,bstrHeader,bstrValue)	\
    (This)->lpVtbl -> setRequestHeader(This,bstrHeader,bstrValue)

#define IXMLHTTPRequest_getResponseHeader(This,bstrHeader,pbstrValue)	\
    (This)->lpVtbl -> getResponseHeader(This,bstrHeader,pbstrValue)

#define IXMLHTTPRequest_getAllResponseHeaders(This,pbstrHeaders)	\
    (This)->lpVtbl -> getAllResponseHeaders(This,pbstrHeaders)

#define IXMLHTTPRequest_send(This,varBody)	\
    (This)->lpVtbl -> send(This,varBody)

#define IXMLHTTPRequest_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IXMLHTTPRequest_get_status(This,plStatus)	\
    (This)->lpVtbl -> get_status(This,plStatus)

#define IXMLHTTPRequest_get_statusText(This,pbstrStatus)	\
    (This)->lpVtbl -> get_statusText(This,pbstrStatus)

#define IXMLHTTPRequest_get_responseXML(This,ppBody)	\
    (This)->lpVtbl -> get_responseXML(This,ppBody)

#define IXMLHTTPRequest_get_responseText(This,pbstrBody)	\
    (This)->lpVtbl -> get_responseText(This,pbstrBody)

#define IXMLHTTPRequest_get_responseBody(This,pvarBody)	\
    (This)->lpVtbl -> get_responseBody(This,pvarBody)

#define IXMLHTTPRequest_get_responseStream(This,pvarBody)	\
    (This)->lpVtbl -> get_responseStream(This,pvarBody)

#define IXMLHTTPRequest_get_readyState(This,plState)	\
    (This)->lpVtbl -> get_readyState(This,plState)

#define IXMLHTTPRequest_put_onreadystatechange(This,pReadyStateSink)	\
    (This)->lpVtbl -> put_onreadystatechange(This,pReadyStateSink)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_open_Proxy( 
    IXMLHTTPRequest * This,
    /* [in] */ BSTR bstrMethod,
    /* [in] */ BSTR bstrUrl,
    /* [optional][in] */ VARIANT varAsync,
    /* [optional][in] */ VARIANT bstrUser,
    /* [optional][in] */ VARIANT bstrPassword);


void __RPC_STUB IXMLHTTPRequest_open_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_setRequestHeader_Proxy( 
    IXMLHTTPRequest * This,
    /* [in] */ BSTR bstrHeader,
    /* [in] */ BSTR bstrValue);


void __RPC_STUB IXMLHTTPRequest_setRequestHeader_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_getResponseHeader_Proxy( 
    IXMLHTTPRequest * This,
    /* [in] */ BSTR bstrHeader,
    /* [retval][out] */ BSTR *pbstrValue);


void __RPC_STUB IXMLHTTPRequest_getResponseHeader_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_getAllResponseHeaders_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ BSTR *pbstrHeaders);


void __RPC_STUB IXMLHTTPRequest_getAllResponseHeaders_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_send_Proxy( 
    IXMLHTTPRequest * This,
    /* [optional][in] */ VARIANT varBody);


void __RPC_STUB IXMLHTTPRequest_send_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_abort_Proxy( 
    IXMLHTTPRequest * This);


void __RPC_STUB IXMLHTTPRequest_abort_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_status_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ long *plStatus);


void __RPC_STUB IXMLHTTPRequest_get_status_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_statusText_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ BSTR *pbstrStatus);


void __RPC_STUB IXMLHTTPRequest_get_statusText_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_responseXML_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ IDispatch **ppBody);


void __RPC_STUB IXMLHTTPRequest_get_responseXML_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_responseText_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ BSTR *pbstrBody);


void __RPC_STUB IXMLHTTPRequest_get_responseText_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_responseBody_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ VARIANT *pvarBody);


void __RPC_STUB IXMLHTTPRequest_get_responseBody_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_responseStream_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ VARIANT *pvarBody);


void __RPC_STUB IXMLHTTPRequest_get_responseStream_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_get_readyState_Proxy( 
    IXMLHTTPRequest * This,
    /* [retval][out] */ long *plState);


void __RPC_STUB IXMLHTTPRequest_get_readyState_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propput] */ HRESULT STDMETHODCALLTYPE IXMLHTTPRequest_put_onreadystatechange_Proxy( 
    IXMLHTTPRequest * This,
    /* [in] */ IDispatch *pReadyStateSink);


void __RPC_STUB IXMLHTTPRequest_put_onreadystatechange_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IXMLHTTPRequest_INTERFACE_DEFINED__ */


#ifndef __IServerXMLHTTPRequest_INTERFACE_DEFINED__
#define __IServerXMLHTTPRequest_INTERFACE_DEFINED__

/* interface IServerXMLHTTPRequest */
/* [unique][helpstring][oleautomation][dual][uuid][object] */ 


EXTERN_C const IID IID_IServerXMLHTTPRequest;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2e9196bf-13ba-4dd4-91ca-6c571f281495")
    IServerXMLHTTPRequest : public IXMLHTTPRequest
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setTimeouts( 
            /* [in] */ long resolveTimeout,
            /* [in] */ long connectTimeout,
            /* [in] */ long sendTimeout,
            /* [in] */ long receiveTimeout) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE waitForResponse( 
            /* [optional][in] */ VARIANT timeoutInSeconds,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE getOption( 
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setOption( 
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [in] */ VARIANT value) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IServerXMLHTTPRequestVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IServerXMLHTTPRequest * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IServerXMLHTTPRequest * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IServerXMLHTTPRequest * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *open )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ BSTR bstrMethod,
            /* [in] */ BSTR bstrUrl,
            /* [optional][in] */ VARIANT varAsync,
            /* [optional][in] */ VARIANT bstrUser,
            /* [optional][in] */ VARIANT bstrPassword);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setRequestHeader )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ BSTR bstrHeader,
            /* [in] */ BSTR bstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getResponseHeader )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ BSTR bstrHeader,
            /* [retval][out] */ BSTR *pbstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAllResponseHeaders )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrHeaders);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *send )( 
            IServerXMLHTTPRequest * This,
            /* [optional][in] */ VARIANT varBody);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IServerXMLHTTPRequest * This);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ long *plStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_statusText )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseXML )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ IDispatch **ppBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseText )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ BSTR *pbstrBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseBody )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseStream )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IServerXMLHTTPRequest * This,
            /* [retval][out] */ long *plState);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_onreadystatechange )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ IDispatch *pReadyStateSink);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setTimeouts )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ long resolveTimeout,
            /* [in] */ long connectTimeout,
            /* [in] */ long sendTimeout,
            /* [in] */ long receiveTimeout);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *waitForResponse )( 
            IServerXMLHTTPRequest * This,
            /* [optional][in] */ VARIANT timeoutInSeconds,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getOption )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setOption )( 
            IServerXMLHTTPRequest * This,
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [in] */ VARIANT value);
        
        END_INTERFACE
    } IServerXMLHTTPRequestVtbl;

    interface IServerXMLHTTPRequest
    {
        CONST_VTBL struct IServerXMLHTTPRequestVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IServerXMLHTTPRequest_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IServerXMLHTTPRequest_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IServerXMLHTTPRequest_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IServerXMLHTTPRequest_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IServerXMLHTTPRequest_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IServerXMLHTTPRequest_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IServerXMLHTTPRequest_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IServerXMLHTTPRequest_open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)	\
    (This)->lpVtbl -> open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)

#define IServerXMLHTTPRequest_setRequestHeader(This,bstrHeader,bstrValue)	\
    (This)->lpVtbl -> setRequestHeader(This,bstrHeader,bstrValue)

#define IServerXMLHTTPRequest_getResponseHeader(This,bstrHeader,pbstrValue)	\
    (This)->lpVtbl -> getResponseHeader(This,bstrHeader,pbstrValue)

#define IServerXMLHTTPRequest_getAllResponseHeaders(This,pbstrHeaders)	\
    (This)->lpVtbl -> getAllResponseHeaders(This,pbstrHeaders)

#define IServerXMLHTTPRequest_send(This,varBody)	\
    (This)->lpVtbl -> send(This,varBody)

#define IServerXMLHTTPRequest_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IServerXMLHTTPRequest_get_status(This,plStatus)	\
    (This)->lpVtbl -> get_status(This,plStatus)

#define IServerXMLHTTPRequest_get_statusText(This,pbstrStatus)	\
    (This)->lpVtbl -> get_statusText(This,pbstrStatus)

#define IServerXMLHTTPRequest_get_responseXML(This,ppBody)	\
    (This)->lpVtbl -> get_responseXML(This,ppBody)

#define IServerXMLHTTPRequest_get_responseText(This,pbstrBody)	\
    (This)->lpVtbl -> get_responseText(This,pbstrBody)

#define IServerXMLHTTPRequest_get_responseBody(This,pvarBody)	\
    (This)->lpVtbl -> get_responseBody(This,pvarBody)

#define IServerXMLHTTPRequest_get_responseStream(This,pvarBody)	\
    (This)->lpVtbl -> get_responseStream(This,pvarBody)

#define IServerXMLHTTPRequest_get_readyState(This,plState)	\
    (This)->lpVtbl -> get_readyState(This,plState)

#define IServerXMLHTTPRequest_put_onreadystatechange(This,pReadyStateSink)	\
    (This)->lpVtbl -> put_onreadystatechange(This,pReadyStateSink)


#define IServerXMLHTTPRequest_setTimeouts(This,resolveTimeout,connectTimeout,sendTimeout,receiveTimeout)	\
    (This)->lpVtbl -> setTimeouts(This,resolveTimeout,connectTimeout,sendTimeout,receiveTimeout)

#define IServerXMLHTTPRequest_waitForResponse(This,timeoutInSeconds,isSuccessful)	\
    (This)->lpVtbl -> waitForResponse(This,timeoutInSeconds,isSuccessful)

#define IServerXMLHTTPRequest_getOption(This,option,value)	\
    (This)->lpVtbl -> getOption(This,option,value)

#define IServerXMLHTTPRequest_setOption(This,option,value)	\
    (This)->lpVtbl -> setOption(This,option,value)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest_setTimeouts_Proxy( 
    IServerXMLHTTPRequest * This,
    /* [in] */ long resolveTimeout,
    /* [in] */ long connectTimeout,
    /* [in] */ long sendTimeout,
    /* [in] */ long receiveTimeout);


void __RPC_STUB IServerXMLHTTPRequest_setTimeouts_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest_waitForResponse_Proxy( 
    IServerXMLHTTPRequest * This,
    /* [optional][in] */ VARIANT timeoutInSeconds,
    /* [retval][out] */ VARIANT_BOOL *isSuccessful);


void __RPC_STUB IServerXMLHTTPRequest_waitForResponse_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest_getOption_Proxy( 
    IServerXMLHTTPRequest * This,
    /* [in] */ SERVERXMLHTTP_OPTION option,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IServerXMLHTTPRequest_getOption_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest_setOption_Proxy( 
    IServerXMLHTTPRequest * This,
    /* [in] */ SERVERXMLHTTP_OPTION option,
    /* [in] */ VARIANT value);


void __RPC_STUB IServerXMLHTTPRequest_setOption_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IServerXMLHTTPRequest_INTERFACE_DEFINED__ */


#ifndef __IServerXMLHTTPRequest2_INTERFACE_DEFINED__
#define __IServerXMLHTTPRequest2_INTERFACE_DEFINED__

/* interface IServerXMLHTTPRequest2 */
/* [unique][helpstring][oleautomation][dual][uuid][object] */ 


EXTERN_C const IID IID_IServerXMLHTTPRequest2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2e01311b-c322-4b0a-bd77-b90cfdc8dce7")
    IServerXMLHTTPRequest2 : public IServerXMLHTTPRequest
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setProxy( 
            /* [in] */ SXH_PROXY_SETTING proxySetting,
            /* [optional][in] */ VARIANT varProxyServer,
            /* [optional][in] */ VARIANT varBypassList) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE setProxyCredentials( 
            /* [in] */ BSTR bstrUserName,
            /* [in] */ BSTR bstrPassword) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IServerXMLHTTPRequest2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IServerXMLHTTPRequest2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IServerXMLHTTPRequest2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IServerXMLHTTPRequest2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *open )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ BSTR bstrMethod,
            /* [in] */ BSTR bstrUrl,
            /* [optional][in] */ VARIANT varAsync,
            /* [optional][in] */ VARIANT bstrUser,
            /* [optional][in] */ VARIANT bstrPassword);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setRequestHeader )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ BSTR bstrHeader,
            /* [in] */ BSTR bstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getResponseHeader )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ BSTR bstrHeader,
            /* [retval][out] */ BSTR *pbstrValue);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getAllResponseHeaders )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ BSTR *pbstrHeaders);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *send )( 
            IServerXMLHTTPRequest2 * This,
            /* [optional][in] */ VARIANT varBody);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *abort )( 
            IServerXMLHTTPRequest2 * This);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_status )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ long *plStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_statusText )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ BSTR *pbstrStatus);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseXML )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ IDispatch **ppBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseText )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ BSTR *pbstrBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseBody )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_responseStream )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ VARIANT *pvarBody);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_readyState )( 
            IServerXMLHTTPRequest2 * This,
            /* [retval][out] */ long *plState);
        
        /* [helpstring][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_onreadystatechange )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ IDispatch *pReadyStateSink);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setTimeouts )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ long resolveTimeout,
            /* [in] */ long connectTimeout,
            /* [in] */ long sendTimeout,
            /* [in] */ long receiveTimeout);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *waitForResponse )( 
            IServerXMLHTTPRequest2 * This,
            /* [optional][in] */ VARIANT timeoutInSeconds,
            /* [retval][out] */ VARIANT_BOOL *isSuccessful);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *getOption )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setOption )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ SERVERXMLHTTP_OPTION option,
            /* [in] */ VARIANT value);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setProxy )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ SXH_PROXY_SETTING proxySetting,
            /* [optional][in] */ VARIANT varProxyServer,
            /* [optional][in] */ VARIANT varBypassList);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *setProxyCredentials )( 
            IServerXMLHTTPRequest2 * This,
            /* [in] */ BSTR bstrUserName,
            /* [in] */ BSTR bstrPassword);
        
        END_INTERFACE
    } IServerXMLHTTPRequest2Vtbl;

    interface IServerXMLHTTPRequest2
    {
        CONST_VTBL struct IServerXMLHTTPRequest2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IServerXMLHTTPRequest2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IServerXMLHTTPRequest2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IServerXMLHTTPRequest2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IServerXMLHTTPRequest2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IServerXMLHTTPRequest2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IServerXMLHTTPRequest2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IServerXMLHTTPRequest2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IServerXMLHTTPRequest2_open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)	\
    (This)->lpVtbl -> open(This,bstrMethod,bstrUrl,varAsync,bstrUser,bstrPassword)

#define IServerXMLHTTPRequest2_setRequestHeader(This,bstrHeader,bstrValue)	\
    (This)->lpVtbl -> setRequestHeader(This,bstrHeader,bstrValue)

#define IServerXMLHTTPRequest2_getResponseHeader(This,bstrHeader,pbstrValue)	\
    (This)->lpVtbl -> getResponseHeader(This,bstrHeader,pbstrValue)

#define IServerXMLHTTPRequest2_getAllResponseHeaders(This,pbstrHeaders)	\
    (This)->lpVtbl -> getAllResponseHeaders(This,pbstrHeaders)

#define IServerXMLHTTPRequest2_send(This,varBody)	\
    (This)->lpVtbl -> send(This,varBody)

#define IServerXMLHTTPRequest2_abort(This)	\
    (This)->lpVtbl -> abort(This)

#define IServerXMLHTTPRequest2_get_status(This,plStatus)	\
    (This)->lpVtbl -> get_status(This,plStatus)

#define IServerXMLHTTPRequest2_get_statusText(This,pbstrStatus)	\
    (This)->lpVtbl -> get_statusText(This,pbstrStatus)

#define IServerXMLHTTPRequest2_get_responseXML(This,ppBody)	\
    (This)->lpVtbl -> get_responseXML(This,ppBody)

#define IServerXMLHTTPRequest2_get_responseText(This,pbstrBody)	\
    (This)->lpVtbl -> get_responseText(This,pbstrBody)

#define IServerXMLHTTPRequest2_get_responseBody(This,pvarBody)	\
    (This)->lpVtbl -> get_responseBody(This,pvarBody)

#define IServerXMLHTTPRequest2_get_responseStream(This,pvarBody)	\
    (This)->lpVtbl -> get_responseStream(This,pvarBody)

#define IServerXMLHTTPRequest2_get_readyState(This,plState)	\
    (This)->lpVtbl -> get_readyState(This,plState)

#define IServerXMLHTTPRequest2_put_onreadystatechange(This,pReadyStateSink)	\
    (This)->lpVtbl -> put_onreadystatechange(This,pReadyStateSink)


#define IServerXMLHTTPRequest2_setTimeouts(This,resolveTimeout,connectTimeout,sendTimeout,receiveTimeout)	\
    (This)->lpVtbl -> setTimeouts(This,resolveTimeout,connectTimeout,sendTimeout,receiveTimeout)

#define IServerXMLHTTPRequest2_waitForResponse(This,timeoutInSeconds,isSuccessful)	\
    (This)->lpVtbl -> waitForResponse(This,timeoutInSeconds,isSuccessful)

#define IServerXMLHTTPRequest2_getOption(This,option,value)	\
    (This)->lpVtbl -> getOption(This,option,value)

#define IServerXMLHTTPRequest2_setOption(This,option,value)	\
    (This)->lpVtbl -> setOption(This,option,value)


#define IServerXMLHTTPRequest2_setProxy(This,proxySetting,varProxyServer,varBypassList)	\
    (This)->lpVtbl -> setProxy(This,proxySetting,varProxyServer,varBypassList)

#define IServerXMLHTTPRequest2_setProxyCredentials(This,bstrUserName,bstrPassword)	\
    (This)->lpVtbl -> setProxyCredentials(This,bstrUserName,bstrPassword)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest2_setProxy_Proxy( 
    IServerXMLHTTPRequest2 * This,
    /* [in] */ SXH_PROXY_SETTING proxySetting,
    /* [optional][in] */ VARIANT varProxyServer,
    /* [optional][in] */ VARIANT varBypassList);


void __RPC_STUB IServerXMLHTTPRequest2_setProxy_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IServerXMLHTTPRequest2_setProxyCredentials_Proxy( 
    IServerXMLHTTPRequest2 * This,
    /* [in] */ BSTR bstrUserName,
    /* [in] */ BSTR bstrPassword);


void __RPC_STUB IServerXMLHTTPRequest2_setProxyCredentials_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IServerXMLHTTPRequest2_INTERFACE_DEFINED__ */


#ifndef __IMXNamespacePrefixes_INTERFACE_DEFINED__
#define __IMXNamespacePrefixes_INTERFACE_DEFINED__

/* interface IMXNamespacePrefixes */
/* [unique][nonextensible][oleautomation][dual][helpstring][uuid][object][local] */ 


EXTERN_C const IID IID_IMXNamespacePrefixes;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("c90352f4-643c-4fbc-bb23-e996eb2d51fd")
    IMXNamespacePrefixes : public IDispatch
    {
    public:
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_item( 
            /* [in] */ long index,
            /* [retval][out] */ BSTR *prefix) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ long *length) = 0;
        
        virtual /* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
            /* [retval][out] */ IUnknown **ppUnk) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXNamespacePrefixesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXNamespacePrefixes * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXNamespacePrefixes * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXNamespacePrefixes * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMXNamespacePrefixes * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMXNamespacePrefixes * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMXNamespacePrefixes * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMXNamespacePrefixes * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_item )( 
            IMXNamespacePrefixes * This,
            /* [in] */ long index,
            /* [retval][out] */ BSTR *prefix);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IMXNamespacePrefixes * This,
            /* [retval][out] */ long *length);
        
        /* [id][hidden][restricted][propget] */ HRESULT ( STDMETHODCALLTYPE *get__newEnum )( 
            IMXNamespacePrefixes * This,
            /* [retval][out] */ IUnknown **ppUnk);
        
        END_INTERFACE
    } IMXNamespacePrefixesVtbl;

    interface IMXNamespacePrefixes
    {
        CONST_VTBL struct IMXNamespacePrefixesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXNamespacePrefixes_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXNamespacePrefixes_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXNamespacePrefixes_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXNamespacePrefixes_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IMXNamespacePrefixes_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IMXNamespacePrefixes_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IMXNamespacePrefixes_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IMXNamespacePrefixes_get_item(This,index,prefix)	\
    (This)->lpVtbl -> get_item(This,index,prefix)

#define IMXNamespacePrefixes_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define IMXNamespacePrefixes_get__newEnum(This,ppUnk)	\
    (This)->lpVtbl -> get__newEnum(This,ppUnk)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id][propget] */ HRESULT STDMETHODCALLTYPE IMXNamespacePrefixes_get_item_Proxy( 
    IMXNamespacePrefixes * This,
    /* [in] */ long index,
    /* [retval][out] */ BSTR *prefix);


void __RPC_STUB IMXNamespacePrefixes_get_item_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propget] */ HRESULT STDMETHODCALLTYPE IMXNamespacePrefixes_get_length_Proxy( 
    IMXNamespacePrefixes * This,
    /* [retval][out] */ long *length);


void __RPC_STUB IMXNamespacePrefixes_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][hidden][restricted][propget] */ HRESULT STDMETHODCALLTYPE IMXNamespacePrefixes_get__newEnum_Proxy( 
    IMXNamespacePrefixes * This,
    /* [retval][out] */ IUnknown **ppUnk);


void __RPC_STUB IMXNamespacePrefixes_get__newEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXNamespacePrefixes_INTERFACE_DEFINED__ */


#ifndef __IVBMXNamespaceManager_INTERFACE_DEFINED__
#define __IVBMXNamespaceManager_INTERFACE_DEFINED__

/* interface IVBMXNamespaceManager */
/* [unique][nonextensible][oleautomation][dual][helpstring][uuid][object][local] */ 


EXTERN_C const IID IID_IVBMXNamespaceManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("c90352f5-643c-4fbc-bb23-e996eb2d51fd")
    IVBMXNamespaceManager : public IDispatch
    {
    public:
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_allowOverride( 
            /* [in] */ VARIANT_BOOL fOverride) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_allowOverride( 
            /* [retval][out] */ VARIANT_BOOL *fOverride) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE reset( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE pushContext( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE pushNodeContext( 
            /* [in] */ IXMLDOMNode *contextNode,
            /* [defaultvalue][in] */ VARIANT_BOOL fDeep = -1) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE popContext( void) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE declarePrefix( 
            /* [in] */ BSTR prefix,
            /* [in] */ BSTR namespaceURI) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getDeclaredPrefixes( 
            /* [retval][out] */ IMXNamespacePrefixes **prefixes) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getPrefixes( 
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IMXNamespacePrefixes **prefixes) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getURI( 
            /* [in] */ BSTR prefix,
            /* [retval][out] */ VARIANT *uri) = 0;
        
        virtual /* [id] */ HRESULT STDMETHODCALLTYPE getURIFromNode( 
            /* [in] */ BSTR strPrefix,
            /* [in] */ IXMLDOMNode *contextNode,
            /* [retval][out] */ VARIANT *uri) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVBMXNamespaceManagerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVBMXNamespaceManager * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVBMXNamespaceManager * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVBMXNamespaceManager * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVBMXNamespaceManager * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVBMXNamespaceManager * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVBMXNamespaceManager * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVBMXNamespaceManager * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_allowOverride )( 
            IVBMXNamespaceManager * This,
            /* [in] */ VARIANT_BOOL fOverride);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_allowOverride )( 
            IVBMXNamespaceManager * This,
            /* [retval][out] */ VARIANT_BOOL *fOverride);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *reset )( 
            IVBMXNamespaceManager * This);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *pushContext )( 
            IVBMXNamespaceManager * This);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *pushNodeContext )( 
            IVBMXNamespaceManager * This,
            /* [in] */ IXMLDOMNode *contextNode,
            /* [defaultvalue][in] */ VARIANT_BOOL fDeep);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *popContext )( 
            IVBMXNamespaceManager * This);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *declarePrefix )( 
            IVBMXNamespaceManager * This,
            /* [in] */ BSTR prefix,
            /* [in] */ BSTR namespaceURI);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getDeclaredPrefixes )( 
            IVBMXNamespaceManager * This,
            /* [retval][out] */ IMXNamespacePrefixes **prefixes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getPrefixes )( 
            IVBMXNamespaceManager * This,
            /* [in] */ BSTR namespaceURI,
            /* [retval][out] */ IMXNamespacePrefixes **prefixes);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getURI )( 
            IVBMXNamespaceManager * This,
            /* [in] */ BSTR prefix,
            /* [retval][out] */ VARIANT *uri);
        
        /* [id] */ HRESULT ( STDMETHODCALLTYPE *getURIFromNode )( 
            IVBMXNamespaceManager * This,
            /* [in] */ BSTR strPrefix,
            /* [in] */ IXMLDOMNode *contextNode,
            /* [retval][out] */ VARIANT *uri);
        
        END_INTERFACE
    } IVBMXNamespaceManagerVtbl;

    interface IVBMXNamespaceManager
    {
        CONST_VTBL struct IVBMXNamespaceManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVBMXNamespaceManager_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVBMXNamespaceManager_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVBMXNamespaceManager_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVBMXNamespaceManager_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVBMXNamespaceManager_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVBMXNamespaceManager_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVBMXNamespaceManager_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVBMXNamespaceManager_put_allowOverride(This,fOverride)	\
    (This)->lpVtbl -> put_allowOverride(This,fOverride)

#define IVBMXNamespaceManager_get_allowOverride(This,fOverride)	\
    (This)->lpVtbl -> get_allowOverride(This,fOverride)

#define IVBMXNamespaceManager_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IVBMXNamespaceManager_pushContext(This)	\
    (This)->lpVtbl -> pushContext(This)

#define IVBMXNamespaceManager_pushNodeContext(This,contextNode,fDeep)	\
    (This)->lpVtbl -> pushNodeContext(This,contextNode,fDeep)

#define IVBMXNamespaceManager_popContext(This)	\
    (This)->lpVtbl -> popContext(This)

#define IVBMXNamespaceManager_declarePrefix(This,prefix,namespaceURI)	\
    (This)->lpVtbl -> declarePrefix(This,prefix,namespaceURI)

#define IVBMXNamespaceManager_getDeclaredPrefixes(This,prefixes)	\
    (This)->lpVtbl -> getDeclaredPrefixes(This,prefixes)

#define IVBMXNamespaceManager_getPrefixes(This,namespaceURI,prefixes)	\
    (This)->lpVtbl -> getPrefixes(This,namespaceURI,prefixes)

#define IVBMXNamespaceManager_getURI(This,prefix,uri)	\
    (This)->lpVtbl -> getURI(This,prefix,uri)

#define IVBMXNamespaceManager_getURIFromNode(This,strPrefix,contextNode,uri)	\
    (This)->lpVtbl -> getURIFromNode(This,strPrefix,contextNode,uri)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [id][propput] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_put_allowOverride_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ VARIANT_BOOL fOverride);


void __RPC_STUB IVBMXNamespaceManager_put_allowOverride_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id][propget] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_get_allowOverride_Proxy( 
    IVBMXNamespaceManager * This,
    /* [retval][out] */ VARIANT_BOOL *fOverride);


void __RPC_STUB IVBMXNamespaceManager_get_allowOverride_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_reset_Proxy( 
    IVBMXNamespaceManager * This);


void __RPC_STUB IVBMXNamespaceManager_reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_pushContext_Proxy( 
    IVBMXNamespaceManager * This);


void __RPC_STUB IVBMXNamespaceManager_pushContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_pushNodeContext_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ IXMLDOMNode *contextNode,
    /* [defaultvalue][in] */ VARIANT_BOOL fDeep);


void __RPC_STUB IVBMXNamespaceManager_pushNodeContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_popContext_Proxy( 
    IVBMXNamespaceManager * This);


void __RPC_STUB IVBMXNamespaceManager_popContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_declarePrefix_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ BSTR prefix,
    /* [in] */ BSTR namespaceURI);


void __RPC_STUB IVBMXNamespaceManager_declarePrefix_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_getDeclaredPrefixes_Proxy( 
    IVBMXNamespaceManager * This,
    /* [retval][out] */ IMXNamespacePrefixes **prefixes);


void __RPC_STUB IVBMXNamespaceManager_getDeclaredPrefixes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_getPrefixes_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ BSTR namespaceURI,
    /* [retval][out] */ IMXNamespacePrefixes **prefixes);


void __RPC_STUB IVBMXNamespaceManager_getPrefixes_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_getURI_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ BSTR prefix,
    /* [retval][out] */ VARIANT *uri);


void __RPC_STUB IVBMXNamespaceManager_getURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [id] */ HRESULT STDMETHODCALLTYPE IVBMXNamespaceManager_getURIFromNode_Proxy( 
    IVBMXNamespaceManager * This,
    /* [in] */ BSTR strPrefix,
    /* [in] */ IXMLDOMNode *contextNode,
    /* [retval][out] */ VARIANT *uri);


void __RPC_STUB IVBMXNamespaceManager_getURIFromNode_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVBMXNamespaceManager_INTERFACE_DEFINED__ */


#ifndef __IMXNamespaceManager_INTERFACE_DEFINED__
#define __IMXNamespaceManager_INTERFACE_DEFINED__

/* interface IMXNamespaceManager */
/* [unique][helpstring][uuid][local][object][hidden] */ 


EXTERN_C const IID IID_IMXNamespaceManager;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("c90352f6-643c-4fbc-bb23-e996eb2d51fd")
    IMXNamespaceManager : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE putAllowOverride( 
            /* [in] */ VARIANT_BOOL fOverride) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getAllowOverride( 
            /* [retval][out] */ VARIANT_BOOL *fOverride) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE reset( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE pushContext( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE pushNodeContext( 
            /* [in] */ IXMLDOMNode *contextNode,
            /* [in] */ VARIANT_BOOL fDeep) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE popContext( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE declarePrefix( 
            /* [in] */ const wchar_t *prefix,
            /* [in] */ const wchar_t *namespaceURI) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getDeclaredPrefix( 
            /* [in] */ long nIndex,
            /* [out][in] */ wchar_t *pwchPrefix,
            /* [out][in] */ int *pcchPrefix) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getPrefix( 
            /* [in] */ const wchar_t *pwszNamespaceURI,
            /* [in] */ long nIndex,
            /* [out][in] */ wchar_t *pwchPrefix,
            /* [out][in] */ int *pcchPrefix) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE getURI( 
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ IXMLDOMNode *pContextNode,
            /* [out][in] */ wchar_t *pwchUri,
            /* [out][in] */ int *pcchUri) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IMXNamespaceManagerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMXNamespaceManager * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMXNamespaceManager * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMXNamespaceManager * This);
        
        HRESULT ( STDMETHODCALLTYPE *putAllowOverride )( 
            IMXNamespaceManager * This,
            /* [in] */ VARIANT_BOOL fOverride);
        
        HRESULT ( STDMETHODCALLTYPE *getAllowOverride )( 
            IMXNamespaceManager * This,
            /* [retval][out] */ VARIANT_BOOL *fOverride);
        
        HRESULT ( STDMETHODCALLTYPE *reset )( 
            IMXNamespaceManager * This);
        
        HRESULT ( STDMETHODCALLTYPE *pushContext )( 
            IMXNamespaceManager * This);
        
        HRESULT ( STDMETHODCALLTYPE *pushNodeContext )( 
            IMXNamespaceManager * This,
            /* [in] */ IXMLDOMNode *contextNode,
            /* [in] */ VARIANT_BOOL fDeep);
        
        HRESULT ( STDMETHODCALLTYPE *popContext )( 
            IMXNamespaceManager * This);
        
        HRESULT ( STDMETHODCALLTYPE *declarePrefix )( 
            IMXNamespaceManager * This,
            /* [in] */ const wchar_t *prefix,
            /* [in] */ const wchar_t *namespaceURI);
        
        HRESULT ( STDMETHODCALLTYPE *getDeclaredPrefix )( 
            IMXNamespaceManager * This,
            /* [in] */ long nIndex,
            /* [out][in] */ wchar_t *pwchPrefix,
            /* [out][in] */ int *pcchPrefix);
        
        HRESULT ( STDMETHODCALLTYPE *getPrefix )( 
            IMXNamespaceManager * This,
            /* [in] */ const wchar_t *pwszNamespaceURI,
            /* [in] */ long nIndex,
            /* [out][in] */ wchar_t *pwchPrefix,
            /* [out][in] */ int *pcchPrefix);
        
        HRESULT ( STDMETHODCALLTYPE *getURI )( 
            IMXNamespaceManager * This,
            /* [in] */ const wchar_t *pwchPrefix,
            /* [in] */ IXMLDOMNode *pContextNode,
            /* [out][in] */ wchar_t *pwchUri,
            /* [out][in] */ int *pcchUri);
        
        END_INTERFACE
    } IMXNamespaceManagerVtbl;

    interface IMXNamespaceManager
    {
        CONST_VTBL struct IMXNamespaceManagerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMXNamespaceManager_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IMXNamespaceManager_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IMXNamespaceManager_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IMXNamespaceManager_putAllowOverride(This,fOverride)	\
    (This)->lpVtbl -> putAllowOverride(This,fOverride)

#define IMXNamespaceManager_getAllowOverride(This,fOverride)	\
    (This)->lpVtbl -> getAllowOverride(This,fOverride)

#define IMXNamespaceManager_reset(This)	\
    (This)->lpVtbl -> reset(This)

#define IMXNamespaceManager_pushContext(This)	\
    (This)->lpVtbl -> pushContext(This)

#define IMXNamespaceManager_pushNodeContext(This,contextNode,fDeep)	\
    (This)->lpVtbl -> pushNodeContext(This,contextNode,fDeep)

#define IMXNamespaceManager_popContext(This)	\
    (This)->lpVtbl -> popContext(This)

#define IMXNamespaceManager_declarePrefix(This,prefix,namespaceURI)	\
    (This)->lpVtbl -> declarePrefix(This,prefix,namespaceURI)

#define IMXNamespaceManager_getDeclaredPrefix(This,nIndex,pwchPrefix,pcchPrefix)	\
    (This)->lpVtbl -> getDeclaredPrefix(This,nIndex,pwchPrefix,pcchPrefix)

#define IMXNamespaceManager_getPrefix(This,pwszNamespaceURI,nIndex,pwchPrefix,pcchPrefix)	\
    (This)->lpVtbl -> getPrefix(This,pwszNamespaceURI,nIndex,pwchPrefix,pcchPrefix)

#define IMXNamespaceManager_getURI(This,pwchPrefix,pContextNode,pwchUri,pcchUri)	\
    (This)->lpVtbl -> getURI(This,pwchPrefix,pContextNode,pwchUri,pcchUri)

#endif /* COBJMACROS */


#endif 	/* C style interface */



HRESULT STDMETHODCALLTYPE IMXNamespaceManager_putAllowOverride_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ VARIANT_BOOL fOverride);


void __RPC_STUB IMXNamespaceManager_putAllowOverride_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_getAllowOverride_Proxy( 
    IMXNamespaceManager * This,
    /* [retval][out] */ VARIANT_BOOL *fOverride);


void __RPC_STUB IMXNamespaceManager_getAllowOverride_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_reset_Proxy( 
    IMXNamespaceManager * This);


void __RPC_STUB IMXNamespaceManager_reset_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_pushContext_Proxy( 
    IMXNamespaceManager * This);


void __RPC_STUB IMXNamespaceManager_pushContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_pushNodeContext_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ IXMLDOMNode *contextNode,
    /* [in] */ VARIANT_BOOL fDeep);


void __RPC_STUB IMXNamespaceManager_pushNodeContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_popContext_Proxy( 
    IMXNamespaceManager * This);


void __RPC_STUB IMXNamespaceManager_popContext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_declarePrefix_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ const wchar_t *prefix,
    /* [in] */ const wchar_t *namespaceURI);


void __RPC_STUB IMXNamespaceManager_declarePrefix_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_getDeclaredPrefix_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ long nIndex,
    /* [out][in] */ wchar_t *pwchPrefix,
    /* [out][in] */ int *pcchPrefix);


void __RPC_STUB IMXNamespaceManager_getDeclaredPrefix_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_getPrefix_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ const wchar_t *pwszNamespaceURI,
    /* [in] */ long nIndex,
    /* [out][in] */ wchar_t *pwchPrefix,
    /* [out][in] */ int *pcchPrefix);


void __RPC_STUB IMXNamespaceManager_getPrefix_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


HRESULT STDMETHODCALLTYPE IMXNamespaceManager_getURI_Proxy( 
    IMXNamespaceManager * This,
    /* [in] */ const wchar_t *pwchPrefix,
    /* [in] */ IXMLDOMNode *pContextNode,
    /* [out][in] */ wchar_t *pwchUri,
    /* [out][in] */ int *pcchUri);


void __RPC_STUB IMXNamespaceManager_getURI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IMXNamespaceManager_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_DOMDocument;

#ifdef __cplusplus

class DECLSPEC_UUID("F6D90F11-9C73-11D3-B32E-00C04F990BB4")
DOMDocument;
#endif

EXTERN_C const CLSID CLSID_DOMDocument26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f1b-c551-11d3-89b9-0000f81fe221")
DOMDocument26;
#endif

EXTERN_C const CLSID CLSID_DOMDocument30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f32-c551-11d3-89b9-0000f81fe221")
DOMDocument30;
#endif

EXTERN_C const CLSID CLSID_DOMDocument40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c0-f192-11d4-a65f-0040963251e5")
DOMDocument40;
#endif

EXTERN_C const CLSID CLSID_FreeThreadedDOMDocument;

#ifdef __cplusplus

class DECLSPEC_UUID("F6D90F12-9C73-11D3-B32E-00C04F990BB4")
FreeThreadedDOMDocument;
#endif

EXTERN_C const CLSID CLSID_FreeThreadedDOMDocument26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f1c-c551-11d3-89b9-0000f81fe221")
FreeThreadedDOMDocument26;
#endif

EXTERN_C const CLSID CLSID_FreeThreadedDOMDocument30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f33-c551-11d3-89b9-0000f81fe221")
FreeThreadedDOMDocument30;
#endif

EXTERN_C const CLSID CLSID_FreeThreadedDOMDocument40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c1-f192-11d4-a65f-0040963251e5")
FreeThreadedDOMDocument40;
#endif

EXTERN_C const CLSID CLSID_XMLSchemaCache;

#ifdef __cplusplus

class DECLSPEC_UUID("373984c9-b845-449b-91e7-45ac83036ade")
XMLSchemaCache;
#endif

EXTERN_C const CLSID CLSID_XMLSchemaCache26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f1d-c551-11d3-89b9-0000f81fe221")
XMLSchemaCache26;
#endif

EXTERN_C const CLSID CLSID_XMLSchemaCache30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f34-c551-11d3-89b9-0000f81fe221")
XMLSchemaCache30;
#endif

EXTERN_C const CLSID CLSID_XMLSchemaCache40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c2-f192-11d4-a65f-0040963251e5")
XMLSchemaCache40;
#endif

EXTERN_C const CLSID CLSID_XSLTemplate;

#ifdef __cplusplus

class DECLSPEC_UUID("2933BF94-7B36-11d2-B20E-00C04F983E60")
XSLTemplate;
#endif

EXTERN_C const CLSID CLSID_XSLTemplate26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f21-c551-11d3-89b9-0000f81fe221")
XSLTemplate26;
#endif

EXTERN_C const CLSID CLSID_XSLTemplate30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f36-c551-11d3-89b9-0000f81fe221")
XSLTemplate30;
#endif

EXTERN_C const CLSID CLSID_XSLTemplate40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c3-f192-11d4-a65f-0040963251e5")
XSLTemplate40;
#endif

EXTERN_C const CLSID CLSID_DSOControl;

#ifdef __cplusplus

class DECLSPEC_UUID("F6D90F14-9C73-11D3-B32E-00C04F990BB4")
DSOControl;
#endif

EXTERN_C const CLSID CLSID_DSOControl26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f1f-c551-11d3-89b9-0000f81fe221")
DSOControl26;
#endif

EXTERN_C const CLSID CLSID_DSOControl30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f39-c551-11d3-89b9-0000f81fe221")
DSOControl30;
#endif

EXTERN_C const CLSID CLSID_DSOControl40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c4-f192-11d4-a65f-0040963251e5")
DSOControl40;
#endif

EXTERN_C const CLSID CLSID_XMLHTTP;

#ifdef __cplusplus

class DECLSPEC_UUID("F6D90F16-9C73-11D3-B32E-00C04F990BB4")
XMLHTTP;
#endif

EXTERN_C const CLSID CLSID_XMLHTTP26;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f1e-c551-11d3-89b9-0000f81fe221")
XMLHTTP26;
#endif

EXTERN_C const CLSID CLSID_XMLHTTP30;

#ifdef __cplusplus

class DECLSPEC_UUID("f5078f35-c551-11d3-89b9-0000f81fe221")
XMLHTTP30;
#endif

EXTERN_C const CLSID CLSID_XMLHTTP40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c5-f192-11d4-a65f-0040963251e5")
XMLHTTP40;
#endif

EXTERN_C const CLSID CLSID_ServerXMLHTTP;

#ifdef __cplusplus

class DECLSPEC_UUID("afba6b42-5692-48ea-8141-dc517dcf0ef1")
ServerXMLHTTP;
#endif

EXTERN_C const CLSID CLSID_ServerXMLHTTP30;

#ifdef __cplusplus

class DECLSPEC_UUID("afb40ffd-b609-40a3-9828-f88bbe11e4e3")
ServerXMLHTTP30;
#endif

EXTERN_C const CLSID CLSID_ServerXMLHTTP40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c6-f192-11d4-a65f-0040963251e5")
ServerXMLHTTP40;
#endif

EXTERN_C const CLSID CLSID_SAXXMLReader;

#ifdef __cplusplus

class DECLSPEC_UUID("079aa557-4a18-424a-8eee-e39f0a8d41b9")
SAXXMLReader;
#endif

EXTERN_C const CLSID CLSID_SAXXMLReader30;

#ifdef __cplusplus

class DECLSPEC_UUID("3124c396-fb13-4836-a6ad-1317f1713688")
SAXXMLReader30;
#endif

EXTERN_C const CLSID CLSID_SAXXMLReader40;

#ifdef __cplusplus

class DECLSPEC_UUID("7c6e29bc-8b8b-4c3d-859e-af6cd158be0f")
SAXXMLReader40;
#endif

EXTERN_C const CLSID CLSID_MXXMLWriter;

#ifdef __cplusplus

class DECLSPEC_UUID("fc220ad8-a72a-4ee8-926e-0b7ad152a020")
MXXMLWriter;
#endif

EXTERN_C const CLSID CLSID_MXXMLWriter30;

#ifdef __cplusplus

class DECLSPEC_UUID("3d813dfe-6c91-4a4e-8f41-04346a841d9c")
MXXMLWriter30;
#endif

EXTERN_C const CLSID CLSID_MXXMLWriter40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c8-f192-11d4-a65f-0040963251e5")
MXXMLWriter40;
#endif

EXTERN_C const CLSID CLSID_MXHTMLWriter;

#ifdef __cplusplus

class DECLSPEC_UUID("a4c23ec3-6b70-4466-9127-550077239978")
MXHTMLWriter;
#endif

EXTERN_C const CLSID CLSID_MXHTMLWriter30;

#ifdef __cplusplus

class DECLSPEC_UUID("853d1540-c1a7-4aa9-a226-4d3bd301146d")
MXHTMLWriter30;
#endif

EXTERN_C const CLSID CLSID_MXHTMLWriter40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969c9-f192-11d4-a65f-0040963251e5")
MXHTMLWriter40;
#endif

EXTERN_C const CLSID CLSID_SAXAttributes;

#ifdef __cplusplus

class DECLSPEC_UUID("4dd441ad-526d-4a77-9f1b-9841ed802fb0")
SAXAttributes;
#endif

EXTERN_C const CLSID CLSID_SAXAttributes30;

#ifdef __cplusplus

class DECLSPEC_UUID("3e784a01-f3ae-4dc0-9354-9526b9370eba")
SAXAttributes30;
#endif

EXTERN_C const CLSID CLSID_SAXAttributes40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969ca-f192-11d4-a65f-0040963251e5")
SAXAttributes40;
#endif

EXTERN_C const CLSID CLSID_MXNamespaceManager;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969d5-f192-11d4-a65f-0040963251e5")
MXNamespaceManager;
#endif

EXTERN_C const CLSID CLSID_MXNamespaceManager40;

#ifdef __cplusplus

class DECLSPEC_UUID("88d969d6-f192-11d4-a65f-0040963251e5")
MXNamespaceManager40;
#endif

EXTERN_C const CLSID CLSID_XMLDocument;

#ifdef __cplusplus

class DECLSPEC_UUID("CFC399AF-D876-11d0-9C10-00C04FC99C8E")
XMLDocument;
#endif
#endif /* __MSXML2_LIBRARY_DEFINED__ */

/* interface __MIDL_itf_msxml2_0189 */
/* [local] */ 

//----------------------------
// MSXML SPECIFIC ERROR CODES 
//----------------------------
#define E_XML_NOTWF                0xC00CE223L  // Validate failed because the document is not well formed.
#define E_XML_NODTD                0xC00CE224L  // Validate failed because a DTD/Schema was not specified in the document.
#define E_XML_INVALID              0xC00CE225L  // Validate failed because of a DTD/Schema violation.
#define E_XML_BUFFERTOOSMALL       0xC00CE226L  // Buffer passed in is too small to receive the data.
#ifdef __USE_MSXML2_NAMESPACE__
}
#endif


extern RPC_IF_HANDLE __MIDL_itf_msxml2_0189_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_msxml2_0189_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


