#include "cbase.h"
#ifdef CLIENT_DLL
	#define CBaseAnimating C_BaseAnimating
	#define CASW_Target_Dummy C_ASW_Target_Dummy
	#include "asw_hud_3dmarinenames.h"
#else

#endif
#include "asw_target_dummy_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Target_Dummy, DT_ASW_Target_Dummy )

BEGIN_NETWORK_TABLE( CASW_Target_Dummy, DT_ASW_Target_Dummy )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flDamageTaken ) ),
	RecvPropFloat( RECVINFO( m_flStartDamageTime ) ),
	RecvPropFloat( RECVINFO( m_flLastDamageTime ) ),
	RecvPropFloat( RECVINFO( m_flLastHit ) ),
#else
	SendPropFloat( SENDINFO( m_flDamageTaken ) ),
	SendPropFloat( SENDINFO( m_flStartDamageTime ) ),
	SendPropFloat( SENDINFO( m_flLastDamageTime ) ),
	SendPropFloat( SENDINFO( m_flLastHit ) ),
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
LINK_ENTITY_TO_CLASS( asw_target_dummy, CASW_Target_Dummy );
PRECACHE_WEAPON_REGISTER( asw_target_dummy );
#endif

#define ASW_TARGET_DUMMY_MODEL "models/props/furniture/misc/Fridge.mdl"

#ifndef CLIENT_DLL

BEGIN_DATADESC( CASW_Target_Dummy )
END_DATADESC()

#endif


#ifndef CLIENT_DLL

CUtlVector<CASW_Target_Dummy*> g_vecTargetDummies;

CASW_Target_Dummy::CASW_Target_Dummy()
{
	g_vecTargetDummies.AddToTail( this );
}

CASW_Target_Dummy::~CASW_Target_Dummy()
{
	g_vecTargetDummies.FindAndRemove( this );
}

void CASW_Target_Dummy::Spawn()
{
	BaseClass::Spawn();
	
	//SetModelName( AllocPooledString( ASW_TARGET_DUMMY_MODEL ) );
	
	Precache();
	SetModel( ASW_TARGET_DUMMY_MODEL );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	SetCollisionGroup( COLLISION_GROUP_NONE ); //COLLISION_GROUP_DEBRIS );
	m_takedamage = DAMAGE_YES;
	m_iHealth = 100;
	m_iMaxHealth = m_iHealth;	

	AddFlag( FL_STATICPROP );
	VPhysicsInitStatic();

	ResetThink();
}

void CASW_Target_Dummy::ResetThink()
{
	m_flDamageTaken = 0.0f;
	m_flStartDamageTime = -1.0f;
	m_flLastDamageTime = -1.0f;
	m_flLastHit = 0.0f;

	SetThink( NULL );
}


void CASW_Target_Dummy::Precache()
{
	PrecacheModel( ASW_TARGET_DUMMY_MODEL );

	BaseClass::Precache();
}

int CASW_Target_Dummy::OnTakeDamage( const CTakeDamageInfo &info )
{
	m_flDamageTaken += info.GetDamage();
	m_flLastHit = info.GetDamage();

	if ( m_flStartDamageTime < 0 )
	{
		m_flStartDamageTime = gpGlobals->curtime;
	}
	m_flLastDamageTime = gpGlobals->curtime;

	SetThink( &CASW_Target_Dummy::ResetThink );
	SetNextThink( gpGlobals->curtime + 5.0f );	

	return 0;
}

#else

float CASW_Target_Dummy::GetDPS()
{
	if ( m_flStartDamageTime < 0 || m_flLastDamageTime < 0 || ( m_flLastDamageTime - m_flStartDamageTime ) <= 0 )
	{
		return 0.0f;
	}
	float flTime = m_flLastDamageTime - m_flStartDamageTime;
	return GetDamageTaken() / flTime;
}

void CASW_Target_Dummy::PaintHealthBar( CASWHud3DMarineNames *pSurface )
{
	char pText[64];
	Q_snprintf( pText, sizeof( pText ), "DPS: %.2f", GetDPS() );

	pSurface->PaintGenericText( GetRenderOrigin() + Vector( -20, -10, 0), pText,	  Color( 140, 240, 240, 255 ), 1.0f, Vector2D( 0, -6 )  );

	Q_snprintf( pText, sizeof( pText ), "DAMAGE:%.2f", GetDamageTaken() );
	pSurface->PaintGenericText( GetRenderOrigin() + Vector( -20, -10, 0), pText,	  Color( 140, 240, 240, 255 ), 1.0f, Vector2D( 0, -5 )  );

	Q_snprintf( pText, sizeof( pText ), "LAST:%.2f", m_flLastHit.Get() );
	pSurface->PaintGenericText( GetRenderOrigin() + Vector( -20, -10, 0), pText,	  Color( 140, 240, 240, 255 ), 1.0f, Vector2D( 0, -4 )  );
}
#endif // not client
