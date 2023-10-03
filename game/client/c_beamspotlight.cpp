//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "dlight.h"
#include "iefx.h"

#include "beam_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSpotlightTraceCacheEntry
{
public:
	CSpotlightTraceCacheEntry()
	{
		m_origin.Init();
		m_radius = -1.0f;
	}
	bool IsValidFor( const Vector &origin )
	{
		if ( m_radius > 0 && m_origin.DistToSqr(origin) < 1.0f )
			return true;
		return false;
	}
	void Cache( const Vector &origin, const trace_t &tr )
	{
		m_radius = (tr.endpos - origin).Length();
		m_origin = origin;
	}

	Vector	m_origin;
	float	m_radius;
};

static const int NUM_CACHE_ENTRIES = 64;

class C_BeamSpotLight : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_BeamSpotLight, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_BeamSpotLight();
	~C_BeamSpotLight();

	bool ShouldDraw();
	void ClientThink( void );
	void OnDataChanged( DataUpdateType_t updateType );
	void Release( void );

private:

	Vector SpotlightCurrentPos(void);
	void SpotlightCreate(void);
	void SpotlightDestroy(void);

	// Computes render info for a spotlight
	void ComputeRenderInfo();

private:

	int		m_nHaloIndex;
	int		m_nRotationAxis;
	float	m_flRotationSpeed;
	

	bool m_bSpotlightOn;
	bool m_bHasDynamicLight;

	float m_flSpotlightMaxLength;
	float m_flSpotlightGoalWidth;
	float m_flHDRColorScale;

	Vector	m_vSpotlightTargetPos;
	Vector	m_vSpotlightCurrentPos;
	Vector	m_vSpotlightDir;

	CHandle<C_Beam>	m_hSpotlight;
	
	float	m_flSpotlightCurLength;

	float	m_flLightScale;

	dlight_t*	m_pDynamicLight;

	float m_lastTime;
	CSpotlightTraceCacheEntry *m_pCache;
};

LINK_ENTITY_TO_CLASS( beam_spotlight, C_BeamSpotLight );

IMPLEMENT_CLIENTCLASS_DT( C_BeamSpotLight, DT_BeamSpotlight, CBeamSpotlight )
	RecvPropInt(   RECVINFO(m_nHaloIndex) ),
	RecvPropBool(  RECVINFO(m_bSpotlightOn) ),
	RecvPropBool(  RECVINFO(m_bHasDynamicLight) ),
	RecvPropFloat( RECVINFO(m_flSpotlightMaxLength) ),
	RecvPropFloat( RECVINFO(m_flSpotlightGoalWidth) ),
	RecvPropFloat( RECVINFO(m_flHDRColorScale) ),
	RecvPropInt(   RECVINFO(m_nRotationAxis) ),
	RecvPropFloat( RECVINFO(m_flRotationSpeed) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
C_BeamSpotLight::C_BeamSpotLight()
: m_vSpotlightTargetPos( vec3_origin )
, m_vSpotlightCurrentPos( vec3_origin )
, m_vSpotlightDir( vec3_origin )
, m_flSpotlightCurLength( 0.0f )
, m_flLightScale( 100.0f )
, m_pDynamicLight( NULL )
, m_lastTime( 0.0f )
, m_pCache(NULL)
{
}

C_BeamSpotLight::~C_BeamSpotLight()
{
	delete[] m_pCache;
}


//-----------------------------------------------------------------------------
bool C_BeamSpotLight::ShouldDraw()
{
	return false;
}

//-----------------------------------------------------------------------------
void C_BeamSpotLight::ClientThink( void )
{
 	float dt = gpGlobals->curtime - m_lastTime;
 	if ( !m_lastTime )
 	{
 		dt = 0.0f;
 	}
 	m_lastTime = gpGlobals->curtime;

	// ---------------------------------------------------
	//  If I don't have a spotlight attempt to create one
	// ---------------------------------------------------
	if ( !m_hSpotlight )
	{
		if ( m_bSpotlightOn )
		{
			// Make the spotlight
			SpotlightCreate();
		}
		else
		{
			SetNextClientThink( CLIENT_THINK_NEVER );
			return;
		}
	}
	else if ( !m_bSpotlightOn )
	{
		SpotlightDestroy();
		SetNextClientThink( CLIENT_THINK_NEVER );
		return;
	}

	// update rotation
	if ( m_flRotationSpeed != 0.0f )
	{
		QAngle angles = GetAbsAngles();
		angles[m_nRotationAxis] += m_flRotationSpeed * dt;
		angles[m_nRotationAxis] = anglemod(angles[m_nRotationAxis]);
		if ( !m_pCache )
		{
			m_pCache = new CSpotlightTraceCacheEntry[NUM_CACHE_ENTRIES];
		}

		SetAbsAngles( angles );
	}
	m_vSpotlightCurrentPos = SpotlightCurrentPos();

	Assert( m_hSpotlight );

	m_hSpotlight->SetStartPos( GetAbsOrigin() );
	m_hSpotlight->SetEndPos( m_vSpotlightCurrentPos );

	// Avoid sudden change in where beam fades out when cross disconinuities
	Vector dir = m_vSpotlightCurrentPos - GetAbsOrigin();
	float flBeamLength	= VectorNormalize( dir );
	m_flSpotlightCurLength = (0.60*m_flSpotlightCurLength) + (0.4*flBeamLength);

	ComputeRenderInfo();

	m_hSpotlight->RelinkBeam();

	//NDebugOverlay::Cross3D(GetAbsOrigin(),Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightCurrentPos,Vector(-5,-5,-5),Vector(5,5,5),0,255,0,true,0.1);
	//NDebugOverlay::Cross3D(m_vSpotlightTargetPos,Vector(-5,-5,-5),Vector(5,5,5),255,0,0,true,0.1);

	// Do we need to keep updating?
	if ( !GetMoveParent() && m_flRotationSpeed == 0 )
	{
		// No reason to think again, we're not going to move unless there's a data change
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

//-----------------------------------------------------------------------------
void C_BeamSpotLight::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_flSpotlightCurLength = m_flSpotlightMaxLength;
	}

	// On a data change always think again
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//------------------------------------------------------------------------------
void C_BeamSpotLight::Release()
{
	SpotlightDestroy();
	BaseClass::Release();
}

//------------------------------------------------------------------------------
void C_BeamSpotLight::SpotlightCreate(void)
{
	m_vSpotlightTargetPos = SpotlightCurrentPos();

	{
		//C_Beam *beam = CBeam::BeamCreate( "sprites/spotlight.vmt", m_flSpotlightGoalWidth );
		C_Beam *beam = C_Beam::BeamCreate( "sprites/glow_test02.vmt", m_flSpotlightGoalWidth );
		// Beam only exists client side
		ClientEntityList().AddNonNetworkableEntity( beam );
		m_hSpotlight = beam;
	}

	// Set the temporary spawnflag on the beam so it doesn't save (we'll recreate it on restore)
	m_hSpotlight->SetHDRColorScale( m_flHDRColorScale );
	const color24 c = GetRenderColor();
	m_hSpotlight->SetColor( c.r, c.g, c.b ); 
	m_hSpotlight->SetHaloTexture(m_nHaloIndex);
	m_hSpotlight->SetHaloScale(60);
	m_hSpotlight->SetEndWidth(m_flSpotlightGoalWidth);
	m_hSpotlight->SetBeamFlags( (FBEAM_SHADEOUT|FBEAM_NOTILE) );
	m_hSpotlight->SetBrightness( 64 );
	m_hSpotlight->SetNoise( 0 );

	m_hSpotlight->PointsInit( GetAbsOrigin(), m_vSpotlightTargetPos );
}

//------------------------------------------------------------------------------
void C_BeamSpotLight::SpotlightDestroy(void)
{
	if ( m_hSpotlight )
	{
		UTIL_Remove( m_hSpotlight );
		m_hSpotlight.Term();
	}
}

//------------------------------------------------------------------------------
Vector C_BeamSpotLight::SpotlightCurrentPos(void)
{
	QAngle angles = GetAbsAngles();
	GetVectors( &m_vSpotlightDir, NULL, NULL );
	Vector position = GetAbsOrigin();
	int cacheIndex = -1;
	if ( m_pCache )
	{
		cacheIndex = int( angles[m_nRotationAxis] * float(NUM_CACHE_ENTRIES) * (1.0f / 360.0f)) & (NUM_CACHE_ENTRIES - 1);
		if ( m_pCache[cacheIndex].IsValidFor(GetAbsOrigin()) )
		{
			return position + m_vSpotlightDir * m_pCache[cacheIndex].m_radius;
		}
	}


	//	Get beam end point.  Only collide with solid objects, not npcs
	trace_t tr;
	UTIL_TraceLine( position, position + (m_vSpotlightDir * 2 * m_flSpotlightMaxLength), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	if ( cacheIndex >= 0 )
	{
		m_pCache[cacheIndex].Cache(position, tr);
	}

	return tr.endpos;
}

//-----------------------------------------------------------------------------
// Computes render info for a spotlight
//-----------------------------------------------------------------------------
void C_BeamSpotLight::ComputeRenderInfo()
{
	// Fade out spotlight end if past max length.  
	if ( m_flSpotlightCurLength > 2*m_flSpotlightMaxLength )
	{
		SetRenderAlpha( 0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else if ( m_flSpotlightCurLength > m_flSpotlightMaxLength )		
	{
		SetRenderAlpha( (1-((m_flSpotlightCurLength-m_flSpotlightMaxLength)/m_flSpotlightMaxLength)) );
		m_hSpotlight->SetFadeLength( m_flSpotlightMaxLength );
	}
	else
	{
		SetRenderAlpha( 1.0 );
		m_hSpotlight->SetFadeLength( m_flSpotlightCurLength );
	}

	// Adjust end width to keep beam width constant
	float flNewWidth = m_flSpotlightGoalWidth * (m_flSpotlightCurLength / m_flSpotlightMaxLength);
	flNewWidth = clamp(flNewWidth, 0, MAX_BEAM_WIDTH );
	m_hSpotlight->SetEndWidth(flNewWidth);

	if ( m_bHasDynamicLight )
	{
		// <<TODO>> - magic number 1.8 depends on sprite size
		m_flLightScale = 1.8*flNewWidth;

		if ( m_flLightScale > 0 ) 
		{
			const color24 c = GetRenderColor();
			float a = GetRenderAlpha() / 255.0f;
			ColorRGBExp32 color;
			color.r	= c.r * a;
			color.g	= c.g * a;
			color.b	= c.b * a;
			color.exponent = 0;
			if ( color.r == 0 && color.g == 0 && color.b == 0 )
				return;
		
			// Deal with the environment light
			if ( !m_pDynamicLight || (m_pDynamicLight->key != index) )
			{
				m_pDynamicLight = effects->CL_AllocDlight( index );
				assert (m_pDynamicLight);
			}
		
			//m_pDynamicLight->flags = DLIGHT_NO_MODEL_ILLUMINATION;
			m_pDynamicLight->radius		= m_flLightScale*3.0f;
			m_pDynamicLight->origin		= GetAbsOrigin() + Vector(0,0,5);
			m_pDynamicLight->die		= gpGlobals->curtime + 0.05f;
			m_pDynamicLight->color		= color;
		}
	}
}

