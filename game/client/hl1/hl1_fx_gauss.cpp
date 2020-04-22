//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "c_te_basebeam.h"
#include "iviewrender_beams.h"
#include "c_baseplayer.h"
#include "beam_shared.h"


#define GAUSS_GLOW_SPRITE	"sprites/hotglow.vmt"
#define GAUSS_BEAM_SPRITE	"sprites/smoke.vmt"


int	m_nGlowIndex;
int	m_nBeamIndex;


void PrecacheGaussEffects(void *pUser)
{
	m_nGlowIndex = modelinfo->GetModelIndex( GAUSS_GLOW_SPRITE );
	m_nBeamIndex = modelinfo->GetModelIndex( GAUSS_BEAM_SPRITE );
}
PRECACHE_REGISTER_FN(PrecacheGaussEffects);


void HL1GaussBeam( const CEffectData &data )
{
	// beam expects ent + attach to be encoded in the entity index (legacy system)
	int		nStartEntity	= data.entindex() | ((1 & 0xF)<<12);

	C_BaseEntity * pEnt = cl_entitylist->GetEnt( BEAMENT_ENTITY(nStartEntity) );

	if ( !pEnt->IsPlayer() )
		return;

	C_BasePlayer * pPlayer = static_cast<C_BasePlayer*>(pEnt);
	int nStartAttachment = -1;

	if ( pPlayer->IsLocalPlayer() )
	{
		nStartEntity = pPlayer->GetViewModel()->entindex();
	}
	else
	{
		if ( !pPlayer->GetActiveWeapon() )		// TODO : make sure we have the gauss gun
			return;

		nStartEntity = pPlayer->GetActiveWeapon()->entindex();
		nStartAttachment = 2;
	}

	nStartEntity |= ((1 & 0xF)<<12);

	Vector	vecEndPoint		= data.m_vOrigin;
	bool	fIsPrimaryFire	= data.m_fFlags;
	float	flStartWidth;
	float	flEndWidth;
	color32	beamColor;

	if ( fIsPrimaryFire )	// primary attack
	{
		flStartWidth	= 1.0;
		flEndWidth		= 1.0;

		beamColor.r		= 255;
		beamColor.g		= 255;
		beamColor.b		= 0;
		beamColor.a		= 255;
	}
	else					// secondary
	{
		flStartWidth	= 2.5;
		flEndWidth		= 2.5;

		beamColor.r		= 255;
		beamColor.g		= 255;
		beamColor.b		= 255;
		beamColor.a		= 255;
	}

	beams->CreateBeamEntPoint(
		nStartEntity,				// start ent
		NULL,						// start pos
		0,							// end ent
		&vecEndPoint,				// end pos
		m_nBeamIndex,				// model index
		NULL,						// halo index
		0.0,						// halo scale
		0.1,						// life
		flStartWidth,				// startwidth
		flEndWidth,					// endwidth
		0.0,						// fade length
		0,							// amplitude
		beamColor.a,				// brightness
		0,							// speed
		0,							// startframe
		0,							// framerate
		beamColor.r,				// R
		beamColor.g,				// G
		beamColor.b				// B
	);

	//ADRIANHL1MP
}
DECLARE_CLIENT_EFFECT( "HL1GaussBeam", HL1GaussBeam );


void HL1GaussBeamReflect( const CEffectData &data )
{
	Vector	vecStartPoint	= data.m_vStart;
	Vector	vecEndPoint		= data.m_vOrigin;
	bool	fIsPrimaryFire	= data.m_fFlags;
	float	flStartWidth;
	float	flEndWidth;
	color32	beamColor;

	if ( fIsPrimaryFire )	// primary attack
	{
		flStartWidth	= 1.0;
		flEndWidth		= 1.0;

		beamColor.r		= 255;
		beamColor.g		= 255;
		beamColor.b		= 0;
		beamColor.a		= 255;
	}
	else					// secondary
	{
		flStartWidth	= 2.5;
		flEndWidth		= 2.5;

		beamColor.r		= 255;
		beamColor.g		= 255;
		beamColor.b		= 255;
		beamColor.a		= 255;
	}

	beams->CreateBeamPoints(
		vecStartPoint,				// start pos
		vecEndPoint,				// end pos
		m_nBeamIndex,				// model index
		NULL,						// halo index
		0.0,						// halo scale
		0.1,						// life
		flStartWidth,				// startwidth
		flEndWidth,					// endwidth
		0.0,						// fade length
		0,							// amplitude
		beamColor.a,				// brightness
		0,							// speed
		0,							// startframe
		0,							// framerate
		beamColor.r,				// R
		beamColor.g,				// G
		beamColor.b					// B
	);
}
DECLARE_CLIENT_EFFECT( "HL1GaussBeamReflect", HL1GaussBeamReflect );


void HL1GaussReflect( const CEffectData &data )
{
	Vector	vecStart	= data.m_vOrigin;
	Vector	vecNormal	= data.m_vNormal;
	float	flMagnitude	= data.m_flMagnitude;

	tempents->TempSprite( vecStart, vec3_origin, 0.2, m_nGlowIndex, kRenderGlow, kRenderFxNoDissipation, flMagnitude / 255.0, flMagnitude * 0.05, FTENT_FADEOUT );

	Vector vecForward;
	VectorAdd( vecStart, vecNormal, vecForward );

	tempents->Sprite_Trail( vecStart, vecForward, m_nGlowIndex, 3, 0.1, random->RandomFloat( 0.1, 0.2 ), 100, 255, 100 );
}
DECLARE_CLIENT_EFFECT( "HL1GaussReflect", HL1GaussReflect );


void HL1GaussWallPunchEnter( const CEffectData &data )
{
	Vector	vecStart	= data.m_vOrigin;
	Vector	vecNormal	= data.m_vNormal;

	Vector vecForward;
	VectorSubtract( vecStart, vecNormal, vecForward );

	tempents->Sprite_Trail( vecStart, vecForward, m_nGlowIndex, 3, 0.1, random->RandomFloat( 0.1, 0.2 ), 100, 255, 100 );
}
DECLARE_CLIENT_EFFECT( "HL1GaussWallPunchEnter", HL1GaussWallPunchEnter );


void HL1GaussWallPunchExit( const CEffectData &data )
{
	Vector	vecStart	= data.m_vOrigin;
	Vector	vecNormal	= data.m_vNormal;
	float	flMagnitude	= data.m_flMagnitude;

	tempents->TempSprite( vecStart, vec3_origin, 0.1, m_nGlowIndex, kRenderGlow, kRenderFxNoDissipation, flMagnitude * 1.2 / 255.0, 6.0, FTENT_FADEOUT );

	Vector vecForward;
	VectorSubtract( vecStart, vecNormal, vecForward );

	tempents->Sprite_Trail( vecStart, vecForward, m_nGlowIndex, flMagnitude * 0.3, 0.1, random->RandomFloat( 0.1, 0.2 ), 200, 255, 40 );
}
DECLARE_CLIENT_EFFECT( "HL1GaussWallPunchExit", HL1GaussWallPunchExit );


void HL1GaussWallImpact1( const CEffectData &data )
{
	Vector	vecStart	= data.m_vOrigin;
	float	flMagnitude	= data.m_flMagnitude;

	tempents->TempSprite( vecStart, vec3_origin, 1, m_nGlowIndex, kRenderGlow, kRenderFxNoDissipation, flMagnitude / 255.0, 6.0, FTENT_FADEOUT );
}
DECLARE_CLIENT_EFFECT( "HL1GaussWallImpact1", HL1GaussWallImpact1 );


void HL1GaussWallImpact2( const CEffectData &data )
{
	Vector	vecStart	= data.m_vOrigin;
	Vector	vecNormal	= data.m_vNormal;

	tempents->TempSprite( vecStart, vec3_origin, 0.2, m_nGlowIndex, kRenderGlow, kRenderFxNoDissipation, 240.0 / 255.0, 0.3, FTENT_FADEOUT );

	Vector vecForward;
	VectorAdd( vecStart, vecNormal, vecForward );

	tempents->Sprite_Trail( vecStart, vecForward, m_nGlowIndex, 8, 0.6, random->RandomFloat( 0.1, 0.2 ), 100, 255, 200 );
}
DECLARE_CLIENT_EFFECT( "HL1GaussWallImpact2", HL1GaussWallImpact2 );

