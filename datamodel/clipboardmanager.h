//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

class KeyValues;
class IClipboardCleanup;

// Clipboard:
//
class CClipboardManager
{
public:
	CClipboardManager();
	~CClipboardManager();

	void			EmptyClipboard( bool bClearWindowsClipboard );
	void			SetClipboardData( CUtlVector< KeyValues * >& data, IClipboardCleanup *pfnOptionalCleanuFunction );
	void			AddToClipboardData( KeyValues *add );
	void			GetClipboardData( CUtlVector< KeyValues * >& data );
	bool			HasClipboardData() const;
private:
	CUtlVector< KeyValues * >	m_Data;
	IClipboardCleanup			*m_pfnCleanup;
};

#endif // CLIPBOARDMANAGER_H
