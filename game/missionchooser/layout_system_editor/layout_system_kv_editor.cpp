//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Layout system editor.  Auto-generates UI from KeyValues data
// to edit rule instances.
//
//===============================================================================

#include "KeyValues.h"
#include "ScrollingWindow.h"
#include "layout_system/tilegen_mission_preprocessor.h"
#include "layout_system_editor/mission_panel.h"
#include "layout_system_editor/layout_system_kv_editor.h"

using namespace vgui;

CLayoutSystemKVEditor::CLayoutSystemKVEditor( Panel *pParent, const char *pName ) :
BaseClass( pParent, pName ),
m_pMissionFileKV( NULL ),
m_pMissionPanel( NULL ),
m_bShowOptionalValues( false ),
m_bShowAddButtons( false )
{
	m_pScrollingWindow = new CScrollingWindow( this, "ScrollingWindow" );
}

CLayoutSystemKVEditor::~CLayoutSystemKVEditor()
{
}

void CLayoutSystemKVEditor::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pScrollingWindow->SetBounds( 0, 0, GetWide(), GetTall() );
	if ( m_pMissionPanel != NULL )
	{
		m_pMissionPanel->PerformLayout();
	}
}

void CLayoutSystemKVEditor::SetMissionData( KeyValues *pMissionFileKV )
{
	m_pMissionFileKV = pMissionFileKV;
	RecreateMissionPanel();
}

void CLayoutSystemKVEditor::RecreateMissionPanel()
{
	delete m_pMissionPanel;
	m_pMissionPanel = new CMissionPanel( this, "Mission" );
	m_pMissionPanel->SetEditor( this );
	m_pScrollingWindow->SetChildPanel( m_pMissionPanel );
	m_pScrollingWindow->InvalidateLayout( true );
	m_pMissionPanel->SetData( m_pMissionFileKV );
	m_pMissionPanel->InvalidateLayout( true );
}

void CLayoutSystemKVEditor::ShowOptionalValues( bool bVisible )
{
	if ( bVisible != m_bShowOptionalValues )
	{
		m_bShowOptionalValues = bVisible;
		if ( m_pMissionPanel != NULL )
		{
			m_pMissionPanel->UpdateState();
		}
		InvalidateLayout();
	}
}

void CLayoutSystemKVEditor::ShowAddButtons( bool bVisible )
{
	if ( bVisible != m_bShowAddButtons )
	{
		m_bShowAddButtons = bVisible;
		if ( m_pMissionPanel != NULL )
		{
			m_pMissionPanel->UpdateState();
		}
		InvalidateLayout();
	}
}
