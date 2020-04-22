//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef POINT_H
#define POINT_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: Data describing a point used on the graph, dervied from the "stats" rcon command
//-----------------------------------------------------------------------------

typedef struct {
	float cpu; // the percent CPU usage
	float in; // the ingoing bandwidth in kB/sec
	float out; // the outgoing bandwidth in kB/sec
	float time; // the time this was recorded
	float fps; // the FPS of the server
	float ping; // the ping of the server
} Points_t;


#endif // POINT_H
