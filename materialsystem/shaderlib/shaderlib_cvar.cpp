//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "icvar.h"
#include "tier1/tier1.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------------------------- //
// ConVar stuff.
// ------------------------------------------------------------------------------------------- //
class CShaderLibConVarAccessor : public IConCommandBaseAccessor
{
public:
	virtual bool	RegisterConCommandBase( ConCommandBase *pCommand )
	{
		// Link to engine's list instead
		g_pCVar->RegisterConCommand( pCommand );

		char const *pValue = g_pCVar->GetCommandLineValue( pCommand->GetName() );
		if( pValue && !pCommand->IsCommand() )
		{
			( ( ConVar * )pCommand )->SetValue( pValue );
		}
		return true;
	}
};

CShaderLibConVarAccessor g_ConVarAccessor;


void InitShaderLibCVars( CreateInterfaceFn cvarFactory )
{
	if ( g_pCVar )
	{
		ConVar_Register( FCVAR_MATERIAL_SYSTEM_THREAD, &g_ConVarAccessor );
	}
}
