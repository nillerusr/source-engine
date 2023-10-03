//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "Sprite.h"

#ifdef CLIENT_DLL
	#include "asw_hud_3dmarinenames.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef CLIENT_DLL
	#define CASWHealthBar C_ASWHealthBar
#endif


class CASWHealthBar : public CSprite
#ifdef CLIENT_DLL
	, public IHealthTracked
#endif
{
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif
	DECLARE_CLASS( CASWHealthBar, CSprite );
	DECLARE_NETWORKCLASS();

public:

#ifndef CLIENT_DLL
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual void Spawn( void );

	void TrackHealthThink( void );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
#else
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	// IHealthTracked
	IMPLEMENT_AUTO_LIST_GET();
	virtual void PaintHealthBar( class CASWHud3DMarineNames *pSurface );
#endif

private:

	CNetworkVar( float, m_fHealthFraction );
	CNetworkVar( bool, m_bDisabled );
	CNetworkVar( bool, m_bHideAtFullHealth );

};


LINK_ENTITY_TO_CLASS( asw_health_bar, CASWHealthBar );

IMPLEMENT_NETWORKCLASS_ALIASED( ASWHealthBar, DT_ASWHealthBar );

BEGIN_NETWORK_TABLE( CASWHealthBar, DT_ASWHealthBar )
#ifndef CLIENT_DLL
	SendPropFloat( SENDINFO(m_fHealthFraction), 0,	SPROP_NOSCALE ),
	SendPropBool( SENDINFO(m_bDisabled) ),
	SendPropBool( SENDINFO(m_bHideAtFullHealth) ),
#else
	RecvPropFloat( RECVINFO(m_fHealthFraction) ),
	RecvPropBool( RECVINFO(m_bDisabled) ),
	RecvPropBool( RECVINFO(m_bHideAtFullHealth) ),
#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASWHealthBar )
	DEFINE_FIELD( m_fHealthFraction, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_bHideAtFullHealth, FIELD_BOOLEAN, "hideatfullhealth" ),

	DEFINE_FUNCTION( TrackHealthThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
END_DATADESC()
#endif


#ifndef CLIENT_DLL

bool CASWHealthBar::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		SetRenderColor( tmp[0], tmp[1], tmp[2] );
		SetRenderAlpha( tmp[3] );
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

void CASWHealthBar::Spawn( void )
{
	SetThink( &CASWHealthBar::TrackHealthThink );
	SetNextThink( gpGlobals->curtime );
}

void CASWHealthBar::TrackHealthThink( void )
{
	SetNextThink( gpGlobals->curtime );

	CBaseEntity *pParent = GetParent();

	if ( !pParent )
		return;

	m_fHealthFraction = pParent->GetHealth() / (float)pParent->GetMaxHealth();
}

void CASWHealthBar::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CASWHealthBar::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

#else

int CASWHealthBar::DrawModel( int flags, const RenderableInstance_t &instance )
{
	return 0;
}

// IHealthTracked
void CASWHealthBar::PaintHealthBar( CASWHud3DMarineNames *pSurface )
{
	if ( m_bDisabled )
		return;

	if ( ( m_bHideAtFullHealth && m_fHealthFraction >= 1.0f ) || m_fHealthFraction <= 0 )
		return;

	color24 rgbColor = GetRenderColor();
	pSurface->PaintGenericBar( GetRenderOrigin(), m_fHealthFraction, Color( rgbColor.r, rgbColor.g, rgbColor.b, GetRenderAlpha() ), GetScale() );
}

#endif
