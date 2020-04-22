//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Disconnect prompt to warn leavers of penalty they will incur
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "tf_hud_disconnect_prompt.h"
#include "econ_controls.h"
#include "tf_gc_client.h"
#include "tf_gamerules.h"

using namespace vgui;

CTFDisconnectConfirmDialog::CTFDisconnectConfirmDialog(	
	const char *pTitle, 
	const char *pTextKey, 
	const char *pConfirmBtnText,
	const char *pCancelBtnText, 
	GenericConfirmDialogCallback callback, 
	vgui::Panel *pParent 
) : CTFGenericConfirmDialog( pTitle, pTextKey, pConfirmBtnText, pCancelBtnText, callback, pParent )
{
	m_eAbandonStatus = GTFGCClientSystem()->GetCurrentServerAbandonStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFDisconnectConfirmDialog::GetResFile()
{
	switch ( m_eAbandonStatus )
	{
	case k_EAbandonGameStatus_Safe:
		return "Resource/UI/econ/ConfirmDialogAbandonSafe.res";
	case k_EAbandonGameStatus_AbandonWithoutPenalty:
		return "Resource/UI/econ/ConfirmDialogAbandonNoPenalty.res";
	case k_EAbandonGameStatus_AbandonWithPenalty:
		return "Resource/UI/econ/ConfirmDialogAbandonPenalty.res";
	}
	
	return "Resource/UI/econ/ConfirmDialogOptOut.res";
}

void CTFDisconnectConfirmDialog::SetReason( eDisconnectReason reason )
{
	m_eReason = reason;
}

void CTFDisconnectConfirmDialog::OnCommand( const char *command )
{
	if( FStrEq( command, "confirm" ) )
	{
		// Check this before disconnecting
		if ( GTFGCClientSystem()->BExitMatchmakingAfterDisconnect() )
		{
			GTFGCClientSystem()->EndMatchmaking( true );
		}

		engine->DisconnectInternal();

		int nCount = m_confirmCommands.Count();
		for( int i=0; i<nCount; ++i )
		{
			engine->ClientCmd_Unrestricted( m_confirmCommands[i] );
		}
	}
	else if( FStrEq( command, "cancel" ) )
	{
		int nCount = m_cancelCommands.Count();
		for( int i=0; i<nCount; ++i )
		{
			engine->ClientCmd_Unrestricted( m_cancelCommands[i] );
		}
	}

	// This will clean us up if we get a cancel or confirm command
	BaseClass::OnCommand( command );
}


void CTFDisconnectConfirmDialog::AddConfirmCommand( const char *command )
{
	m_confirmCommands.AddToTail( command );
}


void CTFDisconnectConfirmDialog::AddCancelCommand( const char *command )
{
	m_cancelCommands.AddToTail( command );
}

//
// Extern Helper to Build Dialog
CTFDisconnectConfirmDialog *BuildDisconnectConfirmDialog ()
{
	EAbandonGameStatus eAbandonStatus = GTFGCClientSystem()->GetCurrentServerAbandonStatus();
	const char* pszTitle = NULL; 
	const char* pszBody = NULL; 
	const char* pszConfirm = NULL;

	switch ( eAbandonStatus )
	{
	case k_EAbandonGameStatus_Safe:
		pszTitle = "#TF_MM_Disconnect_Title"; 
		pszBody = "#TF_MM_Disconnect"; 
		pszConfirm = "#TF_MM_Rejoin_Leave";
		break;
	case k_EAbandonGameStatus_AbandonWithoutPenalty:
		pszTitle = "#TF_MM_Abandon_Title"; 
		pszBody = "#TF_MM_Abandon_NoPenalty"; 
		pszConfirm = "#TF_MM_Rejoin_Leave";
		break;
	case k_EAbandonGameStatus_AbandonWithPenalty:
		pszTitle = "#TF_MM_Abandon_Title"; 
		pszBody = ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() ) ? "TF_MM_Abandon_Ban" : "#TF_MM_Abandon";
		pszConfirm = "#TF_MM_Rejoin_Abandon";
		break;
	}

	CTFDisconnectConfirmDialog *pDialog = vgui::SETUP_PANEL( new CTFDisconnectConfirmDialog( 
		pszTitle,
		pszBody,
		pszConfirm,
		"#TF_MM_Rejoin_Stay",
		NULL,
		NULL ) );

	return pDialog;
}

// sent from quit and disconnect attempts
CON_COMMAND( cl_disconnect_prompt, "Prompt about disconnect" )
{
	CTFDisconnectConfirmDialog *pDialog = vgui::SETUP_PANEL( new CTFDisconnectConfirmDialog( "#TF_MM_Abandon_Title",
																							 "#TF_MM_Abandon",
																							 "#TF_Coach_Yes",
																							 "#TF_Coach_No",
																							 NULL, 
																							 NULL ) );

	if ( pDialog )
	{
		pDialog->Show();
	}
}


bool HandleDisconnectAttempt()
{
	// !FIXME! We could show different messages depending on the abandon status

	if( engine->IsInGame() && GameRules() && GameRules()->ShouldConfirmOnDisconnect() &&
	    GTFGCClientSystem()->GetCurrentServerAbandonStatus() == k_EAbandonGameStatus_AbandonWithPenalty )
	{
		CTFDisconnectConfirmDialog *pDialog = vgui::SETUP_PANEL( new CTFDisconnectConfirmDialog( "#TF_MM_Abandon_Title",
																							"#TF_MM_Abandon",
																							"#TF_Coach_Yes",
																							"#TF_Coach_No",
																							NULL,
																							NULL ) );

		if ( pDialog )
		{
			pDialog->Show();
		}

		// The disconnect was handled by us. Stop the engine from handling it.
		return true;
	}

	// We're not changing the disconnect behavior in any way. Let the engine take care of it.
	return false;
}



//-----------------------------------------------------------------------------
// CTFRejoinConfirmDialog
//-----------------------------------------------------------------------------
CTFRejoinConfirmDialog::CTFRejoinConfirmDialog(	
	const char *pTitle, 
	const char *pTextKey, 
	const char *pConfirmBtnText,
	const char *pCancelBtnText, 
	GenericConfirmDialogCallback callback, 
	vgui::Panel *pParent 
	) : CTFGenericConfirmDialog( pTitle, pTextKey, pConfirmBtnText, pCancelBtnText, callback, pParent )
{
	m_eAbandonStatus = GTFGCClientSystem()->GetAssignedMatchAbandonStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CTFRejoinConfirmDialog::GetResFile()
{
	switch ( m_eAbandonStatus )
	{
	case k_EAbandonGameStatus_Safe:
		return "Resource/UI/econ/ConfirmDialogAbandonSafe.res";
	case k_EAbandonGameStatus_AbandonWithoutPenalty:
		return "Resource/UI/econ/ConfirmDialogAbandonNoPenalty.res";
	case k_EAbandonGameStatus_AbandonWithPenalty:
		return "Resource/UI/econ/ConfirmDialogAbandonPenalty.res";
	}

	return "Resource/UI/econ/ConfirmDialogOptOut.res";
}

//-----------------------------------------------------------------------------
void CTFRejoinConfirmDialog::CloseRejoinWindow()
{
	FinishUp();
}
