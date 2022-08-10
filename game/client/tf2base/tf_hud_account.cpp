//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ProgressBar.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// Floating delta text items, float off the top of the frame to 
// show changes to the metal account value
typedef struct 
{
	// amount of delta
	int m_iAmount;

	// die time
	float m_flDieTime;

} account_delta_t;

#define NUM_ACCOUNT_DELTA_ITEMS 10

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudAccountPanel : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudAccountPanel, EditablePanel );

public:
	CHudAccountPanel( const char *pElementName );

	virtual void	ApplySchemeSettings( IScheme *scheme );
	virtual void	LevelInit( void );
	virtual bool	ShouldDraw( void );
	virtual void	Paint( void );

	virtual void	FireGameEvent( IGameEvent *event );
	void OnAccountValueChanged( int iOldValue, int iNewValue );

private:

	int iAccountDeltaHead;
	account_delta_t m_AccountDeltaItems[NUM_ACCOUNT_DELTA_ITEMS];

	CPanelAnimationVarAliasType( float, m_flDeltaItemStartPos, "delta_item_start_y", "100", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDeltaItemEndPos, "delta_item_end_y", "0", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flDeltaItemX, "delta_item_x", "0", "proportional_float" );

	CPanelAnimationVar( Color, m_DeltaPositiveColor, "PositiveColor", "0 255 0 255" );
	CPanelAnimationVar( Color, m_DeltaNegativeColor, "NegativeColor", "255 0 0 255" );

	CPanelAnimationVar( float, m_flDeltaLifetime, "delta_lifetime", "2.0" );

	CPanelAnimationVar( vgui::HFont, m_hDeltaItemFont, "delta_item_font", "Default" );
};

DECLARE_HUDELEMENT( CHudAccountPanel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudAccountPanel::CHudAccountPanel( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudAccount" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	iAccountDeltaHead = 0;

	SetDialogVariable( "metal", 0 );

	for( int i=0; i<NUM_ACCOUNT_DELTA_ITEMS; i++ )
	{
		m_AccountDeltaItems[i].m_flDieTime = 0.0f;
	}

	ListenForGameEvent( "player_account_changed" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAccountPanel::FireGameEvent( IGameEvent *event )
{
	const char * type = event->GetName();

	if ( Q_strcmp(type, "player_account_changed") == 0 )
	{
		int iOldValue = event->GetInt( "old_account" );
		int iNewValue = event->GetInt( "new_account" );
		OnAccountValueChanged( iOldValue, iNewValue );
	}
	else
	{
		CHudElement::FireGameEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAccountPanel::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudAccountPanel.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: called whenever a new level's starting
//-----------------------------------------------------------------------------
void CHudAccountPanel::LevelInit( void )
{
	iAccountDeltaHead = 0;

	CHudElement::LevelInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudAccountPanel::ShouldDraw( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pPlayer->IsAlive() || !pPlayer->IsPlayerClass( TF_CLASS_ENGINEER ) )
	{
		return false;
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudAccountPanel::OnAccountValueChanged( int iOldValue, int iNewValue )
{
	// update the account value
	SetDialogVariable( "metal", iNewValue ); 

	int iDelta = iNewValue - iOldValue;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( iDelta != 0 && pPlayer && pPlayer->IsAlive() )
	{
		// create a delta item that floats off the top
		account_delta_t *pNewDeltaItem = &m_AccountDeltaItems[iAccountDeltaHead];

		iAccountDeltaHead++;
		iAccountDeltaHead %= NUM_ACCOUNT_DELTA_ITEMS;

		pNewDeltaItem->m_flDieTime = gpGlobals->curtime + m_flDeltaLifetime;
		pNewDeltaItem->m_iAmount = iDelta;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Paint the deltas
//-----------------------------------------------------------------------------
void CHudAccountPanel::Paint( void )
{
	BaseClass::Paint();

	for ( int i=0; i<NUM_ACCOUNT_DELTA_ITEMS; i++ )
	{
		// update all the valid delta items
		if ( m_AccountDeltaItems[i].m_flDieTime > gpGlobals->curtime )
		{
			// position and alpha are determined from the lifetime
			// color is determined by the delta - green for positive, red for negative

			Color c = ( m_AccountDeltaItems[i].m_iAmount > 0 ) ? m_DeltaPositiveColor : m_DeltaNegativeColor;

			float flLifetimePercent = ( m_AccountDeltaItems[i].m_flDieTime - gpGlobals->curtime ) / m_flDeltaLifetime;

			// fade out after half our lifetime
			if ( flLifetimePercent < 0.5 )
			{
				c[3] = (int)( 255.0f * ( flLifetimePercent / 0.5 ) );
			}

			float flHeight = ( m_flDeltaItemStartPos - m_flDeltaItemEndPos );
			float flYPos = m_flDeltaItemEndPos + flLifetimePercent * flHeight;

			vgui::surface()->DrawSetTextFont( m_hDeltaItemFont );
			vgui::surface()->DrawSetTextColor( c );
			vgui::surface()->DrawSetTextPos( m_flDeltaItemX, (int)flYPos );

			wchar_t wBuf[20];

			if ( m_AccountDeltaItems[i].m_iAmount > 0 )
			{
				_snwprintf( wBuf, sizeof(wBuf)/sizeof(wchar_t), L"+%d", m_AccountDeltaItems[i].m_iAmount );
			}
			else
			{
				_snwprintf( wBuf, sizeof(wBuf)/sizeof(wchar_t), L"%d", m_AccountDeltaItems[i].m_iAmount );
			}

			vgui::surface()->DrawPrintText( wBuf, wcslen(wBuf), FONT_DRAW_NONADDITIVE );
		}
	}
}