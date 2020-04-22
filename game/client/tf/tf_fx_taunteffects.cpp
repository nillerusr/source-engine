//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "tf_shareddefs.h"

//-----------------------------------------------------------------------------
// Purpose: TF Eject Brass
//-----------------------------------------------------------------------------
void TF_ThrowCigaretteCallback( const CEffectData &data )
{
	C_BaseEntity *pEntity = ClientEntityList().GetEnt( data.entindex() );

	if ( !pEntity )
		return;

	int iTeam = pEntity->GetTeamNumber();

	Vector vForward, vRight, vUp;
	AngleVectors( data.m_vAngles, &vForward, &vRight, &vUp );

	QAngle vecShellAngles;
	VectorAngles( -vUp, vecShellAngles );

	Vector vecVelocity = 180 * vForward +
		random->RandomFloat( -20, 20 ) * vRight +
		random->RandomFloat( 0, 20 ) * vUp;

	vecVelocity.z += 100;

	float flLifeTime = 10.0f;

	const char *pszCigaretteModel = "models/weapons/shells/shell_cigarrette.mdl";	// sic

	model_t *pModel = (model_t *)engine->LoadModel( pszCigaretteModel );
	if ( !pModel )
		return;

	int flags = FTENT_FADEOUT | FTENT_GRAVITY | FTENT_COLLIDEALL | FTENT_ROTATE;

	Assert( pModel );	

	C_LocalTempEntity *pTemp = tempents->SpawnTempModel( pModel, data.m_vOrigin, vecShellAngles, vecVelocity, flLifeTime, FTENT_NEVERDIE );
	if ( pTemp == NULL )
		return;

	pTemp->m_nSkin = ( iTeam == TF_TEAM_RED ) ? 0 : 1;

	pTemp->m_vecTempEntAngVelocity[0] = random->RandomFloat(-512,511);
	pTemp->m_vecTempEntAngVelocity[1] = random->RandomFloat(-255,255);
	pTemp->m_vecTempEntAngVelocity[2] = random->RandomFloat(-255,255);

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
	pTemp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
}

DECLARE_CLIENT_EFFECT( "TF_ThrowCigarette", TF_ThrowCigaretteCallback );