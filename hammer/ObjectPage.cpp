//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "ObjectPage.h"
#include "GlobalFunctions.h"
#include "ObjectProperties.h"


//
// Used to indicate multiselect of entities with different keyvalues.
//
char *CObjectPage::VALUE_DIFFERENT_STRING = "(different)";

//
// Set while we are changing the page layout.
//
BOOL CObjectPage::s_bRESTRUCTURING = FALSE;


IMPLEMENT_DYNCREATE(CObjectPage, CPropertyPage)


//-----------------------------------------------------------------------------
// Purpose: stores whether or not this page can be updated
// Input  : Mode - unused
//			pData - unused
//			bCanEdit - the edit state
//-----------------------------------------------------------------------------
void CObjectPage::UpdateData( int Mode, PVOID pData, bool bCanEdit ) 
{ 
	m_bCanEdit = bCanEdit;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we become the active page.
//-----------------------------------------------------------------------------
BOOL CObjectPage::OnSetActive(void)
{
	//VPROF_BUDGET( "CObjectPage::OnSetActive", "Object Properties" );

	if (CObjectPage::s_bRESTRUCTURING || !GetActiveWorld())
	{
		return CPropertyPage::OnSetActive();
	}

	CObjectProperties *pParent = (CObjectProperties *)GetParent();
	
	pParent->UpdateAnchors( this );

	if (m_bFirstTimeActive)
	{
		m_bFirstTimeActive = false;
		pParent->LoadDataForPages(pParent->GetPageIndex(this));
	}

	return CPropertyPage::OnSetActive();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
PVOID CObjectPage::GetEditObject()
{ 
	//VPROF_BUDGET( "CObjectPage::GetEditObject", "Object Properties" );
	return ((CObjectProperties*) GetParent())->GetEditObject(GetEditObjectRuntimeClass());
}


