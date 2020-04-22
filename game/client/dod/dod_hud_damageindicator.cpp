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
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"
#include "IEffects.h"
#include "hudelement.h"
#include "clienteffectprecachesystem.h"
#include "dod_shareddefs.h"
// NVNT damage
#include "haptics/haptic_utils.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: HUD Damage indication
//-----------------------------------------------------------------------------
class CHudDODDamageIndicator : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudDODDamageIndicator, vgui::Panel );

public:
	CHudDODDamageIndicator( const char *pElementName );
	void Init( void );
	void Reset( void );
	virtual bool ShouldDraw( void );

	// Handler for our message
	void MsgFunc_Damage( bf_read &msg );

private:
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CPanelAnimationVarAliasType( float, m_flDmgX, "dmg_xpos", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDmgY, "dmg_ypos", "80", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDmgWide, "dmg_wide", "30", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDmgTall1, "dmg_tall1", "300", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDmgTall2, "dmg_tall2", "240", "proportional_float" );

	CPanelAnimationVar( Color, m_DmgColorLeft, "DmgColorLeft", "255 0 0 0" );
	CPanelAnimationVar( Color, m_DmgColorRight, "DmgColorRight", "255 0 0 0" );

	CPanelAnimationVar( Color, m_DmgHighColorLeft, "DmgHighColorLeft", "255 0 0 0" );
	CPanelAnimationVar( Color, m_DmgHighColorRight, "DmgHighColorRight", "255 0 0 0" );

	CPanelAnimationVar( Color, m_DmgFullscreenColor, "DmgFullscreenColor", "255 0 0 0" );

	void DrawDamageIndicator(int side);
	void DrawFullscreenDamageIndicator();
	void GetDamagePosition( const Vector &vecDelta, float *flRotation );

	CMaterialReference m_WhiteAdditiveMaterial;
};

DECLARE_HUDELEMENT( CHudDODDamageIndicator );
DECLARE_HUD_MESSAGE( CHudDODDamageIndicator, Damage );

enum
{
	DAMAGE_ANY,
	DAMAGE_LOW,
	DAMAGE_HIGH,
};

#define ANGLE_ANY	0.0f
#define DMG_ANY		0

struct DamageAnimation_t
{
	const char *name;
	int bitsDamage;
	float angleMinimum;
	float angleMaximum;
	int damage; 
};

//-----------------------------------------------------------------------------
// Purpose: List of damage animations, finds first that matches criteria
//-----------------------------------------------------------------------------
static DamageAnimation_t g_DamageAnimations[] =
{
	{ "HudTakeDamageLeft",		DMG_ANY,	45.0f,		135.0f,		DAMAGE_ANY },
	{ "HudTakeDamageRight",		DMG_ANY,	225.0f,		315.0f,		DAMAGE_ANY },
	{ "HudTakeDamageBehind",	DMG_ANY,	135.0f,		225.0f,		DAMAGE_ANY },

	// fall through to front damage
	{ "HudTakeDamageFront",		DMG_ANY,	ANGLE_ANY,	ANGLE_ANY,	DAMAGE_ANY },
	{ NULL },
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudDODDamageIndicator::CHudDODDamageIndicator( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudDamageIndicator")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_WhiteAdditiveMaterial.Init( "vgui/white_additive", TEXTURE_GROUP_VGUI ); 

	SetHiddenBits( HIDEHUD_HEALTH );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::Reset( void )
{
	m_DmgColorLeft[3] = 0;
	m_DmgColorRight[3] = 0;
	m_DmgHighColorLeft[3] = 0;
	m_DmgHighColorRight[3] = 0;
	m_DmgFullscreenColor[3] = 0;
}

void CHudDODDamageIndicator::Init( void )
{
	HOOK_HUD_MESSAGE( CHudDODDamageIndicator, Damage );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudDODDamageIndicator::ShouldDraw( void )
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	if ( !m_DmgColorLeft[3] && !m_DmgColorRight[3] && !m_DmgHighColorLeft[3] && !m_DmgHighColorRight[3] 
	&& !m_DmgFullscreenColor[3] )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a damage quad
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::DrawDamageIndicator(int side)
{
	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_WhiteAdditiveMaterial );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	int insetY = (m_flDmgTall1 - m_flDmgTall2) / 2;

	int x1 = m_flDmgX;
	int x2 = m_flDmgX + m_flDmgWide;
	int y[4] = { (int)(m_flDmgY), (int)(m_flDmgY + insetY), (int)(m_flDmgY + m_flDmgTall1 - insetY), (int)(m_flDmgY + m_flDmgTall1) };
	int alpha[4] = { 0, 1, 1, 0 };

	// see if we're high damage
	bool bHighDamage = false;
	if ( m_DmgHighColorRight[3] > m_DmgColorRight[3] || m_DmgHighColorLeft[3] > m_DmgColorLeft[3] )
	{
		// make more of the screen be covered by damage
		x1 = GetWide() * 0.0f;
		x2 = GetWide() * 0.5f;
		y[0] = 0.0f;
		y[1] = 0.0f;
		y[2] = GetTall();
		y[3] = GetTall();
		alpha[0] = 1.0f;
		alpha[1] = 0.0f;
		alpha[2] = 0.0f;
		alpha[3] = 1.0f;
		bHighDamage = true;
	}

	int r, g, b, a;

	switch ( side )
	{
	case 0:	// left
		{
			if ( bHighDamage )
			{
				r = m_DmgHighColorLeft[0], g = m_DmgHighColorLeft[1], b = m_DmgHighColorLeft[2], a = m_DmgHighColorLeft[3];
			}
			else
			{
				r = m_DmgColorLeft[0], g = m_DmgColorLeft[1], b = m_DmgColorLeft[2], a = m_DmgColorLeft[3];
			}

			meshBuilder.Color4ub( r, g, b, a * alpha[0] );
			meshBuilder.TexCoord2f( 0,0,0 );
			meshBuilder.Position3f( x1, y[0], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[1] );
			meshBuilder.TexCoord2f( 0,1,0 );
			meshBuilder.Position3f( x2, y[1], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[2] );
			meshBuilder.TexCoord2f( 0,1,1 );
			meshBuilder.Position3f( x2, y[2], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[3] );
			meshBuilder.TexCoord2f( 0,0,1 );
			meshBuilder.Position3f( x1, y[3], 0 );
			meshBuilder.AdvanceVertex();
		}
		break;
	case 1:	// right
		{
			if ( bHighDamage )
			{
				r = m_DmgHighColorRight[0], g = m_DmgHighColorRight[1], b = m_DmgHighColorRight[2], a = m_DmgHighColorRight[3];
			}
			else
			{
				r = m_DmgColorRight[0], g = m_DmgColorRight[1], b = m_DmgColorRight[2], a = m_DmgColorRight[3];
			}

			// realign x coords
			x1 = GetWide() - x1;
			x2 = GetWide() - x2;

			meshBuilder.Color4ub( r, g, b, a * alpha[0]);
			meshBuilder.TexCoord2f( 0,0,0 );
			meshBuilder.Position3f( x1, y[0], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[3] );
			meshBuilder.TexCoord2f( 0,0,1 );
			meshBuilder.Position3f( x1, y[3], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[2] );
			meshBuilder.TexCoord2f( 0,1,1 );
			meshBuilder.Position3f( x2, y[2], 0 );
			meshBuilder.AdvanceVertex();

			meshBuilder.Color4ub( r, g, b, a * alpha[1] );
			meshBuilder.TexCoord2f( 0,1,0 );
			meshBuilder.Position3f( x2, y[1], 0 );
			meshBuilder.AdvanceVertex();
		}
		break;
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Draws full screen damage fade
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::DrawFullscreenDamageIndicator()
{
	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, m_WhiteAdditiveMaterial );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );
	int r = m_DmgFullscreenColor[0], g = m_DmgFullscreenColor[1], b = m_DmgFullscreenColor[2], a = m_DmgFullscreenColor[3];

	float wide = GetWide(), tall = GetTall();

	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.TexCoord2f( 0,0,0 );
	meshBuilder.Position3f( 0.0f, 0.0f, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.TexCoord2f( 0,1,0 );
	meshBuilder.Position3f( wide, 0.0f, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.TexCoord2f( 0,1,1 );
	meshBuilder.Position3f( wide, tall, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.TexCoord2f( 0,0,1 );
	meshBuilder.Position3f( 0.0f, tall, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Purpose: Paints the damage display
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::Paint()
{
	// draw fullscreen damage indicators
	DrawFullscreenDamageIndicator();

	// draw side damage indicators
	DrawDamageIndicator(0);
	DrawDamageIndicator(1);
}

// NVNT - hacky way of passing damage
static long lastDamage_s = 0;

//-----------------------------------------------------------------------------
// Purpose: Message handler for Damage message
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::MsgFunc_Damage( bf_read &msg )
{
	int damageTaken = msg.ReadByte();	// health
	long bitsDamage = msg.ReadLong(); // damage bits

	Vector vecFrom;
	msg.ReadBitVec3Coord( vecFrom );

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	Vector vecDelta = (vecFrom - MainViewOrigin());
	VectorNormalize( vecDelta );

	if ( damageTaken > 0 )
	{
		// see which quandrant the effect is in
		float angle;
		GetDamagePosition( vecDelta, &angle );

		// see which effect to play
		DamageAnimation_t *dmgAnim = g_DamageAnimations;
		for ( ; dmgAnim->name != NULL; ++dmgAnim )
		{
			if ( dmgAnim->bitsDamage && !(bitsDamage & dmgAnim->bitsDamage) )
				continue;

			if ( dmgAnim->angleMinimum && angle < dmgAnim->angleMinimum )
				continue;

			if ( dmgAnim->angleMaximum && angle > dmgAnim->angleMaximum )
				continue;

			// we have a match, break
			break;
		}

		if ( dmgAnim->name )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( dmgAnim->name );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Convert a damage position in world units to the screen's units
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::GetDamagePosition( const Vector &vecDelta, float *flRotation )
{
	float flRadius = 360.0f;

	// Player Data
	Vector playerPosition = MainViewOrigin();
	QAngle playerAngles = MainViewAngles();

	Vector forward, right, up(0,0,1);
	AngleVectors (playerAngles, &forward, NULL, NULL );
	forward.z = 0;
	VectorNormalize(forward);
	CrossProduct( up, forward, right );
	float front = DotProduct(vecDelta, forward);
	float side = DotProduct(vecDelta, right);
	float top = DotProduct(vecDelta, up);
	// NVNT pass damage. (use hap_damage amount to apply)
	// do rotation
	Vector hapDir(side,-top,front);
	if ( haptics )
		haptics->ApplyDamageEffect(((float)lastDamage_s), 0, hapDir);

	float xpos = flRadius * -side;
	float ypos = flRadius * -front;

	// Get the rotation (yaw)
	*flRotation = atan2(xpos, ypos) + M_PI;
	*flRotation *= 180 / M_PI;

	float yawRadians = -(*flRotation) * M_PI / 180.0f;
	float ca = cos( yawRadians );
	float sa = sin( yawRadians );

	// Rotate it around the circle
	xpos = (int)((ScreenWidth() / 2) + (flRadius * sa));
	ypos = (int)((ScreenHeight() / 2) - (flRadius * ca));
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudDODDamageIndicator::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	int wide, tall;
	GetHudSize(wide, tall);
	SetSize(wide, tall);
}
