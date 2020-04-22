//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPS_H
#define MAPS_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: Data describing a single map
//-----------------------------------------------------------------------------
typedef struct 
{
	
	char name[100];
	int type;
} Maps_t;


#endif // MAPS_H
