//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "BaseAnimatedTextureProxy.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CAnimatedTextureProxy : public CBaseAnimatedTextureProxy
{
public:
	CAnimatedTextureProxy() {}
	virtual ~CAnimatedTextureProxy() {}
	virtual float GetAnimationStartTime( void* pBaseEntity );
};

EXPOSE_MATERIAL_PROXY( CAnimatedTextureProxy, AnimatedTexture );

#pragma warning (disable : 4100)

float CAnimatedTextureProxy::GetAnimationStartTime( void* pBaseEntity )
{
	return 0;
}

