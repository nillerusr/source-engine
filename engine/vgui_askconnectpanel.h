//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef VGUI_ASKCONNECTPANEL_H
#define VGUI_ASKCONNECTPANEL_H
#ifdef _WIN32
#pragma once
#endif


void SetupDefaultAskConnectAcceptKey();
vgui::Panel* CreateAskConnectPanel( vgui::VPANEL parent );

void ShowAskConnectPanel( const char *pConnectToHostName, float flDuration );
void HideAskConnectPanel();

bool IsAskConnectPanelActive( char *pHostName, int maxHostNameBytes );


#endif // VGUI_ASKCONNECTPANEL_H
