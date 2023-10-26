#include "cbase.h"
#include "precache_register.h"
#include "particles_simple.h"
#include "iefx.h"
#include "dlight.h"
#include "view.h"
#include "fx.h"
#include "clientsideeffects.h"
#include "c_pixel_visibility.h"
#include "c_asw_flare_projectile.h"
#include "soundenvelope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, ASWPrecacheEffectFlares )
PRECACHE_REGISTER_END()


IMPLEMENT_CLIENTCLASS_DT( C_ASW_Flare_Projectile, DT_ASW_Flare_Projectile, CASW_Flare_Projectile )
RecvPropFloat( RECVINFO( m_flTimeBurnOut ) ),
RecvPropFloat( RECVINFO( m_flScale ) ),
RecvPropInt( RECVINFO( m_bLight ) ),
RecvPropInt( RECVINFO( m_bSmoke ) ),
END_RECV_TABLE()

// flares maintain a linked list of themselves, for quick checking for autoaim
C_ASW_Flare_Projectile* g_pHeadFlare = NULL;

ConVar asw_flare_r("asw_flare_r", "240", 0, "Colour of flares");
ConVar asw_flare_g("asw_flare_g", "255", 0, "Colour of flares");
ConVar asw_flare_b("asw_flare_b", "200", 0, "Colour of flares");

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
C_ASW_Flare_Projectile::C_ASW_Flare_Projectile()
{
	m_flTimeBurnOut	= 0.0f;
	m_pDLight = NULL;

	m_bLight		= true;
	m_bSmoke		= true;

	//SetDynamicallyAllocated( false );
	m_queryHandle = 0;
	m_fStartLightTime = 0;
	m_fLightRadius = 0;

	m_pFlareEffect = NULL;

	// keep a linked list of flares (used for autoaim)
	if (g_pHeadFlare)
	{
		m_pNextFlare = g_pHeadFlare;
		g_pHeadFlare = this;
	}
	else
	{
		g_pHeadFlare = this;
		m_pNextFlare = NULL;
	}
}

C_ASW_Flare_Projectile::~C_ASW_Flare_Projectile( void )
{
	// remove ourselves from the linked list of flares
	if (g_pHeadFlare == this)
	{
		g_pHeadFlare = m_pNextFlare;
	}
	else
	{
		C_ASW_Flare_Projectile* pFlare = g_pHeadFlare;
		int k=0;
		while (pFlare != this && pFlare != NULL && k < 256)	// some paranoid checks (should always break out of the while anyway)
		{
			k++;
			if (pFlare->m_pNextFlare == this)
			{
				pFlare->m_pNextFlare = m_pNextFlare;		// pulled ourselves out of the list
				break;
			}
			pFlare = pFlare->m_pNextFlare;			
		}
	}

	if (m_pDLight)
	{
		m_pDLight->die		= gpGlobals->curtime;
		m_pDLight = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void C_ASW_Flare_Projectile::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	if ( state == SHOULDTRANSMIT_END )
	{
		AddEffects( EF_NODRAW );
	}
	else if ( state == SHOULDTRANSMIT_START )
	{
		RemoveEffects( EF_NODRAW );
	}

	BaseClass::NotifyShouldTransmit( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_ASW_Flare_Projectile::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink(gpGlobals->curtime);
		//SetSortOrigin( GetAbsOrigin() );
		SoundInit();
	}

	BaseClass::OnDataChanged( updateType );
}

const Vector& C_ASW_Flare_Projectile::GetEffectOrigin()
{
	static Vector s_vecEffectPos;
	Vector forward, right, up;
	AngleVectors(GetAbsAngles(), &forward, &right, &up);
	s_vecEffectPos = GetAbsOrigin() + up * 5;
	return s_vecEffectPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : timeDelta - 
//-----------------------------------------------------------------------------
//void C_ASW_Flare_Projectile::Update( float timeDelta )
void C_ASW_Flare_Projectile::ClientThink( void )
{
	if ( m_pFlareEffect.GetObject() == NULL )
	{	
		//m_flTimePulse = gpGlobals->curtime + asw_buffgrenade_pulse_interval.GetFloat();

		m_pFlareEffect = ParticleProp()->Create( "flare_fx_main", PATTACH_ABSORIGIN_FOLLOW, -1, GetEffectOrigin() - GetAbsOrigin() );
		//flare_fx_main
	}

	float	baseScale = m_flScale;

	//Account for fading out
	if ( ( m_flTimeBurnOut != -1.0f ) && ( ( m_flTimeBurnOut - gpGlobals->curtime ) <= 10.0f ) )
	{
		baseScale *= ( ( m_flTimeBurnOut - gpGlobals->curtime ) / 10.0f );

		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pBurnSound, clamp<float>(0.6f * baseScale, 0.0f, 0.6f), 0 );
	}

	if ( baseScale < 0.01f )
		return;
	//
	// Dynamic light
	//

	if ( m_bLight )
	{
		if (m_fStartLightTime == 0)
		{
			m_fStartLightTime = gpGlobals->curtime;
		}
		if (!m_pDLight)
		{
			m_pDLight = effects->CL_AllocDlight( index );
			m_pDLight->color.r = asw_flare_r.GetInt();
			m_pDLight->color.g = asw_flare_g.GetInt();
			m_pDLight->color.b = asw_flare_b.GetInt();
			m_pDLight->color.exponent = 3;
		}

		m_pDLight->origin	= GetAbsOrigin() + Vector(0, 0, 5);	// make the dlight slightly higher than the flare, so it doesn't bury the light being so close to the ground

		float flTimeLeft = MAX( 2.0, m_flTimeBurnOut - gpGlobals->curtime );
		if (m_fLightRadius < 8.0f)
		{
			m_fLightRadius += flTimeLeft * (1.0f + random->RandomFloat() * 36.0f);
			if (m_fLightRadius > 8.0f)
				m_fLightRadius = 8.0f;
		}
		m_pDLight->radius = MAX( 64.0, baseScale * 120 * (m_fLightRadius/8.0f) );

		if ( ( m_flTimeBurnOut != -1.0f ) && ( ( m_flTimeBurnOut - gpGlobals->curtime ) <= 4.0f ) )
		{
			// flicker as we're going out
			//m_pDLight->die		= gpGlobals->curtime;
			//m_pDLight = NULL;
		}
		else
		{
			m_pDLight->die		= gpGlobals->curtime + 30.0f;
		}
	}
	else
	{
		m_fStartLightTime = 0;
	}

	SetNextClientThink(gpGlobals->curtime + 0.1f);
}

void C_ASW_Flare_Projectile::OnRestore()
{
	BaseClass::OnRestore();
	SoundInit();	
}

void C_ASW_Flare_Projectile::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	SoundShutdown();
}

void C_ASW_Flare_Projectile::SoundInit()
{
	// play flare start sound!!
	CPASAttenuationFilter filter( this );

	EmitSound("ASW_Flare.IgniteFlare");

	// Bring up the flare burning loop sound
	if( !m_pBurnSound )
	{
		m_pBurnSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASW_Flare.FlareLoop" );
		CSoundEnvelopeController::GetController().Play( m_pBurnSound, 0.0, 100 );
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pBurnSound, 0.6, 2.0 );
	}
}

void C_ASW_Flare_Projectile::SoundShutdown()
{	
	if ( m_pBurnSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pBurnSound );
		m_pBurnSound = NULL;
	}
}
