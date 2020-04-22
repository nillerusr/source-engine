//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERLISTCOMPARE_H
#define SERVERLISTCOMPARE_H
#ifdef _WIN32
#pragma once
#endif

// these functions are common to most server lists in sorts
int __cdecl PasswordCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );
int __cdecl PingCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );
int __cdecl PlayersCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );
int __cdecl MapCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );
int __cdecl GameCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );
int __cdecl ServerNameCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 );

#endif // SERVERLISTCOMPARE_H
