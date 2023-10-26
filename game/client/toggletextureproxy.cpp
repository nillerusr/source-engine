//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "toggletextureproxy.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/ITexture.h"
#include <KeyValues.h>
#include "FunctionProxy.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

EXPOSE_MATERIAL_PROXY( CBaseToggleTextureProxy, ToggleTexture );

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CBaseToggleTextureProxy::CBaseToggleTextureProxy()
{
	Cleanup();
}

CBaseToggleTextureProxy::~CBaseToggleTextureProxy()
{
	Cleanup();
}

C_BaseEntity *CBaseToggleTextureProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return pRend->GetIClientUnknown()->GetBaseEntity();
}

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CBaseToggleTextureProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pTextureVarName = pKeyValues->GetString( "toggleTextureVar" );
	if( !pTextureVarName )
		return false;

	bool foundVar;
	m_TextureVar = pMaterial->FindVar( pTextureVarName, &foundVar, false );
	if( !foundVar )
		return false;

	char const* pTextureFrameNumVarName = pKeyValues->GetString( "toggleTextureFrameNumVar" );
	if( !pTextureFrameNumVarName )
		return false;

	m_TextureFrameNumVar = pMaterial->FindVar( pTextureFrameNumVarName, &foundVar, false );
	if( !foundVar )
		return false;
	
	m_WrapAnimation = !!pKeyValues->GetInt( "toggleShouldWrap", 1 );
	return true;
}

void CBaseToggleTextureProxy::Cleanup()
{
	m_TextureVar = NULL;
	m_TextureFrameNumVar = NULL;
}


//-----------------------------------------------------------------------------
// Does the dirty deed
//-----------------------------------------------------------------------------
void CBaseToggleTextureProxy::OnBind( void *pC_BaseEntity )
{
	assert ( m_TextureVar );

	if (!pC_BaseEntity)
		return;

	if( m_TextureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		return;
	}

	ITexture *pTexture = NULL;

	pTexture = m_TextureVar->GetTextureValue();

	if ( pTexture == NULL )
		 return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	if ( pEntity == NULL )
		 return;
	
	int numFrames = pTexture->GetNumAnimationFrames();
	int frame = pEntity->GetTextureFrameIndex();

	int intFrame = ((int)frame) % numFrames; 

	if ( m_WrapAnimation == false )
	{
		if ( frame > numFrames )
			 intFrame = numFrames;
	}
		
	m_TextureFrameNumVar->SetIntValue( intFrame );
}

IMaterial *CBaseToggleTextureProxy::GetMaterial()
{
	return m_TextureFrameNumVar->GetOwningMaterial();
}
