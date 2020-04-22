//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements an autoselection combo box that color codes the text
//			based on whether the current selection represents a single entity,
//			multiple entities, or an unresolved entity targetname.
//
//			The fonts are as follows:
//
//			Single entity		black, normal weight
//			Multiple entities	black, bold
//			Unresolved			red, normal weight
//
//=============================================================================//

#include "stdafx.h"
#include "MapEntity.h"
#include "TargetNameCombo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning( disable : 4355 )


BEGIN_MESSAGE_MAP(CTargetNameComboBox, CFilteredComboBox)
	//{{AFX_MSG_MAP(CTargetNameComboBox)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTargetNameComboBox::CTargetNameComboBox( CFilteredComboBox::ICallbacks *pPassThru ) : 
	BaseClass( this )
{
	m_pEntityList = NULL;
	m_pPassThru = pPassThru;
}


//-----------------------------------------------------------------------------
// Purpose: Frees allocated memory.
//-----------------------------------------------------------------------------
CTargetNameComboBox::~CTargetNameComboBox(void)
{
	FreeSubLists();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTargetNameComboBox::FreeSubLists(void)
{
	POSITION pos = m_SubLists.GetHeadPosition();
	while (pos != NULL)
	{
		CMapEntityList *pList = m_SubLists.GetNext(pos);
		delete pList;
	}

	m_SubLists.RemoveAll();
}


void CTargetNameComboBox::CreateFonts()
{
	//
	// Create a normal and bold font.
	//
	if (!m_BoldFont.m_hObject)
	{
		CFont &nf = GetNormalFont();
		
		if ( nf.m_hObject )
		{
			LOGFONT LogFont;
			nf.GetLogFont(&LogFont);
			LogFont.lfWeight = FW_BOLD;
			m_BoldFont.CreateFontIndirect(&LogFont);
		}
	}
}


CTargetNameComboBox* CTargetNameComboBox::Create( CFilteredComboBox::ICallbacks *pCallbacks, DWORD dwStyle, RECT rect, CWnd *pParentWnd, UINT nID )
{
	CTargetNameComboBox *pRet = new CTargetNameComboBox( pCallbacks );
	pRet->BaseClass::Create( dwStyle, rect, pParentWnd, nID );
	return pRet;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches an entity list to the combo box. This list will be used
//			for matching targetnames to entities in the world.
// Input  : pEntityList - The beauty of Hungarian notation and meaningful naming
//				makes this comment utterly unnecessary.
//-----------------------------------------------------------------------------
void CTargetNameComboBox::SetEntityList(const CMapEntityList *pEntityList)
{
	// We want all notifications, even if the current text doesn't match an exact entity name.
	SetOnlyProvideSuggestions( false );
	
	// Setup the list.
	m_pEntityList = pEntityList;

	FreeSubLists();

	m_EntityLists.RemoveAll();

	if (m_pEntityList != NULL)
	{
		FOR_EACH_OBJ( *m_pEntityList, pos )
		{
			CMapEntity *pEntity = m_pEntityList->Element(pos);
			const char *pszTargetName = pEntity->GetKeyValue("targetname");
			if (pszTargetName != NULL)
			{
				//
				// If the targetname is not in the combo box, add it to the combo as the
				// first entry in an entity list. The list is necessary because there
				// may be several entities in the map with the same targetname.
				//
				int nIndex = m_EntityLists.Find( pszTargetName );
				if (nIndex == m_EntityLists.InvalidIndex())
				{
					CMapEntityList *pList = new CMapEntityList;
					pList->AddToTail(pEntity);

					m_EntityLists.Insert( pszTargetName, pList );

					//
					// Keep track of all the sub lists so we can delete them later.
					//
					m_SubLists.AddTail(pList);
				}
				//
				// Else append the entity to the given targetname's list.
				//
				else
				{
					CMapEntityList *pList = m_EntityLists[nIndex];
					pList->AddToTail(pEntity);
				}
			}
		}
	}

	// Setup the suggestions.
	CUtlVector<CString> suggestions;
	for ( int i=m_EntityLists.First(); i != m_EntityLists.InvalidIndex(); i=m_EntityLists.Next( i ) )
	{
		suggestions.AddToTail( m_EntityLists.GetElementName( i ) );
	}
	SetSuggestions( suggestions );
}


CMapEntityList* CTargetNameComboBox::GetSubEntityList( const char *pName )
{
	int testIndex = m_EntityLists.Find( pName );
	if ( testIndex != m_EntityLists.InvalidIndex() )
	{
		return m_EntityLists[testIndex];
	}
	
	return NULL;	
}


void CTargetNameComboBox::OnTextChanged( const char *pText )
{
	// Make sure our fonts are created.
	CreateFonts();
	
	// Update the fonts.
	int nCount = 0;
	CMapEntityList *pList = GetSubEntityList( pText );
	if ( pList )
		nCount = pList->Count();
	
	// Figure out the font and color that we want.
	CFont *pWantedFont = &m_BoldFont;
	if ( (nCount == 0) || (nCount == 1) )
		pWantedFont = &GetNormalFont();

	COLORREF clrWanted = RGB(255,0,0);
	if ( nCount > 0 )
		clrWanted = RGB(0,0,0);

	SetEditControlFont( *pWantedFont );
	SetEditControlTextColor( clrWanted );

	// Pass it through to the owner if they want notification.
	if ( m_pPassThru )
		m_pPassThru->OnTextChanged( pText );	
}
