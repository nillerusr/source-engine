//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUISTYLEFACTORY_H
#define IUISTYLEFACTORY_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "layout/stylesymbol.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Public interface for style factory (usuable in client code)
//-----------------------------------------------------------------------------
class IUIStyleFactory
{
public:

	virtual bool BRegisteredProperty( panorama::CStyleSymbol symName ) = 0;
	virtual CStyleSymbol GetPropertyNameForAlias( panorama::CStyleSymbol symName ) = 0;
	virtual CStyleProperty *CreateStyleProperty( panorama::CStyleSymbol symName ) = 0;
	virtual void FreeStyleProperty( panorama::CStyleProperty *pProperty ) = 0;
	virtual const CUtlVector< CUtlString > &GetSortedPropertyAndAliasNames() = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Sort comparator
//-----------------------------------------------------------------------------
inline int CompareStylePropertyName( const CUtlString *plhs, const CUtlString *prhs )
{
	return V_strcmp( plhs->Get(), prhs->Get() );
}


//-----------------------------------------------------------------------------
// Purpose: Less than function
//-----------------------------------------------------------------------------
inline bool StylePropertyNameLessThan( const CUtlString &lhs, const CUtlString &rhs, void *pContext )
{
	return CompareStylePropertyName( &lhs, &rhs ) < 0;
}


}
#endif // IUISTYLEFACTORY_H