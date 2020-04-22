//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SV_UPLOADGAMESTATS_H
#define SV_UPLOADGAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

class IUploadGameStats;
extern IUploadGameStats *g_pUploadGameStats;

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob );
void AsyncUpload_Shutdown();

#endif // SV_UPLOADGAMESTATS_H
