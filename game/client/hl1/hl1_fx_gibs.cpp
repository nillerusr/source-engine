//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx.h"
#include "c_gib.h"
#include "c_te_effect_dispatch.h"
#include "iefx.h"
#include "decals.h"

#define HUMAN_GIBS 1
#define ALIEN_GIBS 2

#define MAX_GIBS 4

#define	HUMAN_GIB_COUNT			6
#define ALIEN_GIB_COUNT			4

const char *pHumanGibsModel = "models/gibs/hgibs.mdl";
const char *pAlienGibsModel = "models/gibs/agibs.mdl";

void GetBloodColorHL1( int bloodtype, unsigned char &r, unsigned char &g, unsigned char &b )
{
	if (bloodtype == BLOOD_COLOR_RED)
	{
		r = 64;
		g = 0;
		b = 0;
	}
	else if ( bloodtype == BLOOD_COLOR_GREEN ) 
	{
		r = 195;
		g = 195;
		b = 0;
	}
	else if ( bloodtype == BLOOD_COLOR_YELLOW )
	{
		r = 0;
		g = 195;
		b = 195;
	}
}

class C_HL1Gib : public C_Gib
{
	typedef C_BaseAnimating BaseClass;
public:
	
	static C_HL1Gib *CreateClientsideGib( const char *pszModelName, Vector vecOrigin, Vector vecForceDir, AngularImpulse vecAngularImp )
	{
		C_HL1Gib *pGib = new C_HL1Gib;

		if ( pGib == NULL )
			return NULL;

		if ( pGib->InitializeGib( pszModelName, vecOrigin, vecForceDir, vecAngularImp ) == false )
			return NULL;

		return pGib;
	}

	// Decal the surface
	virtual	void HitSurface( C_BaseEntity *pOther )
	{
		int index = -1;
		if ( m_iType == HUMAN_GIBS )
		{
			if ( !UTIL_IsLowViolence() ) // no blood decals if we're low violence.
			{
				index = decalsystem->GetDecalIndexForName( "Blood" );
			}
		}
		else
		{
			index = decalsystem->GetDecalIndexForName( "YellowBlood" );
		}

		if ( index >= 0 )
		{
			effects->DecalShoot( index, pOther->entindex(), pOther->GetModel(), pOther->GetAbsOrigin(), pOther->GetAbsAngles(), GetAbsOrigin(), 0, 0 );
		}


		if ( GetFlags() & FL_ONGROUND )
		{
			QAngle vAngles = GetAbsAngles();
			QAngle vAngularVelocity = GetLocalAngularVelocity();

			SetAbsVelocity( GetAbsVelocity() * 0.9 );

			vAngles.x = 0;
			vAngles.z = 0;
			vAngularVelocity.x = 0;
			vAngularVelocity.z = 0;

			SetAbsAngles( vAngles );
			SetLocalAngularVelocity( vAngularVelocity );
		}
	}

	virtual void ClientThink( void );

	int m_iType;
};

void C_HL1Gib::ClientThink( void )
{
	SetRenderMode( kRenderTransAlpha );
	m_nRenderFX		= kRenderFxFadeSlow;

	if ( m_clrRender->a == 5 )
	{
		Release();
		return;
	}

	SetNextClientThink( gpGlobals->curtime + 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//-----------------------------------------------------------------------------
void FX_HL1Gib( const Vector &origin, const Vector &direction, float scale, int iType, int iHealth, int iColor )
{
	Vector	offset;
	int i;
	
	offset = RandomVector( -16, 16 ) + origin;

	if ( iType == HUMAN_GIBS )
	{
		Vector vVelocity;
		AngularImpulse aImpulse;

		// mix in some noise
		vVelocity.x = random->RandomFloat( -100,100 );
		vVelocity.y = random->RandomFloat ( -100,100 );
		vVelocity.z = random->RandomFloat ( 200,300 );

		aImpulse.x = random->RandomFloat ( 100, 200 );
		aImpulse.y = random->RandomFloat ( 100, 300 );

		if ( iHealth > -50)
		{
			vVelocity = vVelocity * 0.7;
		}
		else if ( iHealth > -200)
		{
			vVelocity = vVelocity * 2;
		}
		else
		{
			vVelocity = vVelocity * 4;
		}

		
		C_HL1Gib *pGib = C_HL1Gib::CreateClientsideGib( pHumanGibsModel, offset, vVelocity * 2, aImpulse );

		//Spawn a head.
		if ( pGib )
		{
			pGib->m_nBody = 0;
			pGib->m_iType = iType;
		}
	}

	// Spawn all the unique gibs
	for ( i = 0; i < MAX_GIBS; i++ )
	{
		const char *pModelName = NULL;
		int  iNumBody = 0;

		offset = RandomVector( -16, 16 ) + origin;

		//TODO
		Vector vVelocity = direction;
		AngularImpulse aAImpulse;

		// mix in some noise
		vVelocity.x += random->RandomFloat( -0.25, 0.25 );
		vVelocity.y += random->RandomFloat ( -0.25, 0.25 );
		vVelocity.z += random->RandomFloat ( -0.25, 0.25 );

		vVelocity = vVelocity * random->RandomFloat ( 300, 400 );

		if ( iHealth > -50)
		{
			vVelocity = vVelocity * 0.7;
		}
		else if ( iHealth > -200)
		{
			vVelocity = vVelocity * 2;
		}
		else
		{
			vVelocity = vVelocity * 4;
		}

		aAImpulse.x = random->RandomFloat ( 100, 200 );
		aAImpulse.y = random->RandomFloat ( 100, 300 );

		if ( iType == HUMAN_GIBS )
		{
			pModelName = pHumanGibsModel;
			iNumBody = HUMAN_GIB_COUNT;
		}
		else
		{
			pModelName = pAlienGibsModel;
			iNumBody = ALIEN_GIB_COUNT;
		}
			 

		C_HL1Gib *pGib = C_HL1Gib::CreateClientsideGib( pModelName, offset, vVelocity * 2, aAImpulse );

		if ( pGib )
		{
			if ( iType == HUMAN_GIBS )
				 pGib->m_nBody = random->RandomInt( 1, iNumBody-1 );
			else 
				 pGib->m_nBody = random->RandomInt( 0, iNumBody-1 );

			pGib->m_iType = iType;
		}
	}

	//
	// Throw some blood (unless we're low violence, then we're done)
	//
	if ( iColor == BLOOD_COLOR_RED && UTIL_IsLowViolence() )
		return;

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "FX_HL1Gib" );
	pSimple->SetSortOrigin( origin );

	Vector	vDir;

	vDir.Random( -1.0f, 1.0f );

	for ( i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[0], origin );
			
		if ( sParticle == NULL )
			return;

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 1;
			
		float	speed = random->RandomFloat( 32.0f, 128.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] -= 16.0f;

		GetBloodColorHL1( iColor, sParticle->m_uchColor[0], sParticle->m_uchColor[1], sParticle->m_uchColor[2] );
		
		sParticle->m_uchStartAlpha	= 255;
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*random->RandomInt( 1, 4 );
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
	}

	for ( i = 0; i < 4; i++ )
	{
		SimpleParticle *sParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_BloodPuff[1], origin );
			
		if ( sParticle == NULL )
		{
			return;
		}

		sParticle->m_flLifetime		= 0.0f;
		sParticle->m_flDieTime		= 1;
			
		float	speed = random->RandomFloat( 16.0f, 128.0f );

		sParticle->m_vecVelocity	= vDir * -speed;
		sParticle->m_vecVelocity[2] -= 16.0f;

		GetBloodColorHL1( iColor, sParticle->m_uchColor[0], sParticle->m_uchColor[1], sParticle->m_uchColor[2] );

		sParticle->m_uchStartAlpha	= random->RandomInt( 64, 128 );
		sParticle->m_uchEndAlpha	= 0;
		sParticle->m_uchStartSize	= random->RandomInt( 16, 32 );
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize*random->RandomInt( 1, 4 );
		sParticle->m_flRoll			= random->RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void HL1GibCallback( const CEffectData &data )
{
	FX_HL1Gib( data.m_vOrigin, data.m_vNormal, data.m_flScale, data.m_nMaterial, -data.m_nHitBox, data.m_nColor );
}

DECLARE_CLIENT_EFFECT( "HL1Gib", HL1GibCallback );
