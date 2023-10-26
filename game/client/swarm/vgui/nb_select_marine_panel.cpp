#include "cbase.h"
#include "nb_select_marine_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "nb_skill_panel.h"
#include "vgui_controls/Label.h"
#include "asw_marine_profile.h"
#include "nb_select_marine_entry.h"
#include "asw_briefing.h"
#include "vgui_bitmapbutton.h"
#include "nb_main_panel.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include <vgui/ILocalize.h>
#include "asw_gamerules.h"
#include "c_asw_campaign_save.h"
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Select_Marine_Panel::CNB_Select_Marine_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pMarineNameLabel = new vgui::Label( this, "MarineNameLabel", "" );
	m_pBioTitle = new vgui::Label( this, "BioTitle", "" );
	m_pBioLabel = new vgui::Label( this, "BioLabel", "" );	
	m_pSkillPanel0 = new CNB_Skill_Panel( this, "SkillPanel0" );
	m_pSkillPanel1 = new CNB_Skill_Panel( this, "SkillPanel1" );
	m_pSkillPanel2 = new CNB_Skill_Panel( this, "SkillPanel2" );
	m_pSkillPanel3 = new CNB_Skill_Panel( this, "SkillPanel3" );
	m_pSkillPanel4 = new CNB_Skill_Panel( this, "SkillPanel4" );
	m_pPortraitsBackground = new vgui::ImagePanel( this, "PortraitsBackground" );
	m_pPortraitsLowerBackground = new vgui::ImagePanel( this, "PortraitsLowerBackground" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pAcceptButton = new CNB_Button( this, "AcceptButton", "", this, "AcceptButton" );
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );
	m_pSkillPointsLabel = new vgui::Label( this, "SkillPointsLabel", "" );
	m_pSkillPointsLabel->SetVisible( false );

	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_BLUE );

	m_nHighlightedEntry = -1;
	m_nInitialProfileIndex = -1;
	m_nPreferredLobbySlot = -1;
}

CNB_Select_Marine_Panel::~CNB_Select_Marine_Panel()
{

}

void CNB_Select_Marine_Panel::InitMarineList()
{
	// create entry panels for our horizontal list
	if ( !MarineProfileList() )
	{
		Warning( "Failed to get marine profile list\n" );
		MarkForDeletion();
		return;
	}

	int nProfiles = MarineProfileList()->m_NumProfiles;
	for ( int i = 0; i < nProfiles; i++ )
	{
		CNB_Select_Marine_Entry *pEntry = new CNB_Select_Marine_Entry( this, "Entry" );

		//Assert( NELEMS( s_nProfileOrder ) == nProfiles );		// if this assert fails, then s_nProfileOrder needs to be updated to match the new profile list
		int nProfileIndex = i;//s_nProfileOrder[ i ];
		vgui::PHandle handle;
		handle = pEntry;
		pEntry->SetProfileIndex( nProfileIndex );
		pEntry->m_nPreferredLobbySlot = m_nPreferredLobbySlot;
		pEntry->InvalidateLayout( true, true );
		m_Entries.AddToTail( handle );
	}
	m_nHighlightedEntry = m_nInitialProfileIndex;
	if ( m_nHighlightedEntry == -1 )
		m_nHighlightedEntry = 0;
	InvalidateLayout( true, true );
}

void CNB_Select_Marine_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_select_marine_panel.res" );

	//m_pHorizList->m_pBackgroundImage->SetImage( "briefing/select_marine_list_bg" );
	//m_pHorizList->m_pForegroundImage->SetImage( "briefing/horiz_list_fg" );
}

void CNB_Select_Marine_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	int titlex, titley, titlew, titleh;
	m_pTitle->GetBounds( titlex, titley, titlew, titleh );

	// stack up children side by side
	int curx = titlex;
	int cury = YRES( 25 ); //titley + titleh + YRES( 6 );
	int padding = 0;
	int maxh = 0;
	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		m_Entries[i]->SetPos( curx, cury );
		m_Entries[i]->InvalidateLayout( true );

		int w, h;
		m_Entries[i]->GetSize( w, h );
		maxh = MAX( maxh, h );

		curx += w + padding;
		if ( i == 3 )
		{
			curx = titlex;
			cury += YRES( 131 ); //h;// + YRES( 6 );
		}
	}
	//SetSize( curx - padding, maxh );		// stretch container to fit all children

	// set our height from the tallest entry
	//int sw, sh;
	//GetSize( sw, sh );
	//SetSize( sw, maxh );
	//m_pBackgroundImage->SetSize( sw, maxh );
	//m_pForegroundImage->SetSize( sw, maxh );
}

void CNB_Select_Marine_Panel::OnThink()
{
	BaseClass::OnThink();
	
	for ( int i = 0; i < m_Entries.Count(); i++ )
	{
		if ( m_Entries[i]->IsCursorOver() )
		{
			vgui::Panel *pPanel = m_Entries[i];
			CNB_Select_Marine_Entry *pCursorOver = dynamic_cast<CNB_Select_Marine_Entry*>( pPanel );
			if ( pCursorOver && pCursorOver->m_pPortraitImage->IsCursorOver() )
			{
				SetHighlight( i );
				break;
			}
		}
	}

	bool bShowSpareSkillPoints = false;
	CNB_Select_Marine_Entry *pHighlighted = dynamic_cast<CNB_Select_Marine_Entry*>( GetHighlightedEntry() );
	if ( pHighlighted )
	{
		int nProfileIndex = pHighlighted->GetProfileIndex();
		CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( nProfileIndex );
		if ( pProfile )
		{
			m_pMarineNameLabel->SetText( pProfile->GetShortName() );
			m_pBioLabel->SetText( pProfile->m_Bio );
			m_pSkillPanel0->SetSkillDetails( nProfileIndex, 0, Briefing()->GetProfileSkillPoints( nProfileIndex, 0 ) );
			m_pSkillPanel1->SetSkillDetails( nProfileIndex, 1, Briefing()->GetProfileSkillPoints( nProfileIndex, 1 ) );
			m_pSkillPanel2->SetSkillDetails( nProfileIndex, 2, Briefing()->GetProfileSkillPoints( nProfileIndex, 2 ) );
			m_pSkillPanel3->SetSkillDetails( nProfileIndex, 3, Briefing()->GetProfileSkillPoints( nProfileIndex, 3 ) );
			m_pSkillPanel4->SetSkillDetails( nProfileIndex, 4, Briefing()->GetProfileSkillPoints( nProfileIndex, 4 ) );

			if ( ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave() && !ASWGameRules()->GetCampaignSave()->UsingFixedSkillPoints() )
			{
				int nSkillPoints = Briefing()->GetProfileSkillPoints( nProfileIndex, ASW_SKILL_SLOT_SPARE );
				if ( nSkillPoints > 0 )
				{
					wchar_t buffer[128];
					char sparebuffer[8];
					Q_snprintf(sparebuffer, sizeof(sparebuffer), "%d", nSkillPoints);
					wchar_t wsparebuffer[8];
					g_pVGuiLocalize->ConvertANSIToUnicode(sparebuffer, wsparebuffer, sizeof( wsparebuffer ));

					g_pVGuiLocalize->ConstructString( buffer, sizeof(buffer),
						g_pVGuiLocalize->Find("#asw_unspent_points"), 1,
						wsparebuffer);
					
					m_pSkillPointsLabel->SetText( buffer );
					bShowSpareSkillPoints = true;
				}
			}
		}

		m_pAcceptButton->SetEnabled( !Briefing()->IsProfileSelectedBySomeoneElse( nProfileIndex ) );
	}
	else
	{
		
	}

	m_pSkillPointsLabel->SetVisible( bShowSpareSkillPoints );
}

vgui::Panel* CNB_Select_Marine_Panel::GetHighlightedEntry()
{
	if ( m_nHighlightedEntry < 0 || m_nHighlightedEntry >= m_Entries.Count() )
		return NULL;

	return m_Entries[ m_nHighlightedEntry ].Get();
}

void CNB_Select_Marine_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		Briefing()->SetChangingWeaponSlot( 0 );
		MarkForDeletion();

		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, -1, "ASWComputer.MenuBack" );

		return;
	}
	else if ( !Q_stricmp( command, "AcceptButton" ) )
	{
		CNB_Select_Marine_Entry *pHighlighted = dynamic_cast<CNB_Select_Marine_Entry*>( GetHighlightedEntry() );
		if ( pHighlighted )
		{
			int nProfileIndex = pHighlighted->GetProfileIndex();
			if ( !Briefing()->IsProfileSelectedBySomeoneElse( nProfileIndex ) )
			{
				Briefing()->SelectMarine( 0, nProfileIndex, m_nPreferredLobbySlot );

				// is this the first marine we've selected?
				if ( Briefing()->IsOfflineGame() && ASWGameResource() && ASWGameResource()->GetNumMarines( NULL ) <= 0 )
				{
					Briefing()->AutoSelectFullSquadForSingleplayer( nProfileIndex );
				}

				bool bHasPointsToSpend = Briefing()->IsCampaignGame() && !Briefing()->UsingFixedSkillPoints() && ( Briefing()->GetProfileSkillPoints( nProfileIndex, ASW_SKILL_SLOT_SPARE ) > 0 );

				if ( bHasPointsToSpend )
				{
					CNB_Main_Panel *pPanel = dynamic_cast<CNB_Main_Panel*>( GetParent() );
					if ( pPanel )
					{
						pPanel->SpendSkillPointsOnMarine( nProfileIndex );
					}
				}
				else
				{
					MarkForDeletion();
					Briefing()->SetChangingWeaponSlot( 0 );
				}
			}
			else
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1, "ASWComputer.TimeOut" );
			}
		}		
		return;
	}
	BaseClass::OnCommand( command );
}

void CNB_Select_Marine_Panel::SetHighlight( int nEntryIndex )
{
	if ( nEntryIndex < 0 || nEntryIndex >= m_Entries.Count() )
		return;

	m_nHighlightedEntry = nEntryIndex;
}