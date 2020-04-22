//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef FILEMEMCACHE_H
#define FILEMEMCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/generichash.h"
#include <unordered_map>

#pragma warning ( disable : 4200 )

class CachedFileData
{
	friend class FileCache;

protected: // Constructed by FileCache
	CachedFileData() {}
	static CachedFileData *Create( char const *szFilename );
	void Free( void );

public:
	static CachedFileData *GetByDataPtr( void const *pvDataPtr );
	
	char const * GetFileName() const;
	void const * GetDataPtr() const;
	int GetDataLen() const;

	int UpdateRefCount( int iDeltaRefCount ) { return m_numRefs += iDeltaRefCount; }

	bool IsValid() const;

protected:
	enum { eHeaderSize = 256 };
	char m_chFilename[256 - 12];
	int m_numRefs;
	int m_numDataBytes;
	int m_signature;
	unsigned char m_data[0]; // file data spans further
};

class FileCache
{
public:
	FileCache();
	~FileCache() { Clear(); }

public:
	CachedFileData *Get( char const *szFilename );
	void Clear( void );

protected:
	struct eqstr {
		inline size_t operator()( const char *s ) const {
			if ( !s )
				return 0;
			return HashString( s );
		}
		inline bool operator()( const char *s1, const char *s2 ) const {
			return _stricmp( s1, s2 ) < 0;
		}
	};

	typedef std::unordered_map< const char *, CachedFileData *, eqstr, eqstr > Mapping;
	Mapping m_map;
};

#endif // #ifndef FILEMEMCACHE_H
