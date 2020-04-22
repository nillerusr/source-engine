//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the spawnflags page of the Entity Properties dialog.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "OP_Flags.h"
#include "OP_Entity.h"
#include "ObjectProperties.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// COP_Flags property page

IMPLEMENT_DYNCREATE(COP_Flags, CObjectPage)

COP_Flags::COP_Flags() : CObjectPage(COP_Flags::IDD)
{
	//{{AFX_DATA_INIT(COP_Flags)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pEditObjectRuntimeClass = RUNTIME_CLASS(editCEditGameClass);
	m_nNumSelectedObjects = 0;
	m_pEntityPage = NULL;
}

COP_Flags::~COP_Flags()
{
}

void COP_Flags::SetEntityPage( COP_Entity *pPage )
{
	m_pEntityPage = pPage;
}

void COP_Flags::DoDataExchange(CDataExchange* pDX)
{
	CObjectPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COP_Flags)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COP_Flags, CObjectPage)
	//{{AFX_MSG_MAP(COP_Flags)
	ON_CLBN_CHKCHANGE(IDC_CHECKLIST, OnCheckListChange)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COP_Flags message handlers

void COP_Flags::UpdateData( int Mode, PVOID pData, bool bCanEdit )
{
	__super::UpdateData( Mode, pData, bCanEdit );

	if(!IsWindow(m_hWnd) || !pData)
	{
		return;
	}
	
	CEditGameClass *pObj = (CEditGameClass*) pData;

	if (Mode == LoadFirstData)
	{
		UpdateForClass(pObj);
		
	}
	else if (Mode == LoadData)
	{
		MergeForClass(pObj);
	}
    CreateCheckList();

	m_CheckList.EnableWindow( m_bCanEdit ? TRUE : FALSE );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool COP_Flags::SaveData(void)
{
	if (!IsWindow(m_hWnd))
	{
		return(false);
	}

	//
	// Apply the dialog data to all the objects being edited.
	//
	FOR_EACH_OBJ( *m_pObjectList, pos )
	{
		CMapClass *pObject = m_pObjectList->Element(pos);
		CEditGameClass *pEdit = dynamic_cast <CEditGameClass *>(pObject);
		Assert(pEdit != NULL);

		if ( pEdit != NULL )
		{
			for ( int i = 0; i < m_CheckListItems.Count(); i++ )
			{
				CheckListItem currentItem = m_CheckListItems.Element( i );
				// don't save tri-stated bit
				if ( m_CheckList.GetCheck(i) != 2 )
				{
					pEdit->SetSpawnFlag( currentItem.nItemBit, m_CheckList.GetCheck(i) ? TRUE : FALSE );
				}
			}
		}
	}

	return(true);
}

//-----------------------------------------------------------------------------
// Purpose: This function is used to initialize the flag checklist.
//			It is called to place all the flags belonging to the first
//			selected object into the temporary CheckListItems vector
//-----------------------------------------------------------------------------

void COP_Flags::UpdateForClass(CEditGameClass* pObj)
{
	extern GameData *pGD;

	GDclass * pClass = pGD->ClassForName(pObj->GetClassName());

	if(!IsWindow(m_hWnd))
		return;

	m_nNumSelectedObjects = 1;

	m_CheckListItems.RemoveAll();

	if(pClass)
	{
		GDinputvariable *pVar = pClass->VarForName("spawnflags");

		if (pVar)
		{
			int nItems = pVar->GetFlagCount();		

			for ( int i = 0; i < nItems; i++ )
			{
				CheckListItem newItem;
				newItem.nItemBit = pVar->GetFlagMask( i );
				newItem.pszItemString = pVar->GetFlagCaption( i );
				newItem.state = pObj->GetSpawnFlag( newItem.nItemBit ) ? 1 : 0;
				m_CheckListItems.AddToTail( newItem );			
			}
		}
	}

	Assert( m_CheckListItems.Count() <= 32 );

	for ( int i = 0; i < 32; i++ )
	{
		int nBitPattern = 1 << i;
		// is spawnflag for this bit set?
		if ( pObj->GetSpawnFlag(nBitPattern) )
		{
			int j;
			// then see if its allowed to be
			for ( j = 0; j < m_CheckListItems.Count(); j ++ )
			{
				int nCheckListPattern = m_CheckListItems.Element(j).nItemBit;
				if ( nCheckListPattern == nBitPattern )
					break;
			}
			// we fail to find it?
			if ( j == m_CheckListItems.Count() )
			{
				CheckListItem newItem;
				newItem.nItemBit = nBitPattern;
				newItem.pszItemString = "????";
				newItem.state = 1;
				m_CheckListItems.AddToTail( newItem );
			}				
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: This function is called to combine flags when multiple objects are selected
//			It removes flags from the CheckListItem vector that are not present in all selected objects
//-----------------------------------------------------------------------------
void COP_Flags::MergeForClass(CEditGameClass* pObj)
{
	extern GameData *pGD;
	GDclass * pClass = pGD->ClassForName(pObj->GetClassName());

	if( !IsWindow(m_hWnd) )
		return;

	m_nNumSelectedObjects++;

	if( pClass )
	{
		GDinputvariable *pVar = pClass->VarForName("spawnflags");

		for ( int i = m_CheckListItems.Count() - 1; i >= 0; i-- )
		{	
			bool bFound = false;
			CheckListItem currentItem = m_CheckListItems.Element( i ); 
			if ( pVar )
			{
				for ( int j = 0; j < pVar->GetFlagCount(); j++ )
				{
					CheckListItem newItem;
					newItem.nItemBit = pVar->GetFlagMask(j);
					newItem.pszItemString = pVar->GetFlagCaption(j);
					if ( newItem == currentItem )
					{
						bFound = true;
						int nNewState = pObj->GetSpawnFlag( newItem.nItemBit ) ? 1 : 0;
						if ( currentItem.state != nNewState )
						{
							m_CheckListItems.Element( i ).state = 2;
						}
						break;
					}
				}
			}
			if ( !bFound )
			{
				m_CheckListItems.FastRemove( i );
			}
		}
	}
	Assert( m_CheckListItems.Count() <= 32 );	
}

//-----------------------------------------------------------------------------
// Purpose: Creates the checklist by stepping through the CheckListItems vector that
//			was created during Update/MergeForClass
//-----------------------------------------------------------------------------
void COP_Flags::CreateCheckList()
{
	m_CheckList.ResetContent();
	
	if ( m_nNumSelectedObjects > 1 )
	{
		m_CheckList.SetCheckStyle(BS_AUTO3STATE);
	}

	for ( int i = 0; i < m_CheckListItems.Count(); i++ )
	{
		CheckListItem newItem = m_CheckListItems.Element(i);
		m_CheckList.InsertString(i, newItem.pszItemString);
		m_CheckList.SetCheck(i, newItem.state);
	}
}

void COP_Flags::OnUpdateSpawnFlags( unsigned long value )
{
	for ( int i=0; i < m_CheckListItems.Count(); i++ )
	{
		CheckListItem &item = m_CheckListItems[i];
		m_CheckList.SetCheck( i, (value & item.nItemBit) != 0 );
	}
}

BOOL COP_Flags::OnInitDialog() 
{	
	CObjectPage::OnInitDialog();

	m_nNumSelectedObjects = 0;

	// Subclass checklistbox
	m_CheckList.SubclassDlgItem(IDC_CHECKLIST, this);
	m_CheckList.SetCheckStyle(BS_AUTOCHECKBOX);
	m_CheckList.ResetContent();

	CAnchorDef anchorDefs[] =
	{
		CAnchorDef( IDC_CHECKLIST, k_eSimpleAnchorAllSides )
	};
	m_AnchorMgr.Init( GetSafeHwnd(), anchorDefs, ARRAYSIZE( anchorDefs ) );
	
	return TRUE;	             
}

void COP_Flags::OnCheckListChange() 
{
	if ( !m_pEntityPage )
		return;

	unsigned long bitsSet = 0;
	unsigned long triStateMask = 0;

	// This is just like SaveData.. collect the state of all the checks.	
	for ( int i = 0; i < m_CheckListItems.Count(); i++ )
	{
		CheckListItem currentItem = m_CheckListItems.Element( i );
		
		// If multiple of the selected entities have a different value for this flag,
		// note that. The entity page will use triStateMask to denote flags that
		// it should leave alone.
		if ( m_CheckList.GetCheck(i) == 2 )
			triStateMask |= currentItem.nItemBit;
		else if ( m_CheckList.GetCheck( i ) )
			bitsSet |= currentItem.nItemBit;
	}

	m_pEntityPage->OnUpdateSpawnFlags( triStateMask, bitsSet );    
}

void COP_Flags::OnSize( UINT nType, int cx, int cy )
{
	m_AnchorMgr.OnSize();
}
