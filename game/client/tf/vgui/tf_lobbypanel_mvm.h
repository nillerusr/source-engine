//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef TF_LOBBYPANEL_MVM_H
#define TF_LOBBYPANEL_MVM_H


#include "cbase.h"
#include "game/client/iviewport.h"
#include "vgui_bitmapimage.h"
#include "tf_lobbypanel.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


class CLobbyPanel_MvM : public CBaseLobbyPanel
{
	DECLARE_CLASS_SIMPLE( CLobbyPanel_MvM, CBaseLobbyPanel );

public:
	CLobbyPanel_MvM( vgui::Panel *pParent, CBaseLobbyContainerFrame* pLobbyContainer );
	
	virtual ~CLobbyPanel_MvM();

	//
	// Panel overrides
	//
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;

	//
	// CGameEventListener overrides
	//
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	virtual void OnCommand( const char *command ) OVERRIDE;

	void SetMannUpTicketCount( int nCount );
	void SetSquadSurplusCount( int nCount );

	virtual EMatchGroup GetMatchGroup( void ) const OVERRIDE;

	void ToggleSquadSurplusCheckButton( void )
	{
		if ( !GTFGCClientSystem()->BLocalPlayerInventoryHasSquadSurplusVoucher() )
			return;

		m_pSquadSurplusCheckButton->SetSelected( !m_pSquadSurplusCheckButton->IsSelected() );
	}

private:

	virtual const char* GetResFile() const OVERRIDE { return "Resource/UI/LobbyPanel_MvM.res"; } ;

	CPanelAnimationVarAliasType( int, m_iChallengeSpacer, "challenge_spacer", "4", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iChallengeNameWidth, "challenge_name_width", "190", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iChallengeSkillWidth, "challenge_skill_width", "110", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iChallengeCompletedSize, "challenge_completed_size", "15", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iMapImageWidth, "challenge_map_width", "60", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iMapImageHeight, "challenge_map_height", "40", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iHasTicketWidth, "has_ticket_width", "12", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iSquadSurplusWidth, "squad_surplus_width", "12", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTourMapWidth, "squad_surplus_width", "20", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iBadgeLevelWidth, "badge_level_width", "20", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_iTourNameWidth, "tour_name_width", "160", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTourSkillWidth, "tour_skill_width", "90", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTourProgressWidth, "tour_progress_width", "70", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTourNumberWidth, "tour_number_width", "40", "proportional_int" );

	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );

	MESSAGE_FUNC_PTR( OnItemLeftClick, "ItemLeftClick", panel );

	virtual void ApplyChatUserSettings( const LobbyPlayerInfo& player, KeyValues* pSettings ) const OVERRIDE;
#ifdef USE_MVM_TOUR
	void OnClickedOnTour();
#endif // USE_MVM_TOUR
	void OnClickedOnChallenge();

	class ChallengeList : public vgui::SectionedListPanel
	{
	public:
		ChallengeList( CLobbyPanel_MvM *pLobbyPanel, vgui::Panel *parent, const char *name )
		: vgui::SectionedListPanel( parent, name )
		, m_pLobbyPanel( pLobbyPanel )
		{
			m_imageChallengeCompleted.SetImageFile( "vgui/pve/mvm_challenge_completed" );
		}

		virtual void OnMouseDoublePressed(vgui::MouseCode code) OVERRIDE { /* Just eat it */ }
		virtual void Paint() OVERRIDE;

		CLobbyPanel_MvM *m_pLobbyPanel;

		BitmapImage m_imageChallengeCompleted;
		CUtlVector<BitmapImage> m_vecMapImages;
	};

	CUtlVector<vgui::Label *> m_vecSearchCriteriaLabels;

	vgui::EditablePanel *m_pMvMMannVsMachineGroupPanel;
	vgui::EditablePanel *m_pMvMMannUpGroupPanel;
	vgui::EditablePanel *m_pMvMPracticeGroupPanel;

	vgui::EditablePanel *m_pMvMTourOfDutyGroupPanel;
		vgui::EditablePanel *m_pMvMTourOfDutyListGroupBox;
		vgui::SectionedListPanel *m_pTourList;

	vgui::EditablePanel *m_MvMEconItemsGroupBox;
		vgui::CheckButton *m_pSquadSurplusCheckButton;
		vgui::Button *m_pOpenStoreButton;
		vgui::Button *m_pOpenStoreButton2;
		vgui::Button *m_pOpenHelpButton;
		vgui::ImagePanel *m_pMannUpTicketImage;
		vgui::ImagePanel *m_pSquadSurplusImage;
		vgui::Button *m_pMannUpNowButton;

	vgui::EditablePanel *m_pMannUpTourLootDescriptionBox;
		vgui::ImagePanel *m_pMannUpTourLootImage;
		vgui::Label *m_pTourDifficultyWarning;
		//vgui::Label *m_pMannUpTourLootDetailLabel;

	vgui::EditablePanel *m_MvMPracticeGroupPanel;

	vgui::EditablePanel *m_pMvMSelectChallengeGroupPanel;
		vgui::EditablePanel *m_pMVMChallengeListGroupBox;
		ChallengeList *m_pChallengeList;


	vgui::HFont m_fontChallengeListHeader;
	vgui::HFont m_fontChallengeListItem;

	int m_iImageNoTicket;
	int m_iImageHasTicket;
	int m_iImageNoSquadSurplus;
	int m_iImageSquadSurplus;

	void WriteGameSettingsControls() OVERRIDE;
	virtual bool ShouldShowLateJoin() const OVERRIDE;
#ifdef USE_MVM_TOUR
	void WriteTourList();
#endif // USE_MVM_TOUR
	void WriteChallengeList();
};

#endif // TF_LOBBYPANEL_MVM_H