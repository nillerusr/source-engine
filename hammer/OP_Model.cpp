//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "MapEntity.h"
#include "MapStudioModel.h"
#include "OP_Model.h"
#include "ObjectProperties.h"
#include "mapdoc.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning( disable : 4355 )


IMPLEMENT_DYNCREATE(COP_Model, CObjectPage)


BEGIN_MESSAGE_MAP(COP_Model, CObjectPage)
	//{{AFX_MSG_MAP(COP_Model)
	ON_WM_HSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


const int FRAME_SCROLLBAR_RANGE = 1000;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COP_Model::COP_Model() : CObjectPage(COP_Model::IDD), m_ComboSequence( this )
{
	//{{AFX_DATA_INIT(COP_Model)
	//}}AFX_DATA_INIT

	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCEditGameClass);
	m_ComboSequence.SetOnlyProvideSuggestions( true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COP_Model::~COP_Model()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COP_Model::DoDataExchange(CDataExchange* pDX)
{
	CObjectPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COP_Model)
	DDX_Control(pDX, IDC_SEQUENCE, m_ComboSequence);
	DDX_Control(pDX, IDC_FRAME_SCROLLBAR, m_ScrollBarFrame);
	//}}AFX_DATA_MAP
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Mode - 
//			pData - 
//-----------------------------------------------------------------------------
void COP_Model::UpdateData( int Mode, PVOID pData, bool bCanEdit )
{
	__super::UpdateData( Mode, pData, bCanEdit );

	if (!IsWindow(m_hWnd) || !pData)
	{
		return;
	}
	
	if (Mode == LoadFirstData)
	{
		CMapStudioModel *pModel = GetModelHelper();
		if ( pModel )
		{
			// If they were on a previous animation, remember it and we'll set the combo box to that after
			// we tell it the list of suggestions.
			char txt[512];
			txt[0] = 0;
			int iSequence = pModel->GetSequence();
			if ( iSequence )
				pModel->GetSequenceName( iSequence, txt );

			// Set the list of suggestions.
			CUtlVector<CString> suggestions;
	
			int nCount = pModel->GetSequenceCount();
			for ( int i = 0; i < nCount; i++ )
			{
				char szName[MAX_PATH];
				pModel->GetSequenceName(i, szName);
				suggestions.AddToTail( szName );
			}
			
			m_ComboSequence.SetSuggestions( suggestions, 0 );
			
			// Tell it to go to the last sequence the model was on.
			if ( txt[0] )
			{
				m_ComboSequence.SelectItem( txt );
			}
		}
	}
	else if (Mode == LoadData)
	{
		Assert(false);
	}

	SetReadOnly( !m_bCanEdit );
}

BOOL COP_Model::OnSetActive()
{
	m_bOldAnimatedModels = Options.view3d.bAnimateModels;
	Options.view3d.bAnimateModels = true;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc )
	{
		pDoc->UpdateAllViews( MAPVIEW_UPDATE_ANIMATION|MAPVIEW_OPTIONS_CHANGED );
	}


	return CObjectPage::OnSetActive();
}

BOOL COP_Model::OnKillActive() 
{
	Options.view3d.bAnimateModels = m_bOldAnimatedModels;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc )
	{
		pDoc->UpdateAllViews( MAPVIEW_UPDATE_ANIMATION|MAPVIEW_OPTIONS_CHANGED );
	}

	return CObjectPage::OnKillActive();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COP_Model::SaveData(void)
{
	if (!IsWindow(m_hWnd))
	{
		return false;
	}

	// Apply the dialog data to all the objects being edited.
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszClass - 
//-----------------------------------------------------------------------------
void COP_Model::UpdateForClass(LPCTSTR pszClass)
{
	if (!IsWindow(m_hWnd))
	{
		return;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flFrame - 
//-----------------------------------------------------------------------------
void COP_Model::UpdateFrameText(float flFrame)
{
	char szFrame[40];
	sprintf(szFrame, "%0.2f", flFrame);
	GetDlgItem(IDC_FRAME_TEXT)->SetWindowText(szFrame);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COP_Model::OnInitDialog() 
{
	CObjectPage::OnInitDialog();

	//
	// Set the frame number scrollbar range.
	//
	m_ScrollBarFrame.SetRange(0, FRAME_SCROLLBAR_RANGE);

	return TRUE;	             
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSBCode - 
//			nPos - 
//			pScrollBar - 
//-----------------------------------------------------------------------------
void COP_Model::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar) 
{
	if (pScrollBar == (CScrollBar *)&m_ScrollBarFrame)
	{
		CMapStudioModel *pModel = GetModelHelper();
		if (pModel != NULL)
		{
			float flFrame = (float)nPos / (float)FRAME_SCROLLBAR_RANGE;
			pModel->SetFrame(flFrame);
			UpdateFrameText(flFrame);

			CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
			if ( pDoc )
			{
				pDoc->UpdateAllViews( MAPVIEW_UPDATE_ANIMATION );
			}
		}
	}
	
	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COP_Model::OnTextChanged( const char *pText )
{
	CMapStudioModel *pModel = GetModelHelper();
	if (pModel != NULL)
	{
		int iSequence = pModel->GetSequenceIndex( pText );
		if ( iSequence != -1 )
			pModel->SetSequence( iSequence );

		UpdateFrameText(pModel->GetFrame());

		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if ( pDoc )
		{
			pDoc->UpdateAllViews( MAPVIEW_UPDATE_ANIMATION );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapStudioModel *COP_Model::GetModelHelper(void)
{
	if ( m_pObjectList->Count() == 0 )
		return NULL;

	CMapClass *pObject = m_pObjectList->Element( 0 );

	if (pObject != NULL)
	{
		CMapEntity *pEntity = dynamic_cast <CMapEntity *>(pObject);
		if (pEntity != NULL)
		{
			CMapStudioModel *pModel = pEntity->GetChildOfType((CMapStudioModel *)NULL);
			return pModel;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: sets the controls to be read only
// Input  : bReadOnly - indicates if the controls should be read only
//-----------------------------------------------------------------------------
void COP_Model::SetReadOnly( bool bReadOnly )
{
	m_ComboSequence.EnableWindow( bReadOnly ? FALSE : TRUE );
	m_ScrollBarFrame.EnableWindow( bReadOnly ? FALSE : TRUE );
}
