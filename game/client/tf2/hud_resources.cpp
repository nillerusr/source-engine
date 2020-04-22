//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Hud display of the local player's resource counts
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud_numeric.h"
#include "c_basetfplayer.h"
#include "hud_macros.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudResources : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudResources, CHudNumeric )
public:
	CHudResources( const char *pElementName );

	virtual const char *GetLabelText() { return m_szResourcesLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return increment ? "ResourceIncrement" : "ResourceDecrement"; }
	virtual bool		GetValue( char *val, int maxlen );

private:
	bool				GetResourceCount( int& value );

	CPanelAnimationStringVar( 128, m_szResourcesLabel, "ResourcesLabel", "Resources" );
};

DECLARE_HUDELEMENT( CHudResources );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudResources::CHudResources( const char *pElementName ) : CHudNumeric( pElementName, "HudResources")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

bool CHudResources::GetValue( char *val, int maxlen )
{
	int bank = 0;
	if ( !GetResourceCount( bank ) )
		return false;

	Q_snprintf( val, maxlen, "%i", bank );
	return true;
}

bool CHudResources::GetResourceCount( int& value )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;
	if ( pPlayer->GetTeamNumber() == 0)
		return false;

	value = pPlayer->m_TFLocal.m_iBankResources;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudResourcesPickup : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudResourcesPickup, CHudNumeric )
public:
	CHudResourcesPickup( const char *pElementName );

	virtual void		Init( void );
	virtual const char *GetLabelText() { return m_szResourcesPickupLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return "ResourcePickup"; }
	virtual bool		GetValue( char *val, int maxlen );

	// Handler for our message
	void MsgFunc_PickupRes( bf_read &msg );

private:
	int		m_iPickupAmount;

	CPanelAnimationStringVar( 128, m_szResourcesPickupLabel, "ResourcesPickupLabel", "" );
};

DECLARE_HUDELEMENT( CHudResourcesPickup );
DECLARE_HUD_MESSAGE( CHudResourcesPickup, PickupRes );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudResourcesPickup::CHudResourcesPickup( const char *pElementName ) : CHudNumeric( pElementName, "HudResourcesPickup")
{
	m_iPickupAmount = 0;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudResourcesPickup::Init( void )
{
	HOOK_HUD_MESSAGE( CHudResourcesPickup, PickupRes );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudResourcesPickup::GetValue( char *val, int maxlen )
{
	if ( !m_iPickupAmount )
		return false;

	Q_snprintf( val, maxlen, "+%i", m_iPickupAmount );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for PickupRes message
//-----------------------------------------------------------------------------
void CHudResourcesPickup::MsgFunc_PickupRes( bf_read &msg )
{
	m_iPickupAmount = msg.ReadByte();

	ForcePulse();
}