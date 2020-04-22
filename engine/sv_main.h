//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose functions from sv_main.cpp.
//
// $NoKeywords: $
//===========================================================================//

#ifndef SV_MAIN_H
#define SV_MAIN_H


#include "edict.h"
#include "packed_entity.h"
#include "utlvector.h"
#include "convar.h"
#include "netadr.h"
#include "checksum_crc.h"
#include "soundflags.h"
#include "tier1/bitbuf.h"

class CGameClient;

//===========================================================

// sv_main.c

// Which areas are we going to transmit (usually 1, but with portals you can see into multiple other areas).
extern CUtlVector<int> g_AreasNetworked;

void SV_Frame( bool send_client_updates );
void SV_FrameExecuteThreadDeferred();

void SV_InitGameDLL( void );

void SV_ReplicateConVarChange( ConVar const *var, char const *newValue );
void SV_ExecuteRemoteCommand( const char *pCommand, int nClientSlot = -1 );
void SV_InstallClientStringTableMirrors( void );

void SV_ResetModInfo( void );

class IRecipientFilter;
void SV_StartSound ( IRecipientFilter& filter, edict_t *pSoundEmittingEntity, int iChannel, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP, const Vector *pOrigin, float soundtime, int speakerentity, CUtlVector< Vector >* pUtlVecOrigins );


int SV_ModelIndex (const char *name);
int SV_FindOrAddModel (const char *name, bool preload );
int SV_SoundIndex (const char *name);
int SV_FindOrAddSound(const char *name, bool preload );
int SV_GenericIndex(const char *name);
int SV_FindOrAddGeneric(const char *name, bool preload );
int SV_DecalIndex(const char *name);
int SV_FindOrAddDecal(const char *name, bool preload );

void SV_ForceSimpleMaterial( const char *name );
void SV_ForceModelBounds( const char *name, const Vector &mins, const Vector &maxs );

void SV_Physics( bool bIsSimulating );
void SV_PreClientUpdate( bool bIsSimulating );

class IServerEntity;

void SV_ExecuteClientMessage (CGameClient *cl);

bool SV_ActivateServer();
void SV_InitGameServerSteam();

#ifdef ENABLE_RPT
void SV_NotifyRPTOfDisconnect( int nClientSlot );
#endif // ENABLE_RPT

// sv_redirect.cpp

enum redirect_t
{
	RD_NONE = 0, // server console
	RD_CLIENT, // client console
	RD_PACKET, // connectionless UDP
	RD_SOCKET // TCP/IP remote socket
};

bool SV_RedirectActive( void );
void SV_RedirectAddText( const char *txt );
void SV_RedirectStart( redirect_t rd, const netadr_t *addr );
void SV_RedirectEnd( void );


class ServerClass;
class IClient;
class CClientFrame;


// Builds an alternate copy of the datatable for any classes that have datatables with props excluded.
void SV_InitSendTables( ServerClass *pClasses );
void SV_TermSendTables( ServerClass *pClasses );

// send voice data from cl to other clients
void SV_BroadcastVoiceData(IClient * cl, int nBytes, char * data, int64 xuid);
void SV_SendRestoreMsg( bf_write &dest );

// A client has uploaded its logo to us;
void SV_SendLogo( CRC32_t& logoCRC );
void SV_PruneRequestList( void );


/*
=============
Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
void SV_ResetPVS( byte* pvs, int pvssize );
void SV_AddOriginToPVS( const Vector& origin );

extern CGlobalVars g_ServerGlobalVariables;

void SV_FlushMemoryOnNextServer();

#endif


