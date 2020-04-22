//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//--------------------------------------------------------------------------------------------------------------
// download.h
// 
// Header file for optional HTTP asset downloading
// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004
//--------------------------------------------------------------------------------------------------------------

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

//--------------------------------------------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------------------------------------------
void	CL_HTTPStop_f(void);
bool	CL_DownloadUpdate(void);
void	CL_QueueDownload( const char *filename );
bool	CL_FileReceived( const char *filename, unsigned int requestID );
bool	CL_FileDenied( const char *filename, unsigned int requestID );
int		CL_GetDownloadQueueSize(void);
int		CL_CanUseHTTPDownload(void);
void	CL_MarkMapAsUsingHTTPDownload(void);
bool	CL_IsGamePathValidAndSafeForDownload( const char *pGamePath );

#endif // DOWNLOAD_H
