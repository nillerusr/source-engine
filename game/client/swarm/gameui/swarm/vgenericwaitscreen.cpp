//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGenericWaitScreen.h"
#include "EngineInterface.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"

#include "vfooterpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

//=============================================================================
GenericWaitScreen::GenericWaitScreen( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, false, false, false )
{
	SetProportional( true );

	SetTitle( "#L4D360UI_WaitScreen_WorkingMsg", false );

	AddFrameListener( this );
	SetDeleteSelfOnClose( true );

	m_LastEngineTime = 0;
	m_CurrentSpinnerValue = 0;

	m_callbackPanel = 0;
	m_callbackMessage = NULL;
	
	m_MsgStartDisplayTime = -1.0f;
	m_MsgMinDisplayTime = 0.0f;
	m_MsgMaxDisplayTime = 0.0f;
	m_pfnMaxTimeOut = NULL;
	m_bClose = false;
	m_currentDisplayText = "";
	m_bTextSet = true;

	m_pAsyncOperationAbortable = NULL;
}

//=============================================================================
GenericWaitScreen::~GenericWaitScreen()
{
	RemoveFrameListener( this );
}

//=============================================================================
void GenericWaitScreen::SetMessageText( const char *message )
{
	m_currentDisplayText = message;
	m_bTextSet = false;

	vgui::Label *pLblMessage = dynamic_cast< vgui::Label * > ( FindChildByName( "LblMessage" ) );
	if ( pLblMessage )
	{
		m_bTextSet = true;
		pLblMessage->SetText( message );
	}
}

void GenericWaitScreen::SetMessageText( const wchar_t *message )
{
	vgui::Label *pLblMessage = dynamic_cast< vgui::Label * > ( FindChildByName( "LblMessage" ) );
	if ( pLblMessage )
	{
		m_bTextSet = true;
		pLblMessage->SetText( message );
	}
}


//=============================================================================
void GenericWaitScreen::AddMessageText( const char* message, float minDisplayTime )
{
	float time = Plat_FloatTime();
	if ( time > m_MsgStartDisplayTime + m_MsgMinDisplayTime )
	{
		SetMessageText( message );
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = minDisplayTime;
	}
	else
	{
		WaitMessage msg;
		msg.mDisplayString = message;
		msg.minDisplayTime = minDisplayTime;
		m_MsgVector.AddToTail( msg );
	}
}

void GenericWaitScreen::AddMessageText( const wchar_t* message, float minDisplayTime )
{
	float time = Plat_FloatTime();
	if ( time > m_MsgStartDisplayTime + m_MsgMinDisplayTime )
	{
		SetMessageText( message );
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = minDisplayTime;
	}
	else
	{
		WaitMessage msg;
		msg.mWchMsgText = message;
		msg.minDisplayTime = minDisplayTime;
		m_MsgVector.AddToTail( msg );
	}
}

//=============================================================================
void GenericWaitScreen::SetCloseCallback( vgui::Panel * panel, const char *message )
{
	if ( message )
	{
		m_callbackPanel = panel;
		m_callbackMessage = message;
	}
	else 
	{
		m_bClose = true;
	}

	CheckIfNeedsToClose();
}

//=============================================================================
void GenericWaitScreen::ClearData()
{
	m_MsgVector.Purge();
	m_callbackPanel = 0;
	m_callbackMessage = NULL;
	m_MsgStartDisplayTime = -1.0f;
	m_MsgMinDisplayTime = 0.0f;
	m_MsgMaxDisplayTime = 0.0f;
	m_pfnMaxTimeOut = NULL;
	m_bClose = false;
}

//=============================================================================
void GenericWaitScreen::OnThink()
{
	BaseClass::OnThink();
	
	SetUpText();
	ClockAnim();
	UpdateFooter();
	CheckIfNeedsToClose();
}

void GenericWaitScreen::RunFrame()
{
	CheckIfNeedsToClose();
}

void GenericWaitScreen::SetDataSettings( KeyValues *pSettings )
{
	m_pAsyncOperationAbortable = ( IMatchAsyncOperation * ) pSettings->GetPtr( "options/asyncoperation", NULL );
}

void GenericWaitScreen::OnKeyCodePressed(vgui::KeyCode code)
{
	CBaseModPanel::GetSingleton().SetLastActiveUserId( GetJoystickForCode( code ) );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		if ( m_pAsyncOperationAbortable &&
			 AOS_RUNNING == m_pAsyncOperationAbortable->GetState() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			
			m_pAsyncOperationAbortable->Abort();
			m_pAsyncOperationAbortable = NULL;
		}
		break;
	}
}

void GenericWaitScreen::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

void GenericWaitScreen::ApplySchemeSettings( vgui::IScheme * pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	OnThink();
}

void GenericWaitScreen::ClockAnim()
{
	// clock the anim at 10hz
	float time = Plat_FloatTime();
	if ( ( m_LastEngineTime + 0.1f ) < time )
	{
		m_LastEngineTime = time;
		vgui::ImagePanel *pWorkingAnim = dynamic_cast< vgui::ImagePanel * >( FindChildByName( "WorkingAnim" ) );
		if ( pWorkingAnim )
		{
			pWorkingAnim->SetFrame( m_CurrentSpinnerValue++ );
		}
	}
}

void GenericWaitScreen::CheckIfNeedsToClose()
{
	float time = Plat_FloatTime();
	if ( time <= m_MsgStartDisplayTime + m_MsgMinDisplayTime )
		return;

	// my timer is up
	if ( m_MsgVector.Count() )
	{
		int iLastElement = m_MsgVector.Count() - 1;
		
		WaitMessage &msg = m_MsgVector[iLastElement];
		if ( msg.mWchMsgText )
			SetMessageText( msg.mWchMsgText );
		else
			SetMessageText( m_MsgVector[iLastElement].mDisplayString.Get() );
		
		m_MsgStartDisplayTime = time;
		m_MsgMinDisplayTime = m_MsgVector[iLastElement].minDisplayTime;
		m_MsgVector.Remove( iLastElement );
	}
	else if ( !m_callbackMessage.IsEmpty() )
	{
		if ( !m_callbackPanel )
			m_callbackPanel = GetNavBack();

		if ( m_callbackPanel )
			m_callbackPanel->PostMessage( m_callbackPanel, new KeyValues( m_callbackMessage.Get() ) );

		NavigateBack();
	}
	else if ( m_bClose )//just nav back.
	{
		NavigateBack();
	}
	else if ( m_MsgMaxDisplayTime > 0 )
	{
		if ( m_MsgStartDisplayTime + m_MsgMaxDisplayTime < time )
		{
			if ( !NavigateBack() )
			{
				Close();
				CBaseModPanel::GetSingleton().OpenFrontScreen();
			}
			
			if ( m_pfnMaxTimeOut )
				m_pfnMaxTimeOut();
		}
	}
}

void GenericWaitScreen::SetMaxDisplayTime( float flMaxTime, void (*pfn)() )
{
	m_MsgMaxDisplayTime = flMaxTime;
	m_pfnMaxTimeOut = pfn;
}

void GenericWaitScreen::SetUpText()
{
	if ( !m_bTextSet )
	{
		SetMessageText( m_currentDisplayText.Get() );
	}
}

void GenericWaitScreen::UpdateFooter()
{
	if ( IsPC() )
		// No footer voodoo for PC
		return;

	bool bCanCancel = ( m_pAsyncOperationAbortable &&
		AOS_RUNNING == m_pAsyncOperationAbortable->GetState() );

	CBaseModFooterPanel *pFooter = ( CBaseModFooterPanel * ) BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( !pFooter )
		return;
	
	if ( bCanCancel )
	{
		pFooter->SetVisible( true );
		pFooter->SetButtons( FB_BBUTTON, FF_AB_ONLY, false );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Cancel" );
	}
	else
	{
		pFooter->SetVisible( false );
	}
}

