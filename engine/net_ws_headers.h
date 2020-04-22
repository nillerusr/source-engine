//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_HEADERS_H
#define NET_WS_HEADERS_H
#ifdef _WIN32
#pragma once
#endif


#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#endif
#endif

#include "tier0/vcrmode.h"
#include "vstdlib/random.h"
#include "convar.h"
#include "tier0/icommandline.h"
#include "filesystem_engine.h"
#include "proto_oob.h"
#include "net_chan.h"
#include "inetmsghandler.h"
#include "protocol.h" // CONNECTIONLESS_HEADER
#include "sv_filter.h"
#include "sys.h"
#include "tier0/tslist.h"
#include "tier1/mempool.h"
#include "../utils/bzip2/bzlib.h"
#include "matchmaking.h"

#if defined(_WIN32)

#if !defined( _X360 )
#include <winsock.h>
#else
#include "winsockx.h"
#endif

// #include <process.h>
typedef int socklen_t;

#elif defined POSIX

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAEMSGSIZE			EMSGSIZE
#define WSAEADDRNOTAVAIL	EADDRNOTAVAIL
#define WSAEAFNOSUPPORT		EAFNOSUPPORT
#define WSAECONNRESET		ECONNRESET
#define WSAECONNREFUSED     ECONNREFUSED
#define WSAEADDRINUSE		EADDRINUSE
#define WSAENOTCONN			ENOTCONN

#define ioctlsocket ioctl
#define closesocket close

#undef SOCKET
typedef int SOCKET;
#define FAR

#endif

#include "sv_rcon.h"
#ifndef SWDS
#include "cl_rcon.h"
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#endif // NET_WS_HEADERS_H
