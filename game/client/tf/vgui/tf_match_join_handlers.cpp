//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"
#include "tf_shareddefs.h"
#include "tf_match_join_handlers.h"
#include "tf_gc_client.h"
#include "tf_controls.h"
#include "tf_gamerules.h"
#include "ienginevgui.h"
#include "clientmode_tf.h"
#include "tf_hud_disconnect_prompt.h"
#include <vgui_controls/AnimationController.h>
#include "vgui_int.h"
#include "tf_gc_client.h"
#include "tf_party.h"
#include "../vgui2/src/VPanel.h"

using namespace vgui;

#define MM_REJOIN_WAIT_TIME	1.0f
#define MM_REJOIN_PROMPT_TIMEOUT 3.f



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IMatchJoiningHandler::IMatchJoiningHandler()
{}

IMatchJoiningHandler::~IMatchJoiningHandler()
{}

void IMatchJoiningHandler::JoinMatch()
{
	GTFGCClientSystem()->JoinMMMatch();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFImmediateAutoJoinHandler::CTFImmediateAutoJoinHandler()
	: m_flNextAutoJoinTime( 0.f )
{}

void CTFImmediateAutoJoinHandler::MatchFound()
{
	// No special logic.  Just try to join every 2 seconds
	if ( Plat_FloatTime() > m_flNextAutoJoinTime && !GTFGCClientSystem()->BConnectedToMatchServer( false ) )
	{
		JoinMatch();
		m_flNextAutoJoinTime = Plat_FloatTime() + 2.f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFRejoinConfirmDialog* CTFMatchMakingPopupPrompJoinHandler::m_pRejoinPrompt = NULL;

CTFMatchMakingPopupPrompJoinHandler::CTFMatchMakingPopupPrompJoinHandler()
	: m_flNextRejoinThinkTime( 0.f )
{}


void CTFMatchMakingPopupPrompJoinHandler::MatchFound()
{
	if ( !m_pRejoinPrompt )
	{
		if ( m_flNextRejoinThinkTime == 0.f || Plat_FloatTime() >= ( m_flNextRejoinThinkTime + MM_REJOIN_PROMPT_TIMEOUT ) )
		{
			m_flNextRejoinThinkTime = Plat_FloatTime() + MM_REJOIN_WAIT_TIME;
		}
		else if ( Plat_FloatTime() > m_flNextRejoinThinkTime )
		{
			UpdatePromptState();
			m_flNextRejoinThinkTime = 0.f;
		}
	}
}

void CTFMatchMakingPopupPrompJoinHandler::OnJoinLobbyInProgressCallback( bool bConfirmed, void *pContext )
{
	GTFGCClientSystem()->RejoinLobby( bConfirmed );
	m_pRejoinPrompt = NULL;
}

void CTFMatchMakingPopupPrompJoinHandler::UpdatePromptState()
{
	bool bHasMatch = GTFGCClientSystem()->BHaveLiveMatch();
	bool bInLiveMatch = GTFGCClientSystem()->BConnectedToMatchServer( false );

	if ( !m_pRejoinPrompt && bHasMatch && !bInLiveMatch )
	{
		if ( enginevgui == NULL || GetClientModeTFNormal()->GameUI() == NULL )
			return;

		// Check if this player is in Abandon territory, if so warn them
		EAbandonGameStatus eAbandonStatus = GTFGCClientSystem()->GetAssignedMatchAbandonStatus();
		const char* pszTitle = "#TF_MM_Rejoin_Title";
		const char* pszBody = NULL;
		const char* pszConfirm = "#TF_MM_Rejoin_Confirm";
		const char* pszCancel = NULL;

		switch ( eAbandonStatus )
		{
		case k_EAbandonGameStatus_Safe:
			pszBody = "#TF_MM_Rejoin_BaseText";
			pszCancel = "#TF_MM_Rejoin_Leave";
			break;
		case k_EAbandonGameStatus_AbandonWithoutPenalty:
			pszBody = "#TF_MM_Rejoin_AbandonText_NoPenalty";
			pszCancel = "#TF_MM_Rejoin_Abandon";
			break;
		case k_EAbandonGameStatus_AbandonWithPenalty:
			pszBody = "#TF_MM_Rejoin_AbandonText";
			pszCancel = "#TF_MM_Rejoin_Abandon";
			break;
		}

		m_pRejoinPrompt = vgui::SETUP_PANEL( new CTFRejoinConfirmDialog(
			pszTitle,
			pszBody,
			pszConfirm,
			pszCancel,
			&OnJoinLobbyInProgressCallback,
			NULL
		));

		if ( m_pRejoinPrompt )
		{
			m_pRejoinPrompt->Show();
			// VGUI is being dumb so I need to manually calculate this windows position
			int sW, sT, dW, dT;
			vgui::surface()->GetScreenSize( sW, sT );
			m_pRejoinPrompt->GetSize( dW, dT );
			m_pRejoinPrompt->SetPos( (sW - dW) / 2, (sT - dT) / 2 );
		}
	}
}
