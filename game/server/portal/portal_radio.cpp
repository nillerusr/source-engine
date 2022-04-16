//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=====================================================================================//

#include "cbase.h"					// for pch
#include "props.h"
#include "filters.h"
#include "achievementmgr.h"

extern CAchievementMgr g_AchievementMgrPortal;

#define RADIO_MODEL_NAME "models/props/radio_reference.mdl"
//#define RADIO_DEBUG_SERVER

class CDinosaurSignal : public CBaseEntity 
{
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
	DECLARE_CLASS( CDinosaurSignal, CBaseEntity );
	void Spawn();
	int UpdateTransmitState();
#if RADIO_DEBUG_SERVER
	int DrawDebugTextOverlays( void );
#endif

	CNetworkString( m_szSoundName, 128 );
	CNetworkVar( float, m_flInnerRadius );
	CNetworkVar( float, m_flOuterRadius );
	CNetworkVar( int, m_nSignalID );
};

LINK_ENTITY_TO_CLASS( updateitem1, CDinosaurSignal );

BEGIN_DATADESC( CDinosaurSignal )
	DEFINE_AUTO_ARRAY( m_szSoundName, FIELD_CHARACTER ),
	DEFINE_FIELD( m_flOuterRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flInnerRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_nSignalID, FIELD_INTEGER ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CDinosaurSignal, DT_DinosaurSignal )
	SendPropString( SENDINFO(m_szSoundName) ), 
	SendPropFloat( SENDINFO(m_flOuterRadius) ),
	SendPropFloat( SENDINFO(m_flInnerRadius) ),
	SendPropInt( SENDINFO(m_nSignalID) ),
END_SEND_TABLE()

void CDinosaurSignal::Spawn()
{
	PrecacheScriptSound( m_szSoundName.Get() );
	BaseClass::Spawn();
	SetTransmitState( FL_EDICT_ALWAYS );
}

int CDinosaurSignal::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}

#if RADIO_DEBUG_SERVER
int CDinosaurSignal::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();
	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		NDebugOverlay::Sphere( GetAbsOrigin(), GetAbsAngles(), m_flInnerRadius, 255, 0, 0, 64, false, 0.1f );
		NDebugOverlay::Sphere( GetAbsOrigin(), GetAbsAngles(), m_flOuterRadius, 0, 255, 0, 64, false, 0.1f );
	}
	return text_offset;
}
#endif


class CPortal_Dinosaur : public CPhysicsProp 
{
public:
	DECLARE_CLASS( CPortal_Dinosaur, CPhysicsProp );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual void Spawn();
	virtual void Precache();
	virtual QAngle PreferredCarryAngles( void ) { return QAngle( 0, 180, 0 ); }
	virtual bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }
	virtual void Activate();


	CNetworkHandle( CDinosaurSignal, m_hDinosaur_Signal );
	CNetworkVar( bool, m_bAlreadyDiscovered );
};

LINK_ENTITY_TO_CLASS( updateitem2, CPortal_Dinosaur );

BEGIN_DATADESC( CPortal_Dinosaur )
	DEFINE_FIELD( m_hDinosaur_Signal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bAlreadyDiscovered, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CPortal_Dinosaur, DT_PropDinosaur )
	SendPropEHandle( SENDINFO( m_hDinosaur_Signal ) ),
	SendPropBool( SENDINFO( m_bAlreadyDiscovered ) ),
END_SEND_TABLE()

void CPortal_Dinosaur::Precache()
{
	PrecacheModel( RADIO_MODEL_NAME );

	PrecacheScriptSound( "Portal.room1_radio" );
	PrecacheScriptSound( "UpdateItem.Static" );
	PrecacheScriptSound( "UpdateItem.Dinosaur01" );
	PrecacheScriptSound( "UpdateItem.Dinosaur02" );
	PrecacheScriptSound( "UpdateItem.Dinosaur03" );
	PrecacheScriptSound( "UpdateItem.Dinosaur04" );
	PrecacheScriptSound( "UpdateItem.Dinosaur05" );
	PrecacheScriptSound( "UpdateItem.Dinosaur06" );
	PrecacheScriptSound( "UpdateItem.Dinosaur07" );
	PrecacheScriptSound( "UpdateItem.Dinosaur08" );
	PrecacheScriptSound( "UpdateItem.Dinosaur09" );
	PrecacheScriptSound( "UpdateItem.Dinosaur10" );
	PrecacheScriptSound( "UpdateItem.Dinosaur11" );
	PrecacheScriptSound( "UpdateItem.Dinosaur12" );
	PrecacheScriptSound( "UpdateItem.Dinosaur13" );
	PrecacheScriptSound( "UpdateItem.Dinosaur14" );
	PrecacheScriptSound( "UpdateItem.Dinosaur15" );
	PrecacheScriptSound( "UpdateItem.Dinosaur16" );
	PrecacheScriptSound( "UpdateItem.Dinosaur17" );
	PrecacheScriptSound( "UpdateItem.Dinosaur18" );
	PrecacheScriptSound( "UpdateItem.Dinosaur19" );
	PrecacheScriptSound( "UpdateItem.Dinosaur20" );
	PrecacheScriptSound( "UpdateItem.Dinosaur21" );
	PrecacheScriptSound( "UpdateItem.Dinosaur22" );
	PrecacheScriptSound( "UpdateItem.Dinosaur23" );
	PrecacheScriptSound( "UpdateItem.Dinosaur24" );
	PrecacheScriptSound( "UpdateItem.Dinosaur25" );
	PrecacheScriptSound( "UpdateItem.Dinosaur26" );

	PrecacheScriptSound( "UpdateItem.Fizzle" );
}


void CPortal_Dinosaur::Spawn()
{
	Precache();
	KeyValue( "model", RADIO_MODEL_NAME );
	m_spawnflags |= SF_PHYSPROP_START_ASLEEP;
	BaseClass::Spawn();
}

void CPortal_Dinosaur::Activate( void )
{
	// Find the current completion status of the dinosaurs
	uint64 fStateFlags = 0;
	CBaseAchievement *pTransmissionRecvd = dynamic_cast<CBaseAchievement *>(g_AchievementMgrPortal.GetAchievementByName("PORTAL_TRANSMISSION_RECEIVED"));
	if ( pTransmissionRecvd )
	{
		fStateFlags = pTransmissionRecvd->GetComponentBits();
	}

	if ( m_hDinosaur_Signal != NULL )
	{
		uint64 nId = m_hDinosaur_Signal.Get()->m_nSignalID;
		// See if we're already tripped
		if ( fStateFlags & ((uint64)1<<nId) )
		{
			m_bAlreadyDiscovered = true;
		}
	}

	BaseClass::Activate();
}

struct radiolocs 
{
	const char *mapname;
	const char *soundname;
	int			id;
	float		radiopos[3];
	float		radioang[3];
	float		soundpos[3];
	float		soundouterrad;
	float		soundinnerrad;
};
static const radiolocs s_radiolocs[] =
{
	{
		"testchmb_a_00",
		"UpdateItem.Dinosaur01",
		0,
		{ 0, 0, 0 },
		{ 0, 0, 0 },
		{ -506, -924, 161 },
		200,
		64
	},
	{
		"testchmb_a_00",
		"UpdateItem.Dinosaur02",
		1,
		{ -960, -634, 783 },
		{ 0, 90, 0 },
		{ -926.435, -256.323, 583 },
		200,
		64,
	},
	{
		"testchmb_a_01",
		"UpdateItem.Dinosaur03",
		2,
		{ 233, 393, 130 },
		{ 0, 225, 0 },
		{ 96, 160, -108 },
		224,
		128,
	},
	{
		"testchmb_a_01",
		"UpdateItem.Dinosaur04",
		3,
		{ -1439.89, 1076.04, 779.102 },
		{ 0, 270, 0 },
		{ -731, 735, 888 },
		400,
		64,
	},
	{
		// new entry
		"testchmb_a_02",
		"UpdateItem.Dinosaur05",
		4,
		{ 2, 65, 390 },
		{ 0, 270, 0 },
		{ -864, 192, 64},
		192,
		96,
	},
	{
		"testchmb_a_02",
		"UpdateItem.Dinosaur06",
		21,
		{ 111, 832, 577 },
		{ 0, 0, 0 },
		{ 918, 831, 512},
		192,
		96,
	},
	{
		"testchmb_a_03",
		"UpdateItem.Dinosaur07",
		5, 
		{ -53.2337, 78.181, 236 },
		{ 0, 225, 0 },
		{ 304, 0, -96 },
		256,
		128
	},
	// new entry
	{
		"testchmb_a_03",
		"UpdateItem.Dinosaur08",
		6, 
		{ 428.112, 0.22326, 1201 },
		{ 0, 180, 0 },
		{ -581.096, 193.694, 1351 },
		165,
		128
	},
	{
		"testchmb_a_04",
		"UpdateItem.Dinosaur09",
		7,
		{ 118, -56.6, -38.8 },
		{ 0, 180, 0 },
		{ -640, 256, 8 },
		512,
		128
	},
	// new entry
	{
		"testchmb_a_05",
		"UpdateItem.Dinosaur10",
		8, 
		{ 64, 144, 160 },
		{ 0, 270, 0 },
		{ 64, 740, 7 },
		350,
		128
	},
	{
		"testchmb_a_06",
		"UpdateItem.Dinosaur11",
		9, 
		{ 529, 315, 320 },
		{ 0, 270, 0 },
		{ 608, 128, -184 },
		384,
		160
	},
	{
		"testchmb_a_07",
		"UpdateItem.Dinosaur12",
		10, 
		{ 192, -1546, 1425 },
		{ 0, 113, 0 },
		{ 272, -496, 1328 },
		432,
		88
	},
	// new entry
	{
		"testchmb_a_07",
		"UpdateItem.Dinosaur13",
		11, 
		{ -144, -768, 256 },
		{ 0, 90, 0 },
		{ -192, -384, 176 },
		256,
		128
	},
	{
		"testchmb_a_08",
		"UpdateItem.Dinosaur14",
		12, 
		{ 267, -378, 256 },
		{ 0, 90, 0 },
		{ -560, 96, 320 },
		288,
		128,
	},
	{
		"testchmb_a_09",
		"UpdateItem.Dinosaur15",
		13, 
		{ 634, 1308, 256 },
		{ 0, 180, 0 },
		{ 386.699, 1792.43, 7},
		548,
		64
	},
	{
		"testchmb_a_10",
		"UpdateItem.Dinosaur16",
		14, 
		{ -1420, -2752, 76 },
		{ 0, 0, 0  },
		{ -1968, -2880, -334 },
		448,
		196,
	},
	// new entry
	{
		"testchmb_a_10",
		"UpdateItem.Dinosaur17",
		15, 
		{ 112, 1392, -63 },
		{ 0, 260, 0  },
		{ -189, 1220, 65 },
		192,
		128,
	},
	{
		"testchmb_a_11",
		"UpdateItem.Dinosaur18",
		16,
		{0,0,0},
		{0,0,0},
		{-512,644,64},
		192,
		96,
	},
	{
		"testchmb_a_13",
		"UpdateItem.Dinosaur19",
		17,
		{955,931,-267},
		{-90,0,0},
		{1472,-191,-12},
		256,
		128,
	},
	{
		"testchmb_a_14",
		"UpdateItem.Dinosaur20",
		18,
		{0,0,0},
		{0,0,0},
		{144,192,1288},
		807,
		128,
	},
	{
		"testchmb_a_14",
		"UpdateItem.Dinosaur21",
		22,
		{1285, 1344, 1412},
		{0,0,0},
		{2712, 894, 1011},
		200,
		120,
	},
	{
		"testchmb_a_14",
		"UpdateItem.Dinosaur22",
		23,
		{-952, 336, -256},
		{0,0,0},
		{-1144, -249, 3336},
		400,
		128,
	},
	{
		"testchmb_a_15",
		"UpdateItem.Dinosaur23",
		19,
		{-1529,293,-283},
		{0,90,0},
		{761,443,810},
		256,
		128,
	},
	{
		"escape_00",
		"UpdateItem.Dinosaur24",
		24,
		{192, -1344, -832},
		{0, 135, 0},
		{891, 322, -184},
		285,
		150,
	},
	{
		"escape_01",
		"UpdateItem.Dinosaur25",
		20,
		{0,0,0},
		{0,0,0},
		{-624, 1440, -464},
		512,
		128,
	},
	{
		"escape_02",
		"UpdateItem.Dinosaur26",
		25,
		{5504, 131, -1422},
		{0, 90, 0},
		{4218, 674, 8},
		300,
		100,
	},
};

class CSpawnDinosaurHack : CAutoGameSystem
{
public:
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();

	CPortal_Dinosaur *SpawnDinosaur( radiolocs& loc );
	CDinosaurSignal *SpawnSignal( radiolocs& loc );

	void ApplyMapSpecificHacks();
};

static CSpawnDinosaurHack g_SpawnRadioHack;

void CSpawnDinosaurHack::LevelInitPreEntity()
{
	UTIL_PrecacheOther( "updateitem2", RADIO_MODEL_NAME );

	ApplyMapSpecificHacks();
}

// Spawn all the Dinosaurs and sstv images
void CSpawnDinosaurHack::LevelInitPostEntity() 
{
	if ( gpGlobals->eLoadType == MapLoad_LoadGame )
	{
#if defined ( RADIO_DEBUG_SERVER )
		Msg( "Not spawning any Dinosaurs: Detected a map load\n" );
#endif

		return;
	}

	IAchievement *pHeartbreaker = g_AchievementMgrPortal.GetAchievementByName("PORTAL_BEAT_GAME");
	if ( pHeartbreaker == NULL || pHeartbreaker->IsAchieved() == false )
	{
#if defined ( RADIO_DEBUG_SERVER )
		Msg( "Not spawning any Dinosaurs: Player has not beat the game, or failed to get heartbreaker achievement from mgr\n" );
#endif

		return;
	}

	for ( int i = 0; i < ARRAYSIZE( s_radiolocs ); ++i )
	{
		radiolocs loc = s_radiolocs[i];
		if ( V_strcmp( STRING(gpGlobals->mapname), loc.mapname ) == 0 )
		{
#if defined ( RADIO_DEBUG_SERVER )
			Msg( "Found Dinosaur and signal info for %s, spawning.\n", loc.mapname );
			Msg( "Dinosaur pos: %f %f %f, ang: %f %f %f\n", loc.radiopos[0], loc.radiopos[1], loc.radiopos[2], loc.radioang[0], loc.radioang[1], loc.radioang[2] );
			Msg( "Signal pos: %f %f %f, inner rad: %f, outter rad: %f\n", loc.soundpos[0], loc.soundpos[1], loc.soundpos[2], loc.soundinnerrad, loc.soundouterrad );
#endif
			
			CPortal_Dinosaur *pDinosaur = SpawnDinosaur( loc );
			CDinosaurSignal *pSignal = SpawnSignal( loc );

			Assert ( pDinosaur && pSignal );
			if ( pDinosaur && pSignal )
			{
#if defined ( RADIO_DEBUG_SERVER )
				Msg( "SUCCESS: Spawned Dinosaur and signal and linked them.\n" );
#endif
				// OK, so these really could have been the same class... not worth changing it now though.
				pDinosaur->m_hDinosaur_Signal.Set( pSignal );
				pDinosaur->Activate();
			}
		}
	}		 
}

CPortal_Dinosaur *CSpawnDinosaurHack::SpawnDinosaur( radiolocs& loc )
{
	Vector vSpawnPos ( loc.radiopos[0], loc.radiopos[1], loc.radiopos[2] );
	QAngle vSpawnAng ( loc.radioang[0], loc.radioang[1], loc.radioang[2] );

	// origin and angles of zero means skip this Dinosaur creation and look for an existing radio
	if ( loc.radiopos[0] == 0 && 
		loc.radiopos[1] == 0 && 
		loc.radiopos[2] == 0 && 
		loc.radioang[0] == 0 && 
		loc.radioang[1] == 0 && 
		loc.radioang[2] == 0 )
	{

#if defined ( RADIO_DEBUG_SERVER )
		Msg( "Dinosaur found with zero angles and origin. Replacing existing radio.\n" );
#endif
		// Find existing Dinosaur, kill it and spawn at its position
		CPhysicsProp *pOldDinosaur	= (CPhysicsProp*)gEntList.FindEntityByClassname( NULL, "prop_physics" );
		while ( pOldDinosaur )
		{
			if ( V_strcmp( STRING( pOldDinosaur->GetModelName() ), RADIO_MODEL_NAME ) == 0 )
			{
				vSpawnPos = pOldDinosaur->GetAbsOrigin();
				vSpawnAng = pOldDinosaur->GetAbsAngles();

				UTIL_Remove( pOldDinosaur );

#if defined ( RADIO_DEBUG_SERVER )
				Msg( "Found Dinosaur exiting in level, replacing with %f, %f %f and %f %f %f.\n", XYZ(vSpawnPos), XYZ(vSpawnAng) );
#endif
				break;
			}

			pOldDinosaur = (CPhysicsProp*)gEntList.FindEntityByClassname( pOldDinosaur, "prop_physics" );
		}
	}

	Assert( vSpawnPos != vec3_origin );

	CPortal_Dinosaur *pDinosaur = (CPortal_Dinosaur*)CreateEntityByName( "updateitem2" );
	Assert ( pDinosaur );
	if ( pDinosaur )
	{
		pDinosaur->SetAbsOrigin( vSpawnPos );
		pDinosaur->SetAbsAngles( vSpawnAng );
		DispatchSpawn( pDinosaur );
	}

	return pDinosaur;
}

CDinosaurSignal *CSpawnDinosaurHack::SpawnSignal( radiolocs& loc )
{
	CDinosaurSignal *pSignal = (CDinosaurSignal*)CreateEntityByName( "updateitem1" );
	Assert ( pSignal );
	if ( pSignal )
	{
#if defined ( RADIO_DEBUG_SERVER )
		if ( loc.soundinnerrad > loc.soundouterrad )
		{
			Assert( 0 );
			Warning( "Dinosaur BUG: Inner radius is greater than outer radius. Will swap them.\n" );
			swap( loc.soundinnerrad, loc.soundouterrad );
		}
#endif
		pSignal->SetAbsOrigin( Vector( loc.soundpos[0], loc.soundpos[1], loc.soundpos[2] ) );
		pSignal->m_flInnerRadius	= loc.soundinnerrad;
		pSignal->m_flOuterRadius	= loc.soundouterrad;
		V_strncpy( pSignal->m_szSoundName.GetForModify(), loc.soundname, 128 );
		pSignal->m_nSignalID		= loc.id;
		DispatchSpawn( pSignal );
	}

	return pSignal;
}


void CSpawnDinosaurHack::ApplyMapSpecificHacks()
{
	if ( V_strcmp( STRING(gpGlobals->mapname), "testchmb_a_02" ) == 0 )
	{
		CBaseEntity *pFilter = CreateEntityByName( "filter_activator_name" );
		Assert( pFilter );
		if ( pFilter )
		{
			pFilter->KeyValue( "filtername", "box_2" );
			pFilter->KeyValue( "targetname", "filter_weight_box" );
			DispatchSpawn( pFilter );
		}
	}
}

