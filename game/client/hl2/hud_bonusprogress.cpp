//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// BonusProgress.cpp
//
// implementation of CHudBonusProgress class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "hudelement.h"
#include "hud_numericdisplay.h"

#include "convar.h"

#ifdef MAPBASE
#include "point_bonusmaps_accessor.h"
#endif

#ifdef EZ
#include "hl2_shareddefs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_BONUS_PROGRESS -1

#ifdef MAPBASE
ConVar hud_bonus_progress_enhanced( "hud_bonus_progress_enhanced", "1", FCVAR_NONE, "Enables some enhancements for the bonus progress HUD element." );
#endif

//-----------------------------------------------------------------------------
// Purpose: BonusProgress panel
//-----------------------------------------------------------------------------
class CHudBonusProgress : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudBonusProgress, CHudNumericDisplay );

public:
	CHudBonusProgress( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();

#ifdef EZ
	virtual void Paint();
#endif

protected:

#ifdef MAPBASE
	bool UsesUniqueSecondaryColor() const { return true; }
#endif

private:
	void SetChallengeLabel( void );

private:
	// old variables
	int		m_iBonusProgress;

	int		m_iLastChallenge;

#ifdef MAPBASE
	int		m_iBronze, m_iSilver, m_iGold;
	int		m_iCurrentGoal;
	bool	m_bReverse;

	enum
	{
		GOAL_NONE,
		GOAL_BRONZE,
		GOAL_SILVER,
		GOAL_GOLD,
	};

	CPanelAnimationVarAliasType( float, goal_xpos, "goal_xpos", "64", "proportional_float" );
	CPanelAnimationVarAliasType( float, goal_ypos, "goal_ypos", "16", "proportional_float" );

	//CPanelAnimationVar( Color, m_BronzeColor, "BronzeColor", "164 108 64 224" );
	//CPanelAnimationVar( Color, m_SilverColor, "SilverColor", "205 235 255 224" );
	//CPanelAnimationVar( Color, m_GoldColor, "GoldColor", "255 205 100 224" );
#endif
};	

DECLARE_HUDELEMENT( CHudBonusProgress );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBonusProgress::CHudBonusProgress( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay(NULL, "HudBonusProgress")
{
#ifdef MAPBASE
	SetHiddenBits( HIDEHUD_BONUS_PROGRESS | HIDEHUD_PLAYERDEAD );
#else
	SetHiddenBits( HIDEHUD_BONUS_PROGRESS );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBonusProgress::Init()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBonusProgress::Reset()
{
	m_iBonusProgress = INIT_BONUS_PROGRESS;

	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( local )
		m_iLastChallenge = local->GetBonusChallenge();

	SetChallengeLabel();

#ifdef MAPBASE
	if (local)
	{
		BonusMapChallengeObjectives( m_iBronze, m_iSilver, m_iGold );
		m_bReverse = m_iBronze <= 0;
		if (m_bReverse)
		{
			m_iBronze = -m_iBronze; m_iSilver = -m_iSilver; m_iGold = -m_iGold;
		}
		m_iCurrentGoal = GOAL_NONE;
	}
#endif

	SetDisplayValue(m_iBonusProgress);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBonusProgress::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBonusProgress::OnThink()
{
	C_GameRules *pGameRules = GameRules();

	if ( !pGameRules )
	{
		// Not ready to init!
		return;
	}

	int newBonusProgress = 0;
	int iBonusChallenge = 0;

	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( !local )
	{
		// Not ready to init!
		return;
	}

	// Never below zero
#ifdef MAPBASE
	newBonusProgress = m_bReverse ? -local->GetBonusProgress() : MAX( local->GetBonusProgress(), 0 );
#else
	newBonusProgress = MAX( local->GetBonusProgress(), 0 );
#endif
	iBonusChallenge = local->GetBonusChallenge();

	// Only update the fade if we've changed bonusProgress
#ifdef MAPBASE
	if ( newBonusProgress == m_iBonusProgress && m_iLastChallenge == iBonusChallenge && gpGlobals->curtime > 6.5f )
#else
	if ( newBonusProgress == m_iBonusProgress && m_iLastChallenge == iBonusChallenge )
#endif
	{
		return;
	}

	m_iBonusProgress = newBonusProgress;

#ifdef MAPBASE
	if ( m_iLastChallenge != iBonusChallenge )
	{
		m_iLastChallenge = iBonusChallenge;
		SetChallengeLabel();
		BonusMapChallengeObjectives( m_iBronze, m_iSilver, m_iGold );
		m_bReverse = m_iBronze <= 0;
		if (m_bReverse)
		{
			m_iBronze = -m_iBronze; m_iSilver = -m_iSilver; m_iGold = -m_iGold;
		}
		m_iCurrentGoal = GOAL_NONE;
	}
#else
	if ( m_iLastChallenge != iBonusChallenge )
	{
		m_iLastChallenge = iBonusChallenge;
		SetChallengeLabel();
	}
#endif

#ifdef MAPBASE
	if (hud_bonus_progress_enhanced.GetBool())
	{
		// If the bonus progress is greater than bronze, turn red to indicate this isn't getting a medal
		if (m_iBonusProgress >= m_iBronze && !m_bReverse)
		{
			if (m_iCurrentGoal != GOAL_NONE)
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressSpoil" );
				m_iCurrentGoal = GOAL_NONE;
			}
			SetShouldDisplaySecondaryValue( false );
		}
		else
		{
			// Use the goal as the secondary value
			SetShouldDisplaySecondaryValue( true );

			int iShowGoalProgress = m_iBonusProgress;
			int iLastGoal, iNextGoal;

			if (gpGlobals->curtime <= 6.0f)
			{
				// Cycle through each goal at the start of a level
				if (gpGlobals->curtime <= 3.0f)
					iShowGoalProgress = 0; // Gold
				else if (gpGlobals->curtime <= 4.0f)
					iShowGoalProgress = m_iGold; // Silver
				else if (gpGlobals->curtime <= 5.0f)
					iShowGoalProgress = m_iBronze; // Bronze
			}

			if (m_bReverse ? iShowGoalProgress < m_iBronze : iShowGoalProgress >= m_iSilver)
			{
				if (m_iCurrentGoal != GOAL_BRONZE)
				{
					SetSecondaryValue( m_iBronze );
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressNewGoalBronze" ); //m_Ammo2Color = m_BronzeColor;
					m_iCurrentGoal = GOAL_BRONZE;
				}
				iLastGoal = m_iSilver; iNextGoal = m_iBronze;
			}
			else if (m_bReverse ? iShowGoalProgress < m_iSilver : iShowGoalProgress >= m_iGold)
			{
				if (m_iCurrentGoal != GOAL_SILVER)
				{
					SetSecondaryValue( m_iSilver );
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressNewGoalSilver" ); //m_Ammo2Color = m_SilverColor;
					m_iCurrentGoal = GOAL_SILVER;
				}
				iLastGoal = m_iGold; iNextGoal = m_iSilver;
			}
			else
			{
				if (m_iCurrentGoal != GOAL_GOLD)
				{
					SetSecondaryValue( m_iGold );
					g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressNewGoalGold" ); //m_Ammo2Color = m_GoldColor;
					m_iCurrentGoal = GOAL_GOLD;
				}
				iLastGoal = 0; iNextGoal = m_iGold;
			}

			// If 75% of the way to the next goal, flash red
			if (!m_bReverse && ((float)(iLastGoal - m_iBonusProgress) / (float)(iLastGoal - iNextGoal)) >= 0.75f)
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressFlashRed" );
			else if (m_iBonusProgress == iLastGoal)
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressFlashNewGoal" );
			else
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressFlash" );
		}
	}
	else
#endif
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("BonusProgressFlash");

	if ( pGameRules->IsBonusChallengeTimeBased() )
	{
		SetIsTime( true );
		SetIndent( false );
	}
	else
	{
		SetIsTime( false );
		SetIndent( true );
	}

	SetDisplayValue(m_iBonusProgress);
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: renders the vgui panel
//-----------------------------------------------------------------------------
void CHudBonusProgress::Paint()
{
	// For special challenges, count the maximum value as a sign this challenge doesn't use a counter
	if (m_iLastChallenge >= EZ_CHALLENGE_SPECIAL && m_iBonusProgress == 65536)
	{
		// Only draw the label
		PaintLabel();
	}
	else
	{
		BaseClass::Paint();
	}
}
#endif

void CHudBonusProgress::SetChallengeLabel( void )
{
	// Blank for no challenge
	if ( m_iLastChallenge == 0 )
	{
		SetLabelText(L"");
		return;
	}

#ifdef EZ
	if (m_iLastChallenge == EZ_CHALLENGE_MASS)
	{
		// Give the number enough room
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressExpandToMassChallenge" );
	}
	else
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BonusProgressExpandToNormal" );
	}
#endif

	char szBonusTextName[] = "#Valve_Hud_BONUS_PROGRESS00";

	int iStringLength = Q_strlen( szBonusTextName );

	szBonusTextName[ iStringLength - 2 ] = ( m_iLastChallenge / 10 ) + '0';
	szBonusTextName[ iStringLength - 1 ] = ( m_iLastChallenge % 10 ) + '0';

	wchar_t *tempString = g_pVGuiLocalize->Find(szBonusTextName);

	if (tempString)
	{
		SetLabelText(tempString);
		return;
	}

	// Couldn't find a special string for this challenge
	tempString = g_pVGuiLocalize->Find("#Valve_Hud_BONUS_PROGRESS");
	if (tempString)
	{
		SetLabelText(tempString);
		return;
	}

	// Couldn't find any localizable string
	SetLabelText(L"BONUS");
}
