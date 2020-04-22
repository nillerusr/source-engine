//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIFILERESOURCE_H
#define UIFILERESOURCE_H


#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"

namespace panorama
{

//
// A file reference container, you pass in an opaque string and it interprets what kind of reference is it
//   Supports local file and HTTP url right now
//
class CFileResource
{
public:
	CFileResource();
	CFileResource( const char *pchUTF8Path );
	CFileResource( const CFileResource &src)  { operator=(src); }
	~CFileResource();

	bool BIsValid() const;
	bool BIsHTTPURL() const;
	bool BIsLocalPath() const;
	bool BIsSMBShare() const;
	const CUtlVector<CUtlString> &GetCookieHeadersForHTTPURL() const;

	const CUtlString &GetReferencePath() const;
	CUtlString &GetReferencePathForModify();

	void Set( const char *pchUTF8Path );
	CFileResource &operator=( const CFileResource & src);
	bool operator<( const CFileResource &left ) const;
	bool operator==( const CFileResource &left);
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_sReference );
		ValidateObj( m_vecCookieHeaders );
		FOR_EACH_VEC( m_vecCookieHeaders, iVec )
			ValidateObj( m_vecCookieHeaders[iVec] );
	}
#endif
private:

	enum EReferenceType
	{
		eNone,
		eHTTPURL,
		eFile,
		eSMB,
		eSource2Relative
	};
	EReferenceType m_eRefType;
	CUtlString m_sReference;

	// Only used for eHTTPURL type
	CUtlVector<CUtlString> m_vecCookieHeaders;
};


//-----------------------------------------------------------------------------
// Purpose: ResourceImage types
//-----------------------------------------------------------------------------
enum EResourceImageType
{
	k_EResourceImageTypeUnknown = 0,
	k_EResourceImageTypeImage,
	k_EResourceImageTypeMovie
};

// Function to determine resource type for a file resource
EResourceImageType DetermineResourceType( const CFileResource &resource );

} // namespace panorama

#endif // UIFILERESOURCE_H