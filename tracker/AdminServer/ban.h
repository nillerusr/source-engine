//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BAN_H
#define BAN_H
#ifdef _WIN32
#pragma once
#endif

enum Bans { ID, WONID,AUTHID,IP };

//-----------------------------------------------------------------------------
// Purpose: Data describing a single player
//-----------------------------------------------------------------------------
typedef struct 
{
	
	char name[20]; // id of the band
	float time;
	Bans type; 
} Bans_t;


#endif // BAN_H
