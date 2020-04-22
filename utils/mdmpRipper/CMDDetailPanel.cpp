//========= Copyright Valve Corporation, All rights reserved. ============//
#include "vgui_controls/HTML.h"
#include "CMDDetailPanel.h"

using namespace vgui;

CMDDetailPanel::CMDDetailPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName, true )
{
	SetParent( pParent );	
	m_pDetailWindow = new vgui::HTML(this, "Details");	
	m_pDetailWindow->SetParent( this );
	m_pDetailWindow->SetSize( 770, 475 );
	LoadControlSettings( "MDDetailPanel.res" );		
	m_pDetailWindow->OpenURL( "about:blank" );
}

void CMDDetailPanel::OpenURL( const char *url )
{
	m_pDetailWindow->OpenURL( "about:blank" );
	m_pDetailWindow->OpenURL( url );
	m_pDetailWindow->SetVisible( true );
}

	
void CMDDetailPanel::OnCommand( const char *pCommand )
{	
	if ( !Q_strcmp( pCommand, "Close" ) )
	{
		Close();
	}		
}

void CMDDetailPanel::Close()
{
	m_pDetailWindow->SetVisible( false );
	SetVisible( false );
	KeyValues *kv = new KeyValues( "Refresh" );
	this->PostActionSignal( kv );
}
