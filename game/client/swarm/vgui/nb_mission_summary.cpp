#include "cbase.h"
#include "nb_mission_summary.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextImage.h"
#include "vgui_bitmapbutton.h"
#include "asw_hud_minimap.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_objective.h"
#include "nb_main_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Mission_Summary::CNB_Mission_Summary( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pBackgroundInner = new vgui::Panel( this, "BackgroundInner" );
	m_pTitleBG = new vgui::Panel( this, "TitleBG" );
	m_pTitleBGBottom = new vgui::Panel( this, "TitleBGBottom" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pDifficultyTitle = new vgui::Label( this, "DifficultyTitle", "" );
	m_pDifficultyLabel = new vgui::Label( this, "DifficultyLabel", "" );
	m_pMissionTitle = new vgui::Label( this, "MissionTitle", "" );
	m_pMissionLabel = new vgui::Label( this, "MissionLabel", "" );
	m_pObjectivesTitle = new vgui::Label( this, "ObjectivesTitle", "" );
	m_pObjectivesLabel = new vgui::Label( this, "ObjectivesLabel", "" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pDetailsButton = new CBitmapButton( this, "DetailsButton", "" );
	m_pDetailsButton->AddActionSignalTarget( this );
	m_pDetailsButton->SetCommand( "MissionDetails" );
}

CNB_Mission_Summary::~CNB_Mission_Summary()
{

}

void CNB_Mission_Summary::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_mission_summary.res" );

	color32 blue;
	blue.r = 83;
	blue.g = 148;
	blue.b = 192;
	blue.a = 255;

	color32 white;
	white.r = 255;
	white.g = 255;
	white.b = 255;
	white.a = 255;
	m_pDetailsButton->SetImage( CBitmapButton::BUTTON_ENABLED, "vgui/swarm/Computer/IconFiles", blue );
	m_pDetailsButton->SetImage( CBitmapButton::BUTTON_ENABLED_MOUSE_OVER, "vgui/swarm/Computer/IconFiles", white );
	m_pDetailsButton->SetImage( CBitmapButton::BUTTON_PRESSED, "vgui/swarm/Computer/IconFiles", white );
}

void CNB_Mission_Summary::PerformLayout()
{
	BaseClass::PerformLayout();

	//m_pObjectivesLabel->GetTextImage()->ResizeImageToContent();
	int nLabelWidth = YRES( 168 );

	m_pObjectivesLabel->SetSize( nLabelWidth, 400 );
	m_pObjectivesLabel->GetTextImage()->SetDrawWidth( nLabelWidth );
	m_pObjectivesLabel->InvalidateLayout( true );
	int texwide, texttall;	
	m_pObjectivesLabel->GetTextImage()->ResizeImageToContentMaxWidth( nLabelWidth );
	m_pObjectivesLabel->GetTextImage()->GetContentSize( texwide, texttall );	
	m_pObjectivesLabel->InvalidateLayout( true );

	int x, y, w, h;
	m_pObjectivesLabel->GetBounds( x, y, w, h );

	h = texttall;

	int dw, dh;
	m_pDetailsButton->GetSize( dw, dh );

	h += dh;

	int border = YRES( 10 );
	SetTall( y + h + border );
	m_pBackground->SetTall( y + h + border );

	int bx, by;
	m_pBackgroundInner->GetPos( bx, by );
	m_pBackgroundInner->SetTall( y + h + border - ( by * 2 ) );

	m_pDetailsButton->SetPos( GetWide() - ( dw + border ), GetTall() - ( dh + border ) );
}

void CNB_Mission_Summary::OnThink()
{
	BaseClass::OnThink();

	if ( !ASWGameResource() )
		return;

	int nSkillLevel = ASWGameRules()->GetSkillLevel();
	const wchar_t *pDifficulty = NULL;
	switch( nSkillLevel )
	{
		case 1: pDifficulty = g_pVGuiLocalize->Find( "#asw_difficulty_easy" ); break;
		default:
		case 2: pDifficulty = g_pVGuiLocalize->Find( "#asw_difficulty_normal" ); break;
		case 3: pDifficulty = g_pVGuiLocalize->Find( "#asw_difficulty_hard" ); break;
		case 4: pDifficulty = g_pVGuiLocalize->Find( "#asw_difficulty_insane" ); break;
		case 5: pDifficulty = g_pVGuiLocalize->Find( "#asw_difficulty_imba" ); break;
	}
	if ( !pDifficulty )
	{
		pDifficulty = L"";
	}

	if ( CAlienSwarm::IsOnslaught() )
	{
		wchar_t wszText[ 128 ];
		_snwprintf( wszText, sizeof( wszText ), L"%s %s", pDifficulty, g_pVGuiLocalize->FindSafe( "#nb_onslaught_title" ) );
		m_pDifficultyLabel->SetText( wszText );
	}
	else
	{
		m_pDifficultyLabel->SetText( pDifficulty );
	}

	CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );
	if ( pMap )
	{
		m_pMissionLabel->SetText(pMap->m_szMissionTitle);
	}

	// compose objectives list
	wchar_t wszObjectivesBuffer[ 1024 ];
	wchar_t wszBuffer[ 1024 ];

	wszObjectivesBuffer[ 0 ] = 0;

	int nObjectives = 0;
	for ( int i = 0 ; i < ASW_MAX_OBJECTIVES; i++ )
	{
		C_ASW_Objective* pObjective = ASWGameResource()->GetObjective(i);
		if ( pObjective && !pObjective->IsObjectiveHidden() && !pObjective->IsObjectiveDummy() )
		{
			if ( nObjectives == 0 )
			{
				_snwprintf( wszObjectivesBuffer, sizeof( wszObjectivesBuffer ), L"- %s", pObjective->GetObjectiveTitle() );
			}
			else
			{
				_snwprintf( wszBuffer, sizeof( wszBuffer ), L"%s\n- %s", wszObjectivesBuffer, pObjective->GetObjectiveTitle() );
				_snwprintf( wszObjectivesBuffer , sizeof( wszObjectivesBuffer ), L"%s", wszBuffer );
			}
			nObjectives++;
		}
	}
	m_pObjectivesLabel->SetText( wszObjectivesBuffer );

	int nLabelWidth = YRES( 168 );
	m_pObjectivesLabel->SetSize( nLabelWidth, 400 );
	m_pObjectivesLabel->GetTextImage()->SetDrawWidth( nLabelWidth );
	m_pObjectivesLabel->InvalidateLayout( true );
	int texwide, texttall;	
	m_pObjectivesLabel->GetTextImage()->ResizeImageToContentMaxWidth( nLabelWidth );
	m_pObjectivesLabel->SetWrap( true );
	m_pObjectivesLabel->GetTextImage()->GetContentSize( texwide, texttall );	
	//m_pObjectivesLabel->SetSize( texwide, texttall );
	m_pObjectivesLabel->InvalidateLayout( true );
	m_pObjectivesLabel->GetTextImage()->GetContentSize( texwide, texttall );
	m_pObjectivesLabel->InvalidateLayout( true );
}



void CNB_Mission_Summary::OnCommand( const char *command )
{
	CNB_Main_Panel *pMainPanel = dynamic_cast<CNB_Main_Panel*>( GetParent() );
	if ( !pMainPanel )
	{
		Warning( "Error, parent of CNB_Mission_Summary is not the main panel\n" );
		return;

	}
	if ( !Q_stricmp( command, "MissionDetails" ) )
	{
		pMainPanel->ShowMissionDetails();
	}
}