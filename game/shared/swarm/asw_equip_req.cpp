#include "cbase.h"
#ifdef CLIENT_DLL	
	#include "c_asw_marine_resource.h"
	#include "c_asw_game_resource.h"
	#define CASW_Equip_Req C_ASW_Equip_Req
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Game_Resource C_ASW_Game_Resource
#else
	#include "asw_marine_resource.h"	
	#include "asw_game_resource.h"
	#include "asw_weapon_parse.h"
#endif
#include "asw_equipment_list.h"
#include "asw_gamerules.h"
#include "asw_equip_req.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Equip_Req, DT_ASW_Equip_Req )

BEGIN_NETWORK_TABLE( CASW_Equip_Req, DT_ASW_Equip_Req )
#ifdef CLIENT_DLL
	RecvPropString( RECVINFO( m_szEquipClass1 ) ),
	RecvPropString( RECVINFO( m_szEquipClass2 ) ),
	RecvPropString( RECVINFO( m_szEquipClass3 ) ),
#else
	SendPropString( SENDINFO( m_szEquipClass1 ) ),
	SendPropString( SENDINFO( m_szEquipClass2 ) ),
	SendPropString( SENDINFO( m_szEquipClass3 ) ),
#endif
END_NETWORK_TABLE()

// Server =====================================

#ifndef CLIENT_DLL

LINK_ENTITY_TO_CLASS( asw_equip_req, CASW_Equip_Req );

BEGIN_DATADESC( CASW_Equip_Req )
	DEFINE_AUTO_ARRAY( m_szEquipClass1, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szEquipClass2, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szEquipClass3, FIELD_CHARACTER ),
END_DATADESC()

void CASW_Equip_Req::Spawn()
{
	if ( ASWGameRules() )
	{
		if (ASWGameRules()->m_hEquipReq.Get() != NULL)
		{
			Msg("ERROR: Only 1 asw_equip_req entity allowed per map\n");
		}
		ASWGameRules()->m_hEquipReq = this;
	}
}

bool CASW_Equip_Req::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "equipclass1" ) )
	{
		Q_strncpy( m_szEquipClass1.GetForModify(), szValue, 255 );
		return true;
	}
	else if ( FStrEq( szKeyName, "equipclass2" ) )
	{
		Q_strncpy( m_szEquipClass2.GetForModify(), szValue, 255 );
		return true;
	}
	else if ( FStrEq( szKeyName, "equipclass3" ) )
	{
		Q_strncpy( m_szEquipClass3.GetForModify(), szValue, 255 );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

const char* CASW_Equip_Req::GetEquipClass(int i)
{
	if (i < 0 || i> 2)
		return "";

	if (i == 0)
		return m_szEquipClass1.Get();
	else if (i == 1)
		return m_szEquipClass2.Get();
	return m_szEquipClass3.Get();
}

// always send this info to players
int CASW_Equip_Req::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CASW_Equip_Req::ReportMissingEquipment()
{
	if ( !ASWGameResource() )
		return;
	
	// check status based on valid strings
	int numEquippedClasses[ ASW_MAX_EQUIP_REQ_CLASSES ] = {0};
	if ( AreRequirementsMet( numEquippedClasses ) )
		return;

	for ( int k = 0; k < ASW_MAX_EQUIP_REQ_CLASSES; ++ k )
	{
		if ( numEquippedClasses[k] )
			continue;

		CASW_WeaponInfo* pWI = ASWEquipmentList()->GetWeaponDataFor(GetEquipClass(k));
		if (pWI)
		{
			UTIL_ClientPrintAll( ASW_HUD_PRINTTALKANDCONSOLE, "#asw_need_equip", pWI->szPrintName );
			return;
		}
	}
}

#else

// Client ==================================

C_ASW_Equip_Req::C_ASW_Equip_Req()
{
	m_szEquipClass1[0] = '\0';
	m_szEquipClass2[0] = '\0';
	m_szEquipClass3[0] = '\0';
}

const char* C_ASW_Equip_Req::GetEquipClass(int i)
{
	if (i < 0 || i >= ASW_MAX_EQUIP_REQ_CLASSES)
		return "";

	if (i == 0)
		return m_szEquipClass1;
	else if (i == 1)
		return m_szEquipClass2;
	return m_szEquipClass3;
}

CASW_Equip_Req* C_ASW_Equip_Req::FindEquipReq()
{
	unsigned int c = ClientEntityList().GetHighestEntityIndex();
	for ( unsigned int i = 0; i <= c; i++ )
	{
		C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;
		
		CASW_Equip_Req *pTemp = dynamic_cast<CASW_Equip_Req*>(e);
		if (pTemp)
		{
			return pTemp;
		}
	}
	return NULL;
}

bool C_ASW_Equip_Req::ForceWeaponUnlocked( const char *szWeaponClass )
{
	unsigned int c = ClientEntityList().GetHighestEntityIndex();
	for ( unsigned int i = 0; i <= c; i++ )
	{
		C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;

		C_ASW_Equip_Req *pTemp = dynamic_cast<C_ASW_Equip_Req*>(e);
		if (pTemp)
		{
			for ( int k = 0; k < ASW_MAX_EQUIP_REQ_CLASSES; k++ )
			{
				const char *szReqEquip = pTemp->GetEquipClass( k );
				if ( !Q_stricmp( szReqEquip, szWeaponClass ) )
					return true;
			}
		}
	}
	return false;
}

#endif

// Shared ==================================

bool CASW_Equip_Req::AreRequirementsMet( int arrEquippedReqClasses[ASW_MAX_EQUIP_REQ_CLASSES] /*= NULL*/ )
{
	if ( !ASWGameResource() )
		return false;

	// Required classes to equip
	int *numEquippedClasses = arrEquippedReqClasses ? arrEquippedReqClasses : ( int* ) stackalloc( sizeof( arrEquippedReqClasses[0] ) * ASW_MAX_EQUIP_REQ_CLASSES );
	memset( numEquippedClasses, 0, sizeof( arrEquippedReqClasses[0] ) * ASW_MAX_EQUIP_REQ_CLASSES );

	// check status based on valid strings
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for ( int i = 0; i<pGameResource->GetMaxMarineResources(); ++i )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;

		for ( int k = 0; k < ASW_MAX_EQUIP_SLOTS; ++ k )
		{
			CASW_EquipItem* pWpn = ASWEquipmentList()->GetItemForSlot( k, pMR->m_iWeaponsInSlots[k] );
			if ( !pWpn )
				continue;

			for ( int iReq = 0; iReq < ASW_MAX_EQUIP_REQ_CLASSES; ++ iReq )
			{
				char const *szReq = GetEquipClass( iReq );
				if ( StringHasPrefix( STRING( pWpn->m_EquipClass ), szReq ) )
					++ numEquippedClasses[ iReq ];
			}
		}
	}

	// check if one of the required classes is not equipped
	for ( int iReq = 0; iReq < ASW_MAX_EQUIP_REQ_CLASSES; ++ iReq )
	{
		if ( !numEquippedClasses[iReq] )
			return false;
	}
	return true;
}