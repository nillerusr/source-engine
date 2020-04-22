//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EXPRESSIONPROPERTIES_H
#define EXPRESSIONPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CExpressionParams : public CBaseDialogParams
{
	char			m_szName[ 256 ];
	char			m_szDescription[ 256 ];
};

int ExpressionProperties( CExpressionParams *params );

#endif // EXPRESSIONPROPERTIES_H
