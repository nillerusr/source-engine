//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2's Hud health display
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hud_numeric.h"
#include "c_basetfplayer.h"
#include "basetfvehicle.h"
#include <vgui_controls/AnimationController.h>
#include <KeyValues.h>
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Local player health
//-----------------------------------------------------------------------------
class CHudHealth : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudHealth, CHudNumeric );

public:
	CHudHealth( const char *pElementName );

	virtual const char *GetLabelText() { return m_szHealthLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "HealthIncrement" : "HealthDecrement"; }
	virtual bool		GetValue( char *val, int maxlen );

	virtual Color GetColor();
	virtual Color GetBoxColor();

private:
	bool				GetHealth( int& value );

	CPanelAnimationStringVar( 128, m_szHealthLabel, "HealthLabel", "Health" );
};

DECLARE_HUDELEMENT( CHudHealth );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudHealth::CHudHealth( const char *pElementName ) :
	CHudNumeric( pElementName, "HudHealth" )
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

bool CHudHealth::GetHealth( int& value )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	value = pPlayer->GetHealth();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudHealth::GetValue( char *val, int maxlen )
{
	int value = 0;
	if ( !GetHealth( value ) )
		return false;

	Q_snprintf( val, maxlen, "%i", value );
	return true;
}

Color CHudHealth::GetColor()
{
	int value = 0;
	if ( !GetHealth( value ) || value > 25  )
		return m_TextColor;

	return m_TextColorCritical;
}

Color CHudHealth::GetBoxColor()
{
	int value = 0;
	if ( !GetHealth( value ) || value > 25  )
		return m_BoxColor;
	return m_BoxColorCritical;
}

//-----------------------------------------------------------------------------
// Purpose: Local player's vehicle health, if in one
//-----------------------------------------------------------------------------
class CHudVehicleHealth : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudVehicleHealth, CHudNumeric );

public:
	CHudVehicleHealth( const char *pElementName );

	virtual const char *GetLabelText() { return m_szVehicleHealthLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "HealthVehicleIncrement" : "HealthVehicleDecrement"; }
	virtual bool		GetValue( char *val, int maxlen );

	virtual Color GetColor();
	virtual Color GetBoxColor();

private:
	bool				GetHealth( int& value );

	CPanelAnimationVar( float, m_flLingerTime, "exit_vehicle_linger_time", "2.0" );

	bool				m_bPrevHealth;
	float				m_flLingerFinish;
	int					m_nLastHealth;

	CPanelAnimationStringVar( 128, m_szVehicleHealthLabel, "VehicleHealthLabel", "Vehicle Health" );
};

DECLARE_HUDELEMENT( CHudVehicleHealth );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudVehicleHealth::CHudVehicleHealth( const char *pElementName ) :
	CHudNumeric( pElementName, "HudVehicleHealth" )
{
	m_bPrevHealth = false;
	m_flLingerFinish = 0.0f;
	m_flLingerTime = 2.0f;
	m_nLastHealth = 0;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

bool CHudVehicleHealth::GetHealth( int& value )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		// Draw the vehicle health:
		C_BaseTFVehicle *pVehicleEnt = ( C_BaseTFVehicle* )pPlayer->GetVehicle();
		if( pVehicleEnt )
		{
			value = pVehicleEnt->GetHealth();
			m_nLastHealth = value;
			bool changed = m_bPrevHealth != true;

			if ( changed )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(  "HealthVehicleEnterVehicle" );
			}

			m_bPrevHealth = true;
			return true;
		}
	}
	
	bool changed = m_bPrevHealth != false;
	if ( changed )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "HealthVehicleExitVehicle" );
		m_flLingerFinish = gpGlobals->curtime + m_flLingerTime;
	}

	m_bPrevHealth = false;

	if ( gpGlobals->curtime < m_flLingerFinish )
	{
		value = m_nLastHealth;
		return true;
	}

	m_flLingerFinish = 0.0f;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudVehicleHealth::GetValue( char *val, int maxlen )
{
	int value = 0;
	bool show = GetHealth( value );
	if ( !show )
	{
		return false;
	}

	Q_snprintf( val, maxlen,  "%i", value );
	return true;
}

Color CHudVehicleHealth::GetColor()
{
	int value = 0;
	if ( !GetHealth( value ) || value > 75 )
		return m_TextColor;

	return m_TextColorCritical;
}

Color CHudVehicleHealth::GetBoxColor()
{
	int value = 0;
	if ( !GetHealth( value ) || value > 75 )
		return m_BoxColor;
	return m_BoxColorCritical;
}
