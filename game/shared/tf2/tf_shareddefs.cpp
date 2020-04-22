//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Data shared between the client & game dlls
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_shareddefs.h"
#include "tier0/dbg.h"
#include "basetypes.h"
#include <KeyValues.h>

#ifndef CLIENT_DLL

	#include "tf_team.h"
	#include "tf_class_commando.h"
	#include "tf_class_defender.h"
	#include "tf_class_escort.h"
	#include "tf_class_infiltrator.h"
	#include "tf_class_medic.h"
	#include "tf_class_recon.h"
	#include "tf_class_sniper.h"
	#include "tf_class_support.h"
	#include "tf_class_sapper.h"
	#include "tf_class_pyro.h"

#else

	#include "c_tfteam.h"
	#include "c_tf_class_commando.h"
	#include "c_tf_class_defender.h"
	#include "c_tf_class_escort.h"
	#include "c_tf_class_infiltrator.h"
	#include "c_tf_class_medic.h"
	#include "c_tf_class_recon.h"
	#include "c_tf_class_sniper.h"
	#include "c_tf_class_support.h"
	#include "c_tf_class_sapper.h"
	#include "c_tf_class_pyro.h"

#define CTFTeam C_TFTeam
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"	

ConVar inv_demo( "inv_demo","0", FCVAR_REPLICATED, "Invasion demo." );
ConVar lod_effect_distance( "lod_effect_distance","3240000", FCVAR_REPLICATED, "Distance at which effects LOD." );
ConVar tf_cheapobjects( "tf_cheapobjects","0", FCVAR_REPLICATED, "Set to 1 and all objects will cost 0" );


//--------------------------------------------------------------------------
// OBJECTS
//--------------------------------------------------------------------------
static int g_iClassInfo_Undecided[] =
{
	OBJ_LAST
};

static int g_iClassInfo_Recon[] =
{
	OBJ_WAGON,
	OBJ_LAST
};

static int g_iClassInfo_Commando[] =
{
	OBJ_POWERPACK,
	OBJ_VEHICLE_BOOST,
	OBJ_DRAGONSTEETH,
	OBJ_MANNED_MISSILELAUNCHER,
	OBJ_SANDBAG_BUNKER,
	OBJ_DRAGONSTEETH,

	OBJ_LAST
};

static int g_iClassInfo_Medic[] =
{
	OBJ_POWERPACK,
	OBJ_SELFHEAL,
	OBJ_BUFF_STATION,
	OBJ_MANNED_PLASMAGUN,
	OBJ_SANDBAG_BUNKER,
	OBJ_BUNKER,
	OBJ_DRAGONSTEETH,
	OBJ_SHIELDWALL,

	OBJ_LAST
};

static int g_iClassInfo_Defender[] =
{
	OBJ_POWERPACK,
	OBJ_SENTRYGUN_PLASMA,
	OBJ_MANNED_MISSILELAUNCHER,
	OBJ_BARBED_WIRE,
	OBJ_DRAGONSTEETH,
	OBJ_TOWER,
	OBJ_SANDBAG_BUNKER,
	OBJ_BUNKER,
	OBJ_DRIVER_MACHINEGUN,
	//OBJ_MORTAR,

	OBJ_LAST
};

static int g_iClassInfo_Sniper[] =
{
	OBJ_WAGON,
	OBJ_LAST
};

static int g_iClassInfo_Support[] =
{
	OBJ_WAGON,
	OBJ_LAST
};

static int g_iClassInfo_Escort[] =
{
	OBJ_SHIELDWALL,
	OBJ_MANNED_SHIELD,
	OBJ_SANDBAG_BUNKER,
	OBJ_BUNKER,

	OBJ_LAST
};

static int g_iClassInfo_Sapper[] =
{
	OBJ_POWERPACK,
	OBJ_DRAGONSTEETH,
	OBJ_TOWER,
	OBJ_SANDBAG_BUNKER,
	OBJ_MANNED_PLASMAGUN,

	OBJ_LAST
};

static int g_iClassInfo_Infiltrator[] =
{
	OBJ_WAGON,
	OBJ_LAST
};

static int g_iClassInfo_Pyro[] =
{
	OBJ_WAGON,
	OBJ_LAST
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool IsObjectAnUpgrade( int iObjectType )
{
	return ( iObjectType >= OBJ_SELFHEAL && iObjectType < OBJ_BATTERING_RAM );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool IsObjectAVehicle( int iObjectType )
{
	return ( iObjectType >= OBJ_BATTERING_RAM && iObjectType < OBJ_TOWER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool IsObjectADefensiveBuilding( int iObjectType )
{
	return ( iObjectType >= OBJ_TOWER );
}

//--------------------------------------------------------------------------
// PLAYER CLASSES
//--------------------------------------------------------------------------

#if defined( CLIENT_DLL )
	
	#define DEFINE_PLAYERCLASS_ALLOC_FNS( className, iClass )				\
		C_PlayerClass* AllocClient##className##( C_BaseTFPlayer *pPlayer )	\
		{																	\
			return new C_PlayerClass##className##( pPlayer );				\
		}																	\
		CPlayerClass* AllocServer##className##( CBaseTFPlayer *pPlayer )	\
		{																	\
			Assert( false );												\
			return NULL;													\
		}

	#define GENERATE_PLAYERCLASS_INFO( className )		\
		AllocClient##className##, AllocServer##className, NULL


	// ------------------------------------------------------------------------------------- //
	// DT_AllPlayerClasses recv table.
	// ------------------------------------------------------------------------------------- //

	BEGIN_RECV_TABLE_NOBASE( C_AllPlayerClasses, DT_AllPlayerClasses )
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_COMMANDO]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassCommandoData ),		DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_DEFENDER]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassDefenderData ),		DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_ESCORT]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassEscortData ),			DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_INFILTRATOR]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassInfiltratorData ), DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_MEDIC]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassMedicData ),				DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_RECON]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassReconData ),				DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_SNIPER]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassSniperData ),			DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_SUPPORT]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassSupportData ),			DataTableRecvProxy_PointerDataTable ),
		RecvPropDataTable( RECVINFO_DT(m_pClasses[TFCLASS_SAPPER]), 0, &REFERENCE_RECV_TABLE( DT_PlayerClassSapperData ),	DataTableRecvProxy_PointerDataTable )
	END_RECV_TABLE()

#else		

	#define DEFINE_PLAYERCLASS_ALLOC_FNS( className, iClass )				\
		ConVar class_##className##_health( "class_" #className "_health", "0", FCVAR_NONE, #className "'s max health" ); \
		C_PlayerClass* AllocClient##className##( C_BaseTFPlayer *pPlayer )	\
		{																	\
			Assert( false );												\
			return NULL;													\
		}																	\
		CPlayerClass* AllocServer##className##( CBaseTFPlayer *pPlayer )	\
		{																	\
			return new CPlayerClass##className##( pPlayer, iClass );		\
		}																	

	#define GENERATE_PLAYERCLASS_INFO( className )		\
		AllocClient##className##, AllocServer##className, &class_##className##_health


	// ------------------------------------------------------------------------------------- //
	// DT_AllPlayerClasses recv table.
	// ------------------------------------------------------------------------------------- //

	BEGIN_SEND_TABLE_NOBASE( CAllPlayerClasses, DT_AllPlayerClasses )
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_COMMANDO]), 	&REFERENCE_SEND_TABLE( DT_PlayerClassCommandoData ),	SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_DEFENDER]), 	&REFERENCE_SEND_TABLE( DT_PlayerClassDefenderData ),	SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_ESCORT]), 	&REFERENCE_SEND_TABLE( DT_PlayerClassEscortData ),		SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_INFILTRATOR]),&REFERENCE_SEND_TABLE( DT_PlayerClassInfiltratorData ), SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_MEDIC]),		&REFERENCE_SEND_TABLE( DT_PlayerClassMedicData ),		SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_RECON]),		&REFERENCE_SEND_TABLE( DT_PlayerClassReconData ),		SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_SNIPER]),		&REFERENCE_SEND_TABLE( DT_PlayerClassSniperData ),		SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_SUPPORT]),	&REFERENCE_SEND_TABLE( DT_PlayerClassSupportData ),		SendProxy_DataTablePtrToDataTable ),
		SendPropDataTable( SENDINFO_DT(m_pClasses[TFCLASS_SAPPER]),		&REFERENCE_SEND_TABLE( DT_PlayerClassSapperData ),	SendProxy_DataTablePtrToDataTable )
	END_SEND_TABLE()

#endif



// ------------------------------------------------------------------------------------- //
// CAllPlayerClasses implementation.
// ------------------------------------------------------------------------------------- //

CAllPlayerClasses::CAllPlayerClasses( PLAYER_TYPE *pPlayer )
{
	for ( int i=0; i < TFCLASS_CLASS_COUNT; i++ )
	{
		m_pClasses[i] = NULL;

#if defined( CLIENT_DLL )
		if ( GetTFClassInfo( i )->m_pClientAlloc )
			m_pClasses[i] = GetTFClassInfo( i )->m_pClientAlloc( pPlayer );
#else
		if ( GetTFClassInfo( i )->m_pServerAlloc )
			m_pClasses[i] = GetTFClassInfo( i )->m_pServerAlloc( pPlayer );
#endif
	}
}

CAllPlayerClasses::~CAllPlayerClasses()
{
	for ( int i=0; i < TFCLASS_CLASS_COUNT; i++ )
	{
		delete m_pClasses[i];
	}
}

PLAYER_CLASS_TYPE* CAllPlayerClasses::GetPlayerClass( int iClass )
{
	Assert( iClass >= 0 && iClass < TFCLASS_CLASS_COUNT );
	return m_pClasses[iClass];
}



DEFINE_PLAYERCLASS_ALLOC_FNS( Recon,		TFCLASS_RECON );
DEFINE_PLAYERCLASS_ALLOC_FNS( Commando,		TFCLASS_COMMANDO );
DEFINE_PLAYERCLASS_ALLOC_FNS( Medic,		TFCLASS_MEDIC );
DEFINE_PLAYERCLASS_ALLOC_FNS( Defender,		TFCLASS_DEFENDER );
DEFINE_PLAYERCLASS_ALLOC_FNS( Sniper,		TFCLASS_SNIPER );
DEFINE_PLAYERCLASS_ALLOC_FNS( Support,		TFCLASS_SUPPORT );
DEFINE_PLAYERCLASS_ALLOC_FNS( Escort,		TFCLASS_ESCORT );
DEFINE_PLAYERCLASS_ALLOC_FNS( Sapper,	TFCLASS_SAPPER );
DEFINE_PLAYERCLASS_ALLOC_FNS( Infiltrator,	TFCLASS_INFILTRATOR );
DEFINE_PLAYERCLASS_ALLOC_FNS( Pyro,			TFCLASS_PYRO );

CTFClassInfo g_TFClassInfos[ TFCLASS_CLASS_COUNT ] =
{
	{ "Undecided",	g_iClassInfo_Undecided,		false, NULL, NULL, NULL },
	{ "Recon",		g_iClassInfo_Recon,			false, GENERATE_PLAYERCLASS_INFO( Recon ) },
	{ "Commando",	g_iClassInfo_Commando,		true, GENERATE_PLAYERCLASS_INFO( Commando ) },
	{ "Medic",		g_iClassInfo_Medic,			true, GENERATE_PLAYERCLASS_INFO( Medic ) },
	{ "Defender",	g_iClassInfo_Defender,		true, GENERATE_PLAYERCLASS_INFO( Defender ) },
	{ "Sniper",		g_iClassInfo_Sniper,		false, GENERATE_PLAYERCLASS_INFO( Sniper ) },
	{ "Support",	g_iClassInfo_Support,		false, GENERATE_PLAYERCLASS_INFO( Support ) },
	{ "Escort",		g_iClassInfo_Escort,		true, GENERATE_PLAYERCLASS_INFO( Escort ) },
	{ "Sapper",		g_iClassInfo_Sapper,		true, GENERATE_PLAYERCLASS_INFO( Sapper ) },
	{ "Infiltrator",g_iClassInfo_Infiltrator,	false, GENERATE_PLAYERCLASS_INFO( Infiltrator ) },
	{ "Pyro",		g_iClassInfo_Pyro,			false, GENERATE_PLAYERCLASS_INFO( Pyro ) }
};


const CTFClassInfo* GetTFClassInfo( int i )
{
	Assert( i >= 0 && i < TFCLASS_CLASS_COUNT );
	return &g_TFClassInfos[i];
}


// ------------------------------------------------------------------------------------------------ //
// CObjectInfo tables.
// ------------------------------------------------------------------------------------------------ //

CObjectInfo::CObjectInfo( char *pObjectName )
{
	m_pObjectName = pObjectName;
	m_pClassName = NULL;
	m_flBuildTime = -9999;
	m_nMaxObjects = -9999;
	m_Cost = -9999;
	m_CostMultiplierPerInstance = -999;
	m_UpgradeCost = -9999;
	m_MaxUpgradeLevel = -9999;
	m_pBuilderWeaponName = NULL;
	m_pBuilderPlacementString = NULL;
	m_SelectionSlot = -9999;
	m_SelectionPosition = -9999;
	m_bSolidToPlayerMovement = false;
	m_flSapperAttachTime = -9999;
	m_pIconActive = NULL;
}


CObjectInfo::~CObjectInfo()
{
	delete [] m_pClassName;
	delete [] m_pStatusName;
	delete [] m_pBuilderWeaponName;
	delete [] m_pBuilderPlacementString;
	delete [] m_pIconActive;
}


CObjectInfo g_ObjectInfos[OBJ_LAST] =
{
	CObjectInfo( "OBJ_POWERPACK" ),
	CObjectInfo( "OBJ_RESUPPLY" ),
	CObjectInfo( "OBJ_SENTRYGUN_PLASMA" ),
	CObjectInfo( "OBJ_SENTRYGUN_ROCKET_LAUNCHER" ),
	CObjectInfo( "OBJ_SHIELDWALL" ),
	CObjectInfo( "OBJ_RESOURCEPUMP" ),
	CObjectInfo( "OBJ_RESPAWN_STATION" ),
	CObjectInfo( "OBJ_RALLYFLAG" ),
	CObjectInfo( "OBJ_MANNED_PLASMAGUN" ),
	CObjectInfo( "OBJ_MANNED_MISSILELAUNCHER" ),
	CObjectInfo( "OBJ_MANNED_SHIELD" ),
	CObjectInfo( "OBJ_EMPGENERATOR" ),
	CObjectInfo( "OBJ_BUFF_STATION" ),
	CObjectInfo( "OBJ_BARBED_WIRE" ),
	CObjectInfo( "OBJ_MCV_SELECTION_PANEL" ),
	CObjectInfo( "OBJ_MAPDEFINED" ),
	CObjectInfo( "OBJ_MORTAR" ),
	CObjectInfo( "OBJ_SELFHEAL" ),
	CObjectInfo( "OBJ_ARMOR_UPGRADE" ),
	CObjectInfo( "OBJ_VEHICLE_BOOST" ),
	CObjectInfo( "OBJ_EXPLOSIVES" ),
	CObjectInfo( "OBJ_DRIVER_MACHINEGUN" ),
	CObjectInfo( "OBJ_BATTERING_RAM" ),
	CObjectInfo( "OBJ_SIEGE_TOWER" ),
	CObjectInfo( "OBJ_WAGON" ),
	CObjectInfo( "OBJ_FLATBED" ),
	CObjectInfo( "OBJ_VEHICLE_MORTAR" ),
	CObjectInfo( "OBJ_VEHICLE_TELEPORT_STATION" ),
	CObjectInfo( "OBJ_VEHICLE_TANK" ),
	CObjectInfo( "OBJ_VEHICLE_MOTORCYCLE" ),
	CObjectInfo( "OBJ_WALKER_STRIDER" ),
	CObjectInfo( "OBJ_WALKER_MINI_STRIDER" ),
	CObjectInfo( "OBJ_TOWER" ),
	CObjectInfo( "OBJ_TUNNEL" ),
	CObjectInfo( "OBJ_SANDBAG_BUNKER" ),
	CObjectInfo( "OBJ_BUNKER" ),
	CObjectInfo( "OBJ_DRAGONSTEETH" ),
};


char* ReadAndAllocStringValue( KeyValues *pSub, const char *pName, const char *pFilename )
{
	const char *pValue = pSub->GetString( pName, NULL );
	if ( !pValue )
	{
		DevWarning( "Can't get key value	'%s' from file '%s'.\n", pName, pFilename );
		return "";
	}

	int len = Q_strlen( pValue ) + 1;
	char *pAlloced = new char[ len ];
	Assert( pAlloced );
	Q_strncpy( pAlloced, pValue, len );
	return pAlloced;
}


bool AreObjectInfosLoaded()
{
	return g_ObjectInfos[0].m_pClassName != NULL;
}


void LoadObjectInfos( IBaseFileSystem *pFileSystem )
{
	const char *pFilename = "scripts/objects.txt";

	// Make sure this stuff hasn't already been loaded.
	Assert( !AreObjectInfosLoaded() );

	KeyValues *pValues = new KeyValues( "Object descriptions" );
	if ( !pValues->LoadFromFile( pFileSystem, pFilename, "GAME" ) )
	{
		Error( "Can't open %s for object info.", pFilename );
		pValues->deleteThis();
		return;
	}

	// Now read each class's information in.
	for ( int iObj=0; iObj < ARRAYSIZE( g_ObjectInfos ); iObj++ )
	{
		CObjectInfo *pInfo = &g_ObjectInfos[iObj];
		KeyValues *pSub = pValues->FindKey( pInfo->m_pObjectName );
		if ( !pSub )
		{
			Error( "Missing section '%s' from %s.", pInfo->m_pObjectName, pFilename );
			pValues->deleteThis();
			return;
		}

		// Read all the info in.
		if ( (pInfo->m_flBuildTime = pSub->GetFloat( "BuildTime", -999 )) == -999 ||
			 (pInfo->m_nMaxObjects = pSub->GetInt( "MaxObjects", -999 )) == -999 ||
	 		 (pInfo->m_Cost = pSub->GetInt( "Cost", -999 )) == -999 ||
			 (pInfo->m_CostMultiplierPerInstance = pSub->GetFloat( "CostMultiplier", -999 )) == -999 ||
			 (pInfo->m_UpgradeCost = pSub->GetInt( "UpgradeCost", -999 )) == -999 ||
			 (pInfo->m_MaxUpgradeLevel = pSub->GetInt( "MaxUpgradeLevel", -999 )) == -999 ||
			 (pInfo->m_SelectionSlot = pSub->GetInt( "SelectionSlot", -999 )) == -999 ||
			 (pInfo->m_SelectionPosition = pSub->GetInt( "SelectionPosition", -999 )) == -999 ||
			 (pInfo->m_flSapperAttachTime = pSub->GetInt( "SapperAttachTime", -999 )) == -999 )
		{
			Error( "Missing data for object '%s' in %s.", pInfo->m_pObjectName, pFilename );
			pValues->deleteThis();
			return;
		}

		pInfo->m_pClassName = ReadAndAllocStringValue( pSub, "ClassName", pFilename );
		pInfo->m_pStatusName = ReadAndAllocStringValue( pSub, "StatusName", pFilename );
		pInfo->m_pBuilderWeaponName = ReadAndAllocStringValue( pSub, "BuilderWeaponName", pFilename );
		pInfo->m_pBuilderPlacementString = ReadAndAllocStringValue( pSub, "BuilderPlacementString", pFilename );
		pInfo->m_bSolidToPlayerMovement = pSub->GetInt( "SolidToPlayerMovement", 0 ) ? true : false;
		pInfo->m_pIconActive = ReadAndAllocStringValue( pSub, "Icon", pFilename );
	}

	pValues->deleteThis();
}


const CObjectInfo* GetObjectInfo( int iObject )
{
	Assert( iObject >= 0 && iObject < OBJ_LAST );
	Assert( AreObjectInfosLoaded() );
	return &g_ObjectInfos[iObject];
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the specified class is allowed to build the specified object type
//-----------------------------------------------------------------------------
bool ClassCanBuild( int iClass, int iObjectType )
{
	for ( int i = 0; i < OBJ_LAST; i++ )
	{
		// Hit the end?
		if ( g_TFClassInfos[iClass].m_pClassObjects[i] == OBJ_LAST )
			return false;

		// Found it?
		if ( g_TFClassInfos[iClass].m_pClassObjects[i] == iObjectType )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return the cost of another object of the specified type
//			If bLast is set, return the cost of the last built object of the specified type
//-----------------------------------------------------------------------------

int CalculateObjectCost( int iObjectType, int iNumberOfObjects, int iTeam, bool bLast )
{
	if ( tf_cheapobjects.GetInt() )
	{
		return 0;
	}

	// Find out how much the next object should cost
	if ( bLast )
	{
		iNumberOfObjects = MAX(0,iNumberOfObjects-1);
	}

	int iCost = GetObjectInfo( iObjectType )->m_Cost;

	// If a cost is negative, it means the first object of that type is free, and then
	// it counts up as normal, using the negative value.
	if ( iCost < 0 )
	{
		if ( iNumberOfObjects == 0 )
			return 0;
		iCost *= -1;
		iNumberOfObjects--;
	}

	// MCVs have special rules: The team's first one is always free
	if ( iObjectType == OBJ_VEHICLE_TELEPORT_STATION )
	{
		CTFTeam *pTeam = (CTFTeam *)GetGlobalTeam(iTeam);
		if ( pTeam && pTeam->GetNumObjects(OBJ_VEHICLE_TELEPORT_STATION) == 0 )
		{
			iCost = 0;
		}
	}

	// Human objects cost less across the board
	if ( iTeam == TEAM_HUMANS )
	{
		iCost = ( ((float)iCost) * 0.8 );
	}

	// Calculate the cost based upon the number of objects
	for ( int i = 0; i < iNumberOfObjects; i++ )
	{
		iCost *= GetObjectInfo( iObjectType )->m_CostMultiplierPerInstance;
	}

	return iCost;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the cost to upgrade an object of a specific type
//-----------------------------------------------------------------------------
int	CalculateObjectUpgrade( int iObjectType, int iObjectLevel )
{
	// Max level?
	if ( iObjectLevel >= GetObjectInfo( iObjectType )->m_MaxUpgradeLevel )
		return 0;

	int iCost = GetObjectInfo( iObjectType )->m_UpgradeCost;
	for ( int i = 0; i < (iObjectLevel - 1); i++ )
	{
		iCost *= OBJECT_UPGRADE_COST_MULTIPLIER_PER_LEVEL;
	}

	return iCost;
}

//--------------------------------------------------------------------------
// MORTAR
//--------------------------------------------------------------------------
// Names for each mortar ammo type
char *MortarAmmoNames[ MA_LASTAMMOTYPE ] =
{
	"Normal Rounds",
	//"Smoke Rounds",
	"Cluster Rounds",
	"Starburst Rounds",
};

// Techs needs for each mortar ammo type
char *MortarAmmoTechs[ MA_LASTAMMOTYPE ] =
{
	"",
	//"mortar_ammo_smoke",
	"mortar_ammo_cluster",
	"mortar_ammo_starburst",
};

// Max amounts of each mortar ammo type in a single mortar
int	MortarAmmoMax[ MA_LASTAMMOTYPE ] = 
{
	-1,		// -1 is infinite ammo
	//20,
	20,
	10,
};
