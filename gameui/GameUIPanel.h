//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUIPANEL_H
#define GAMEUIPANEL_H
#ifdef _WIN32
#pragma once
#endif

// Retrieve the root panel for the GameUI subsystem
namespace vgui
{
class Panel;
};

vgui::Panel *GetGameUIRootPanel( void );

#endif // GAMEUIPANEL_H
