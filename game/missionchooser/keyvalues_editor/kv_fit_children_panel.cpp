#include "kv_fit_children_panel.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CKV_Fit_Children_Panel::CKV_Fit_Children_Panel( Panel *parent, const char *name ) : 
BaseClass( parent, name ),
m_iAutoPositionStartY( 5 ),
m_iBorder( 5 ),
m_iSpacing( 1 )
{
}

bool FitChildrenLessFunc::Less( CKV_Editor_Base_Panel* const &src1, CKV_Editor_Base_Panel* const &src2, void *pCtx )
{
	const CKV_Editor_Base_Panel *pEditorPanel1 = dynamic_cast<const CKV_Editor_Base_Panel*>( src1 );
	const CKV_Editor_Base_Panel *pEditorPanel2 = dynamic_cast<const CKV_Editor_Base_Panel*>( src2 );
	if ( pEditorPanel1 && pEditorPanel2 )
	{
		return pEditorPanel1->GetSortOrder() < pEditorPanel2->GetSortOrder();
	}
	return src1->GetZPos() < src2->GetZPos();
}

void CKV_Fit_Children_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_pFileSpecNode )
	{
		m_iAutoPositionStartY = m_pFileSpecNode->GetInt( "AutoPositionStartY", m_iAutoPositionStartY );
		m_iBorder = m_pFileSpecNode->GetInt( "Border", m_iBorder );
		m_iSpacing = m_pFileSpecNode->GetInt( "Spacing", m_iSpacing );
	}

	// make sure all children are laid out and find the extents of them
	// lay out children in a vertical list	
	int cursor_y = m_iAutoPositionStartY;
	int wide = 0, tall = cursor_y;
	
	for ( int i = 0; i < m_AutoPositionPanels.Count(); i++ )
	{		
		vgui::Panel *pPanel = m_AutoPositionPanels[i];
		bool bNode = IsNodePanel( pPanel );
		if ( bNode )		// extra gap before node panel
		{
			cursor_y += m_iSpacing * 2;
		}
		pPanel->SetPos( m_iBorder, cursor_y );
		pPanel->InvalidateLayout( true );
		int x, y;
		pPanel->GetPos( x, y );
		x += pPanel->GetWide();
		y += pPanel->GetTall();
		cursor_y += pPanel->GetTall() + m_iSpacing;
		if ( bNode )		// extra gap after node panel
		{
			cursor_y += m_iSpacing * 2;
		}
		wide = MAX( wide, x );
		tall = MAX( tall, y );
	}

	SetSize( wide + m_iBorder, tall + m_iBorder );
}

bool CKV_Fit_Children_Panel::IsNodePanel( vgui::Panel *pPanel )
{
	return ( dynamic_cast< CKV_Fit_Children_Panel* > ( pPanel ) != NULL );
}

void CKV_Fit_Children_Panel::AddAutoPositionPanel( CKV_Editor_Base_Panel *pPanel )
{
	m_AutoPositionPanels.Insert( pPanel );
}

void CKV_Fit_Children_Panel::RemoveAutoPositionPanel( CKV_Editor_Base_Panel *pPanel )
{
	m_AutoPositionPanels.Remove( pPanel );
}

void CKV_Fit_Children_Panel::ClearAutoPositionPanels()
{
	m_AutoPositionPanels.RemoveAll();
}