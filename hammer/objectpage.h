//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OBJECTPAGE_H
#define OBJECTPAGE_H
#ifdef _WIN32
#pragma once
#endif


#include "MapClass.h"


class CObjectPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CObjectPage)

public:

	CObjectPage(void)
	{
		m_bMultiEdit = false;
		m_bFirstTimeActive = true;
		m_bHasUpdatedData = false;
	}

	CObjectPage(UINT nResourceID) : CPropertyPage(nResourceID) 
	{
		m_bMultiEdit = false;
		m_bFirstTimeActive = false;
	}

	~CObjectPage() {}

	virtual void MarkDataDirty() {}

	enum
	{
		LoadFirstData,
		LoadData,
		LoadFinished,
	};

	inline void SetObjectList(const CMapObjectList *pObjectList);

	// Called by the sheet to update the selected objects. pData points to the object being added to the selection.
	virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );
	
	// Called by the sheet to store this page's data into the objects being edited.
	virtual bool SaveData(void) { return(true); }

	// Called by the sheet to let the dialog remember its state before a refresh of the data.
	virtual void RememberState(void) {}

	virtual void SetMultiEdit(bool b) { m_bMultiEdit = b; }
	virtual void OnShowPropertySheet(BOOL bShow, UINT nStatus) {}

	bool IsMultiEdit() { return m_bMultiEdit; }

	CRuntimeClass * GetEditObjectRuntimeClass(void) { return m_pEditObjectRuntimeClass; }
	PVOID GetEditObject();

	BOOL OnSetActive(void);
	BOOL OnApply(void) { return(TRUE); }

	bool m_bFirstTimeActive;					// Used to detect the first time this page becomes active.
	bool m_bHasUpdatedData;						// Used to prevent SaveData() called on pages that haven't had loaded the data yet.

	// Set while we are changing the page layout.
	static BOOL s_bRESTRUCTURING;

protected:

	const CMapObjectList *m_pObjectList;		// The list of objects that we are editing.
	bool m_bMultiEdit;							// Set to true if we are editing more than one object.
	bool m_bCanEdit;							// Set to true if this page allows for editing

	CRuntimeClass *m_pEditObjectRuntimeClass;	// The type of object that this page can edit.

	static char *VALUE_DIFFERENT_STRING;
};


//-----------------------------------------------------------------------------
// Purpose: Sets the list of objects that this dialog should reflect.
// Input  : pObjectList - List of objects (typically the selection list).
//-----------------------------------------------------------------------------
void CObjectPage::SetObjectList(const CMapObjectList *pObjectList)
{
	Assert(pObjectList != NULL);
	m_pObjectList = pObjectList;
}


#endif // OBJECTPAGE_H
