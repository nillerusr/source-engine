//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef STYLESYMBOL_H
#define STYLESYMBOL_H

#ifdef _WIN32
#pragma once
#endif

#define MAX_PANORAMA_STYLE_SYMBOLS 128

#include "tier0/validator.h"
#include "utlsymbol.h"
#include "utlvector.h"
#ifdef SOURCE_PANORAMA_FIXME
#include "UtlSortVector.h"
#else
#include "utlsortvector.h"
#endif
#include "utlstring.h"


namespace panorama
{

	class CStyleProperty;

	class CStyleSymbol
	{
	public:
		// constructor, destructor
		CStyleSymbol() : m_Id( 0xFF ) {}
		CStyleSymbol( uint8 id ) : m_Id( id ) {}
		CStyleSymbol( char const* pStr );
		CStyleSymbol( char const* pStr, bool bCreateNew );
		CStyleSymbol( CStyleSymbol const& sym ) : m_Id(sym.m_Id) {}

		// operator=
		CStyleSymbol& operator=( CStyleSymbol const& src ) { m_Id = src.m_Id; return *this; }

		// operator==
		bool operator==( CStyleSymbol const& src ) const { return m_Id == src.m_Id; }
		bool operator==( char const* pStr ) const;

		// operator != 
		bool operator!=( CStyleSymbol const& src ) const { return m_Id != src.m_Id; }

		// operator <
		bool operator < (const CStyleSymbol &rhs ) const { return m_Id < rhs.m_Id; }


		uint8 GetID() const { return m_Id; }

		// Is valid?
		bool IsValid() const { return m_Id != 0xFF; }

		// Gets the string associated with the symbol
		char const* String() const;

		operator const char *() const { return String(); }

	protected:
		uint8   m_Id;
	};

	uint32 HashItem( const CStyleSymbol &item );

} // namespace panorama

#endif // STYLESYMBOL_H
