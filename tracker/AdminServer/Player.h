//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYER_H
#define PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_ListPanel.h>

//-----------------------------------------------------------------------------
// Purpose: Data describing a single player
//-----------------------------------------------------------------------------
typedef struct 
{
	
	char name[100];
	int ping;
	int userid;
	char authid[20];
	int loss;
	int frags;
	float time;
	char addr[4];
} Players_t;


#endif // PLAYER_H
