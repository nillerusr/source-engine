//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines an autoselection combo box that color codes the text
//			based on whether the current selection represents a single entity,
//			multiple entities, or an unresolved entity targetname.
//
//			The fonts are as follows:
//
//			Single entity		black, normal weight
//			Multiple entities	black, bold
//			Unresolved			red, normal weight
//
// $NoKeywords: $
//=============================================================================//

#ifndef TARGETNAMECOMBO_H
#define TARGETNAMECOMBO_H
#ifdef _WIN32
#pragma once
#endif

#include <afxtempl.h>
#include "FilteredComboBox.h"
#include "utldict.h"

class CTargetNameComboBox : public CFilteredComboBox, protected CFilteredComboBox::ICallbacks
{
	typedef CFilteredComboBox BaseClass;

	public:

		CTargetNameComboBox( CFilteredComboBox::ICallbacks *pCallbacks );
		~CTargetNameComboBox(void);

		// For dynamic creation.
		static CTargetNameComboBox* Create( CFilteredComboBox::ICallbacks *pCallbacks, DWORD dwStyle, RECT rect, CWnd *pParentWnd, UINT nID );

		// Initialize the control with the entity list you want it to represent.
		void SetEntityList(const CMapEntityList *pEntityList);
		
		// Gets the list of entities with this name. The data is valid until the next SetEntityList call.
		CMapEntityList* GetSubEntityList( const char *pName );

	protected:

		// CFilteredComboBox::ICallbacks
		virtual void OnTextChanged( const char *pText );

		void FreeSubLists(void);
		void CreateFonts();

	protected:

		CUtlDict<CMapEntityList*,int> m_EntityLists;

		const CMapEntityList *m_pEntityList;
		CTypedPtrList<CPtrList, CMapEntityList *> m_SubLists;
		
		CFilteredComboBox::ICallbacks *m_pPassThru;

		CFont m_BoldFont;						// Bold font used when there are multiple matches.

		DECLARE_MESSAGE_MAP()
};


#endif // TARGETNAMECOMBO_H
