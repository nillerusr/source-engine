//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERLISTCOMPARE_H
#define PLAYERLISTCOMPARE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/ListPanel.h>	

// these functions are common to most server lists in sorts
int __cdecl PlayerNameCompare(const void *elem1, const void *elem2 );
int __cdecl PlayerAuthCompare(const void *elem1, const void *elem2 );
int __cdecl PlayerPingCompare(const void *elem1, const void *elem2 );
int __cdecl PlayerLossCompare(const void *elem1, const void *elem2 );
int __cdecl PlayerTimeCompare( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 );
int __cdecl PlayerFragsCompare(const void *elem1, const void *elem2 );


#endif // PLAYERLISTCOMPARE_H
