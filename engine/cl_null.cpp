//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: replaces the cl_*.cpp files with stubs
//
//=============================================================================//

#include "client_pch.h"
#ifdef SWDS
#include "hltvclientstate.h"
#include "convar.h"
#include "enginestats.h"
#include "bspfile.h" // dworldlight_t
#include "audio/public/soundservice.h"
#include "tier0/systeminformation.h"

ISoundServices *g_pSoundServices = NULL;
Vector		listener_origin;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MAXPRINTMSG	4096

CEngineStats g_EngineStats;

ClientClass *g_pClientClassHead = NULL;

bool CL_IsHL2Demo()
{
	return false;
}

bool CL_IsPortalDemo()
{
	return false;
}

bool HandleRedirectAndDebugLog( const char *msg );

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode ) {}
void RefreshScreenIfNecessary() {}
void EndLoadingUpdates() {}


void Con_ColorPrintf( const Color& clr, const char *fmt, ... )
{
    va_list argptr; 
	char		msg[MAXPRINTMSG];
	static bool	inupdate;

	if ( !host_initialized )
		return;

	va_start (argptr,fmt);
	Q_vsnprintf (msg,sizeof( msg ), fmt,argptr);
	va_end (argptr);

	if ( !HandleRedirectAndDebugLog( msg ) )
	{
		return;
	}
	return;
//	printf( "%s", msg );
}

void Con_NPrintf( int pos, const char *fmt, ... )
{
	va_list		argptr;
	char		text[4096];
	va_start (argptr, fmt);
	Q_vsnprintf(text, sizeof( text ), fmt, argptr);
	va_end (argptr);

	return;
//	printf( "%s", text );
}

void SCR_UpdateScreen (void)
{
}

void SCR_EndLoadingPlaque (void)
{
}

void ClientDLL_FrameStageNotify( ClientFrameStage_t frameStage )
{
}

ClientClass *ClientDLL_GetAllClasses( void )
{
	return g_pClientClassHead;
}

#define LIGHT_MIN_LIGHT_VALUE 0.03f

float ComputeLightRadius( dworldlight_t *pLight, bool bIsHDR )
{
	float flLightRadius = pLight->radius;
	if (flLightRadius == 0.0f)
	{
		// Compute the light range based on attenuation factors
		float flIntensity = sqrtf( DotProduct( pLight->intensity, pLight->intensity ) );
		if (pLight->quadratic_attn == 0.0f)
		{
			if (pLight->linear_attn == 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (flIntensity / LIGHT_MIN_LIGHT_VALUE - pLight->constant_attn) / pLight->linear_attn;
			}
		}
		else
		{
			float a = pLight->quadratic_attn;
			float b = pLight->linear_attn;
			float c = pLight->constant_attn - flIntensity / LIGHT_MIN_LIGHT_VALUE;
			float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flLightRadius < 0)
					flLightRadius = 0;
			}
		}
	}

	return flLightRadius;
}



CClientState::CClientState() {}
CClientState::~CClientState() {}
void CClientState::ConnectionClosing( const char * reason ) {}
void CClientState::ConnectionCrashed( const char * reason ) {}
bool CClientState::ProcessConnectionlessPacket( netpacket_t *packet ){ return false; }
void CClientState::PacketStart(int incoming_sequence, int outgoing_acknowledged) {}
void CClientState::PacketEnd( void ) {}
void CClientState::FileReceived( const char *fileName, unsigned int transferID ) {}
void CClientState::FileRequested( const char *fileName, unsigned int transferID ) {}
void CClientState::FileDenied(const char *fileName, unsigned int transferID ) {}
void CClientState::FileSent( const char *fileName, unsigned int transferID ) {}
void CClientState::Disconnect( const char *pszReason, bool showmainmenu  ) {}
void CClientState::FullConnect( netadr_t &adr ) {}
bool CClientState::SetSignonState ( int state, int count ) { return false;}
void CClientState::SendClientInfo( void ) {}
void CClientState::SendServerCmdKeyValues( KeyValues *pKeyValues ) {}
void CClientState::InstallStringTableCallback( char const *tableName ) {}
bool CClientState::InstallEngineStringTableCallback( char const *tableName ) { return false;}
void CClientState::ReadEnterPVS( CEntityReadInfo &u ) {}
void CClientState::ReadLeavePVS( CEntityReadInfo &u ) {}
void CClientState::ReadDeltaEnt( CEntityReadInfo &u ) {}
void CClientState::ReadPreserveEnt( CEntityReadInfo &u ) {}
void CClientState::ReadDeletions( CEntityReadInfo &u ) {}
const char *CClientState::GetCDKeyHash( void ) { return "123";}
void CClientState::Clear( void ) {}
bool CClientState::ProcessGameEvent(SVC_GameEvent *msg) { return true; }
bool CClientState::ProcessUserMessage(SVC_UserMessage *msg) { return true; }
bool CClientState::ProcessEntityMessage(SVC_EntityMessage *msg) { return true; }
bool CClientState::ProcessBSPDecal( SVC_BSPDecal *msg ) { return true; }
bool CClientState::ProcessCrosshairAngle( SVC_CrosshairAngle *msg ) { return true; }
bool CClientState::ProcessFixAngle( SVC_FixAngle *msg ) { return true; }
bool CClientState::ProcessVoiceData( SVC_VoiceData *msg ) { return true; }
bool CClientState::ProcessVoiceInit( SVC_VoiceInit *msg ) { return true; }
bool CClientState::ProcessSetPause( SVC_SetPause *msg ) { return true; }
bool CClientState::ProcessSetPauseTimed( SVC_SetPauseTimed *msg ) { return true; }
bool CClientState::ProcessClassInfo( SVC_ClassInfo *msg ) { return true; }
bool CClientState::ProcessStringCmd( NET_StringCmd *msg ) { return true; }
bool CClientState::ProcessServerInfo( SVC_ServerInfo *msg ) { return true; }
bool CClientState::ProcessTick( NET_Tick *msg ) { return true; }
bool CClientState::ProcessTempEntities( SVC_TempEntities *msg ) { return true; }
bool CClientState::ProcessPacketEntities( SVC_PacketEntities *msg ) { return true; }
bool CClientState::ProcessSounds( SVC_Sounds *msg )	 { return true; }
bool CClientState::ProcessPrefetch( SVC_Prefetch *msg ) {return true;}
float CClientState::GetTime() const { return 0.0f;}
void CClientState::RunFrame() {}
bool CClientState::HookClientStringTable( char const *tableName ) { return false; }

CClientState	cl;

char g_minidumpinfo[ 4096 ] = {0};
PAGED_POOL_INFO_t g_pagedpoolinfo = { 0 };

#endif
