#include "cbase.h"
#include "hud.h"
#include "asw_hud_objective.h"
#include "iclientmode.h"
#include "clientmode_shared.h"
#include "view.h"
#include "vgui_controls/controls.h"
#include "vgui/ISurface.h"
#include "vgui/Cursor.h"
#include "IVRenderView.h"
#include "iinput.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "c_asw_objective.h"
#include "c_asw_game_resource.h"
#include "asw_gamerules.h"
#include "tier0/vprof.h"
#include <vgui/IVgui.h>
#include "usermessages.h"
#include "c_asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;
extern ConVar asw_hud_alpha;

using namespace vgui;

#define LETTER_INTERVAL 0.05f

DECLARE_HUDELEMENT( CASWHudObjective );


static void MsgFunc_ShowObjectives( bf_read &msg )
{
	CASWHudObjective *pObjectives = GET_HUDELEMENT( CASWHudObjective );
	if ( !pObjectives )
		return;

	pObjectives->ShowObjectives( msg.ReadFloat() );
}

void HudScaleChanged( IConVar *var, char const *pOldString, float flOldValue )
{
	ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
	if ( !mode )
		return;

	mode->ReloadScheme();
	mode->ReloadScheme();
}

ConVar asw_hud_scale("asw_hud_scale", "0.8", FCVAR_NONE, "Scale of the HUD overall", true, 0.7f, true, 1.0f, HudScaleChanged);

CASWHudObjective::CASWHudObjective( const char *pElementName ) :
  CASW_HudElement( pElementName ), BaseClass( NULL, "ASWHudObjective" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);
	
	SetHiddenBits( HIDEHUD_REMOTE_TURRET );

	m_hDefaultFont = INVALID_FONT;
	m_hDefaultGlowFont = INVALID_FONT;
	m_hSmallFont = INVALID_FONT;	
	m_hSmallGlowFont = INVALID_FONT;
	m_font = INVALID_FONT;
	m_iNumLetters = 0;
	m_bShowObjectives = false;
	m_iLastNumLetters = -1;
	m_fNextLetterTime = 0;
	m_iLastHeight = 0;
	m_bPlayMissionCompleteSequence = false;
	m_hObjectiveComplete = NULL;
	m_flCurrentAlpha = 0;
	m_fLastVisibleTime = 0;
	m_flShowObjectivesTime = 0;
	m_bLastSpectating = false;

	m_pHeaderGlowLabel = new vgui::Label(this, "HeaderLabelGlow", "#asw_objectives");
	m_pObjectiveGlowLabel = new vgui::Label(this, "ObjectiveGlowLabel", "");
	m_pHeaderLabel = new vgui::Label(this, "HeaderLabel", "#asw_objectives");
	m_pHeaderGlowLabel->SetContentAlignment(vgui::Label::a_west);
	m_pHeaderLabel->SetContentAlignment(vgui::Label::a_west);
	
	wchar_t *text = L" ";
	
	m_pCompleteLabel = new vgui::Label(this, "ASWHudObjectiveComplete", text);
	m_pCompleteLabel->SetContentAlignment(Label::a_west);
	m_pCompleteLabel->SetMouseInputEnabled(false);	

	m_pCompleteLabelBD = new vgui::Label(this, "ASWHudObjectiveCompleteBD", text);
	m_pCompleteLabelBD->SetContentAlignment(Label::a_west);
	m_pCompleteLabelBD->SetMouseInputEnabled(false);

	
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		m_hObjectives[i] = NULL;
		m_pTickBox[i] = new vgui::ImagePanel(this, "ObjectiveTickBox");
		m_pTickBox[i]->SetVisible(false);
		m_pObjectiveIcon[i] = new vgui::ImagePanel(this, "ObjectiveIcon");
		m_pObjectiveIcon[i]->SetVisible(false);
		m_pObjectiveLabel[i] = new vgui::Label(this, "ObjectiveLabel", "");
		m_pObjectiveLabel[i]->SetVisible(false);
		m_bObjectiveTitleEmpty[i] = true;
	}

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

void CASWHudObjective::Init()
{
	  CASW_HudElement::Init();
	  usermessages->HookMessage( "ShowObjectives", MsgFunc_ShowObjectives );
}

void CASWHudObjective::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetPaintBackgroundEnabled( false );
	
	SetDefaultHUDFonts();
	//m_font = ASW_GetDefaultFont();

	m_pHeaderLabel->SetFont( m_font );
	m_pHeaderLabel->SetFgColor(Color(255,255,255,255));
	m_pHeaderGlowLabel->SetFont( m_hGlowFont );	
	m_pHeaderGlowLabel->SetFgColor(Color(35,214,250,255));
	m_pObjectiveGlowLabel->SetFont( m_hGlowFont );	
	m_pObjectiveGlowLabel->SetFgColor(Color(35,214,250,255));

	m_pCompleteLabel->SetPaintBackgroundEnabled(false);
	m_pCompleteLabel->SetFgColor(Color(255,255,255,255));
	m_pCompleteLabel->SetFont( m_font );
	//m_pCompleteLabel->SetFont(scheme->GetFont("DefaultShadowed"));
	
	m_pCompleteLabelBD->SetPaintBackgroundEnabled(false);
	m_pCompleteLabelBD->SetFgColor(Color(0,0,0,255));
	m_pCompleteLabelBD->SetZPos(-1);
	m_pCompleteLabelBD->SetFont( m_font );
	
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		m_pObjectiveLabel[i]->SetFgColor(Color(255,255,255,255));
		m_pObjectiveLabel[i]->SetFont( m_font );
		m_pTickBox[i]->SetShouldScaleImage(true);
	}

	SetAlpha( m_flCurrentAlpha * 255.0f );
}


void CASWHudObjective::Paint( void )
{
	VPROF_BUDGET( "CASWHudObjective::Paint", VPROF_BUDGETGROUP_ASW_CLIENT );
	// paint black backdrop
	if (m_nBlackBarTexture != -1)
	{
		vgui::surface()->DrawSetColor(Color(0,0,0,asw_hud_alpha.GetInt()));
		vgui::surface()->DrawSetTexture(m_nBlackBarTexture);
		int tw, th, tx, ty;
		m_pHeaderLabel->GetContentSize(tw, th);
		m_pHeaderLabel->GetPos(tx, ty);
		float fScale = ScreenWidth() / 1024.0f;
		int width = 213 * fScale;
		int left_side = tx - 4.0f * fScale;
		int top = ty - 4.0f * fScale;
		int height = th + 8.0f * fScale;
		vgui::Vertex_t points[4] = 
		{ 
		vgui::Vertex_t( Vector2D(left_side, top),	Vector2D(0,0) ), 
		vgui::Vertex_t( Vector2D(left_side + width, top),		Vector2D(1,0) ), 
		vgui::Vertex_t( Vector2D(left_side + width - height, top + height),		Vector2D(1,1) ), 
		vgui::Vertex_t( Vector2D(left_side, top + height),	Vector2D(0,1) )
		}; 
		vgui::surface()->DrawTexturedPolygon( 4, points );
	}

	// update the mission complete sequence if necessary
	if (m_iNumLetters != m_iLastNumLetters)
	{		
		m_iLastNumLetters = m_iNumLetters;
		if (m_iNumLetters > 0)
		{
			int k = MIN(m_iNumLetters, 20);
			char buffer[64];
			Q_strncpy(buffer, "Objective complete! ", k);
			if (m_iNumLetters <= 20)
			{
				buffer[k] = '_';
				buffer[k+1] = '\0';
			}
			else
			{
				buffer[k] = '\0';
			}
			m_pCompleteLabel->SetText(buffer);
			m_pCompleteLabelBD->SetText(buffer);

			// make sure we're positioned on the right objective
			int ox, oy, ow, ot;//, cx, cy;
			// grab the position of the last objective			
			m_pObjectiveLabel[m_iNumObjectivesListed-1]->GetBounds(ox, oy, ow, ot);			
			m_pCompleteLabel->SetPos(ox, oy + ot*1.5f);
			m_pCompleteLabelBD->SetPos(ox + 1, oy + ot*1.5f + 1);

			m_pObjectiveLabel[m_iObjectiveCompleteSequencePos]->GetBounds(ox, oy, ow, ot);
			m_pObjectiveGlowLabel->SetBounds(ox, oy, ow, ot);
		}
		else
		{
			m_pCompleteLabel->SetText(" ");
			m_pCompleteLabelBD->SetText(" ");
		}
	}

	BaseClass::Paint();

	// allow custom painting
	int ob_x = 0;
	int ob_y = 0;
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		if (m_hObjectives[i].Get() != NULL && m_pObjectiveLabel[i])
		{
			m_pObjectiveLabel[i]->GetPos(ob_x, ob_y);
			float f = ob_y;
			m_hObjectives[i]->PaintObjective(f);
		}
	}
}

void CASWHudObjective::UpdateObjectiveList()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;
	int iCurrent = 0;
	bool bNeedsLayout = false;
	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		C_ASW_Objective* pObjective = pGameResource->GetObjective(i);
		bool bUpdatedTitle = false;
		if (pObjective && !pObjective->IsObjectiveDummy() && !pObjective->IsObjectiveHidden())			
		{
			if (m_hObjectives[iCurrent].Get() != pObjective || pObjective->NeedsTitleUpdate()
					|| m_bObjectiveTitleEmpty[iCurrent])
			{
				if (m_hObjectives[iCurrent].Get() != pObjective)
				{
					m_bObjectiveComplete[iCurrent] = pObjective->IsObjectiveComplete();
					if (pObjective->GetObjectiveIconName()[0] != '\0')
					{
						m_pObjectiveIcon[iCurrent]->SetImage(pObjective->GetObjectiveIconName());
						//Msg("Set objective icon %d to %s\n", iCurrent, pObjective->GetObjectiveIconName());
					}
					if (pObjective->IsObjectiveComplete())
						m_pTickBox[iCurrent]->SetImage("swarm/HUD/TickBoxTicked");
					else
						m_pTickBox[iCurrent]->SetImage("swarm/HUD/TickBoxEmpty");
					m_hObjectives[iCurrent] = pObjective;
				}				
				// set the content for this objective
				const wchar_t *pTitle = pObjective->GetObjectiveTitle();
				bUpdatedTitle = true;
				m_bObjectiveTitleEmpty[iCurrent] = (pTitle[0] == '\0');
				m_pObjectiveLabel[iCurrent]->SetText(pTitle);
				if (m_bPlayMissionCompleteSequence && m_hObjectiveComplete.Get() == pObjective)
				{
					m_pObjectiveGlowLabel->SetText(pTitle);
					//Msg("A) Set glow label to %s\n", pTitle);
				}
				
				// make sure they're visible
				if (!m_pObjectiveLabel[iCurrent]->IsVisible())
					m_pObjectiveLabel[iCurrent]->SetVisible(true);
				if (pObjective->GetObjectiveIconName()[0] != '\0' && !m_pObjectiveIcon[iCurrent]->IsVisible())
				{
					//Msg("setting objective icon %d visible\n", iCurrent);
					m_pObjectiveIcon[iCurrent]->SetVisible(true);
				}
				else
				{	
					m_pObjectiveIcon[iCurrent]->SetVisible(false);
				}
				if (!m_pTickBox[iCurrent]->IsVisible())
					m_pTickBox[iCurrent]->SetVisible(true);

				bNeedsLayout = true;
			}

			// already have this objective in our list, but check if it's become complete
			if (m_bObjectiveComplete[iCurrent] != pObjective->IsObjectiveComplete())
			{
				//Msg("Objective %d is now complete!\n", iCurrent);
				m_bObjectiveComplete[iCurrent] = pObjective->IsObjectiveComplete();
				if (pObjective->IsObjectiveComplete())
					m_pTickBox[iCurrent]->SetImage("swarm/HUD/TickBoxTicked");
				else
					m_pTickBox[iCurrent]->SetImage("swarm/HUD/TickBoxEmpty");
				if (ASWGameRules() && ASWGameRules()->GetGameState() == ASW_GS_INGAME)//  && !ASWGameRules()->IsTutorialMap()
				{
					m_iObjectiveCompleteSequencePos = iCurrent;
					m_hObjectiveComplete = pObjective;
					m_bPlayMissionCompleteSequence = true;
					m_iNumLetters = 0;
					GetAnimationController()->RunAnimationCommand(m_pCompleteLabel, "Alpha", 255, 0, 0.1f, AnimationController::INTERPOLATOR_LINEAR);
					GetAnimationController()->RunAnimationCommand(m_pCompleteLabelBD, "Alpha", 255, 0, 0.1f, AnimationController::INTERPOLATOR_LINEAR);
					GetAnimationController()->RunAnimationCommand(m_pObjectiveGlowLabel, "Alpha", 255, 0, 0.1f, AnimationController::INTERPOLATOR_LINEAR);					
					const wchar_t *pTitle = pObjective->GetObjectiveTitle();
					m_pObjectiveGlowLabel->SetText(pTitle);
					//Msg("1) Set glow label to %s\n", pTitle);
				}
			}
			if ( m_hObjectiveComplete.Get() != NULL && m_hObjectiveComplete.Get() == pObjective )
			{
				if ( iCurrent != m_iObjectiveCompleteSequencePos )
				{
					m_iObjectiveCompleteSequencePos = iCurrent;
					bNeedsLayout = true;
				}
			}
			iCurrent++;			
		}
	}
	if ( m_iNumObjectivesListed != iCurrent)
	{
		m_iNumObjectivesListed = iCurrent;
		bNeedsLayout = true;
	}
	
	for (int i=iCurrent;i<ASW_MAX_OBJECTIVES;i++)
	{
		//Msg("Clearing objective slot %d\n", i);
		m_hObjectives[i] = NULL;
		if (m_pObjectiveLabel[i]->IsVisible())
			m_pObjectiveLabel[i]->SetVisible(false);
		if (m_pObjectiveIcon[i]->IsVisible())
			m_pObjectiveIcon[i]->SetVisible(false);
		if (m_pTickBox[i]->IsVisible())
			m_pTickBox[i]->SetVisible(false);
	}
	if ( IsSpectating() != m_bLastSpectating )
	{
		bNeedsLayout = true;
	}

	if ( bNeedsLayout )
	{
		LayoutObjectives();
	}
}

void CASWHudObjective::PerformLayout()
{
	BaseClass::PerformLayout();
	LayoutObjectives();
}

bool CASWHudObjective::IsSpectating()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return false;

	C_ASW_Marine *pSpectating = pPlayer->GetSpectatingMarine();
	return pSpectating != NULL;
}

void CASWHudObjective::LayoutObjectives()
{
	int iCurrent = 0;
	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();
	int icon_size = 16.0f * fScale;
	int tick_size = 16.0f * fScale;
	int border = 14.0f * fScale;
	int label_x = border + tick_size + icon_size;
	int font_tall = vgui::surface()->GetFontTall(m_font);
	int header_y = 14.0f * fScale;
	if ( IsSpectating() )
	{
		header_y += YRES( 32 );
	}

	int header_height = font_tall * 1.2f;
	int row_height = MAX(font_tall, tick_size);
	int label_y = header_y + header_height + 4.0f * fScale;
	int label_width = 256.0f * fScale;

	m_bLastSpectating = IsSpectating();

	m_pHeaderLabel->SetBounds(border, header_y, label_width, header_height);
	m_pHeaderGlowLabel->SetBounds(border, header_y, label_width, header_height);	

	for (int i=0;i<ASW_MAX_OBJECTIVES;i++)
	{
		if (m_hObjectives[i].Get() != NULL)
		{
			m_pObjectiveLabel[i]->SetBounds(label_x, label_y + iCurrent * row_height, label_width, row_height);
			m_pTickBox[i]->SetBounds(border, label_y + iCurrent * row_height, tick_size, tick_size);
			m_pObjectiveIcon[i]->SetBounds(border+tick_size, label_y + iCurrent * row_height, icon_size, icon_size);
			iCurrent++;
		}		
	}

	m_pCompleteLabel->SetWide(GetWide() - 16 * fScale);
	m_pCompleteLabelBD->SetWide(GetWide() - 16 * fScale);

	int ox, oy, ow, ot;
	// grab the position of the last objective
	m_pObjectiveLabel[m_iNumObjectivesListed-1]->GetBounds(ox, oy, ow, ot);			
	m_pCompleteLabel->SetPos(ox, oy + ot*1.5f);
	m_pCompleteLabelBD->SetPos(ox + 1, oy + ot*1.5f + 1);

	if ( m_iObjectiveCompleteSequencePos >= 0 && m_iObjectiveCompleteSequencePos < ASW_MAX_OBJECTIVES && m_hObjectives[m_iObjectiveCompleteSequencePos].Get() != NULL )
	{
		m_pObjectiveLabel[m_iObjectiveCompleteSequencePos]->GetBounds(ox, oy, ow, ot);
		m_pObjectiveGlowLabel->SetBounds(ox, oy, ow, ot);
	}
}

void CASWHudObjective::OnThink()
{

}

void CASWHudObjective::OnTick()
{
	VPROF_BUDGET( "CASWHudObjective::OnTick", VPROF_BUDGETGROUP_ASW_CLIENT );

	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	UpdateObjectiveList();

	if (m_bPlayMissionCompleteSequence)
	{
		//Msg("m_bPlayMissionCompleteSequence is true, num letts is %d\n", m_iNumLetters);
		if (m_iNumLetters == 0)
		{
			// initialise the played complete sequence
			m_fNextLetterTime = gpGlobals->curtime + LETTER_INTERVAL;
			m_iNumLetters = 1;
		}
		// increase the number of visible letters over time
		if (gpGlobals->curtime >= m_fNextLetterTime)
		{
			m_fNextLetterTime = gpGlobals->curtime + LETTER_INTERVAL;
			m_iNumLetters++;
		}
		if (m_iNumLetters >= 29)  // 19 for objective complete! + 20 for delay
		{
			// finished the play complete sequence, we can fade out
			m_bPlayMissionCompleteSequence = false;
			// fade out
			GetAnimationController()->RunAnimationCommand(m_pCompleteLabel, "Alpha", 0, 2.0f, 0.5f, AnimationController::INTERPOLATOR_LINEAR);
			GetAnimationController()->RunAnimationCommand(m_pCompleteLabelBD, "Alpha", 0, 2.0f, 0.5f, AnimationController::INTERPOLATOR_LINEAR);
			GetAnimationController()->RunAnimationCommand(m_pObjectiveGlowLabel, "Alpha", 0, 2.0f, 0.5f, AnimationController::INTERPOLATOR_LINEAR);			
		}
	}

	bool bShowObjectives = ( m_flShowObjectivesTime > gpGlobals->curtime ) || m_bPlayMissionCompleteSequence || (ASWGameRules() && ASWGameRules()->IsTutorialMap());

	if ( m_bShowObjectives != bShowObjectives )
	{
		m_bShowObjectives = bShowObjectives;
	}

	if ( bShowObjectives )
	{
		m_fLastVisibleTime = gpGlobals->curtime;
	}		

	if ( gpGlobals->curtime > (m_fLastVisibleTime + 6.0f) )
	{
		m_flCurrentAlpha = MAX( 0.0f, m_flCurrentAlpha - gpGlobals->frametime );
		SetAlpha( m_flCurrentAlpha * 255.0f );
	}
	else
	{
		m_flCurrentAlpha = MIN( 1.0f, m_flCurrentAlpha + gpGlobals->frametime * 2.0f );
		SetAlpha( m_flCurrentAlpha * 255.0f );
	}
}

void CASWHudObjective::LevelShutdown()
{
	m_iNumLetters = 0;
	m_fNextLetterTime = 0;
	m_bPlayMissionCompleteSequence = false;
	m_pObjectiveGlowLabel->SetAlpha(0);
	m_flShowObjectivesTime = 0;
}

void CASWHudObjective::ShowObjectives( float fDuration )
{
	m_flShowObjectivesTime = gpGlobals->curtime + fDuration;
}

static const char* s_FontNames[]={
	"Sansman8",
	//"Sansman9",
	"Sansman10",
	//"Sansman11",
	"Sansman12",
	//"Sansman13",
	"Sansman14",
	"Sansman16",
	//"Sansman17",
	"Sansman18",
	//"Sansman19",
	"Sansman20"
};

static const char* s_BlurFontNames[]={
	"Sansman8Blur",
	//"Sansman9Blur",
	"Sansman10Blur",
	//"Sansman11Blur",
	"Sansman12Blur",
	//"Sansman13Blur",
	"Sansman14Blur",
	"Sansman16Blur",
	//"Sansman17Blur",
	"Sansman18Blur",
	//"Sansman19Blur",
	"Sansman20Blur"
};

// finds a medium and a small font
void CASWHudObjective::SetDefaultHUDFonts()
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );
	if (!pScheme)
		return;

	float fScale = (ScreenHeight() / 768.0f) * asw_hud_scale.GetFloat();

	// see which is the biggest font that'll fit for the marine's name
	int labelw = 65.0f * fScale;
	int labelt = 21.0f * fScale;

	// make more use of the space at low resolution
	if (ScreenHeight() < 600)
	{
		labelw = 75.0f * fScale;
	}

	m_iLastHeight = ScreenHeight();
	
	int num_fonts = NELEMS(s_FontNames);
	m_hDefaultFont = vgui::INVALID_FONT;
	m_hDefaultGlowFont = vgui::INVALID_FONT;
	m_hSmallFont = vgui::INVALID_FONT;	
	m_hSmallGlowFont = vgui::INVALID_FONT;	
	for (int i=num_fonts-1;i>=0;i--)
	{
		m_hDefaultFont = pScheme->GetFont(s_FontNames[i], false);
		int tw, tt;
		vgui::surface()->GetTextSize(m_hDefaultFont, L"Bastille", tw, tt);

		if (tw < labelw && tt < labelt)
		{
			//Msg("standard hud font is %s\n", s_FontNames[i]);
			int iSmallFont = MAX(0, i-1);
			m_hSmallFont = pScheme->GetFont(s_FontNames[iSmallFont], false);
			m_hDefaultGlowFont = pScheme->GetFont(s_BlurFontNames[i], false);
			m_hSmallGlowFont = pScheme->GetFont(s_BlurFontNames[iSmallFont], false);
			break;
		}
	}
}

C_ASW_Objective* CASWHudObjective::GetObjective( int i )
{
	if ( i < 0 || i >= ASW_MAX_OBJECTIVES )
		return NULL;

	return m_hObjectives[ i ].Get();
}

// ====== Standard fonts =======

vgui::DHANDLE<CASWHudObjective> g_hObjectivesHUD;

vgui::HFont ASW_GetDefaultFont(bool bGlow)
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SwarmSchemeNew" ) );
	if ( !pScheme )
		return vgui::INVALID_FONT;

	return pScheme->GetFont( "Default", true );

	if (!g_hObjectivesHUD.Get())
	{
		g_hObjectivesHUD = GET_HUDELEMENT( CASWHudObjective );
	}

	if (!g_hObjectivesHUD.Get())
	{
		return vgui::INVALID_FONT;
	}

	if (g_hObjectivesHUD->m_iLastHeight != ScreenHeight())
		g_hObjectivesHUD->SetDefaultHUDFonts();

	if (bGlow)
		return g_hObjectivesHUD->m_hDefaultGlowFont;

	return g_hObjectivesHUD->m_hDefaultFont;
}

vgui::HFont ASW_GetSmallFont(bool bGlow)
{
	vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SwarmSchemeNew" ) );
	if ( !pScheme )
		return vgui::INVALID_FONT;

	return pScheme->GetFont( "DefaultSmall", true );

	if (!g_hObjectivesHUD.Get())
	{
		g_hObjectivesHUD = GET_HUDELEMENT( CASWHudObjective );
	}

	if (!g_hObjectivesHUD.Get())
	{
		return vgui::INVALID_FONT;
	}

	if (g_hObjectivesHUD->m_iLastHeight != ScreenHeight())
		g_hObjectivesHUD->SetDefaultHUDFonts();

	if (bGlow)
		return g_hObjectivesHUD->m_hSmallGlowFont;

	return g_hObjectivesHUD->m_hSmallFont;
}