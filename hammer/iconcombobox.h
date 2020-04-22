//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ICONCOMBOBOX_H
#define ICONCOMBOBOX_H
#pragma once

//=============================================================================
//
// Icon Combo Box
//
// NOTE: the combo box setting should contain the following:
//	Type: DropList	
//	Owner Draw: Variable
//  HasStrings: checked
//	VSCROLL
//
class CIconComboBox : public CComboBox
{
public:

	//=========================================================================
	//
	// Construction/Deconstruction
	//
	CIconComboBox();
	virtual ~CIconComboBox();
 
	void Init( void );

	//=========================================================================
	//
	// Operations
	//
    int AddIcon( LPCTSTR pIconName );
	int InsertIcon( LPCTSTR pIconName, int ndx );
	int SelectIcon( LPCTSTR pIconName );
	int SelectIcon( int ndx );
	int DeleteIcon( LPCTSTR pIconName );
	int DeleteIcon( int ndx );

//protected:

	CSize m_IconSize;				//	icon dimensions

	//=========================================================================
	//
	// Overloaded String Operations
	//
	int AddString( LPCTSTR lpszString );
	int InsertString( int nIndex, LPCTSTR lpszString );
	int DeleteString( int nIndex );

	//=========================================================================
	//
	// Overrides
	//
	void MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct );
	void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	//=========================================================================
	//
	//
	//
	void OnDrawIcon( LPDRAWITEMSTRUCT lpDrawItemStruct );
	void SetDisabledBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, CBrush **ppOldBrush, CPen **ppOldPen );
	void SetUnSelectedBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, CBrush **ppOldBrush, CPen **ppOldPen );
	void SetSelectedBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, CBrush **ppOldBrush, CPen **ppOldPen );
	void ResetBrushAndPen( LPDRAWITEMSTRUCT lpDrawItemStruct, CBrush *pBrush, CPen *pPen );
};

#endif // ICONCOMBOBOX_H