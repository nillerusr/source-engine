//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef _MYCHECKLISTBOX_H
#define _MYCHECKLISTBOX_H

// for SOME REASON CompareItem in a CCheckListBox has to be overridden.
// dumb MFC.

class CMyCheckListBox : public CCheckListBox
{
public:
	BOOL CompareItem(LPCOMPAREITEMSTRUCT lpcis)
	{ return 0; }
};


#endif // _MYCHECKLISTBOX_H