//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "InfoTargetPropertiesPanel.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "iregistry.h"
#include "vgui/ivgui.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/radiobutton.h"
#include "vgui_controls/messagebox.h"
#include "vgui_controls/scrollbar.h"
#include "vcdblockdoc.h"
#include "vcdblocktool.h"
#include "datamodel/dmelement.h"
#include "dmevmfentity.h"
#include "dme_controls/soundpicker.h"
#include "dme_controls/soundrecordpanel.h"
#include "matsys_controls/picker.h"
#include "vgui_controls/fileopendialog.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

class CScrollableEditablePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CScrollableEditablePanel, vgui::EditablePanel );

public:
	CScrollableEditablePanel( vgui::Panel *pParent, vgui::EditablePanel *pChild, const char *pName );
	virtual ~CScrollableEditablePanel() {}
	virtual void PerformLayout();

	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );

private:
	vgui::ScrollBar *m_pScrollBar;
	vgui::EditablePanel *m_pChild;
};


CScrollableEditablePanel::CScrollableEditablePanel( vgui::Panel *pParent, vgui::EditablePanel *pChild, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pChild = pChild;
	m_pChild->SetParent( this );

	m_pScrollBar = new vgui::ScrollBar( this, "VerticalScrollBar", true ); 
	m_pScrollBar->SetWide( 16 );
	m_pScrollBar->SetAutoResize( PIN_TOPRIGHT, AUTORESIZE_DOWN, 0, 0, -16, 0 );
	m_pScrollBar->AddActionSignalTarget( this );
}

void CScrollableEditablePanel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pChild->SetWide( GetWide() - 16 );
	m_pScrollBar->SetRange( 0, m_pChild->GetTall() );
	m_pScrollBar->SetRangeWindow( GetTall() );
}


//-----------------------------------------------------------------------------
// Called when the scroll bar moves
//-----------------------------------------------------------------------------
void CScrollableEditablePanel::OnScrollBarSliderMoved()
{
	InvalidateLayout();

	int nScrollAmount = m_pScrollBar->GetValue();
	m_pChild->SetPos( 0, -nScrollAmount ); 
}

	
//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CInfoTargetPropertiesPanel::CInfoTargetPropertiesPanel( CVcdBlockDoc *pDoc, vgui::Panel* pParent )
	: BaseClass( pParent, "InfoTargetPropertiesPanel" ), m_pDoc( pDoc )
{
	SetPaintBackgroundEnabled( true );
	SetKeyBoardInputEnabled( true );

	m_pInfoTarget = new vgui::EditablePanel( (vgui::Panel*)NULL, "InfoTarget" );

	m_pTargetName = new vgui::TextEntry( m_pInfoTarget, "TargetName" );
	m_pTargetName->AddActionSignalTarget( this );

	m_pTargetPosition[0] = new vgui::TextEntry( m_pInfoTarget, "PositionX" );
	m_pTargetPosition[0]->AddActionSignalTarget( this );
 	m_pTargetPosition[1] = new vgui::TextEntry( m_pInfoTarget, "PositionY" );
	m_pTargetPosition[1]->AddActionSignalTarget( this );
 	m_pTargetPosition[2] = new vgui::TextEntry( m_pInfoTarget, "PositionZ" );
	m_pTargetPosition[2]->AddActionSignalTarget( this );

 	m_pTargetOrientation[0] = new vgui::TextEntry( m_pInfoTarget, "Pitch" );
	m_pTargetOrientation[0]->AddActionSignalTarget( this );
 	m_pTargetOrientation[1] = new vgui::TextEntry( m_pInfoTarget, "Yaw" );
	m_pTargetOrientation[1]->AddActionSignalTarget( this );
 	m_pTargetOrientation[2] = new vgui::TextEntry( m_pInfoTarget, "Roll" );
	m_pTargetOrientation[2]->AddActionSignalTarget( this );

	m_pInfoTarget->LoadControlSettings( "resource/infotargetpropertiessubpanel_target.res" );

	m_pInfoTargetScroll = new CScrollableEditablePanel( this, m_pInfoTarget, "InfoTargetScroll" );

	LoadControlSettings( "resource/infotargetpropertiespanel.res" );

	m_pInfoTargetScroll->SetVisible( false );
}


//-----------------------------------------------------------------------------
// Text to attribute...
//-----------------------------------------------------------------------------
void CInfoTargetPropertiesPanel::TextEntryToAttribute( vgui::TextEntry *pEntry, const char *pAttributeName )
{
	int nLen = pEntry->GetTextLength();
	char *pBuf = (char*)_alloca( nLen+1 );
	pEntry->GetText( pBuf, nLen+1 );
	m_hEntity->SetValue( pAttributeName, pBuf );
}

void CInfoTargetPropertiesPanel::TextEntriesToVector( vgui::TextEntry *pEntry[3], const char *pAttributeName )
{
	Vector vec;
	for ( int i = 0; i < 3; ++i )
	{
		int nLen = pEntry[i]->GetTextLength();
		char *pBuf = (char*)_alloca( nLen+1 );
		pEntry[i]->GetText( pBuf, nLen+1 );
		vec[i] = atof( pBuf );
	}
	m_hEntity->SetValue( pAttributeName, vec );
	clienttools->MarkClientRenderableDirty( m_hEntity );
}


//-----------------------------------------------------------------------------
// Updates entity state when text fields change
//-----------------------------------------------------------------------------
void CInfoTargetPropertiesPanel::UpdateInfoTarget()
{
	if ( !m_hEntity.Get() )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Info Target Change", "Info Target Change" );
	TextEntryToAttribute( m_pTargetName, "targetname" );
	TextEntriesToVector( m_pTargetPosition, "origin" );
	TextEntriesToVector( m_pTargetOrientation, "angles" );
	m_hEntity->MarkDirty();
}


//-----------------------------------------------------------------------------
// Populates the info_target fields
//-----------------------------------------------------------------------------
void CInfoTargetPropertiesPanel::PopulateInfoTargetFields()
{
	if ( !m_hEntity.Get() )
		return;

	m_pTargetName->SetText( m_hEntity->GetTargetName() );

	Vector vecPosition = m_hEntity->GetRenderOrigin();
	QAngle vecAngles = m_hEntity->GetRenderAngles();

	for ( int i = 0; i < 3; ++i )
	{
		char pTemp[512];
		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecPosition[i] );
		m_pTargetPosition[i]->SetText( pTemp );

		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecAngles[i] );
		m_pTargetOrientation[i]->SetText( pTemp );
	}
}


//-----------------------------------------------------------------------------
// Sets the object to look at
//-----------------------------------------------------------------------------
void CInfoTargetPropertiesPanel::SetObject( CDmeVMFEntity *pEntity )
{
	m_hEntity = pEntity;
	m_pInfoTargetScroll->SetVisible( false );

	if ( pEntity )
	{
		if ( !Q_stricmp( pEntity->GetClassName(), "info_target" ) )
		{
			PopulateInfoTargetFields();
			m_pInfoTargetScroll->SetVisible( true );
			m_pTargetName->RequestFocus();
			return;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Called when text is changed
//-----------------------------------------------------------------------------
void CInfoTargetPropertiesPanel::OnTextChanged( KeyValues *pParams )
{
	vgui::Panel *pPanel = (vgui::Panel*)pParams->GetPtr( "panel" );
	if ( pPanel->GetParent() == m_pInfoTarget )
	{
		UpdateInfoTarget();
		return;
	}
}



