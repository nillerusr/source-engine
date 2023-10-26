//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "SpriteTrail.h"

#ifdef CLIENT_DLL

#include "clientsideeffects.h"
#include "materialsystem/IMaterialSystem.h"
#include "materialsystem/IMesh.h"
#include "mathlib/VMatrix.h"
#include "view.h"
#include "beamdraw.h"
#include "enginesprite.h"
#include "tier0/vprof.h"

extern CEngineSprite *Draw_SetSpriteTexture( const model_t *pSpriteModel, int frame, int rendermode );

#endif // CLIENT_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// constants
//-----------------------------------------------------------------------------
#define SCREEN_SPACE_TRAILS 0


//-----------------------------------------------------------------------------
// Save/Restore
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )

BEGIN_SIMPLE_DATADESC( TrailPoint_t )
	// DEFINE_FIELD( m_vecScreenPos,	FIELD_CLASSCHECK_IGNORE ) // do this or else we get a warning about multiply-defined fields
#if SCREEN_SPACE_TRAILS
	DEFINE_FIELD( m_vecScreenPos,	FIELD_VECTOR ),
#else
	DEFINE_FIELD( m_vecScreenPos,	FIELD_POSITION_VECTOR ),
#endif

	DEFINE_FIELD( m_flDieTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flTexCoord,		FIELD_FLOAT ),
	DEFINE_FIELD( m_flWidthVariance,FIELD_FLOAT ),
END_DATADESC()

#endif

BEGIN_DATADESC( CSpriteTrail )

	DEFINE_KEYFIELD( m_flLifeTime,			FIELD_FLOAT, "lifetime" ),
	DEFINE_KEYFIELD( m_flStartWidth,		FIELD_FLOAT, "startwidth" ),
	DEFINE_KEYFIELD( m_flEndWidth,			FIELD_FLOAT, "endwidth" ),
	DEFINE_KEYFIELD( m_iszSpriteName,		FIELD_STRING, "spritename" ),
	DEFINE_KEYFIELD( m_bAnimate,			FIELD_BOOLEAN, "animate" ),
	DEFINE_FIELD( m_flStartWidthVariance,	FIELD_FLOAT ),
	DEFINE_FIELD( m_flTextureRes,		FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinFadeLength,	FIELD_FLOAT ),
	DEFINE_FIELD( m_vecSkyboxOrigin,	FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flSkyboxScale,		FIELD_FLOAT ),

	// These are client-only
#if defined( CLIENT_DLL )
	DEFINE_EMBEDDED_AUTO_ARRAY( m_vecSteps ),
	DEFINE_FIELD( m_nFirstStep, FIELD_INTEGER ),
	DEFINE_FIELD( m_nStepCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_flUpdateTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecPrevSkyboxOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flPrevSkyboxScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecRenderMins, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecRenderMaxs, FIELD_VECTOR ),
#endif

END_DATADESC()

LINK_ENTITY_TO_CLASS( env_spritetrail, CSpriteTrail );


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( SpriteTrail, DT_SpriteTrail );

BEGIN_NETWORK_TABLE( CSpriteTrail, DT_SpriteTrail )
#if !defined( CLIENT_DLL )
	SendPropFloat( SENDINFO(m_flLifeTime),		0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flStartWidth),	0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flEndWidth),		0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flStartWidthVariance),		0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTextureRes),	0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flMinFadeLength),	0,	SPROP_NOSCALE ),
	SendPropVector( SENDINFO(m_vecSkyboxOrigin),0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flSkyboxScale),	0,	SPROP_NOSCALE ),
#else
	RecvPropFloat( RECVINFO(m_flLifeTime)),
	RecvPropFloat( RECVINFO(m_flStartWidth)),
	RecvPropFloat( RECVINFO(m_flEndWidth)),
	RecvPropFloat( RECVINFO(m_flStartWidthVariance)),
	RecvPropFloat( RECVINFO(m_flTextureRes)),
	RecvPropFloat( RECVINFO(m_flMinFadeLength)),
	RecvPropVector( RECVINFO(m_vecSkyboxOrigin)),
	RecvPropFloat( RECVINFO(m_flSkyboxScale)),
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Prediction
//-----------------------------------------------------------------------------
BEGIN_PREDICTION_DATA( CSpriteTrail )
END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSpriteTrail::CSpriteTrail( void )
{
#ifdef CLIENT_DLL
	m_nFirstStep = 0;
	m_nStepCount = 0;
#endif

	m_flStartWidthVariance = 0;
	m_vecSkyboxOrigin.Init( 0, 0, 0 );
	m_flSkyboxScale = 1.0f;
	m_flEndWidth = -1.0f;
}

void CSpriteTrail::Spawn( void )
{
#ifdef CLIENT_DLL
	BaseClass::Spawn();
#else

	if ( GetModelName() != NULL_STRING )
	{
		BaseClass::Spawn();
		return;
	}

	SetModelName( m_iszSpriteName );
	BaseClass::Spawn();

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NOCLIP );

	SetCollisionBounds( vec3_origin, vec3_origin );
	TurnOn();

#endif
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::Precache( void ) 
{
	BaseClass::Precache();

	if ( m_iszSpriteName != NULL_STRING )
	{
		PrecacheModel( STRING(m_iszSpriteName) );
	}
}

//-----------------------------------------------------------------------------
// Sets parameters of the sprite trail
//-----------------------------------------------------------------------------
void CSpriteTrail::SetLifeTime( float time ) 
{ 
	m_flLifeTime = time; 
}

void CSpriteTrail::SetStartWidth( float flStartWidth )
{ 
	m_flStartWidth = flStartWidth; 
	m_flStartWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetStartWidthVariance( float flStartWidthVariance )
{ 
	m_flStartWidthVariance = flStartWidthVariance; 
	m_flStartWidthVariance /= m_flSkyboxScale;
}

void CSpriteTrail::SetEndWidth( float flEndWidth )
{ 
	m_flEndWidth = flEndWidth; 
	m_flEndWidth /= m_flSkyboxScale;
}

void CSpriteTrail::SetTextureResolution( float flTexelsPerInch )
{ 
	m_flTextureRes = flTexelsPerInch; 
	m_flTextureRes *= m_flSkyboxScale;
}

void CSpriteTrail::SetMinFadeLength( float flMinFadeLength )
{
	m_flMinFadeLength = flMinFadeLength;
	m_flMinFadeLength /= m_flSkyboxScale;
}

void CSpriteTrail::SetSkybox( const Vector &vecSkyboxOrigin, float flSkyboxScale )
{
	m_flTextureRes /= m_flSkyboxScale;
	m_flMinFadeLength *= m_flSkyboxScale;
	m_flStartWidth *= m_flSkyboxScale;
	m_flEndWidth *= m_flSkyboxScale;
	m_flStartWidthVariance *= m_flSkyboxScale;

	m_vecSkyboxOrigin = vecSkyboxOrigin;
	m_flSkyboxScale = flSkyboxScale;

	m_flTextureRes *= m_flSkyboxScale;
	m_flMinFadeLength /= m_flSkyboxScale;
	m_flStartWidth /= m_flSkyboxScale;
	m_flEndWidth /= m_flSkyboxScale;
	m_flStartWidthVariance /= m_flSkyboxScale;

	if ( IsInSkybox() )
	{
		AddEFlags( EFL_IN_SKYBOX ); 
	}
	else
	{
		RemoveEFlags( EFL_IN_SKYBOX ); 
	}
}


//-----------------------------------------------------------------------------
// Is the trail in the skybox?
//-----------------------------------------------------------------------------
bool CSpriteTrail::IsInSkybox() const
{
	return (m_flSkyboxScale != 1.0f) || (m_vecSkyboxOrigin != vec3_origin);
}


#ifdef CLIENT_DLL


//-----------------------------------------------------------------------------
// On data update
//-----------------------------------------------------------------------------
void CSpriteTrail::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );
	m_vecPrevSkyboxOrigin = m_vecSkyboxOrigin;
	m_flPrevSkyboxScale = m_flSkyboxScale;
}

void CSpriteTrail::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		if ((m_flPrevSkyboxScale != m_flSkyboxScale) || (m_vecPrevSkyboxOrigin != m_vecSkyboxOrigin))
		{
			ConvertSkybox();
		}
	}
}


//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::ClientThink()
{
	// Update the trail + bounding box
	UpdateTrail();
	UpdateBoundingBox();
}


//-----------------------------------------------------------------------------
// Render bounds
//-----------------------------------------------------------------------------
void CSpriteTrail::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_vecRenderMins;
	maxs = m_vecRenderMaxs;
}


//-----------------------------------------------------------------------------
// Converts the trail when it changes skyboxes
//-----------------------------------------------------------------------------
void CSpriteTrail::ConvertSkybox()
{
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = GetTrailPoint(i);

		VectorSubtract( pPoint->m_vecScreenPos, m_vecPrevSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_vecScreenPos *= m_flPrevSkyboxScale / m_flSkyboxScale;
		VectorSubtract( pPoint->m_vecScreenPos, m_vecSkyboxOrigin, pPoint->m_vecScreenPos );
		pPoint->m_flWidthVariance *= m_flPrevSkyboxScale / m_flSkyboxScale;
	}
}


//-----------------------------------------------------------------------------
// Gets at the nth item in the list
//-----------------------------------------------------------------------------
TrailPoint_t *CSpriteTrail::GetTrailPoint( int n )
{
	Assert( n < MAX_SPRITE_TRAIL_POINTS );
	COMPILE_TIME_ASSERT( (MAX_SPRITE_TRAIL_POINTS & (MAX_SPRITE_TRAIL_POINTS-1)) == 0 );
	int nIndex = (n + m_nFirstStep) & MAX_SPRITE_TRAIL_MASK; 
	return &m_vecSteps[nIndex];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::ComputeScreenPosition( Vector *pScreenPos )
{
#if SCREEN_SPACE_TRAILS
	VMatrix	viewMatrix;
	materials->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	*pScreenPos = viewMatrix * GetRenderOrigin();
#else
	*pScreenPos = GetRenderOrigin();
#endif
}


//-----------------------------------------------------------------------------
// Compute position	+ bounding box
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateBoundingBox( void )
{
	Vector vecRenderOrigin = GetRenderOrigin();
	m_vecRenderMins = vecRenderOrigin;
	m_vecRenderMaxs = vecRenderOrigin;

	float flMaxWidth = m_flStartWidth;
	if (( m_flEndWidth >= 0.0f ) && ( m_flEndWidth > m_flStartWidth ))
	{
		flMaxWidth = m_flEndWidth;
	}

	Vector mins, maxs;
	for ( int i = 0; i < m_nStepCount; ++i )
	{
		TrailPoint_t *pPoint = GetTrailPoint(i);

		float flActualWidth = (flMaxWidth + pPoint->m_flWidthVariance) * 0.5f;
		Vector size( flActualWidth, flActualWidth, flActualWidth );
		VectorSubtract( pPoint->m_vecScreenPos, size, mins );
		VectorAdd( pPoint->m_vecScreenPos, size, maxs );
		
		VectorMin( m_vecRenderMins, mins, m_vecRenderMins ); 
		VectorMax( m_vecRenderMaxs, maxs, m_vecRenderMaxs ); 
	}
	
	m_vecRenderMins -= vecRenderOrigin; 
	m_vecRenderMaxs -= vecRenderOrigin;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSpriteTrail::UpdateTrail( void )
{
	// Can't update too quickly
	if ( m_flUpdateTime > gpGlobals->curtime )
		return;

	Vector	screenPos;
	ComputeScreenPosition( &screenPos );
	TrailPoint_t *pLast = m_nStepCount ? GetTrailPoint( m_nStepCount-1 ) : NULL;
	if ( ( pLast == NULL ) || ( pLast->m_vecScreenPos.DistToSqr( screenPos ) > 4.0f ) )
	{
		// If we're over our limit, steal the last point and put it up front
		if ( m_nStepCount >= MAX_SPRITE_TRAIL_POINTS )
		{
			--m_nStepCount;
			++m_nFirstStep;
		}

		// Save off its screen position, not its world position
		TrailPoint_t *pNewPoint = GetTrailPoint( m_nStepCount );
		pNewPoint->m_vecScreenPos = screenPos;
		pNewPoint->m_flDieTime	= gpGlobals->curtime + m_flLifeTime;
		pNewPoint->m_flWidthVariance = random->RandomFloat( -m_flStartWidthVariance, m_flStartWidthVariance );
		if (pLast)
		{
			pNewPoint->m_flTexCoord	= pLast->m_flTexCoord + pLast->m_vecScreenPos.DistTo( screenPos ) * m_flTextureRes;
		}
		else
		{
			pNewPoint->m_flTexCoord = 0.0f;
		}

		++m_nStepCount;
	}

	// Don't update again for a bit
	m_flUpdateTime = gpGlobals->curtime + ( m_flLifeTime / (float) MAX_SPRITE_TRAIL_POINTS );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSpriteTrail::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "CSpriteTrail::DrawModel", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	// Must have at least one point
	if ( m_nStepCount < 1 )
		return 1;

	//See if we should draw
	if ( !IsVisible() || ( m_bReadyToDraw == false ) )
		return 0;

	CEngineSprite *pSprite = Draw_SetSpriteTexture( GetModel(), m_flFrame, GetRenderMode() );
	if ( pSprite == NULL )
		return 0;

	// Specify all the segments.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CBeamSegDraw segDraw;
	segDraw.Start( pRenderContext, m_nStepCount + 1, pSprite->GetMaterial( GetRenderMode() ) );
	
	// Setup the first point, always emanating from the attachment point
	TrailPoint_t *pLast = GetTrailPoint( m_nStepCount-1 );
	TrailPoint_t currentPoint;
	currentPoint.m_flDieTime = gpGlobals->curtime + m_flLifeTime;
	ComputeScreenPosition( &currentPoint.m_vecScreenPos );
	currentPoint.m_flTexCoord = pLast->m_flTexCoord + currentPoint.m_vecScreenPos.DistTo(pLast->m_vecScreenPos) * m_flTextureRes;
	currentPoint.m_flWidthVariance = 0.0f;

#if SCREEN_SPACE_TRAILS
	VMatrix	viewMatrix;
	materials->GetMatrix( MATERIAL_VIEW, &viewMatrix );
	viewMatrix = viewMatrix.InverseTR();
#endif

	TrailPoint_t *pPrevPoint = NULL;
	float flTailAlphaDist = m_flMinFadeLength;
	for ( int i = 0; i <= m_nStepCount; ++i )
	{
		// This makes it so that we're always drawing to the current location
		TrailPoint_t *pPoint = (i != m_nStepCount) ? GetTrailPoint(i) : &currentPoint;

		float flLifePerc = (pPoint->m_flDieTime - gpGlobals->curtime) / m_flLifeTime;
		flLifePerc = clamp( flLifePerc, 0.0f, 1.0f );

		color24 c = GetRenderColor();
		BeamSeg_t curSeg;
		curSeg.m_color.r = c.r; curSeg.m_color.g = c.g; curSeg.m_color.b = c.b;

		float flAlphaFade = flLifePerc;
		if ( flTailAlphaDist > 0.0f )
		{
			if ( pPrevPoint )
			{
				float flDist = pPoint->m_vecScreenPos.DistTo( pPrevPoint->m_vecScreenPos );
				flTailAlphaDist -= flDist;
			}

			if ( flTailAlphaDist > 0.0f )
			{
				float flTailFade = Lerp( (m_flMinFadeLength - flTailAlphaDist) / m_flMinFadeLength, 0.0f, 1.0f );
				if ( flTailFade < flAlphaFade )
				{
					flAlphaFade = flTailFade;
				}
			}
		}
		curSeg.m_color.a  = GetRenderBrightness() * flAlphaFade;

#if SCREEN_SPACE_TRAILS
		curSeg.m_vPos = viewMatrix * pPoint->m_vecScreenPos;
#else
		curSeg.m_vPos = pPoint->m_vecScreenPos;
#endif

		if ( m_flEndWidth >= 0.0f )
		{
			curSeg.m_flWidth = Lerp( flLifePerc, m_flEndWidth.Get(), m_flStartWidth.Get() );
		}
		else
		{
			curSeg.m_flWidth = m_flStartWidth.Get();
		}
		curSeg.m_flWidth += pPoint->m_flWidthVariance;
		if ( curSeg.m_flWidth < 0.0f )
		{
			curSeg.m_flWidth = 0.0f;
		}

		curSeg.m_flTexCoord = pPoint->m_flTexCoord;

		segDraw.NextSeg( &curSeg );

		// See if we're done with this bad boy
		if ( pPoint->m_flDieTime <= gpGlobals->curtime )
		{
			// Push this back onto the top for use
			++m_nFirstStep;
			--i;
			--m_nStepCount;
		}

		pPrevPoint = pPoint;
	}

	segDraw.End();

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Vector const&
//-----------------------------------------------------------------------------
const Vector &CSpriteTrail::GetRenderOrigin( void )
{
	static Vector vOrigin;
	vOrigin = GetAbsOrigin();

	if ( m_hAttachedToEntity )
	{
		C_BaseEntity *ent = m_hAttachedToEntity->GetBaseEntity();
		if ( ent )
		{
			QAngle dummyAngles;
			ent->GetAttachment( m_nAttachment, vOrigin, dummyAngles );
		}
	}

	return vOrigin;
}

const QAngle &CSpriteTrail::GetRenderAngles( void )
{
	return vec3_angle;
}

#endif	//CLIENT_DLL

#if !defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSpriteName - 
//			&origin - 
//			animate - 
// Output : CSpriteTrail
//-----------------------------------------------------------------------------
CSpriteTrail *CSpriteTrail::SpriteTrailCreate( const char *pSpriteName, const Vector &origin, bool animate )
{
	CSpriteTrail *pSprite = CREATE_ENTITY( CSpriteTrail, "env_spritetrail" );

	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->SetSolid( SOLID_NONE );
	pSprite->SetMoveType( MOVETYPE_NOCLIP );
	
	UTIL_SetSize( pSprite, vec3_origin, vec3_origin );

	if ( animate )
	{
		pSprite->TurnOn();
	}

	return pSprite;
}

#endif	//CLIENT_DLL == false
