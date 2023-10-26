#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#define PAIN_NAME "sprites/%d_pain.vmt"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "asw_hudelement.h"
#include "hud_numericdisplay.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "asw_vgui_ammo_list.h"
#include "asw_vgui_marine_ammo_report.h"
#include "c_asw_weapon.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "tier0/vprof.h"
#include "ConVar.h"
#include "asw_weapon_parse.h"
#include "asw_vgui_fast_reload.h"
#include "asw_hud_objective.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_draw_hud("asw_draw_hud", "1", 0, "Draw the HUD or not");
ConVar asw_hud_alpha("asw_hud_alpha", "192", 0, "Alpha of the black parts of the HUD (0->255)");
extern ConVar asw_hud_scale;

// Panel shows bullets, clips and grenades for the current weapon
class CASWHudAmmo : public vgui::Panel, public CASW_HudElement
{
	DECLARE_CLASS_SIMPLE( CASWHudAmmo, vgui::Panel );

public:
	CASWHudAmmo( const char *pElementName );
	virtual ~CASWHudAmmo();
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	virtual void HideAmmoDisplay();
	virtual void Paint();
	virtual void PerformLayout();
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CASW_HudElement::ShouldDraw(); }
	virtual void ApplySchemeSettings(IScheme *pScheme);

	void SetAmmo(int ammo);
	void SetClips(int ammo2);
	void SetGrenades(int ammo3);	

	// digits
	int PaintRedNumbers(int xpos, int ypos, int value, int digits);
	void LoadRedNumbers();

	// ammo warnings
	void PaintAmmoWarnings();
	void DrawAmmoWarningMessage(int row, char* buffer, int r, int g, int b);

	CASW_VGUI_Ammo_List *m_pAmmoList;
	CASW_VGUI_Marine_Ammo_Report *m_pReportList;

	CPanelAnimationVar( vgui::HFont, m_hAmmoWarningFont, "AmmoWarningFont", "DefaultSmall" );
	CPanelAnimationVarAliasType( int, m_nBlackBarTexture, "BlackBarTexture", "vgui/swarm/HUD/ASWHUDBlackBar", "textureid" );
	CPanelAnimationVarAliasType( int, m_nLowerAmmoBarTexture, "LowerAmmoBarTexture", "vgui/swarm/HUD/ASWHUDLowerAmmoBar", "textureid" );
	
	float digit_xpos, digit_ypos, digit2_xpos, digit2_ypos, digit3_xpos, digit3_ypos;
	int m_iAmmo, m_iClips, m_iGrenades, m_iOverflow;
	int m_iDisplayPrimaryValue;
	int m_iDisplayTertiaryValue;
	int m_iDisplaySecondaryValue;
	int m_iRedNumberTextureID[10];

	bool m_bFadingAmmoListIn, m_bFadingAmmoListOut;

	vgui::IImage* m_pBulletsImage;
	vgui::IImage* m_pClipsImage;
	vgui::IImage* m_pGrenadesImage;
	vgui::IImage* m_pHealImage;
	vgui::IImage* m_pSelfHealImage;
	vgui::Label *m_pWeaponLabel;
	vgui::Label *m_pWeaponGlowLabel;
	vgui::Panel *m_pAltAmmoPos;
	CHandle<C_ASW_Weapon> m_hLastWeapon;
	CASW_VGUI_Fast_Reload* m_pFastReload;
	bool m_bActiveAmmobag;
};	

CASWHudAmmo::~CASWHudAmmo()
{

}

// Disabled - merged into ASW_Hud_Master
//DECLARE_HUDELEMENT( CASWHudAmmo );

CASWHudAmmo::CASWHudAmmo( const char *pElementName ) : BaseClass( GetClientMode()->GetViewport(), "ASWHudAmmo" ), CASW_HudElement( pElementName )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_REMOTE_TURRET);	
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);	
	m_iDisplayPrimaryValue = 1;
	m_iDisplaySecondaryValue = 1;
	m_iDisplayTertiaryValue = 1;
	m_iAmmo = m_iClips = m_iGrenades = 0;
	m_pAmmoList = NULL;
	m_pReportList = NULL;
	m_hLastWeapon = NULL;
	m_bFadingAmmoListIn = false;
	m_bFadingAmmoListOut = false;
	LoadRedNumbers();

	m_pBulletsImage = NULL;
	m_pClipsImage = NULL;
	m_pGrenadesImage = NULL;
	m_pHealImage = NULL;
	m_pSelfHealImage = NULL;

	m_pWeaponGlowLabel = new vgui::Label(this, "WeaponGlowLabel", "");
	m_pWeaponLabel = new vgui::Label(this, "WeaponLabel", "");
	m_pFastReload = new CASW_VGUI_Fast_Reload(this, "FastReload");

	m_pAltAmmoPos = new vgui::Panel( this, "AltAmmoPos" );

	if (!m_pAmmoList)
	{
		m_pAmmoList = new CASW_VGUI_Ammo_List( GetParent(),
			"AmmoListWindow");
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
		m_pAmmoList->SetScheme(scheme);
		m_pAmmoList->SetAlpha(0);
	}
}

void CASWHudAmmo::Init()
{
	Reset();
}

void CASWHudAmmo::Reset()
{
	m_iAmmo = m_iClips = m_iGrenades = 0;
	
	m_iDisplayPrimaryValue = 1;
	m_iDisplaySecondaryValue = 1;
	m_iDisplayTertiaryValue = 1;
	m_bActiveAmmobag = false;

	if (m_pReportList)
		m_pReportList->SetAlpha(0);
}

void CASWHudAmmo::VidInit()
{
	Reset();
}

void CASWHudAmmo::OnThink()
{
	VPROF_BUDGET( "CASWHudAmmo::OnThink", VPROF_BUDGETGROUP_ASW_CLIENT );	

	if (m_pAmmoList)
	{
		//GetAnimationController()->RunAnimationCommand(m_pWeaponLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
			//GetAnimationController()->RunAnimationCommand(m_pWeaponGlowLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
		m_pWeaponLabel->SetAlpha(255 - m_pAmmoList->GetAlpha());
		m_pWeaponGlowLabel->SetAlpha(255 - m_pAmmoList->GetAlpha());
	}
	else
	{
		m_pWeaponLabel->SetAlpha(255);
		m_pWeaponGlowLabel->SetAlpha(255);
	}
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if ( local )
	{
		C_ASW_Marine *marine = local->GetMarine();
		if (marine)
		{			
			C_ASW_Weapon *wpn = marine->GetActiveASWWeapon();
			m_bActiveAmmobag = false;
			if (wpn)
			{
				SetAmmo(wpn->Clip1());	// ammo inside the gun
				int bullets = marine->GetAmmoCount(wpn->GetPrimaryAmmoType());   // ammo the marine is carrying outside the gun
				int clips = bullets / wpn->GetMaxClip1();		// divide it down to get the number of clips
				// check ammo bag
				CASW_Weapon_Ammo_Bag* pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(marine->GetASWWeapon(0));
				if (!pAmmoBag)
					pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(marine->GetASWWeapon(1));
				if (pAmmoBag && wpn != pAmmoBag)
				{
					clips += pAmmoBag->NumClipsForWeapon(wpn);					
				}

				pAmmoBag = dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(wpn);

				m_bActiveAmmobag = pAmmoBag != NULL;
				if (wpn->DisplayClipsDoubled())
					SetClips(clips*2);
				else
					SetClips(clips);
				SetGrenades(wpn->Clip2());
				// changed weapon?
				if (wpn != m_hLastWeapon.Get() && wpn->m_bWeaponCreated)
				{
					m_hLastWeapon = wpn;
					
					const CASW_WeaponInfo* pInfo = wpn->GetWeaponInfo();
					if (pInfo)
					{
						m_iDisplayPrimaryValue = pInfo->m_iShowBulletsOnHUD;
						m_iDisplaySecondaryValue =  pInfo->m_iShowClipsOnHUD;
						m_iDisplayTertiaryValue = pInfo->m_iShowGrenadesOnHUD;

						if (!pAmmoBag)
						{
							m_pWeaponLabel->SetText(pInfo->szPrintName);
							m_pWeaponGlowLabel->SetText(pInfo->szPrintName);
						}
					}
				}
				m_iOverflow = bullets - (clips * wpn->GetMaxClip1());
				if (pAmmoBag)
				{					
					if (!m_pAmmoList)
					{
						m_pAmmoList = new CASW_VGUI_Ammo_List( GetParent(),
							"AmmoListWindow");
						vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
						m_pAmmoList->SetScheme(scheme);
						m_pAmmoList->SetAlpha(0);
					}
					if (m_pAmmoList)
					{
						if (m_pAmmoList->GetAlpha() < 255)
						{
							if (!m_bFadingAmmoListIn)
							{
								m_bFadingAmmoListIn = true;
								m_bFadingAmmoListOut = false;
								GetAnimationController()->RunAnimationCommand(m_pAmmoList, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
							}
							//GetAnimationController()->RunAnimationCommand(m_pWeaponLabel, "Alpha", 0, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
							//GetAnimationController()->RunAnimationCommand(m_pWeaponGlowLabel, "Alpha", 0, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);							
						}
						else
						{
							m_bFadingAmmoListIn = false;
						}
					}

					if (local->GetHighlightEntity())
					{
						if (!m_pReportList)
						{						
							m_pReportList = new CASW_VGUI_Marine_Ammo_Report( GetParent(),
								"MarineAmmoReportWindow");
							vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
							m_pReportList->SetScheme(scheme);
							m_pReportList->SetAlpha(0);						
						}
					}
				}
				else 
				{
					if (m_pAmmoList)
					{
						if (m_pAmmoList->GetAlpha() >= 255)
						{
							if (!m_bFadingAmmoListOut)
							{
								m_bFadingAmmoListOut = true;
								m_bFadingAmmoListIn = false;
								GetAnimationController()->RunAnimationCommand(m_pAmmoList, "Alpha", 0.0f, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
							}
							//GetAnimationController()->RunAnimationCommand(m_pWeaponLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
							//GetAnimationController()->RunAnimationCommand(m_pWeaponGlowLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
						}
						else
						{
							m_bFadingAmmoListOut = false;
						}
					}
				}
			}
			else
			{
				HideAmmoDisplay();
			}
		}
		else
		{
			HideAmmoDisplay();
		}
	}
	else
	{
		HideAmmoDisplay();
	}
}

void CASWHudAmmo::HideAmmoDisplay()
{
	SetAmmo(0);
	SetClips(0);
	SetGrenades(0);

	m_iDisplayPrimaryValue = 0;
	m_iDisplaySecondaryValue =  0;
	m_iDisplayTertiaryValue = 0;

	if (m_pAmmoList)
	{
		if (m_pAmmoList->GetAlpha() >= 255)
		{
			if (!m_bFadingAmmoListOut)
			{
				m_bFadingAmmoListOut = true;
				m_bFadingAmmoListIn = false;
				GetAnimationController()->RunAnimationCommand(m_pAmmoList, "Alpha", 0.0f, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
			}
			//GetAnimationController()->RunAnimationCommand(m_pWeaponLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);
			//GetAnimationController()->RunAnimationCommand(m_pWeaponGlowLabel, "Alpha", 255, 0, 0.2f, AnimationController::INTERPOLATOR_LINEAR);					
		}
		else
		{
			m_bFadingAmmoListOut = false;
		}
	}

	if (m_hLastWeapon.Get())
	{
		m_hLastWeapon = NULL;
		m_pWeaponLabel->SetText("#asw_weapon_empty");
		m_pWeaponGlowLabel->SetText("#asw_weapon_empty");
	}
}

void CASWHudAmmo::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);	
	SetPaintBackgroundEnabled(false);
	SetBgColor(Color(255,255,255,0));
	SetFgColor(Color(255,255,255,255));

	m_pBulletsImage = scheme()->GetImage("swarm/HUD/BulletsIcon", true);
	m_pClipsImage = scheme()->GetImage("swarm/HUD/AmmoClipsIcon", true);
	m_pGrenadesImage = scheme()->GetImage("swarm/HUD/AmmoGrenadeIcon", true);
	m_pHealImage = scheme()->GetImage("swarm/HUD/MedicHealIcon", true);
	m_pSelfHealImage = scheme()->GetImage("swarm/HUD/MedicHealSelfIcon", true);
	
	m_pWeaponGlowLabel->SetFont(ASW_GetSmallFont(true));
	m_pWeaponLabel->SetFont(ASW_GetSmallFont(false));
	m_pWeaponGlowLabel->SetFgColor(Color(35,214,250,255));
	m_pWeaponLabel->SetFgColor(Color(255,255,255,255));	
	m_hLastWeapon = NULL;

	m_pAltAmmoPos->SetAlpha( 0 );

	// do a single think so we hide any panels that shouldn't be showing
	OnThink();
}

void CASWHudAmmo::PerformLayout()
{
	BaseClass::PerformLayout();	

	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();

	// work out our y offset due to asw_hud_scale
	int y_offset = (130 * (ScreenHeight() / 768.0f)) - (130 * fScale);

	digit_xpos = 29 * fScale * 1.6f;
	digit_ypos = 50 * fScale * 1.6f + y_offset;
	digit2_xpos = 97 * fScale * 1.6f;
	digit2_ypos = 50 * fScale * 1.6f + y_offset;
	digit3_xpos = 131 * fScale * 1.6f;
	digit3_ypos = 50 * fScale * 1.6f + y_offset;

	if (m_pBulletsImage)
		m_pBulletsImage->SetSize(28.0f * fScale, 28.0f * fScale);
	if (m_pClipsImage)
		m_pClipsImage->SetSize(28.0f * fScale, 28.0f * fScale);
	if (m_pGrenadesImage)
		m_pGrenadesImage->SetSize(28.0f * fScale, 28.0f * fScale);
	if (m_pHealImage)
		m_pHealImage->SetSize(25.0f * fScale, 25.0f * fScale);
	if (m_pSelfHealImage)
		m_pSelfHealImage->SetSize(25.0f * fScale, 25.0f * fScale);
	m_pWeaponLabel->SetBounds(16.0f * fScale, digit_ypos - 19.0f * fScale,
								512.0f * fScale, 21.0f * fScale);
	m_pWeaponGlowLabel->SetBounds(16.0f * fScale, digit_ypos - 19.0f * fScale,
								512.0f * fScale, 21.0f * fScale);
	m_pWeaponLabel->SetTextInset( 3 * fScale, 0 );
	m_pWeaponGlowLabel->SetTextInset( 3 * fScale, 0 );

	m_pFastReload->SetBounds(16.0f * fScale, 10.0f * fScale, 192.0f * fScale, 21.0f * fScale);

	int x, y;
	GetPos(x, y);
}

void CASWHudAmmo::Paint()
{	
	VPROF_BUDGET( "CASWHudAmmo::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );	

	float fAlpha = m_pWeaponLabel->GetAlpha() / 255.0f;
	if (fAlpha <= 0)
		return;

	// paint our backdrop bit
	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();
	int border = fScale * 4.0f;
	int top = digit_ypos - 21.0f * fScale;
	int left = 18.0f * fScale - border;
	int weapon_name_size_x, weapon_name_size_y;   m_pWeaponLabel->GetContentSize(weapon_name_size_x, weapon_name_size_y);
	if (weapon_name_size_x < 120 * fScale)
		weapon_name_size_x = 120 * fScale;
	int top_right_x = left + weapon_name_size_x + border * 2;	
	int digit_top = digit_ypos - border;
	int x, y, w, h;
	GetBounds( x, y, w, h );
	int charWidth = (w / 12.5) * asw_hud_scale.GetFloat();   // ideal size at 1024 but scaling ruins scanlines
	//int charHeight = charWidth * 2;
	int bottom = digit_ypos + 37.0f * fScale; //charHeight + border;
	int ammo_bar_tall = 8 * fScale;
	int ammo_bar_padding = 5 * fScale;
	int bottom_with_ammo_bar = bottom + 13 * fScale;
	int bottom_right_x = digit_xpos + 234.0f * fScale;
	
	if ((m_iDisplaySecondaryValue==0) && (m_iDisplayTertiaryValue==0))
	{
		bottom_right_x = top_right_x + (bottom - top);
	}
	else if ((m_iDisplaySecondaryValue==0) || (m_iDisplayTertiaryValue==0))
	{
		bottom_right_x = 226.0f * fScale;		
	}	
	digit2_xpos = 128.0f * fScale + charWidth;
	digit3_xpos = 182.0f * fScale + charWidth;
	// if we have only 2 digits of ammo, pull all the other elements left 1 character
	if (m_iAmmo < 100)
	{
		bottom_right_x -= charWidth;
		digit3_xpos -= charWidth;		
		digit2_xpos -= charWidth;
	}
	// if we have more than 10 clips, push the grenades count right one character
	if (m_iClips > 9)
	{
		bottom_right_x += charWidth;
		digit3_xpos += charWidth;		
	}	

	// make sure we haven't pulled in the bottom section too far left to ruin the diagonal
	if (bottom_right_x < top_right_x + (bottom - top))
		bottom_right_x = top_right_x + (bottom - top);

	// top section where weapon name goes
	vgui::surface()->DrawSetColor(Color(255,255,255,fAlpha * asw_hud_alpha.GetInt()));
	vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(left,								top),			Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(top_right_x,						top),			Vector2D(1,0) ), 
	vgui::Vertex_t( Vector2D(top_right_x + (digit_top - top),	digit_top),		Vector2D(1,1) ),
	vgui::Vertex_t( Vector2D(left,								digit_top),		Vector2D(0,1) )
	};
	vgui::surface()->DrawTexturedPolygon( 4, points );	
	// lower section with angled lower right corner
	/*
	vgui::Vertex_t lpoints[4] = 
	{ 
	vgui::Vertex_t( Vector2D(left,									digit_top),	Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(bottom_right_x - (bottom - digit_top), digit_top),	Vector2D(0.6f, 1) ),
	vgui::Vertex_t( Vector2D(bottom_right_x,						bottom),	Vector2D(0.3f,1) ),
	vgui::Vertex_t( Vector2D(left,									bottom),	Vector2D(0,1) )
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, lpoints );
	*/
	// lower section rectangular, with room for ammo bar
	if (m_iDisplayPrimaryValue > 0)
	{
		vgui::Vertex_t lpoints[4] = 
		{ 
		vgui::Vertex_t( Vector2D(left,									digit_top),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(bottom_right_x - (bottom - digit_top), digit_top),	Vector2D(1,0) ),
		vgui::Vertex_t( Vector2D(bottom_right_x - (bottom - digit_top),	bottom_with_ammo_bar),	Vector2D(1,1) ),
		vgui::Vertex_t( Vector2D(left,									bottom_with_ammo_bar),	Vector2D(0,1) )
		}; 
		vgui::surface()->DrawTexturedPolygon( 4, lpoints );
	}
	else	// just paint enough to make a little padding on the lower side of the weapon name
	{

		vgui::Vertex_t lpoints[4] = 
		{ 
		vgui::Vertex_t( Vector2D(left,									digit_top),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(top_right_x + (digit_top - top),		digit_top),	Vector2D(1,0) ),
		vgui::Vertex_t( Vector2D(top_right_x + (digit_top - top),	digit_top+border*2),	Vector2D(1,1) ),
		vgui::Vertex_t( Vector2D(left,								digit_top+border*2),	Vector2D(0,1) )
		}; 
		vgui::surface()->DrawTexturedPolygon( 4, lpoints );		
	}

	// draw our numbers	
	vgui::surface()->DrawSetColor(Color(255,255,255,fAlpha * 255.0f));
	if (m_iDisplayPrimaryValue > 0)
	{
		if (m_iAmmo < 100)
			PaintRedNumbers(digit_xpos, digit_ypos, m_iAmmo, 2	);
		else
			PaintRedNumbers(digit_xpos, digit_ypos, m_iAmmo, 3	);
	}

	// total ammo
	if (m_iDisplaySecondaryValue > 0)
	{
		if (m_iClips > 9)
			PaintRedNumbers(digit2_xpos, digit2_ypos, m_iClips, 2);
		else
			PaintRedNumbers(digit2_xpos, digit2_ypos, m_iClips, 1);		
		if (m_iOverflow > 0)
		{
			PaintRedNumbers(digit2_xpos + 50, digit2_ypos, m_iOverflow, 2);
		}
	}

	// draw tertiary display
	if (m_iDisplayTertiaryValue > 0)
	{
		if ( m_iDisplaySecondaryValue == 0)
		{
			PaintRedNumbers(digit2_xpos, digit2_ypos, m_iGrenades, 1);
			m_pAltAmmoPos->SetBounds( digit2_xpos, digit2_ypos, charWidth, charWidth );
		}
		else
		{
			PaintRedNumbers(digit3_xpos, digit3_ypos, m_iGrenades, 1);
			m_pAltAmmoPos->SetBounds( digit3_xpos, digit3_ypos, charWidth, charWidth );
		}
	}

	PaintAmmoWarnings();

	vgui::surface()->DrawSetColor(255, 255, 255, m_pWeaponLabel->GetAlpha());
	// paint images
	if (m_iDisplayPrimaryValue > 0)
	{
		if (m_iDisplayPrimaryValue == 1)
		{
			m_pBulletsImage->SetPos(digit_xpos - charWidth, digit_ypos + 2.0f * fScale);
			m_pBulletsImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
			m_pBulletsImage->Paint();
		}
		else if (m_iDisplayPrimaryValue == 2)
		{
			m_pHealImage->SetPos(digit_xpos - charWidth, digit_ypos + 2.0f * fScale);
			m_pHealImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
			m_pHealImage->Paint();
		}
		else if (m_iDisplayPrimaryValue == 3)
		{
			m_pSelfHealImage->SetPos(digit_xpos - charWidth, digit_ypos + 2.0f * fScale);
			m_pSelfHealImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
			m_pSelfHealImage->Paint();
		}
	}
	if (m_iDisplaySecondaryValue > 0)
	{
		if (m_iDisplaySecondaryValue == 1)
		{
			m_pClipsImage->SetPos(digit2_xpos - charWidth, digit_ypos + 2.0f * fScale);
			m_pClipsImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
			m_pClipsImage->Paint();
		}		
	}
	if (m_iDisplayTertiaryValue > 0)
	{
		if (m_iDisplaySecondaryValue == 0)
		{
			if (m_iDisplayTertiaryValue == 1)
			{
				m_pGrenadesImage->SetPos(digit2_xpos - charWidth, digit_ypos + 2.0f * fScale);
				m_pGrenadesImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
				m_pGrenadesImage->Paint();
			}
			else if (m_iDisplayTertiaryValue == 2)
			{
				m_pSelfHealImage->SetPos(digit2_xpos - charWidth, digit_ypos + 2.0f * fScale);				
				m_pSelfHealImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
				m_pSelfHealImage->Paint();
			}
		}
		else
		{
			m_pGrenadesImage->SetPos(digit3_xpos - charWidth, digit_ypos + 2.0f * fScale);
			m_pGrenadesImage->SetColor(Color(255, 255, 255, m_pWeaponLabel->GetAlpha()));
			m_pGrenadesImage->Paint();
		}
	}
	// paint ammo bar
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (pPlayer && (m_iDisplayPrimaryValue > 0))
	{
		C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(pPlayer->GetMarine());
		if ( pMarine && pMarine->GetMarineResource())
		{
			C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
			if (pWeapon)
			{
				vgui::surface()->DrawSetTexture(m_nLowerAmmoBarTexture);
				int ammo_left = left + ammo_bar_padding * 2;
				int ammo_right = (bottom_right_x - (bottom - digit_top) - ammo_bar_padding);
				int ammo_max_wide = ammo_right - ammo_left;
				int ammo_wide = pMarine->GetMarineResource()->GetAmmoPercent() * float(ammo_max_wide);

				int a = m_pWeaponLabel->GetAlpha();
				if (pWeapon->IsReloading())
				{
					float fStart = pWeapon->m_fReloadStart;
					float fNext = pWeapon->m_flNextPrimaryAttack;
					float fTotalTime = fNext - fStart;
					if (fTotalTime <= 0)
						fTotalTime = 0.1f;
					
					float fProgress = (gpGlobals->curtime - fStart) / fTotalTime;

					ammo_wide = fProgress * float(ammo_max_wide);
					
					// fractions of ammo wide for fast reload start/end
					//float fFastStart = (pWeapon->m_fFastReloadStart - fStart) / fTotalTime;
					//float fFastEnd = (pWeapon->m_fFastReloadEnd - fStart) / fTotalTime;
					

					if ((int(gpGlobals->curtime*4) % 2) == 0)
					{
						vgui::surface()->DrawSetColor(128,128,128,a);
					}
					else
					{
						vgui::surface()->DrawSetColor(128,128,128,a * 0.25f);
					}
				}
				else
				{
					vgui::surface()->DrawSetColor(35,214,250,a);
				}
				
				vgui::Vertex_t ampoints[4] = 
				{ 
				vgui::Vertex_t( Vector2D(ammo_left,				bottom_with_ammo_bar - (ammo_bar_padding + ammo_bar_tall)),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(ammo_left + ammo_wide,	bottom_with_ammo_bar - (ammo_bar_padding + ammo_bar_tall)),	Vector2D(ammo_wide * 0.5f,0) ),
				vgui::Vertex_t( Vector2D(ammo_left + ammo_wide,	bottom_with_ammo_bar - ammo_bar_padding),					Vector2D(ammo_wide * 0.5f,1) ),
				vgui::Vertex_t( Vector2D(ammo_left,				bottom_with_ammo_bar - ammo_bar_padding),					Vector2D(0,1) )
				}; 
				vgui::surface()->DrawTexturedPolygon( 4, ampoints );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates ammo display
//-----------------------------------------------------------------------------
void CASWHudAmmo::SetAmmo(int ammo)
{
	m_iAmmo = ammo;
}

//-----------------------------------------------------------------------------
// Purpose: Updates 2nd ammo display (clips)
//-----------------------------------------------------------------------------
void CASWHudAmmo::SetClips(int num_clips)
{
	if (num_clips != m_iClips)
	m_iClips = num_clips;
}

//-----------------------------------------------------------------------------
// Purpose: Updates 3rd ammo display (grenades)
//-----------------------------------------------------------------------------
void CASWHudAmmo::SetGrenades(int num_grenades)
{
	if (num_grenades < 0)
		num_grenades = 0;
	m_iGrenades = num_grenades;
}

// paints red ammo counter style numbers
int CASWHudAmmo::PaintRedNumbers(int xpos, int ypos, int value, int digits)
{
	// find size to draw numbers at, based on size of this hud element
	int x, y, w, h;
	GetBounds( x, y, w, h );
	int charWidth = (w / 12.5) * asw_hud_scale.GetFloat();   // ideal size at 1024 but scaling ruins scanlines
	//int charWidth = w / 10;	// real size at 1024
	int charHeight = charWidth * 2;

	// split the value up into 3 digits
	int units, tens, hundreds;
	units = tens = hundreds = -1;

	if (value < 10)
	{
		units = value;
		tens = 0;
		hundreds = 0;
	}
	else if (value < 100)
	{
		tens = int(value / 10);
		units = value - (tens * 10);
		hundreds = 0;
	}
	else if (value < 1000)
	{
		hundreds = int(value / 100);
		tens = int((value - (hundreds * 100)) / 10);
		units = value - ((hundreds * 100) + (tens * 10));
	}

	// draw the digits
	if ( hundreds >= 0 && hundreds < 10 && digits >= 3)
	{
		vgui::surface()->DrawSetTexture( m_iRedNumberTextureID[hundreds] );
		vgui::surface()->DrawTexturedRect( xpos, ypos, xpos+charWidth, ypos+charHeight );
		xpos += charWidth;
	}
	if ( tens >= 0 && tens < 10 && digits >= 2)
	{
		vgui::surface()->DrawSetTexture( m_iRedNumberTextureID[tens] );
		vgui::surface()->DrawTexturedRect( xpos, ypos, xpos+charWidth, ypos+charHeight );
		xpos += charWidth;
	}
	if ( units >= 0 && units < 10 && digits >= 1)
	{
		vgui::surface()->DrawSetTexture( m_iRedNumberTextureID[units] );
		vgui::surface()->DrawTexturedRect( xpos, ypos, xpos+charWidth, ypos+charHeight );
		xpos += charWidth;
	}
	if (value < 10)
		return 1;
	else if (value < 100)
		return 2;
	return 3;
}

void CASWHudAmmo::LoadRedNumbers()
{
	char buffer[64];
	for (int i=0;i<10;i++)
	{
		m_iRedNumberTextureID[i] = vgui::surface()->CreateNewTextureID();
		Q_snprintf(buffer, sizeof(buffer), "%s%d", "vgui/swarm/HUD/BlueDigit", i);
		vgui::surface()->DrawSetTextureFile( m_iRedNumberTextureID[i], buffer, true, false);
	}
}


void CASWHudAmmo::PaintAmmoWarnings()
{
	C_ASW_Player *local = C_ASW_Player::GetLocalASWPlayer();
	if (!local)
		return;

	C_ASW_Marine *marine = dynamic_cast<C_ASW_Marine*>(local->GetMarine());
	if ( !marine || !marine->GetMarineResource())
		return;

	bool bNoAmmo = (marine->GetMarineResource()->GetAmmoPercent() <= 0);
	bool bLowAmmo = marine->GetMarineResource()->IsLowAmmo();
	bool bHasActiveWeapon = marine->GetActiveASWWeapon() != NULL;
	bool bReloading = bHasActiveWeapon && marine->GetActiveASWWeapon()->IsReloading();
	bool bHasWeapon = true;
	if (!bHasActiveWeapon)
	{
		bHasWeapon = ((marine->GetWeapon(0)!=NULL) ||
						(marine->GetWeapon(1)!=NULL) ||
							(marine->GetWeapon(2)!=NULL));
	}
	bReloading |= marine->IsAnimatingReload();

	float f=(gpGlobals->curtime - int(gpGlobals->curtime) );
	if (f < 0) f = -f;
	if (f<=0.25 || (f>0.5 && f<=0.75))
	{		
		char buffer[32];
		int row = 0;

		if (!bHasWeapon || m_bActiveAmmobag)
		{
			//Q_snprintf(buffer, sizeof(buffer), " ");
			//DrawAmmoWarningMessage(row, buffer, 255, 0, 0);
			//row++;
		}
		else if (bNoAmmo && !bReloading)
		{
			Q_snprintf(buffer, sizeof(buffer), "- NO AMMO -");
			DrawAmmoWarningMessage(row, buffer, 255, 0, 0);
			row++;
		}
		else if (bLowAmmo && !bReloading)
		{
			Q_snprintf(buffer, sizeof(buffer), "- LOW AMMO -");
			DrawAmmoWarningMessage(row, buffer, 255, 0, 0);
			row++;
		}
	}
}

void CASWHudAmmo::DrawAmmoWarningMessage(int row, char* buffer, int r, int g, int b)
{
	float xPos = this->GetWide() * 0.15f * asw_hud_scale.GetFloat();
	float yPos = this->GetTall() * 0.3f * asw_hud_scale.GetFloat();

	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();
	// work out our y offset due to asw_hud_scale
	int y_offset = (130 * (ScreenHeight() / 768.0f)) - (130 * fScale);

	//int wide = g_pMatSystemSurface->DrawTextLen(m_hAmmoWarningFont, &buffer[0]);
	int tall = vgui::surface()->GetFontTall(m_hAmmoWarningFont);
		
	// centre it on the x
	//xPos -= (0.5f * wide);
	// count down rows
	yPos += tall * 1.5f * row + y_offset;
	// drop shadow
	g_pMatSystemSurface->DrawColoredText(m_hAmmoWarningFont, xPos+1, yPos+1, 0, 0, 
		0, 200, &buffer[0]);
	// actual text
	g_pMatSystemSurface->DrawColoredText(m_hAmmoWarningFont, xPos, yPos, r, g, 
		b, 200, &buffer[0]);
}