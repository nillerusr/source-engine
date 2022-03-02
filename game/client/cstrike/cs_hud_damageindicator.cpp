//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "text_message.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "view.h"
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "IEffects.h"
#include "hudelement.h"
// NVNT damage
#include "haptics/haptic_utils.h"
using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: HDU Damage indication
//-----------------------------------------------------------------------------
class CHudDamageIndicator : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDamageIndicator, vgui::Panel );

public:
	CHudDamageIndicator( const char *pElementName );

	void	Init( void );
	void	Reset( void );
	bool	ShouldDraw( void );

	// Handler for our message
	void	MsgFunc_Damage( bf_read &msg );

private:
	void	Paint();
	void	ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	void	CalcDamageDirection( const Vector &vecFrom );
	void	DrawDamageIndicatorFront( float flFade );
	void	DrawDamageIndicatorRear( float flFade );
	void	DrawDamageIndicatorLeft( float flFade );
	void	DrawDamageIndicatorRight( float flFade );

private:
	float	m_flAttackFront;
	float	m_flAttackRear;
	float	m_flAttackLeft;
	float	m_flAttackRight;

	Color	m_clrIndicator;

	CHudTexture	*icon_up;
	CHudTexture	*icon_down;
	CHudTexture	*icon_left;
	CHudTexture	*icon_right;

	float m_flFadeCompleteTime;	//don't draw past this time
};

DECLARE_HUDELEMENT( CHudDamageIndicator );
DECLARE_HUD_MESSAGE( CHudDamageIndicator, Damage );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudDamageIndicator::CHudDamageIndicator( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudDamageIndicator")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDamageIndicator::Reset( void )
{
	m_flAttackFront	= 0.0;
	m_flAttackRear	= 0.0;
	m_flAttackRight	= 0.0;
	m_flAttackLeft	= 0.0;

	m_flFadeCompleteTime = 0.0;

	m_clrIndicator.SetColor( 250, 0, 0, 255 );
}

void CHudDamageIndicator::Init( void )
{
	HOOK_HUD_MESSAGE( CHudDamageIndicator, Damage );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudDamageIndicator::ShouldDraw( void )
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	if ( ( m_flAttackFront <= 0.0 ) && ( m_flAttackRear <= 0.0 ) && ( m_flAttackLeft <= 0.0 ) && ( m_flAttackRight <= 0.0 ) )
		return false;

	return true;
}

void CHudDamageIndicator::DrawDamageIndicatorFront( float flFade )
{
	if ( m_flAttackFront > 0.4 )
	{
		if ( !icon_up )
		{
			icon_up = gHUD.GetIcon( "pain_up" );
		}

		if ( !icon_up )
		{
			return;
		}

		int	x = ( ScreenWidth() / 2 ) - icon_up->Width() / 2;
		int	y = ( ScreenHeight() / 2 ) - icon_up->Height() * 3;
		icon_up->DrawSelf( x, y, m_clrIndicator );

		m_flAttackFront = MAX( 0.0, m_flAttackFront - flFade );
	}
	else
	{
		m_flAttackFront = 0.0;
	}
}

void CHudDamageIndicator::DrawDamageIndicatorRear( float flFade )
{
	if ( m_flAttackRear > 0.4 )
	{
		if ( !icon_down )
		{
			icon_down = gHUD.GetIcon( "pain_down" );
		}

		if ( !icon_down )
		{
			return;
		}

		int	x = ( ScreenWidth() / 2 ) - icon_down->Width() / 2;
		int	y = ( ScreenHeight() / 2 ) + icon_down->Height() * 2;
		icon_down->DrawSelf( x, y, m_clrIndicator );

		m_flAttackRear = MAX( 0.0, m_flAttackRear - flFade );
	}
	else
	{
		m_flAttackRear = 0.0;
	}
}


void CHudDamageIndicator::DrawDamageIndicatorLeft( float flFade )
{
	if ( m_flAttackLeft > 0.4 )
	{
		if ( !icon_left )
		{
			icon_left = gHUD.GetIcon( "pain_left" );
		}

		if ( !icon_left )
		{
			return;
		}

		int	x = ( ScreenWidth() / 2 ) - icon_left->Width() * 3;
		int	y = ( ScreenHeight() / 2 ) - icon_left->Height() / 2;
		icon_left->DrawSelf( x, y, m_clrIndicator );

		m_flAttackLeft = MAX( 0.0, m_flAttackLeft - flFade );
	}
	else
	{
		m_flAttackLeft = 0.0;
	}
}


void CHudDamageIndicator::DrawDamageIndicatorRight( float flFade )
{
	if ( m_flAttackRight > 0.4 )
	{
		if ( !icon_right )
		{
			icon_right = gHUD.GetIcon( "pain_right" );
		}

		if ( !icon_right )
		{
			return;
		}

		int	x = ( ScreenWidth() / 2 ) + icon_right->Width() * 2;
		int	y = ( ScreenHeight() / 2 ) - icon_right->Height() / 2;
		icon_right->DrawSelf( x, y, m_clrIndicator );

		m_flAttackRight = MAX( 0.0, m_flAttackRight - flFade );
	}
	else
	{
		m_flAttackRight = 0.0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Paints the damage display
//-----------------------------------------------------------------------------
void CHudDamageIndicator::Paint()
{
	if( m_flFadeCompleteTime > gpGlobals->curtime )
	{
		float flFade = gpGlobals->frametime * 2;
		// draw damage indicators	
		DrawDamageIndicatorFront( flFade );
		DrawDamageIndicatorRear( flFade );
		DrawDamageIndicatorLeft( flFade );
		DrawDamageIndicatorRight( flFade );
	}	
}
// NVNT static to pass damage
static float hap_damage_amount = 0;

//-----------------------------------------------------------------------------
// Purpose: Message handler for Damage message
//-----------------------------------------------------------------------------
void CHudDamageIndicator::MsgFunc_Damage( bf_read &msg )
{
	int damageTaken	= msg.ReadByte();	

	Vector vecFrom;
	msg.ReadBitVec3Coord( vecFrom );

	// NVNT pass damage to static holder
	hap_damage_amount = damageTaken;

	if ( damageTaken > 0 )
	{
		m_flFadeCompleteTime = gpGlobals->curtime + 1.0;
		CalcDamageDirection( vecFrom );
	}
//=============================================================================
// HPE_BEGIN:
// [menglish] Added reads for the added location based parameters to this message
//=============================================================================	 
	msg.ReadLong();
	msg.ReadBitVec3Coord( vecFrom );	 
//=============================================================================
// HPE_END
//=============================================================================
}

void CHudDamageIndicator::CalcDamageDirection( const Vector &vecFrom )
{
	if ( vecFrom == vec3_origin )
	{
		m_flAttackFront	= 0.0;
		m_flAttackRear	= 0.0;
		m_flAttackRight	= 0.0;
		m_flAttackLeft	= 0.0;

		return;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	Vector vecDelta = ( vecFrom - pLocalPlayer->GetRenderOrigin() );

	if ( vecDelta.Length() <= 50 )
	{
		m_flAttackFront	= 1.0;
		m_flAttackRear	= 1.0;
		m_flAttackRight	= 1.0;
		m_flAttackLeft	= 1.0;

		return;
	}

	VectorNormalize( vecDelta );

	Vector forward;
	Vector right;
	Vector up;
	AngleVectors( MainViewAngles(), &forward, &right, &up );


	float flFront	= DotProduct( vecDelta, forward );
	float flSide	= DotProduct( vecDelta, right );
	float flUp      = DotProduct( vecDelta, up);

	if ( flFront > 0 )
	{
		if ( flFront > 0.3 )
			m_flAttackFront = MAX( m_flAttackFront, flFront );
	}
	else
	{
		float f = fabs( flFront );
		if ( f > 0.3 )
			m_flAttackRear = MAX( m_flAttackRear, f );
	}

	if ( flSide > 0 )
	{
		if ( flSide > 0.3 )
			m_flAttackRight = MAX( m_flAttackRight, flSide );
	}
	else
	{
		float f = fabs( flSide );
		if ( f > 0.3 )
			m_flAttackLeft = MAX( m_flAttackLeft, f );
	}

	// NVNT pass damage. (use hap_damage amount to apply)
	// do rotation
	Vector hapDir(-flSide,-flUp,flFront);
	if ( haptics )
		haptics->ApplyDamageEffect(hap_damage_amount, 0, hapDir);
}


//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudDamageIndicator::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	int wide, tall;
	GetHudSize(wide, tall);
	SetSize(wide, tall);
}
