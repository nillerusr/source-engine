//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "BaseAnimatedTextureProxy.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CAnimatedOffsetTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedOffsetTextureProxy() : m_flFrameOffset( 0.0f ) {}

	virtual ~CAnimatedOffsetTextureProxy() {}

	virtual float GetAnimationStartTime( void* pBaseEntity );
	virtual void OnBind( void *pBaseEntity );

protected:

	float	m_flFrameOffset;
};

EXPOSE_MATERIAL_PROXY( CAnimatedOffsetTextureProxy, AnimatedOffsetTexture );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pArg - 
// Output : float
//-----------------------------------------------------------------------------
float CAnimatedOffsetTextureProxy::GetAnimationStartTime( void* pArg )
{
	return m_flFrameOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pBaseEntity - 
//-----------------------------------------------------------------------------
void CAnimatedOffsetTextureProxy::OnBind( void *pBaseEntity )
{
	C_BaseEntity* pEntity = (C_BaseEntity*)pBaseEntity;
	
	if ( pEntity )
	{
		m_flFrameOffset = pEntity->GetTextureAnimationStartTime();
	}

	// Call into the base class
	CBaseAnimatedTextureProxy::OnBind( pBaseEntity );
}

