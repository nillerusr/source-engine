//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: simple TCP socket API for communicating as a TCP client over a TEXT
//			connection
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SIMPLESOCKET_H
#define SIMPLESOCKET_H
#pragma once

// opaque socket type
typedef struct socket_s socket_t, *HSOCKET;

// Socket reporting function
typedef void ( *REPORTFUNCTION )( HSOCKET socket, const char *pString );

extern HSOCKET		SocketOpen( const char *pServerName, int port );
extern void			SocketClose( HSOCKET socket );
extern void			SocketSendString( HSOCKET socket, const char *pString );

// This should probably return the data so we can handle errors and receive data
extern void			SocketWait( HSOCKET socket, const char *pString );

// sets the reporting function
extern void			SocketReport( REPORTFUNCTION pReportFunction );
extern void			SocketInit( void );
extern void			SocketExit( void );




#endif // SIMPLESOCKET_H
