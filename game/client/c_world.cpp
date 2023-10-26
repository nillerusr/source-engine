//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_world.h"
#include "ivmodemanager.h"
#include "decals.h"
#include "engine/ivmodelinfo.h"
#include "ivieweffects.h"
#include "shake.h"
#include "precache_register.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CWorld
#undef CWorld
#endif

C_GameRules *g_pGameRules = NULL;
static C_World *g_pClientWorld;


void ClientWorldFactoryInit()
{
	g_pClientWorld = new C_World;
}

void ClientWorldFactoryShutdown()
{
	delete g_pClientWorld;
	g_pClientWorld = NULL;
}

static IClientNetworkable* ClientWorldFactory( int entnum, int serialNum )
{
	Assert( g_pClientWorld != NULL );

	g_pClientWorld->Init( entnum, serialNum );
	return g_pClientWorld;
}


IMPLEMENT_CLIENTCLASS_FACTORY( C_World, DT_World, CWorld, ClientWorldFactory );

BEGIN_RECV_TABLE( C_World, DT_World )
	RecvPropFloat(RECVINFO(m_flWaveHeight)),
	RecvPropVector(RECVINFO(m_WorldMins)),
	RecvPropVector(RECVINFO(m_WorldMaxs)),
	RecvPropInt(RECVINFO(m_bStartDark)),
	RecvPropFloat(RECVINFO(m_flMaxOccludeeArea)),
	RecvPropFloat(RECVINFO(m_flMinOccluderArea)),
	RecvPropFloat(RECVINFO(m_flMaxPropScreenSpaceWidth)),
	RecvPropFloat(RECVINFO(m_flMinPropScreenSpaceWidth)),
	RecvPropString(RECVINFO(m_iszDetailSpriteMaterial)),
	RecvPropInt(RECVINFO(m_bColdWorld)),
	RecvPropInt(RECVINFO(m_iTimeOfDay)),
END_RECV_TABLE()


C_World::C_World( void )
{
}

C_World::~C_World( void )
{
}

bool C_World::Init( int entnum, int iSerialNum )
{
	m_flWaveHeight = 0.0f;

	return BaseClass::Init( entnum, iSerialNum );
}

void C_World::Release()
{
	Term();
}

void C_World::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );
}

void C_World::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Always force reset to normal mode upon receipt of world in new map
	if ( updateType == DATA_UPDATE_CREATED )
	{
		modemanager->SwitchMode( false, true );

		if ( m_bStartDark )
		{
			ScreenFade_t sf;
			memset( &sf, 0, sizeof( sf ) );
			sf.a = 255;
			sf.r = 0;
			sf.g = 0;
			sf.b = 0;
			sf.duration = (float)(1<<SCREENFADE_FRACBITS) * 5.0f;
			sf.holdTime = (float)(1<<SCREENFADE_FRACBITS) * 1.0f;
			sf.fadeFlags = FFADE_IN | FFADE_PURGE;
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
				GetViewEffects()->Fade( sf );
			}
		}

		OcclusionParams_t params;
		params.m_flMaxOccludeeArea = m_flMaxOccludeeArea;
		params.m_flMinOccluderArea = m_flMinOccluderArea;
		engine->SetOcclusionParameters( params );

		modelinfo->SetLevelScreenFadeRange( m_flMinPropScreenSpaceWidth, m_flMaxPropScreenSpaceWidth );
	}
}

// -----------------------------------------
//	Sprite Index info
// -----------------------------------------
int		g_sModelIndexLaser;			// holds the index for the laser beam
int		g_sModelIndexLaserDot;		// holds the index for the laser beam dot
int		g_sModelIndexFireball;		// holds the index for the fireball
int		g_sModelIndexSmoke;			// holds the index for the smoke cloud
int		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
int		g_sModelIndexBubbles;		// holds the index for the bubbles model
int		g_sModelIndexBloodDrop;		// holds the sprite index for the initial blood
int		g_sModelIndexBloodSpray;	// holds the sprite index for splattered blood

//-----------------------------------------------------------------------------
// Purpose: Precache global weapon resources
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, WeaponSprites )
	PRECACHE_INDEX( MODEL, "sprites/zerogxplode.vmt", g_sModelIndexFireball )
	PRECACHE_INDEX( MODEL, "sprites/WXplo1.vmt", g_sModelIndexWExplosion )
	PRECACHE_INDEX( MODEL, "sprites/steam1.vmt", g_sModelIndexSmoke )
	PRECACHE_INDEX( MODEL, "sprites/bubble.vmt", g_sModelIndexBubbles )
	PRECACHE_INDEX( MODEL, "sprites/bloodspray.vmt", g_sModelIndexBloodSpray )
	PRECACHE_INDEX( MODEL, "sprites/blood.vmt", g_sModelIndexBloodDrop )
	PRECACHE_INDEX( MODEL, "sprites/laserbeam.vmt", g_sModelIndexLaser )
	PRECACHE_INDEX( MODEL, "sprites/laserdot.vmt", g_sModelIndexLaserDot )
PRECACHE_REGISTER_END()

void W_Precache(void)
{
	PrecacheFileWeaponInfoDatabase( filesystem, g_pGameRules->GetEncryptionKey() );
}

void C_World::Precache( void )
{
	// Get weapon precaches
	W_Precache();	
}

void C_World::Spawn( void )
{
	Precache();
}



C_World *GetClientWorldEntity()
{
	Assert( g_pClientWorld != NULL );
	return g_pClientWorld;
}

