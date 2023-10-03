#include "cbase.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_marine_resource.h"
#include "c_asw_game_resource.h"
#include "vguimatsurface/imatsystemsurface.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "vgui_hudvideo.h"
#include "asw_hud_marine_portrait.h"
#include "iasw_client_usable_entity.h"
#include "c_asw_door.h"
#include "iinput.h"
#include "asw_input.h"
#include "asw_hud_portraits.h"
#include "asw_hud_health_cross.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_PORTRAIT_WIDE 104
#define ASW_PORTRAIT_TALL 104

#define LEFT_OFFSET (2 * m_fScale)
#define SIDE_ICON_X (6 * m_fScale)

#define PORTRAIT_X ( asw_portrait_face.GetBool() ? (LEFT_OFFSET + 30.0f * m_fScale) : (LEFT_OFFSET + 16.0f * m_fScale) )
#define PORTRAIT_Y (24.0f * m_fScale)
#define PORTRAIT_SIZE (66.0f * m_fScale)

extern ConVar asw_portraits_border;
extern ConVar asw_portraits_border_shrink;
extern ConVar asw_hud_alpha;
extern ConVar asw_hud_scale;

ConVar asw_cross_layout( "asw_cross_layout", "1", FCVAR_ARCHIVE, "If set, marine portraits are laid out in a cross shape in joypad mode" );
ConVar asw_portrait_scale_test( "asw_portrait_scale_test", "0", FCVAR_CHEAT );
ConVar asw_portrait_face( "asw_portrait_face", "1", FCVAR_NONE );
ConVar asw_portrait_health_cross( "asw_portrait_health_cross", "0", FCVAR_NONE );
ConVar asw_portrait_ammo( "asw_portrait_ammo", "1", FCVAR_NONE );
ConVar asw_portrait_leadership( "asw_portrait_leadership", "1", FCVAR_NONE );
ConVar asw_portrait_class( "asw_portrait_class", "1", FCVAR_NONE );

CASW_HUD_Marine_Portrait::CASW_HUD_Marine_Portrait(vgui::Panel *pParent, const char *szPanelName, CASWHudPortraits *pPortraitContainer ) :
	vgui::Panel(pParent, szPanelName)
{
	m_hPortraitContainer = pPortraitContainer;

	m_pVideoFace = new HUDVideoPanel( this, "VideoFace" );

	m_pMarineNameGlow = new vgui::Label(this, "MarineNameGlow", " ");
	m_pMarineName = new vgui::Label(this, "MarineName", " ");	
	m_pMarineNumber = new vgui::Label(this, "MarineNumber", " ");

	m_pHealthCross = new CASW_HUD_Health_Cross( this, "HealthCross" );
	
	m_pLeadershipAuraIcon = new vgui::ImagePanel(this, "LeaderAuraIcon");
	m_pLeadershipAuraIcon->SetShouldScaleImage(true);
	m_pBulletsIcon = new vgui::ImagePanel(this, "BulletsIcon");
	m_pBulletsIcon->SetShouldScaleImage(true);	
	m_pClipsIcon = new vgui::ImagePanel(this, "ClipsIcon");
	m_pClipsIcon->SetShouldScaleImage(true);
	m_pFiringOverlay = new vgui::ImagePanel(this, "FiringOverlay");
	m_pFiringOverlay->SetShouldScaleImage(true);	
	m_pFiringOverlay->SetAlpha(0);
	m_pInfestedOverlay = new vgui::ImagePanel(this, "InfestedOverlay");
	m_pInfestedOverlay->SetShouldScaleImage(true);
	m_pInfestedOverlay->SetAlpha(0);
	
	m_pReloadingOverlay = new vgui::Label(this, "ReloadingOverlay", "#asw_hud_reload");
	m_pReloadingOverlay->SetContentAlignment(vgui::Label::a_center);
	m_pReloadingOverlay->SetAlpha(0);

	m_pLowAmmoOverlay = new vgui::Label(this, "LowAmmoOverlay", "#asw_hud_low");
	m_pLowAmmoOverlay->SetContentAlignment(vgui::Label::a_south);
	m_pLowAmmoOverlay2 = new vgui::Label(this, "LowAmmoOverlay2", "#asw_hud_ammo");
	m_pLowAmmoOverlay2->SetContentAlignment(vgui::Label::a_south);	
	m_pLowAmmoOverlay->SetAlpha(0);
	m_pLowAmmoOverlay2->SetAlpha(0);	
	m_pNoAmmoOverlay = new vgui::Label(this, "NoAmmoOverlay", "#asw_hud_out_of");
	m_pNoAmmoOverlay->SetContentAlignment(vgui::Label::a_south);
	m_pNoAmmoOverlay2 = new vgui::Label(this, "NoAmmoOverlay2", "#asw_hud_ammo");
	m_pNoAmmoOverlay2->SetContentAlignment(vgui::Label::a_south);	
	m_pNoAmmoOverlay->SetAlpha(0);
	m_pNoAmmoOverlay2->SetAlpha(0);

	m_hMarineResource = NULL;
	m_fInfestedFade = 0;
	m_bLocalMarine = false;
	m_iIndex = 0;
	m_bMedic = false;
	m_fLastHealth = 0;
	m_fScale = 1.0f;
	m_bLastWide = false;
	m_bLastFace = false;
	m_bLastHealthCross = false;
	m_bLastInfested = false;
	m_fLastFiringFade = -1;
}

void CASW_HUD_Marine_Portrait::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);	

	m_pFiringOverlay->SetImage("swarm/HUD/FiringHUDIcon");	
	m_pInfestedOverlay->SetImage("swarm/HUD/InfestedHUDIcon");
	m_pBulletsIcon->SetImage("swarm/HUD/TinyBulletsIcon");
	m_pClipsIcon->SetImage("swarm/HUD/TinyClipsIcon");
	m_pLeadershipAuraIcon->SetImage("swarm/ClassIcons/LeaderAuraIcon");
	m_pMarineName->SetContentAlignment(vgui::Label::a_northwest);
	m_pMarineNameGlow->SetContentAlignment(vgui::Label::a_northwest);
	m_pMarineNumber->SetContentAlignment(vgui::Label::a_northwest);
	m_pMarineNumber->SetVisible( false );		// not showing marine numbers for now.  TODO: If player is playing old school style, show keybindings? (F-keys?)

	//vgui::HFont hMedFont = pScheme->GetFont("DefaultVerySmall", true);
	
	if (ScreenHeight() <= 600)
	{
		m_hMarineNameFont = pScheme->GetFont( "DefaultMedium", IsProportional() );		
		m_hMarineNameGlowFont = pScheme->GetFont( "DefaultMediumBlur", IsProportional() );
		m_hMarineNameSmallFont = pScheme->GetFont( "VerdanaSmall", IsProportional() );		
		m_hMarineNameSmallGlowFont = pScheme->GetFont( "VerdanaSmallBlur", IsProportional() );		
	}
	m_pReloadingOverlay->SetFgColor(Color(255,255,255,255));
	m_pLowAmmoOverlay->SetFgColor(Color(255,255,255,255));
	m_pLowAmmoOverlay2->SetFgColor(Color(255,255,255,255));
	m_pNoAmmoOverlay->SetFgColor(Color(255,255,255,255));
	m_pNoAmmoOverlay2->SetFgColor(Color(255,255,255,255));
	m_pInfestedOverlay->SetDrawColor(Color(255,255,255,255));	
	m_pFiringOverlay->SetDrawColor(Color(255,255,255,255.0f * m_fLastFiringFade));

	SetFonts();

	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(Color(0,0,0, asw_hud_alpha.GetInt()));
}

void CASW_HUD_Marine_Portrait::PerformLayout()
{
	int iGlowFontHeight = vgui::surface()->GetFontTall(m_hMarineNameGlowFont);
	int iLabelHeight = 21.0f * m_fScale;
	int iYOffset = (iLabelHeight - iGlowFontHeight) * 0.5f;

	// position labels
	m_pMarineName->SetBounds(LEFT_OFFSET + 14.0f * m_fScale, 3.0f * m_fScale + iYOffset,
								82.0f * m_fScale, 21.0f * m_fScale);
	m_pMarineNameGlow->SetBounds(LEFT_OFFSET + 14.0f * m_fScale, 3.0f * m_fScale + iYOffset,
								82.0f * m_fScale, 21.0f * m_fScale);

	// video face	
	int portrait_x = PORTRAIT_X;
	int portrait_y = PORTRAIT_Y;
	int portrait_size = PORTRAIT_SIZE;
	m_pVideoFace->SetBounds( portrait_x, portrait_y, portrait_size, portrait_size );
	m_pVideoFace->SetZPos( 10 );
	m_pVideoFace->SetLoopVideo( "test_healthy" );

	if ( asw_portrait_face.GetBool() )
	{	
		m_pMarineNumber->SetBounds(LEFT_OFFSET + 32.0f * m_fScale, 24.0f * m_fScale,
									26.0f * m_fScale, 31.0f * m_fScale);

		// leadership aura
		m_pLeadershipAuraIcon->SetBounds(LEFT_OFFSET + 81.0f * m_fScale, 77.0f * m_fScale,
			13.0f * m_fScale, 13.0f * m_fScale);
	}
	else
	{
		m_pMarineNumber->SetBounds( SIDE_ICON_X, asw_portrait_class.GetBool() ? .0f * m_fScale : 3.0f * m_fScale + iYOffset,
			26.0f * m_fScale, 31.0f * m_fScale);

		// leadership aura
		m_pLeadershipAuraIcon->SetBounds(LEFT_OFFSET + 70.0f * m_fScale, 5.0f * m_fScale,
			13.0f * m_fScale, 13.0f * m_fScale);
	}

	// position the firing overlay
	m_pFiringOverlay->SetBounds(LEFT_OFFSET + 61.0f * m_fScale, 29.0f * m_fScale,
								28.0f * m_fScale, 28.0f * m_fScale);
	

	// position infested overlay
	m_pInfestedOverlay->SetBounds(portrait_x, portrait_y,
									portrait_size, portrait_size);		
	m_pReloadingOverlay->SetBounds(portrait_x, portrait_y + portrait_size * 0.5f,
									portrait_size, portrait_size * 0.5f);
	m_pLowAmmoOverlay->SetBounds(portrait_x, portrait_y + portrait_size * 0.45f,
									portrait_size, portrait_size * 0.25f);
	m_pLowAmmoOverlay2->SetBounds(portrait_x, portrait_y + portrait_size * 0.7f,
									portrait_size, portrait_size * 0.25f);
	m_pNoAmmoOverlay->SetBounds(portrait_x, portrait_y + portrait_size * 0.45f,
									portrait_size, portrait_size * 0.25f);
	m_pNoAmmoOverlay2->SetBounds(portrait_x, portrait_y + portrait_size * 0.7f,
									portrait_size, portrait_size * 0.25f);

	// health
	m_pHealthCross->SetBounds(SIDE_ICON_X + 8.0f * m_fScale, portrait_y,
		portrait_size, portrait_size);

	// ammo icons
	if ( asw_portrait_face.GetBool() )
	{
		m_pBulletsIcon->SetBounds( LEFT_OFFSET + 5 * m_fScale, 91 * m_fScale,
									9.0f * m_fScale, 9.0f * m_fScale);
		m_pClipsIcon->SetBounds( LEFT_OFFSET + 16 * m_fScale, 91 * m_fScale,
									9.0f * m_fScale, 9.0f * m_fScale);	
	}
	else
	{
		int portrait_y = PORTRAIT_Y;
		int portrait_size = PORTRAIT_SIZE;

		int healthbar_y = portrait_y + (2 * m_fScale);
		if ( asw_portrait_face.GetBool() )
		{
			healthbar_y += portrait_size;
		}
		int healthbar_height = 7 * m_fScale;
		int healthbar_y2 = healthbar_y + healthbar_height;

		int ammobar_y = healthbar_y2 + 3 * m_fScale;
		int ammobar_height = 8 * m_fScale;
		int ammobar_y2 = ammobar_y + ammobar_height;

		int clipsbar_y = ammobar_y2 + 3 * m_fScale;

		m_pBulletsIcon->SetBounds( SIDE_ICON_X, ammobar_y,
			9.0f * m_fScale, 9.0f * m_fScale);
		m_pClipsIcon->SetBounds( SIDE_ICON_X, clipsbar_y,
			9.0f * m_fScale, 9.0f * m_fScale);	
	}
}

void CASW_HUD_Marine_Portrait::OnThink()
{
	if (m_hMarineResource.Get() != NULL)
	{		
		// check for changing width when we start using something
		bool bWide = (m_bMedic || (GetUsingFraction() > 0));
		bool bFace = asw_portrait_face.GetBool();
		bool bCross = asw_portrait_health_cross.GetBool();
		if (bWide != m_bLastWide || bFace != m_bLastFace || m_hMarineResource->IsInfested() != m_bLastInfested || bCross != m_bLastHealthCross )
		{
			PositionMarinePanel();
			m_bLastWide = bWide;
			m_bLastFace = bFace;
			m_bLastInfested = m_hMarineResource->IsInfested();
			m_bLastHealthCross = bCross;
			m_pHealthCross->SetVisible( bCross );
		}

		float fHealthPercent = m_hMarineResource->GetHealthPercent();

		// update colour of name, if selected
		if ( fHealthPercent <= 0.0f )
		{
			m_pMarineName->SetFgColor(Color(128,128,128,255));
			m_pMarineNumber->SetFgColor(Color(128,128,128,255));
			m_pMarineNameGlow->SetFgColor(Color(64,64,64,255));
		}
		else
		{
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if (pPlayer->GetMarine() && pPlayer->GetMarine()->GetMarineResource() == m_hMarineResource.Get())
			{
				m_pMarineName->SetFgColor(Color(255,255,0,255));
				m_pMarineNumber->SetFgColor(Color(255,255,0,255));
				m_pMarineNameGlow->SetFgColor(Color(235,177,0,255));
			}
			else
			{
				m_pMarineName->SetFgColor(Color(255,255,255,255));
				m_pMarineNumber->SetFgColor(Color(255,255,255,255));
				m_pMarineNameGlow->SetFgColor(Color(35,214,250,255));
			}
		}
		if (m_hMarineResource->IsInfested())
			m_pMarineNumber->SetFgColor(Color(0,0,0,0));
		
		// update firing flash if firing
		float fFiringFade = m_hMarineResource->GetFiringTimer();
		if (fFiringFade != m_fLastFiringFade)
		{
			m_pFiringOverlay->SetDrawColor(Color(255,255,255,255.0f * fFiringFade));
			m_fLastFiringFade = fFiringFade;
		}
		// update infested overlay if infested
		bool bReloading = false;
		bool bLowAmmo = false;
		bool bNoAmmo = false;
		if ( m_hMarineResource->IsInfested() )
		{
			m_fInfestedFade = MIN(1.0f, m_fInfestedFade + gpGlobals->frametime * 5.0f);

			// TODO: Make an infested video
			//m_pVideoFace->SetLoopVideo( "media/test_infested.bik" );	
		}
		else if ( fHealthPercent > 0.0f )
		{
			if ( m_fLastHealth != fHealthPercent )
			{
				if ( m_fLastHealth > fHealthPercent )
				{
					m_pVideoFace->PlayTempVideo( "test_pain", "test_static" );
				}

				if ( fHealthPercent < 0.5f )
				{
					m_pVideoFace->SetLoopVideo( "test_needHealth" );
				}
				else
				{
					m_pVideoFace->SetLoopVideo( "test_healthy", 1, 0.3f );
				}
			}

			m_fInfestedFade = MAX(0, m_fInfestedFade - gpGlobals->frametime * 5.0f);
			if ( !asw_portrait_health_cross.GetBool() )
			{
				if (m_hMarineResource->IsReloading())
				{
					bReloading = true;
				}
				else if (m_hMarineResource->GetAmmoPercent()<=0 && m_hMarineResource->GetClipsPercent()<=0)
				{
					bNoAmmo = true;
				}
				else if (m_hMarineResource->GetAmmoPercent() <= 0.2f && m_hMarineResource->GetAmmoPercent() > 0)
				{
					bLowAmmo = true;
				}
			}
		}
		m_pInfestedOverlay->SetDrawColor(Color(30,255,30,255.0f * m_fInfestedFade));
		m_pReloadingOverlay->SetVisible(bReloading);
		m_pLowAmmoOverlay->SetVisible(bLowAmmo);
		m_pLowAmmoOverlay2->SetVisible(bLowAmmo);
		m_pNoAmmoOverlay->SetVisible(bNoAmmo);
		m_pNoAmmoOverlay2->SetVisible(bNoAmmo);
		m_pLeadershipAuraIcon->SetVisible( asw_portrait_leadership.GetBool() && m_hMarineResource->GetLeadershipResist() > 0);
		m_pFiringOverlay->SetVisible( asw_portrait_ammo.GetBool() );

		m_fLastHealth = fHealthPercent;
	}
	else
	{
		m_pInfestedOverlay->SetDrawColor(Color(0,0,0,0));
		m_pFiringOverlay->SetDrawColor(Color(0,0,0,0));
		m_pLeadershipAuraIcon->SetVisible(false);
		if (IsVisible())
		{
			SetVisible(false);
		}
	}

	m_pBulletsIcon->SetVisible( asw_portrait_ammo.GetBool() );
	m_pClipsIcon->SetVisible( asw_portrait_ammo.GetBool() );

	if (asw_portrait_scale_test.GetBool())
	{
		PositionMarinePanel();
	}
}

void CASW_HUD_Marine_Portrait::Paint()
{
	PaintClassIcon();
	PaintPortrait();
	PaintAmmo();
	float f = GetUsingFraction();
	if (f > 0)
		PaintUsing(f);
	else if (m_bMedic)
		PaintMeds();
}

void CASW_HUD_Marine_Portrait::PaintAmmo()
{
	if (!m_hMarineResource.Get() || !m_hMarineResource->GetProfile())
		return;

	if ( !asw_portrait_ammo.GetBool() )
		return;

	bool bVerticalBar = asw_portrait_face.GetBool() || asw_portrait_health_cross.GetBool();

	if ( bVerticalBar )
	{
		if ( m_nVertAmmoBar != -1 && m_nTinyClipIcon !=-1)
		{
			//int portrait_x = 30.0f * m_fScale;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int ammo_x = LEFT_OFFSET + 6 * m_fScale;		
			int ammo_width = 6 * m_fScale;
			int clips_x = ammo_x + ammo_width + (5 * m_fScale);
			int clips_width = 8 * m_fScale;
			float fAmmo = (1.0f - m_hMarineResource->GetAmmoPercent());
			float fClips = (1.0f - m_hMarineResource->GetClipsPercent());
			int ammo_top = portrait_y + (fAmmo * portrait_size);
			int clips_top = portrait_y + (fClips * portrait_size);

			vgui::surface()->DrawSetColor(Color(255,255,255,255));
			// draw black backs
			vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
			vgui::surface()->DrawTexturedRect(ammo_x, portrait_y, ammo_x + ammo_width, portrait_y + portrait_size);
			vgui::surface()->DrawTexturedRect(clips_x, portrait_y, clips_x + clips_width, portrait_y + portrait_size);
			
			// bullets vert bar
			vgui::surface()->DrawSetTexture(m_nVertAmmoBar);
			vgui::Vertex_t apoints[4] = 
			{ 
			vgui::Vertex_t( Vector2D(ammo_x, ammo_top),	Vector2D(0,fAmmo) ), 
			vgui::Vertex_t( Vector2D(ammo_x + ammo_width, ammo_top),		Vector2D(1,fAmmo) ), 
			vgui::Vertex_t( Vector2D(ammo_x + ammo_width, portrait_y + portrait_size),		Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(ammo_x, portrait_y + portrait_size),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, apoints );

			// clips vert bar
			vgui::surface()->DrawSetTexture(m_nTinyClipIcon);
			vgui::Vertex_t cpoints[4] = 
			{ 
			vgui::Vertex_t( Vector2D(clips_x, clips_top),	Vector2D(0,fClips) ), 
			vgui::Vertex_t( Vector2D(clips_x + clips_width, clips_top),		Vector2D(1,fClips) ), 
			vgui::Vertex_t( Vector2D(clips_x + clips_width, portrait_y + portrait_size),		Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(clips_x, portrait_y + portrait_size),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, cpoints );
		}
	}
	else
	{
		// horizontal ammo
		if ( m_nVertAmmoBar != -1 && m_nTinyClipIconHoriz !=-1)
		{
			int portrait_x = PORTRAIT_X;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int healthbar_y = portrait_y + (2 * m_fScale);
			if ( asw_portrait_face.GetBool() )
			{
				healthbar_y += portrait_size;
			}
			int healthbar_height = 7 * m_fScale;
			int healthbar_y2 = healthbar_y + healthbar_height;

			int ammobar_y = healthbar_y2 + 3 * m_fScale;
			int ammobar_height = 8 * m_fScale;
			int ammobar_y2 = ammobar_y + ammobar_height;

			int clipsbar_y = ammobar_y2 + 3 * m_fScale;
			int clipsbar_height = 8 * m_fScale;
			int clipsbar_y2 = clipsbar_y + clipsbar_height;
			
			float fAmmo = m_hMarineResource->GetAmmoPercent();
			float fClips = m_hMarineResource->GetClipsPercent();
			int ammo_right = portrait_x + (fAmmo * portrait_size);
			int clips_right = portrait_x + (fClips * portrait_size);

			vgui::surface()->DrawSetColor(Color(255,255,255,255));
			// draw black backs
			vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
			vgui::surface()->DrawTexturedRect(portrait_x, ammobar_y, portrait_x + portrait_size, ammobar_y2);
			vgui::surface()->DrawTexturedRect(portrait_x, clipsbar_y, portrait_x + portrait_size, clipsbar_y2);

			// bullets horiz bar
			vgui::surface()->DrawSetTexture(m_nVertAmmoBar);
			vgui::Vertex_t apoints[4] = 
			{ 
				vgui::Vertex_t( Vector2D(portrait_x, ammobar_y),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(ammo_right, ammobar_y),		Vector2D(fAmmo,0) ), 
				vgui::Vertex_t( Vector2D(ammo_right, ammobar_y2),		Vector2D(fAmmo,1) ), 
				vgui::Vertex_t( Vector2D(portrait_x, ammobar_y2),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, apoints );

			// clips horiz bar
			vgui::surface()->DrawSetTexture(m_nTinyClipIconHoriz);
			vgui::Vertex_t cpoints[4] = 
			{ 
				vgui::Vertex_t( Vector2D(portrait_x, clipsbar_y),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(clips_right, clipsbar_y),		Vector2D(fClips,0) ), 
				vgui::Vertex_t( Vector2D(clips_right, clipsbar_y2),		Vector2D(fClips,1) ), 
				vgui::Vertex_t( Vector2D(portrait_x, clipsbar_y2),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, cpoints );
		}
	}
}


void CASW_HUD_Marine_Portrait::PaintMeds()
{
	if (!m_hMarineResource.Get() || !m_hMarineResource->GetProfile())
		return;

	bool bVerticalBar = asw_portrait_face.GetBool() || asw_portrait_health_cross.GetBool();

	if ( bVerticalBar )
	{
		if ( m_nMedChargeTexture )
		{
			int portrait_x = PORTRAIT_X;
			int meds_real_top = 3.0f * m_fScale;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int med_x = portrait_x + portrait_size + 4 * m_fScale;		
			int med_width = 12 * m_fScale;		
			float fMeds = (1.0f - m_hMarineResource->GetMedsPercent());		
			int meds_bottom = portrait_y + portrait_size + (7 * m_fScale);
			if ( !asw_portrait_face.GetBool() ) 
				meds_bottom -= (7 * m_fScale);
			int meds_tall = meds_bottom - meds_real_top;
			int med_top = meds_real_top + (fMeds * meds_tall);		

			vgui::surface()->DrawSetColor(Color(255,255,255,255));
			// draw black backs
			//vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
			//vgui::surface()->DrawTexturedRect(ammo_x, portrait_y, ammo_x + ammo_width, portrait_y + portrait_size);
			//vgui::surface()->DrawTexturedRect(clips_x, portrait_y, clips_x + clips_width, portrait_y + portrait_size);
			
			// meds vert bar
			vgui::surface()->DrawSetTexture(m_nMedChargeTexture);
			vgui::Vertex_t mpoints[4] = 
			{ 
			vgui::Vertex_t( Vector2D(med_x,				med_top),	Vector2D(0,fMeds) ), 
			vgui::Vertex_t( Vector2D(med_x + med_width, med_top),		Vector2D(1,fMeds) ), 
			vgui::Vertex_t( Vector2D(med_x + med_width, meds_real_top + meds_tall),		Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(med_x,				meds_real_top + meds_tall),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints );
		}
	}
	else
	{
		if ( m_nMedChargeHorizTexture )
		{
			int portrait_x = PORTRAIT_X;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int healthbar_y = portrait_y + (2 * m_fScale);
			if ( asw_portrait_face.GetBool() )
			{
				healthbar_y += portrait_size;
			}
			int healthbar_height = 7 * m_fScale;
			int healthbar_y2 = healthbar_y + healthbar_height;

			int ammobar_y = healthbar_y2 + 3 * m_fScale;
			int ammobar_height = 8 * m_fScale;
			int ammobar_y2 = ammobar_y + ammobar_height;

			int clipsbar_y = ammobar_y2 + 3 * m_fScale;
			int clipsbar_height = 8 * m_fScale;
			int clipsbar_y2 = clipsbar_y + clipsbar_height;

			int extrabar_y = asw_portrait_ammo.GetBool() ? clipsbar_y2 + 3 * m_fScale : healthbar_y2 + 3 * m_fScale;
			int extrabar_height = 7 * m_fScale;
			int extrabar_y2 = extrabar_y + extrabar_height;

			float fMeds = m_hMarineResource->GetMedsPercent();
			int med_width = fMeds * portrait_size;			

			vgui::surface()->DrawSetColor(Color(255,255,255,255));

			// meds horiz bar
			vgui::surface()->DrawSetTexture(m_nMedChargeHorizTexture);
			vgui::Vertex_t mpoints[4] = 
			{ 
				vgui::Vertex_t( Vector2D(portrait_x,			 extrabar_y),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(portrait_x + med_width, extrabar_y),		Vector2D(fMeds,0) ), 
				vgui::Vertex_t( Vector2D(portrait_x + med_width, extrabar_y2),		Vector2D(fMeds,1) ), 
				vgui::Vertex_t( Vector2D(portrait_x,			 extrabar_y2),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints );
		}
	}
}

void CASW_HUD_Marine_Portrait::PaintUsing(float fFraction)
{
	if (!m_hMarineResource.Get() || !m_hMarineResource->GetProfile())
		return;

	bool bVerticalBar = asw_portrait_face.GetBool() || asw_portrait_health_cross.GetBool();

	if ( bVerticalBar )
	{
		if ( m_nVertAmmoBar )
		{
			int portrait_x = PORTRAIT_X;
			int meds_real_top = 3.0f * m_fScale;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int med_x = portrait_x + portrait_size + 6 * m_fScale;		
			int med_width = 9 * m_fScale;		
			float fMeds = (1.0f - fFraction);		
			int meds_bottom = portrait_y + portrait_size;
			if ( !asw_portrait_face.GetBool() ) 
				meds_bottom -= (7 * m_fScale);
			int meds_tall = meds_bottom - meds_real_top;
			int med_top = meds_real_top + (fMeds * meds_tall);

			vgui::surface()->DrawSetColor(Color(255,255,255,255));
			// draw black backs
			vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
			vgui::surface()->DrawTexturedRect(med_x, meds_real_top, med_x + med_width, meds_bottom - meds_real_top);
			
			// bullets vert bar
			vgui::surface()->DrawSetTexture(m_nVertAmmoBar);
			vgui::Vertex_t mpoints[4] = 
			{ 
			vgui::Vertex_t( Vector2D(med_x,				med_top),	Vector2D(0,fMeds) ), 
			vgui::Vertex_t( Vector2D(med_x + med_width, med_top),		Vector2D(1,fMeds) ), 
			vgui::Vertex_t( Vector2D(med_x + med_width, meds_real_top + meds_tall),		Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(med_x,				meds_real_top + meds_tall),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints );

			// using hand		
			vgui::surface()->DrawSetTexture(m_nUsingTexture);
			int hand_top = meds_bottom + 2 * m_fScale;
			int hand_size = med_width;
			vgui::Vertex_t mpoints2[4] = 
			{ 
			vgui::Vertex_t( Vector2D(med_x,				hand_top),	Vector2D(0,0) ), 
			vgui::Vertex_t( Vector2D(med_x + hand_size, hand_top),		Vector2D(1,0) ), 
			vgui::Vertex_t( Vector2D(med_x + hand_size, hand_top + hand_size),		Vector2D(1,1) ), 
			vgui::Vertex_t( Vector2D(med_x,				hand_top + hand_size),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints2 );
		}
	}
	else
	{
		if ( m_nVertAmmoBar )
		{
			int portrait_x = PORTRAIT_X;
			int portrait_y = PORTRAIT_Y;
			int portrait_size = PORTRAIT_SIZE;

			int healthbar_y = portrait_y + (2 * m_fScale);
			if ( asw_portrait_face.GetBool() )
			{
				healthbar_y += portrait_size;
			}
			int healthbar_height = 7 * m_fScale;
			int healthbar_y2 = healthbar_y + healthbar_height;

			int ammobar_y = healthbar_y2 + 3 * m_fScale;
			int ammobar_height = 8 * m_fScale;
			int ammobar_y2 = ammobar_y + ammobar_height;

			int clipsbar_y = ammobar_y2 + 3 * m_fScale;
			int clipsbar_height = 8 * m_fScale;
			int clipsbar_y2 = clipsbar_y + clipsbar_height;

			int extrabar_y = asw_portrait_ammo.GetBool() ? clipsbar_y2 + 3 * m_fScale : healthbar_y2 + 3 * m_fScale;
			int extrabar_height = 8 * m_fScale;
			int extrabar_y2 = extrabar_y + extrabar_height;

			int using_width = fFraction * portrait_size;	

			vgui::surface()->DrawSetTexture(m_nVertAmmoBar);
			vgui::Vertex_t mpoints[4] = 
			{ 
				vgui::Vertex_t( Vector2D(portrait_x,			 extrabar_y),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(portrait_x + using_width, extrabar_y),		Vector2D(fFraction,0) ), 
				vgui::Vertex_t( Vector2D(portrait_x + using_width, extrabar_y2),		Vector2D(fFraction,1) ), 
				vgui::Vertex_t( Vector2D(portrait_x,			 extrabar_y2),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints );

			// using hand		
			vgui::surface()->DrawSetTexture(m_nUsingTexture);
			int hand_size = extrabar_height;
			vgui::Vertex_t mpoints2[4] = 
			{ 
				vgui::Vertex_t( Vector2D(SIDE_ICON_X,				extrabar_y),	Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(SIDE_ICON_X + hand_size, extrabar_y),		Vector2D(1,0) ), 
				vgui::Vertex_t( Vector2D(SIDE_ICON_X + hand_size, extrabar_y + hand_size),		Vector2D(1,1) ), 
				vgui::Vertex_t( Vector2D(SIDE_ICON_X,				extrabar_y + hand_size),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, mpoints2 );
		}
	}
}

void CASW_HUD_Marine_Portrait::PaintClassIcon()
{
	if (!m_hMarineResource.Get() || !m_hMarineResource->GetProfile())
		return;

	if ( !asw_portrait_class.GetBool() )
		return;

	CASW_Marine_Profile *pProfile = m_hMarineResource->GetProfile();
	int ClassTexture = pProfile->m_nClassTextureID;
	//int wide = GetWide();
	if ( ClassTexture != -1 )
	{
		vgui::surface()->DrawSetColor(Color(255,255,255,255));		
		vgui::surface()->DrawSetTexture(ClassTexture);
		vgui::surface()->DrawTexturedRect(LEFT_OFFSET + 3.0f * m_fScale, 5.0f * m_fScale,
						LEFT_OFFSET + 16 * m_fScale, 18 * m_fScale);		
	}
}

void CASW_HUD_Marine_Portrait::PaintPortrait()
{
	if ( !m_hMarineResource.Get() || !m_hMarineResource->GetProfile() )
		return;

	CASW_Marine_Profile *pProfile = m_hMarineResource->GetProfile();
	int PortraitTexture = pProfile->m_nPortraitTextureID;
	//int wide = GetWide();
	if ( PortraitTexture != -1 && m_nBlackBarTexture !=-1 && m_nHorizHealthBar != -1 && m_nWhiteTexture != -1)
	{
		int portrait_x = PORTRAIT_X;
		int portrait_y = PORTRAIT_Y;
		int portrait_size = PORTRAIT_SIZE;		

		// draw black back
		vgui::surface()->DrawSetColor(Color(255,255,255,asw_hud_alpha.GetBool()));
		vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
		if ( asw_portrait_face.GetBool() )
		{
			vgui::surface()->DrawTexturedRect(portrait_x, portrait_y, portrait_x + portrait_size, portrait_y + portrait_size);
		}

		// draw health bar underneath
		vgui::surface()->DrawSetColor(Color(255,255,255,255));
		int bar_y = portrait_y + (2 * m_fScale);
		if ( asw_portrait_face.GetBool() )
		{
			bar_y += portrait_size;
		}
		int bar_height = 7 * m_fScale;
		int bar_y2 = bar_y + bar_height;
		float fHealth = m_hMarineResource->GetHealthPercent();
		if (m_hMarineResource->m_bHealthHalved)		// if he was wounded from the last mission, halve the size of his health bar
		{
			fHealth *= 0.5f;
		}
		int health_pixels = (portrait_size * fHealth);
		
		if (fHealth > 0 && health_pixels <= 0)
			health_pixels = 1;
		int bar_x2 = portrait_x + health_pixels;

		if ( !asw_portrait_health_cross.GetBool() )
		{
			// black part first
			vgui::surface()->DrawTexturedRect(portrait_x, bar_y, portrait_x + portrait_size, bar_y2);

			// if he's wounded, draw a grey part
			if (m_hMarineResource->m_bHealthHalved)
			{
				vgui::surface()->DrawSetTexture(m_nWhiteTexture);
				vgui::surface()->DrawSetColor(Color(128,128,128,255));
				vgui::surface()->DrawTexturedRect(portrait_x + (portrait_size * 0.5f), bar_y, portrait_x + portrait_size, bar_y2);
				vgui::surface()->DrawSetColor(Color(255,255,255,255));
			}

			// red part
			vgui::surface()->DrawSetColor(Color(225,0,0,255));
			vgui::surface()->DrawSetTexture(m_nHorizHealthBar);
			vgui::Vertex_t hpoints[4] = 
			{ 
			vgui::Vertex_t( Vector2D(portrait_x, bar_y),	Vector2D(0,0) ), 
			vgui::Vertex_t( Vector2D(bar_x2, bar_y),		Vector2D(fHealth,0) ), 
			vgui::Vertex_t( Vector2D(bar_x2, bar_y2),		Vector2D(fHealth,1) ), 
			vgui::Vertex_t( Vector2D(portrait_x, bar_y2),	Vector2D(0,1) )
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, hpoints );

			if ( !asw_portrait_face.GetBool() )
			{
				// draw hurt over the top
				if (m_nWhiteTexture!=-1 && m_hMarineResource->GetHurtPulse() > 0)
				{
					vgui::surface()->DrawSetColor(Color(128,0,0,255.0f * m_hMarineResource->GetHurtPulse()));
					vgui::surface()->DrawSetTexture(m_nWhiteTexture);
					vgui::surface()->DrawTexturedPolygon( 4, hpoints );
				}
			}

			// draw a green bit at the end of the health bar if he's infested
			if (m_hMarineResource->IsInfested())
			{
				float fInfestPercent = m_hMarineResource->GetInfestedPercent();
				if (fInfestPercent > fHealth)
					fInfestPercent = fHealth;
				vgui::surface()->DrawSetTexture(m_nWhiteTexture);
				vgui::surface()->DrawSetColor(Color(0,255,0,255));
				int fOffset = (fHealth - fInfestPercent) * portrait_size;			
				vgui::surface()->DrawTexturedRect(portrait_x + fOffset, bar_y, bar_x2, bar_y2);			
				vgui::surface()->DrawSetColor(Color(255,255,255,255));
			}
		}

		// draw portrait
		if (m_hMarineResource->GetHealthPercent() <= 0)
		{
			vgui::surface()->DrawSetColor(Color(80,80,80,255));
		}
		else
		{
			vgui::surface()->DrawSetColor(Color(225,255,255,255));
		}

		if ( asw_portrait_face.GetBool() )
		{
			vgui::surface()->DrawSetTexture(PortraitTexture);		
			
			int black_border = asw_portraits_border.GetInt();
			float black_border_f = black_border / 128.0f;
			black_border *= asw_portraits_border_shrink.GetFloat();
			vgui::Vertex_t points[4] = 
			{ 
			//vgui::Vertex_t( Vector2D(black_border + portrait_x, portrait_y + black_border),											Vector2D(black_border_f,black_border_f) ), 
			//vgui::Vertex_t( Vector2D((portrait_x + portrait_size) - black_border, portrait_y + black_border),						Vector2D(1-black_border_f,black_border_f) ), 
			//vgui::Vertex_t( Vector2D((portrait_x + portrait_size) - black_border, (portrait_y + portrait_size) - black_border),		Vector2D(1-black_border_f,1-black_border_f) ), 
			//vgui::Vertex_t( Vector2D(black_border + portrait_x, (portrait_y + portrait_size) - black_border),						Vector2D(black_border_f,1-black_border_f) ) 
			vgui::Vertex_t( Vector2D(portrait_x, portrait_y),											Vector2D(black_border_f,black_border_f) ), 
			vgui::Vertex_t( Vector2D((portrait_x + portrait_size), portrait_y),						Vector2D(1-black_border_f,black_border_f) ), 
			vgui::Vertex_t( Vector2D((portrait_x + portrait_size), (portrait_y + portrait_size)),		Vector2D(1-black_border_f,1-black_border_f) ), 
			vgui::Vertex_t( Vector2D(portrait_x, (portrait_y + portrait_size)),						Vector2D(black_border_f,1-black_border_f) ) 
			}; 
			vgui::surface()->DrawTexturedPolygon( 4, points );


			// draw hurt over the top
			if (m_nWhiteTexture!=-1 && m_hMarineResource->GetHurtPulse() > 0)
			{
				vgui::surface()->DrawSetColor(Color(128,0,0,255.0f * m_hMarineResource->GetHurtPulse()));
				vgui::surface()->DrawSetTexture(m_nWhiteTexture);
				vgui::surface()->DrawTexturedRect(portrait_x, portrait_y, portrait_x + portrait_size, portrait_y + portrait_size);
			}
		}
	}
}

void CASW_HUD_Marine_Portrait::SetMarineResource(C_ASW_Marine_Resource *pMarineResource, int index, bool bLocalMarine, bool bForcePosition)
{
	if (pMarineResource != m_hMarineResource.Get() || bLocalMarine!=m_bLocalMarine)
	{
		m_hMarineResource = pMarineResource;
		m_iIndex = index;
		m_bLocalMarine = bLocalMarine;
		m_fInfestedFade = 0;

		if (pMarineResource == NULL)
		{
			// hide ourselves
			SetVisible(false);
			m_bMedic = false;
		}
		else
		{
			SetVisible(true);

			CASW_Marine_Profile *pProfile = pMarineResource->GetProfile();
			if (!pProfile)
				return;

			m_bMedic = pProfile->CanUseFirstAid();

			// position ourselves 
			PositionMarinePanel();

			// update labels and images
			wchar_t wszMarineName[ 32 ];
			pMarineResource->GetDisplayName( wszMarineName, sizeof( wszMarineName ) );

			m_pMarineName->SetText( wszMarineName) ;
			m_pMarineNameGlow->SetText( wszMarineName);

			char buffer[16];
			if ( bLocalMarine )
				Q_snprintf( buffer, sizeof(buffer), "%d", index + 1 );
			else
				Q_snprintf( buffer, sizeof(buffer), " " );

			m_pMarineNumber->SetText(buffer);
		}
	}
	else if ( bForcePosition )
	{
		PositionMarinePanel();
	}
}

// find if our marine is using something
float CASW_HUD_Marine_Portrait::GetUsingFraction()
{
	if (!m_hMarineResource.Get() || !m_hMarineResource->GetMarineEntity())
		return 0;

	C_ASW_Marine *pMarine = m_hMarineResource->GetMarineEntity();
	if (!pMarine->m_hUsingEntity.Get())
	{
		if (pMarine->GetMarineResource() && pMarine->GetMarineResource()->m_hWeldingDoor.Get())
		{
			if (pMarine->GetMarineResource()->IsFiring())
				return pMarine->GetMarineResource()->m_hWeldingDoor->GetSealAmount();
		}
		/*
		if (pMarine->GetMarineResource() && pMarine->GetMarineResource()->IsFiring())
		{
			// check if he's firing a welder at a door
			C_ASW_Weapon_Welder *pWelder = dynamic_cast<C_ASW_Weapon_Welder*>(pMarine->GetActiveWeapon());
			if (pWelder)
			{
				// todo: find the door area he's in

			}
		}*/
		return 0;
	}

	IASW_Client_Usable_Entity* pUsable = dynamic_cast<IASW_Client_Usable_Entity*>(pMarine->m_hUsingEntity.Get());
	if (!pUsable)
		return 0;

	ASWUseAction action;
	if (!pUsable->GetUseAction(action, pMarine))
		return 0;

	return action.fProgress;
}

void CASW_HUD_Marine_Portrait::SetScale(float fScale)
{
	fScale = MIN(fScale, asw_hud_scale.GetFloat());
	float fNewScale = (ScreenHeight() / 768.0f) * fScale;
	if (asw_portrait_scale_test.GetBool())
	{
		int mx, my, ux, uy;
		mx = 1;
		my = 1;
		bool bHideCursor = false;
		if (!vgui::surface()->IsCursorVisible())
		{
			vgui::surface()->SetCursor( vgui::dc_arrow );
			bHideCursor = true;
		}
		ASWInput()->GetSimulatedFullscreenMousePos(&mx, &my, &ux, &uy);
		if (bHideCursor)
			vgui::surface()->SetCursor( vgui::dc_none );
		
		fNewScale *= float(mx) / float(ScreenWidth());
	}
	if (fNewScale != m_fScale)
	{
		m_fScale = fNewScale;

		SetFonts();
		InvalidateLayout();
	}
}

static const char* s_FontNames[]={
	"Sansman8",
	"Sansman10",
	"Sansman12",
	"Sansman14",
	"Sansman16",
	"Sansman18",
	"Sansman20"
};

static const char* s_BlurFontNames[]={
	"Sansman8Blur",
	"Sansman10Blur",
	"Sansman12Blur",
	"Sansman14Blur",
	"Sansman16Blur",
	"Sansman18Blur",
	"Sansman20Blur"
};

// finds a medium and a small font
void CASW_HUD_Marine_Portrait::SetFonts()
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	if (!pScheme)
		return;

	// see which is the biggest font that'll fit for the marine's name
	int labelw = 65.0f * m_fScale;
	int labelt = 21.0f * m_fScale;

	// make full use of the space in lower resolutions
	if (ScreenHeight() <= 600)
	{
		labelw = 75.0f * m_fScale;
	}
		
	int num_fonts = NELEMS(s_FontNames);
	vgui::HFont hFont = vgui::INVALID_FONT;
	vgui::HFont hMedFont = vgui::INVALID_FONT;
	vgui::HFont hGlowFont = vgui::INVALID_FONT;
	for (int i=num_fonts-1;i>=0;i--)
	{
		hFont = pScheme->GetFont(s_FontNames[i], false);
		int tw, tt;
		vgui::surface()->GetTextSize(hFont, L"Bastille", tw, tt);

		if (tw < labelw && tt < labelt)
		{
			int iMedFont = MAX(0, i-2);
			hMedFont = pScheme->GetFont(s_FontNames[iMedFont], false);
			hGlowFont = pScheme->GetFont(s_BlurFontNames[i], false);
			break;
		}
	}

	if (hFont != vgui::INVALID_FONT)
	{
		m_pMarineName->SetFont(hFont);
		m_pMarineNumber->SetFont(hFont);
	}

	if (hMedFont != vgui::INVALID_FONT)
	{
		m_pReloadingOverlay->SetFont(hMedFont);
		m_pLowAmmoOverlay->SetFont(hMedFont);
		m_pLowAmmoOverlay2->SetFont(hMedFont);
		m_pNoAmmoOverlay->SetFont(hMedFont);
		m_pNoAmmoOverlay2->SetFont(hMedFont);
	}

	if (hGlowFont != vgui::INVALID_FONT)
	{
		m_pMarineNameGlow->SetFont(hGlowFont);
	}	
}

// position and size panel based on index and if it's showing a local marine or not
void CASW_HUD_Marine_Portrait::PositionMarinePanel()
{
	// JOYPAD REMOVED, NOTE: This joypad code no longer works since panel positioning is done in asw_hud_portraits
	/*
	if (engine->ASW_IsJoypadMode() && asw_cross_layout.GetBool())	// draw icons in a cross shape
	{
		SetScale(0.9f);

		int w = ASW_PORTRAIT_WIDE * m_fScale;	
		int widest = w + (12 * m_fScale);	// widest a portrait can be (gets wider for meds/using)
		int h = ASW_PORTRAIT_TALL * m_fScale;
		int right_border = 10.0f * m_fScale;
		int panel_padding = 11 * m_fScale;
		if (m_bMedic || GetUsingFraction() > 0)	// extra room for medical charges
		{
			w+= (12 * m_fScale);
		}

		// todo: don't put marines into a cross if they're not yours
		if (m_iIndex <= 0)	// left
		{
			SetPos(GetParent()->GetWide() - ((widest + right_border) * 2), right_border + (panel_padding + h));
		}
		else if (m_iIndex == 1)	// top
		{
			SetPos(GetParent()->GetWide() - (widest*1.5f + right_border), right_border);
		}
		else if (m_iIndex == 2)	// right
		{
			SetPos(GetParent()->GetWide() - (widest + right_border), right_border + (panel_padding + h));
		}
		else if (m_iIndex == 3)	// bottom
		{
			SetPos(GetParent()->GetWide() - (widest*1.5f + right_border), right_border + (panel_padding + h) * 2);
		}
		else
		{
			SetPos(GetParent()->GetWide() - (w + right_border), right_border + (panel_padding + h) * (m_iIndex-1));
		}
		SetSize(w, h);
	}
	else
	*/
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
		if ( !pPlayer )
			return;
			
		if ( ASWGameResource() )
		{			
			float fOthersMarineScale = (m_hMarineResource.Get() && m_hMarineResource->GetCommander() == pPlayer) ? 1.0f : 0.8f;			// scale down marines owned by other players
			int iMarines = ASWGameResource()->GetNumMarineResources();
			if (iMarines >= 6)
			{
				SetScale(0.8f * fOthersMarineScale);
			}
			else if (iMarines >= 5)
			{
				SetScale(0.9f * fOthersMarineScale);
			}
			else
			{
				SetScale(1.0f * fOthersMarineScale);
			}
		}

		int w = ASW_PORTRAIT_WIDE * m_fScale;	
		int h = ASW_PORTRAIT_TALL * m_fScale;
		//int right_border = 10.0f * m_fScale;
		//int panel_padding = 11 * m_fScale;

		if ( asw_portrait_face.GetBool() )
		{
			if (m_bMedic || GetUsingFraction() > 0)	// extra room for medical charges
			{
				w+= (12 * m_fScale);
			}

			SetSize(w, h);
			//SetPos(GetParent()->GetWide() - (w + right_border), right_border + (panel_padding + h) * m_iIndex);
		}
		else
		{
			w = 95.0f * m_fScale;
			h = 60.0f * m_fScale;

			if ( !asw_portrait_ammo.GetBool() )
			{
				h = 40.0f * m_fScale;
			}

			if ( asw_portrait_health_cross.GetBool() )
			{
				h += 40.0f * m_fScale;

				if (m_bMedic || GetUsingFraction() > 0)	// extra room for medical charges
				{
					w+= (12 * m_fScale);
				}
			}
			
			if (m_bMedic || GetUsingFraction() > 0 
				|| (m_hPortraitContainer.Get() && m_hPortraitContainer->m_bHorizontalLayout) )	// extra room for medical charges
			{				
				h += (12 * m_fScale);				
			}

			if ( m_hMarineResource.Get() && m_hMarineResource->IsInfested() )
			{
				h += 8.0f * m_fScale;
			}
			
			SetSize(w, h);
			//SetPos(GetParent()->GetWide() - (w + right_border), right_border + (panel_padding + h) * m_iIndex);
		}
		if ( m_hPortraitContainer.Get() )
			m_hPortraitContainer->OnPortraitSizeChanged( this );
	}
}