//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMASYMBOL_H
#define PANORAMASYMBOL_H

#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"

namespace panorama
{


// Panorama wrapper around CUtlSymbol which always creates symbol in panorama.dll module
class CPanoramaSymbol
{
public:
	// constructor, destructor
	CPanoramaSymbol() : m_Id( UTL_INVAL_SYMBOL ) {}
	CPanoramaSymbol( UtlSymId_t id ) : m_Id( id ) {}
	CPanoramaSymbol( char const* pStr );
	CPanoramaSymbol( CPanoramaSymbol const& sym ) : m_Id( sym.m_Id ) {}

	// operator=
	CPanoramaSymbol& operator=(CPanoramaSymbol const& src) { m_Id = src.m_Id; return *this; }

	// operator==
	bool operator==(CPanoramaSymbol const& src) const { return m_Id == src.m_Id; }
	bool operator==(char const* pStr) const { return CPanoramaSymbol( pStr ).m_Id == m_Id; }

	// sort by index offset and NOT lexigraphical
	bool operator<(CPanoramaSymbol const& src) const { return m_Id < src.m_Id; }
	bool operator>( CPanoramaSymbol const& src ) const { return m_Id > src.m_Id; }
	bool operator!=(CPanoramaSymbol const& src) const { return !operator==(src); }

	// Is valid?
	bool IsValid() const { return m_Id != UTL_INVAL_SYMBOL; }

	// Gets at the symbol
	operator UtlSymId_t () const { return m_Id; }

	// Gets the string associated with the symbol
	char const* String() const;

private:
	UtlSymId_t m_Id;
};

} // namespace panorama

#endif // PANORAMASYMBOL_H