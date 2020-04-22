//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "dod_shareddefs.h"

#define TE_RIFLE_SHELL 1024
#define TE_PISTOL_SHELL 2048

//-----------------------------------------------------------------------------
// Purpose: DOD Eject Brass
//-----------------------------------------------------------------------------
void DOD_EjectBrassCallback( const CEffectData &data )
{
	int shelltype = data.m_nHitBox;	

	Vector vForward, vRight, vUp;
	AngleVectors( data.m_vAngles, &vForward, &vRight, &vUp );

	QAngle vecShellAngles;
	VectorAngles( -vUp, vecShellAngles );
	
	Vector vecVelocity = random->RandomFloat( 130, 180 ) * vForward +
						 random->RandomFloat( -30, 30 ) * vRight +
						 random->RandomFloat( -30, 30 ) * vUp;

	float flLifeTime = 10.0f;

	int hitsound = 0;
	model_t *pModel = NULL;

	//should be precached .. oh well
	static model_t *pSmallShell = (model_t *)engine->LoadModel( "models/shells/shell_small.mdl" );
	static model_t *pMediumShell = (model_t *)engine->LoadModel( "models/shells/shell_medium.mdl" );
	static model_t *pLargeShell = (model_t *)engine->LoadModel( "models/shells/shell_large.mdl" );
	static model_t *pGarandClip = (model_t *)engine->LoadModel( "models/shells/garand_clip.mdl" );

	int flags = FTENT_FADEOUT | FTENT_GRAVITY | FTENT_COLLIDEALL | FTENT_HITSOUND | FTENT_ROTATE;

	switch( shelltype )
	{
	case EJECTBRASS_PISTOL:	
		hitsound = TE_PISTOL_SHELL;
		pModel = pSmallShell;
		break;
	case EJECTBRASS_RIFLE:
		hitsound = TE_PISTOL_SHELL;
		pModel = pMediumShell;
		break;
	case EJECTBRASS_MG:
	case EJECTBRASS_MG_2:
		hitsound = TE_RIFLE_SHELL;
		pModel = pLargeShell;
		break;
	case EJECTBRASS_GARANDCLIP:
		hitsound = TE_RIFLE_SHELL;
		flags &= ~FTENT_COLLIDEALL;
		flags |= FTENT_COLLIDEWORLD;
		pModel = pGarandClip;
		break;
	default:
		break;
	}

	Assert( pModel );	

	C_LocalTempEntity *pTemp = tempents->SpawnTempModel( pModel, data.m_vOrigin, vecShellAngles, vecVelocity, flLifeTime, FTENT_NEVERDIE );
	if ( pTemp == NULL )
		return;

	pTemp->m_vecTempEntAngVelocity[0] = random->RandomFloat(-512,511);
	pTemp->m_vecTempEntAngVelocity[1] = random->RandomFloat(-255,255);
	pTemp->m_vecTempEntAngVelocity[2] = random->RandomFloat(-255,255);

	pTemp->hitSound = hitsound;

	pTemp->SetGravity( 0.4 );

	pTemp->m_flSpriteScale = 10;

	pTemp->flags = flags;

	// don't collide with owner
	pTemp->clientIndex = data.entindex();
	if ( pTemp->clientIndex < 0 )
	{
		pTemp->clientIndex = 0;
	}

	// ::ShouldCollide decides what this collides with
	pTemp->flags |= FTENT_COLLISIONGROUP;
	pTemp->SetCollisionGroup( DOD_COLLISIONGROUP_SHELLS );
}

DECLARE_CLIENT_EFFECT( "DOD_EjectBrass", DOD_EjectBrassCallback );