//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// defines ids of parts of the status bar
//

enum
{
	ID_INDICATOR_FIRST = 0xE720,
	ID_INDICATOR_SELECTION,
	ID_INDICATOR_COORDS,
	ID_INDICATOR_SIZE,
	ID_INDICATOR_GRIDZOOM,
	ID_INDICATOR_SNAP,
	ID_INDICATOR_LIGHTPROGRESS
};

enum
{
	SBI_PROMPT = 0,
	SBI_SELECTION,
	SBI_COORDS,
	SBI_SIZE,
	SBI_GRIDZOOM,
	SBI_SNAP,
	SBI_LIGHTPROGRESS
};

// mainfrm.cpp:
void SetStatusText(int nIndex, LPCTSTR pszText);
