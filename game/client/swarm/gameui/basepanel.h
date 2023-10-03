//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPANEL_H
#define BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "swarm/basemodpanel.h"
inline BaseModUI::CBaseModPanel * BasePanel() { return &BaseModUI::CBaseModPanel::GetSingleton(); }

#endif // BASEPANEL_H
