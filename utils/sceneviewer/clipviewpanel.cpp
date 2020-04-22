//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ClipViewPanel.h"
#include "dme_controls/dmedageditpanel.h"
#include "tier1/KeyValues.h"

#include "movieobjects/dmedag.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmevertexdata.h"
#include "vstdlib/random.h"

using namespace vgui;



//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CClipViewPanel::CClipViewPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pClipViewPreview = new CDmeDagEditPanel( this, "ClipViewPreview" );

	SetVisible( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetMinimizeToSysTrayButtonVisible( false );
	SetCloseButtonVisible( false );
								 
	SetTitle( "3d View", true );

//	LoadControlSettings( "resource/SceneViewer_ClipView.res" );
}

CClipViewPanel::~CClipViewPanel()
{
}


//-----------------------------------------------------------------------------
// Scheme settings
//-----------------------------------------------------------------------------
void CClipViewPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder( "MenuBorder") );
}


//-----------------------------------------------------------------------------
// Stores the clip
//-----------------------------------------------------------------------------
void CClipViewPanel::SetScene( CDmeDag *pScene )
{
	m_pClipViewPreview->SetDmeElement( pScene );
}

CDmeDag *CClipViewPanel::GetScene( )
{
	return m_pClipViewPreview->GetDmeElement( );
}


//-----------------------------------------------------------------------------
// Sets animation
//-----------------------------------------------------------------------------
void CClipViewPanel::SetAnimationList( CDmeAnimationList *pAnimationList )
{
	m_pClipViewPreview->SetAnimationList( pAnimationList );
}

void CClipViewPanel::SetVertexAnimationList( CDmeAnimationList *pAnimationList )
{
	m_pClipViewPreview->SetVertexAnimationList( pAnimationList );
}

void CClipViewPanel::SetCombinationOperator( CDmeCombinationOperator *pComboOp )
{
	m_pClipViewPreview->SetCombinationOperator( pComboOp );
}

void CClipViewPanel::RefreshCombinationOperator()
{
	m_pClipViewPreview->RefreshCombinationOperator();
}


//-----------------------------------------------------------------------------
// performs the layout
//-----------------------------------------------------------------------------
void CClipViewPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	GetClientArea( x, y, w, h );
	m_pClipViewPreview->SetBounds( x, y, w, h );
}