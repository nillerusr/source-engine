#include "cbase.h"
#include "vgui_BasePanel.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "vguimatsurface/imatsystemsurface.h"
#include "asw_hud_use_icon.h"
#include "vgui/ilocalize.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_use_area.h"
#include "asw_input.h"
#include "asw_hud_objective.h"
#include "c_asw_weapon.h"
#include "vgui_bindpanel.h"
#include <vgui_controls/TextImage.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_hud_alpha;
extern ConVar asw_hud_scale;

//#define USE_AREA_WIDTH (288.f * fScale)
#define USE_AREA_WIDTH (GetWide())
#define USE_AREA_HEIGHT (160.f * fScale)
#define SCREEN_CENTER_X (ScreenWidth() * 0.5f)
#define SCREEN_LEFT (15.0f * fScale)
#ifdef SHIFT_ICON_WHEN_HACKING
#define ICON_Y (m_bHacking ? 0 : (96.f * fScale) )
#else
#define ICON_Y (96.f * fScale)
#endif

CASW_HUD_Use_Icon::CASW_HUD_Use_Icon(vgui::Panel *pParent, const char *szPanelName) :
	vgui::Panel(pParent, szPanelName)
{
	m_bHasQueued = false;

	m_pUseGlowText = new vgui::Label(this, "UseText", "");
	m_pUseGlowText->SetAlpha(0);
	m_pUseGlowText->SetContentAlignment(vgui::Label::a_center);
	m_pUseText = new vgui::Label(this, "UseText", "");
	m_pUseText->SetAlpha(0);	
	m_pUseText->SetContentAlignment(vgui::Label::a_center);	

	m_pHoldUseGlowText = new vgui::Label(this, "UseText", "");
	m_pHoldUseGlowText->SetAlpha(0);
	m_pHoldUseGlowText->SetContentAlignment(vgui::Label::a_center);
	m_pHoldUseText = new vgui::Label(this, "UseText", "");
	m_pHoldUseText->SetAlpha(0);	
	m_pHoldUseText->SetContentAlignment(vgui::Label::a_center);	
	
	m_pBindPanel = new CBindPanel( this, "BindPanel" );
	m_pBindPanel->SetBind( "+use" );
	m_pBindPanel->SetAlpha(0);
	
	m_iImageW = m_iImageT = 10;
	m_iImageX = 0;
	m_iImageY = 0;
	m_szUseKeyText[0] = '\0';

	m_CurrentAction.iUseIconTexture = -1;

	FindUseKeyBind();
}

void CASW_HUD_Use_Icon::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pUseText->SetFont( m_hFont );
	m_pUseText->SetFgColor(Color(255,255,255,255));
	m_pUseGlowText->SetFont( m_hGlowFont );
	m_pUseGlowText->SetFgColor(Color(35,214,250,255));

	m_pHoldUseText->SetFont( m_hFont );
	m_pHoldUseText->SetFgColor(Color(255,255,255,255));
	m_pHoldUseGlowText->SetFont( m_hGlowFont );
	m_pHoldUseGlowText->SetFgColor(Color(35,214,250,255));

	m_pBindPanel->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pBindPanel->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pBindPanel->SetAlpha(0);
}

void CASW_HUD_Use_Icon::PerformLayout()
{
	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();	
	// position ourself in the middle lower part of the screen	
	if (!GetParent())
		return;
	SetBounds(0, ICON_Y, ScreenWidth(), GetParent()->GetTall());

	m_pBindPanel->InvalidateLayout( true );
	int bw, bt;
	m_pBindPanel->GetSize( bw, bt );
		
	// size our icon
	int border = 7.0f * fScale;		
	int border_thin = 3.5f * fScale;
	m_iImageW = 75.0f * fScale;
	m_iImageT = 75.0f * fScale;

	m_iImageY = border;
	int tx, ty;
	m_pUseText->InvalidateLayout( true );
	m_pUseText->GetTextImage()->GetContentSize(tx, ty);
	int htx, hty;
	m_pHoldUseText->GetContentSize(htx, hty);

	int text_x = 0;
#ifdef SHIFT_ICON_WHEN_HACKING
	if ( m_bHacking )
	{
		m_iImageX = border + SCREEN_LEFT;
		text_x = border + SCREEN_LEFT;

		m_pUseText->SetContentAlignment(vgui::Label::a_west);
		m_pUseGlowText->SetContentAlignment(vgui::Label::a_west);
		m_pHoldUseText->SetContentAlignment(vgui::Label::a_west);
		m_pHoldUseGlowText->SetContentAlignment(vgui::Label::a_west);
	}
	else
#endif
	{
		m_iImageX = SCREEN_CENTER_X - (m_iImageW * 0.5f);
		text_x = SCREEN_CENTER_X - USE_AREA_WIDTH * 0.5f;

		m_pUseText->SetContentAlignment(vgui::Label::a_center);
		m_pUseGlowText->SetContentAlignment(vgui::Label::a_center);
		m_pHoldUseText->SetContentAlignment(vgui::Label::a_center);
		m_pHoldUseGlowText->SetContentAlignment(vgui::Label::a_center);
	}

	int text_y = m_iImageY + m_iImageT + border;
	int hold_text_x = text_x;
	if ( m_CurrentAction.bShowUseKey )
	{
		text_x += bw * 0.5f;
		text_y += bt * 0.33f;
	}
	SetUseTextBounds( text_x, text_y, USE_AREA_WIDTH, ty + border_thin * 2 );
	SetHoldTextBounds( hold_text_x, text_y + ty + border, USE_AREA_WIDTH, hty + border_thin * 2 );

	if ( m_CurrentAction.bWideIcon )
	{
		m_iImageX -= m_iImageW * 0.5f;
		m_iImageW *= 2;
	}

	int x, y, w, t;
	m_pUseText->GetBounds( x, y, w, t );
	int bind_x = ( x + w * 0.5f ) - ( tx * 0.5f );

	m_pBindPanel->SetPos( bind_x - bw, y + t * 0.5f - bt * 0.5f );
}

void CASW_HUD_Use_Icon::Paint()
{
	if (m_CurrentAction.iUseIconTexture != -1 && m_pUseText->GetAlpha() > 0)
	{
		int tx, ty, tw, th, cw, ch;
		m_pUseText->GetBounds( tx, ty, tw, th );
		m_pUseText->GetContentSize( cw, ch );
		tx = tx + tw * 0.5f - cw * 0.5f;

		int border = YRES( 2 );
		int box_x = tx - border;
		int box_y = ty;
		int box_w = cw + border * 2;
		int box_h = ch + border * 2;

		int kx, ky, kw, kh;
		kx = ky = kw = kh = 0;
		if ( m_CurrentAction.bShowUseKey )
		{
			m_pBindPanel->GetBounds( kx, ky, kw, kh );

			box_x = kx - border;
			box_y = ky - border;
			box_h = kh + border * 2;
			box_w += ( tx - kx );
		}

		if ( m_CurrentAction.wszHoldButtonText[0] )	// extend box width to fit in hold text, if any
		{
			int hcw, hch;
			m_pHoldUseText->GetContentSize( hcw, hch );
			int extra_width = hcw - cw;
			extra_width += border;
			if ( m_CurrentAction.bShowUseKey )
			{
				extra_width -= ( tx - kx );
			}
			if ( extra_width > 0 )
			{
				box_x -= extra_width * 0.5f;
				box_w += extra_width;
			}
		}

		float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();
		int border_thin = 3.5f * fScale;
		if ( m_CurrentAction.fProgress > 0 )
		{
			box_h += ch * 0.5f + border_thin;
		}
		if ( m_CurrentAction.wszHoldButtonText[0] )
		{
			box_h += ch;
		}

		float flAlpha = ( m_pUseText->GetAlpha() / 255.0f );

		// draw background behind text + use key
		DrawBox( box_x, box_y, box_w, box_h, Color( 32, 57, 82, 192 ), flAlpha, false );

		// draw background behind use icon
		box_x = m_iImageX - border;
		box_y = m_iImageY - border;
		box_w = m_iImageW + border * 2;
		box_h = m_iImageT + border * 2;
		DrawBox( box_x, box_y, box_w, box_h, Color( 32, 57, 82, 192 ), flAlpha, false );

		// paint the backdrop
		if (m_nBlackBarTexture != -1)
		{			
			int bgalpha = m_pUseText->GetAlpha() * (asw_hud_alpha.GetFloat() / 255.0f);
			vgui::surface()->DrawSetColor(Color(255,
												255,
												255,
												bgalpha));			
			int border = 7.0f * fScale;

			int backdrop_width = cw + border * 2;			
			int backdrop_x = SCREEN_CENTER_X - (backdrop_width * 0.5f);
#ifdef SHIFT_ICON_WHEN_HACKING
			if (m_bHacking)
				backdrop_x = SCREEN_LEFT;
#endif

			// paint progress bar
			if ( m_CurrentAction.fProgress > 0 && m_nProgressBar != -1 && m_nProgressBarBG !=-1 )
			{
				vgui::surface()->DrawSetColor(Color(255,255,255,m_pUseText->GetAlpha()));
				vgui::surface()->DrawSetTexture(m_nProgressBarBG);
				int bar_left = backdrop_x + border;
				int bar_width = (backdrop_width - border * 2.0f);
				int bar_top = ty + ch + YRES( 1 );
				if ( m_CurrentAction.bShowUseKey )
				{
					bar_top = ky + kh - YRES( 2 );
				}
				int bar_height = ch;
				vgui::Vertex_t ppointsbg[4] = 
				{ 
				vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
				vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(1,0) ), 
				vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(1,1) ),
				vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
				};
				vgui::surface()->DrawTexturedPolygon( 4, ppointsbg );

				bar_width = (backdrop_width - border * 2.0f) * m_CurrentAction.fProgress;

				vgui::surface()->DrawSetTexture(m_nProgressBar);
				vgui::Vertex_t ppoints[4] = 
				{ 
					vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
					vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(m_CurrentAction.fProgress,0) ), 
					vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(m_CurrentAction.fProgress,1) ),
					vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
				};
				vgui::surface()->DrawTexturedPolygon( 4, ppoints );
			}
		}

		vgui::surface()->DrawSetColor(Color(m_CurrentAction.UseIconRed,
											m_CurrentAction.UseIconGreen,
											m_CurrentAction.UseIconBlue,
											m_pUseText->GetAlpha()));		

		// paint the icon		
		vgui::surface()->DrawSetTexture(m_CurrentAction.iUseIconTexture);
		vgui::Vertex_t points[4] = 
		{ 
		vgui::Vertex_t( Vector2D(m_iImageX, m_iImageY),										Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(m_iImageX + m_iImageW, m_iImageY),									Vector2D(1,0) ), 
		vgui::Vertex_t( Vector2D(m_iImageX + m_iImageW, m_iImageY + m_iImageT),		Vector2D(1,1) ),
		vgui::Vertex_t( Vector2D(m_iImageX, m_iImageY + m_iImageT),		Vector2D(0,1) )
		};
		vgui::surface()->DrawTexturedPolygon( 4, points );
	}
}

void CASW_HUD_Use_Icon::PositionIcon()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	bool bHacking = false;
	if (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->m_hUsingEntity.Get())
	{
		C_ASW_Use_Area *pArea = dynamic_cast<C_ASW_Use_Area*>(pPlayer->GetMarine()->m_hUsingEntity.Get());
		if (pArea)
			bHacking = true;
	}
	InvalidateLayout( true );
	if (bHacking != m_bHacking)
	{
		m_bHacking = bHacking;
#ifdef SHIFT_ICON_WHEN_HACKING
		// we skip fade animation when we move places
		if (m_bHasQueued)
		{
			FadeOut( 0.0f );
		}
#endif
	}
}

void CASW_HUD_Use_Icon::SetCurrentToQueuedAction()
{
	// can make the queued our current and fade in
	m_CurrentAction = m_QueuedAction;
	FindUseKeyBind();

	PositionIcon();

	m_bHasQueued = false;

	if (m_QueuedAction.UseTarget == NULL)
	{
		// nothing to fade in
		m_CurrentAction.UseTarget = NULL;
	}
	else
	{
		// copy the found key into wchar_t format (localize it if it's a token rather than a normal keyname)
		wchar_t keybuffer[24];
		if (m_szUseKeyText[0] == '#')
		{
			const wchar_t *pLocal = g_pVGuiLocalize->Find(m_szUseKeyText);
			if (pLocal)
				wcsncpy(keybuffer, pLocal, 24);
			else
				g_pVGuiLocalize->ConvertANSIToUnicode(m_szUseKeyText, keybuffer, sizeof(keybuffer));
		}
		else
			g_pVGuiLocalize->ConvertANSIToUnicode(m_szUseKeyText, keybuffer, sizeof(keybuffer));

		if (m_CurrentAction.wszText[0])
		{
			if (m_CurrentAction.bShowUseKey)
			{
				m_pUseText->SetText( m_CurrentAction.wszText );
				m_pUseGlowText->SetText( m_CurrentAction.wszText );
			}
			else
			{
				m_pUseText->SetText(m_CurrentAction.wszText);
				m_pUseGlowText->SetText(m_CurrentAction.wszText);
			}
		}
		else
		{
			m_pUseText->SetText("?");
			m_pUseGlowText->SetText("?");
		}

		// set text colors
		m_pUseText->SetFgColor(Color(m_CurrentAction.TextRed,m_CurrentAction.TextGreen,m_CurrentAction.TextBlue,255));	
		if (!m_CurrentAction.bTextGlow)
			m_pHoldUseGlowText->SetFgColor(Color(0,0,0,0));
		else
			m_pHoldUseGlowText->SetFgColor(Color(	35.0f * (m_CurrentAction.TextRed / 255.0f),
			214.0f * (m_CurrentAction.TextGreen / 255.0f),
			250.0f * (m_CurrentAction.TextBlue / 255.0f),
			255));

		// hold use key text
		if (m_CurrentAction.wszHoldButtonText[0])
		{
			m_pHoldUseText->SetText(m_CurrentAction.wszHoldButtonText);
			m_pHoldUseGlowText->SetText(m_CurrentAction.wszHoldButtonText);
		}
		else
		{
			m_pHoldUseText->SetText("");
			m_pHoldUseGlowText->SetText("");
		}

		// set text colors
		m_pHoldUseText->SetFgColor(Color(m_CurrentAction.TextRed,m_CurrentAction.TextGreen,m_CurrentAction.TextBlue,255));	
		if (!m_CurrentAction.bTextGlow)
			m_pHoldUseGlowText->SetFgColor(Color(0,0,0,0));
		else
			m_pHoldUseGlowText->SetFgColor(Color(	35.0f * (m_CurrentAction.TextRed / 255.0f),
			214.0f * (m_CurrentAction.TextGreen / 255.0f),
			250.0f * (m_CurrentAction.TextBlue / 255.0f),
			255));
	}
}

void CASW_HUD_Use_Icon::OnThink()
{
	if (m_bHasQueued)
	{
		if (m_pUseText->GetAlpha() <= 0)
		{
			SetCurrentToQueuedAction();

			if ( m_QueuedAction.UseTarget != NULL )
			{
				FadeIn( 0.15f );
			}
		}
	}
	m_pBindPanel->SetVisible( m_CurrentAction.bShowUseKey );
}

void CASW_HUD_Use_Icon::ClearUseAction()
{
	ASWUseAction empty;
	SetUseAction(empty);
}

void CASW_HUD_Use_Icon::SetUseAction(ASWUseAction &action)
{	
	// don't change if we're already changing to this action, or we're already steadily showing it
	if ((m_bHasQueued && m_QueuedAction == action) || (!m_bHasQueued && m_CurrentAction == action))
		return;

	// if the only thing that changed is the progress then instantly copy that and don't bother with the fade/queuing
	if (m_bHasQueued && m_QueuedAction.RoughlyEqual(action))
	{
		//m_QueuedAction.fProgress = action.fProgress;
		m_QueuedAction = action;
		return;
	}
		
	if (!m_bHasQueued && m_CurrentAction.RoughlyEqual(action))
	{
		//m_CurrentAction.fProgress = action.fProgress;
		m_CurrentAction = action;
		FindUseKeyBind();
		return;
	}

	m_QueuedAction = action;
	m_bHasQueued = true;

	if ( action.bNoFadeIfSameUseTarget && action.UseTarget == m_CurrentAction.UseTarget )
	{
		SetCurrentToQueuedAction();
		return;
	}

	// fade out our labels - the queued action will get copied over to current when the fade completes in OnThink
	if (m_pUseText->GetAlpha() > 0)
	{
		float fDuration = m_QueuedAction.UseTarget == NULL ? 0.4f : 0.15f;
		FadeOut( fDuration );
	}
}

void CASW_HUD_Use_Icon::FindUseKeyBind()
{
	char szOldKey[12];
	Q_snprintf(szOldKey, sizeof(szOldKey), "%s", m_szUseKeyText);

	char *pchCommand = "+use";
	if ( m_CurrentAction.szCommand[ 0 ] )
	{
		pchCommand = m_CurrentAction.szCommand;
	}

	Q_snprintf(m_szUseKeyText, sizeof(m_szUseKeyText), "%s", ASW_FindKeyBoundTo( pchCommand ));
	Q_strupr(m_szUseKeyText);
	if ( Q_stricmp( m_szUseKeyText, szOldKey) )
	{
		ClearUseAction();
		m_pBindPanel->SetBind( pchCommand );
	}
}

void CASW_HUD_Use_Icon::SetUseTextBounds( int x, int y, int w, int t )
{
	m_pUseText->SetBounds( x, y, w, t );
	m_pUseGlowText->SetBounds( x, y, w, t );
}

void CASW_HUD_Use_Icon::SetHoldTextBounds( int x, int y, int w, int t )
{
	m_pHoldUseText->SetBounds( x, y, w, t );
	m_pHoldUseGlowText->SetBounds( x, y, w, t );
}

void CASW_HUD_Use_Icon::FadeOut( float fDuration )
{
	if ( fDuration <= 0.0f )		// invis instantly
	{
		m_pUseText->SetAlpha(0);
		m_pUseGlowText->SetAlpha(0);
		m_pBindPanel->SetAlpha(0);

		m_pHoldUseText->SetAlpha(0);
		m_pHoldUseGlowText->SetAlpha(0);
		return;
	}
	vgui::GetAnimationController()->RunAnimationCommand(m_pUseText, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pUseGlowText, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pBindPanel, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);		

	vgui::GetAnimationController()->RunAnimationCommand(m_pHoldUseText, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pHoldUseGlowText, "alpha", 0, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void CASW_HUD_Use_Icon::FadeIn( float fDuration )
{
	InvalidateLayout( true );

	m_pUseText->SetAlpha(1);
	m_pUseGlowText->SetAlpha(1);

	vgui::GetAnimationController()->RunAnimationCommand(m_pUseText, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pUseGlowText, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	if ( m_CurrentAction.bShowUseKey )
	{
		vgui::GetAnimationController()->RunAnimationCommand(m_pBindPanel, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	}

	m_pHoldUseText->SetAlpha(1);
	m_pHoldUseGlowText->SetAlpha(1);

	vgui::GetAnimationController()->RunAnimationCommand(m_pHoldUseText, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pHoldUseGlowText, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

// custom paint panel calls into this to draw a progress bar above the use icon
void CASW_HUD_Use_Icon::CustomPaintProgressBar( int ix, int iy, float flAlpha, float flProgress, const char *szCountText, vgui::HFont nFont, Color textCol, const char *szText )
{
	// draw background behind use icon
	int border = YRES( 2 );
	int box_w = m_iImageW + border * 2;
	int box_h = m_iImageT + border * 2;

	int nFontTall = vgui::surface()->GetFontTall( nFont );
	box_h = nFontTall + border * 2;

	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();
	int border_thin = 3.5f * fScale;

	if ( flProgress != -1 )	// drawing a progress bar too?
	{
		box_h += nFontTall * 0.5f + border_thin;
	}

	int box_x = ix - border;
	int box_y = iy - box_h - border;

	DrawBox( box_x, box_y, box_w, box_h, Color( 32, 57, 82, 192 ), flAlpha, false );

	if ( flProgress != -1 )	// drawing a progress bar too?
	{
		vgui::surface()->DrawSetColor(Color(255,255,255,m_pUseText->GetAlpha()));
		vgui::surface()->DrawSetTexture(m_nProgressBarBG);
		int bar_left = ix + border;
		int bar_width = (m_iImageW - border * 2.0f);
		int bar_top = box_y + border + nFontTall;
		int bar_height = nFontTall;
		vgui::Vertex_t ppointsbg[4] = 
		{ 
			vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
			vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(1,0) ), 
			vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(1,1) ),
			vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
		};
		vgui::surface()->DrawTexturedPolygon( 4, ppointsbg );

		bar_width = (m_iImageW - border * 2.0f) * flProgress;

		vgui::surface()->DrawSetTexture(m_nProgressBar);
		vgui::Vertex_t ppoints[4] = 
		{ 
			vgui::Vertex_t( Vector2D(bar_left,				bar_top),					Vector2D(0,0) ), 
			vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top),					Vector2D(flProgress,0) ), 
			vgui::Vertex_t( Vector2D(bar_left + bar_width,	bar_top + bar_height),		Vector2D(flProgress,1) ),
			vgui::Vertex_t( Vector2D(bar_left,				bar_top + bar_height),		Vector2D(0,1) )
		};
		vgui::surface()->DrawTexturedPolygon( 4, ppoints );
	}

	if ( nFont != -1 && szText && szCountText )
	{
		wchar_t wszAmmoPercent[ 64 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( szCountText, wszAmmoPercent, sizeof( wszAmmoPercent ) );
		const wchar_t *pwchAmmoDropLabel = g_pVGuiLocalize->Find( szText );

		vgui::surface()->DrawSetTextFont( nFont );

		int text_x = box_x + border;
		int text_y = box_y + border;

		// drop shadow
		vgui::surface()->DrawSetTextColor( 0, 0, 0, flAlpha * 255 );
		vgui::surface()->DrawSetTextPos( text_x+1, text_y+1 );
		vgui::surface()->DrawPrintText( pwchAmmoDropLabel, V_wcslen( pwchAmmoDropLabel ) );

		int label_wide, label_tall;
		vgui::surface()->GetTextSize( nFont, pwchAmmoDropLabel, label_wide, label_tall );

		vgui::surface()->DrawSetTextPos( text_x+1+label_wide, text_y+1 );
		vgui::surface()->DrawPrintText( wszAmmoPercent, V_wcslen( wszAmmoPercent ) );

		// actual text
		vgui::surface()->DrawSetTextColor( textCol.r(), textCol.g(), textCol.b(), flAlpha * 255 );
		vgui::surface()->DrawSetTextPos( text_x, text_y );
		vgui::surface()->DrawPrintText( pwchAmmoDropLabel, V_wcslen( pwchAmmoDropLabel ) );

		vgui::surface()->DrawSetTextPos( text_x+label_wide, text_y );
		vgui::surface()->DrawPrintText( wszAmmoPercent, V_wcslen( wszAmmoPercent ) );
	}
}