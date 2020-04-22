//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef UDP_PROCESS_H
#define UDP_PROCESS_H
#ifdef _WIN32
#pragma once
#endif

class CUDPSocket;
class IConnectionlessPacketHandler;
class ILookupChannel;
class CNetPacket;

void UDP_ProcessSocket( CUDPSocket *socket, IConnectionlessPacketHandler *handler, ILookupChannel *lookup );
void UDP_ProcessConnectionlessPacket( CNetPacket *pPacket, IConnectionlessPacketHandler *pHandler );

#endif // UDP_PROCESS_H
