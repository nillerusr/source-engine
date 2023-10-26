#include "cbase.h"
#include <KeyValues.h>
#include "asw_weapon_parse.h"

FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CASW_WeaponInfo;
}



CASW_WeaponInfo::CASW_WeaponInfo()
{
	flReloadTime = 0;	// asw
	m_flBaseDamage = 0;
	m_flFireRate = 0;
	m_iNumPellets = 0;
	m_iPlayerModelSkin = 0;
	m_iDisplayModelSkin = 0;
	m_iDisplayClipSize = -1;
	m_flDisplayReloadTime = -1.0f;
	szEquipIcon[0] = 0;
	szEquipLongName[0] = 0;
	szEquipDescription1[0] = 0;
	szAttributesText[0] = 0;
	szAltFireText[0] = 0;
	szDisplayModel[0] = 0;
	szDisplayModel2[0] = 0;
	m_flModelPanelZOffset = 0.0f;
	m_bExtra = false;	
	m_bTech = false;
	m_bFirstAid = false;
	m_bSpecialWeapons = false;
	m_bTracker = false;
	m_bSapper = false;
	m_bSarge = false;
	m_bUnique = false;
	m_bShowCharges = false;
	m_bShowBatteryCharge = false;
	m_bShowMultiplayerHotkey = false;
	m_bShowLocalPlayerHotkey = true;
	m_bHUDPlayerActivate = false;
	m_nOffhandOrderType = ASW_OFFHAND_USE_IMMEDIATELY;
	m_bOrientToLaser = true;
	m_bShowClipsInWeaponDetail = true;
	m_bShowClipsDoubled = false;


	m_fFlinchChance = 0;
	m_fStoppingPowerFlinchBonus = 0;

	m_iHUDIconOffsetX = m_iHUDIconOffsetY = 0;
	m_iHUDNumberOffsetX = m_iHUDNumberOffsetY = 0;
}

static const char* g_OffhandOrderNames[]={
	"ASW_OFFHAND_USE_IMMEDIATELY",
	"ASW_OFFHAND_THROW",
	"ASW_OFFHAND_DEPLOY",
};

ASW_Offhand_Order_t OffhandOrderTypeFromString( const char *szText )
{
	for ( int i = 0; i < NELEMS( g_OffhandOrderNames ); i++ )
	{
		if ( !Q_stricmp( szText, g_OffhandOrderNames[ i ] ) )
		{
			return (ASW_Offhand_Order_t) i;
		}
	}
	return ASW_OFFHAND_USE_IMMEDIATELY;
}

void CASW_WeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	// asw: alien swarm specific weapon properties
	flReloadTime = pKeyValuesData->GetFloat( "reload_time", 2.2f );
	m_flBaseDamage = pKeyValuesData->GetFloat( "base_damage", 0.0f );
	m_flFireRate = pKeyValuesData->GetFloat( "fire_rate", 0.0f );
	m_iNumPellets = pKeyValuesData->GetInt("num_pellets", 1 );

	m_iDisplayClipSize = pKeyValuesData->GetFloat( "display_clip_size", -1 );
	m_flDisplayReloadTime = pKeyValuesData->GetFloat( "display_reload_time", -1.0f );

	m_iPlayerModelSkin = pKeyValuesData->GetInt("playermodel_skin", 0 );
	m_iDisplayModelSkin = pKeyValuesData->GetInt("displaymodel_skin", 0 );

	Q_strncpy( szEquipIcon, pKeyValuesData->GetString( "equipicon" ), 64 );
	Q_strncpy( szEquipLongName, pKeyValuesData->GetString( "equiplongname" ), 64 );
	Q_strncpy( szEquipDescription1, pKeyValuesData->GetString( "equipdescription1" ), 256 );
	Q_strncpy( szAltFireText, pKeyValuesData->GetString( "altfiretext" ), 64 );
	Q_strncpy( szAttributesText, pKeyValuesData->GetString( "attributestext" ), 64 );
	Q_strncpy( szDisplayModel, pKeyValuesData->GetString( "displaymodel" ), 64 );
	Q_strncpy( szDisplayModel2, pKeyValuesData->GetString( "displaymodel2" ), 64 );

	m_flModelPanelZOffset = pKeyValuesData->GetFloat( "ModelPanelZOffset", 0.0f );
	m_bExtra = !!pKeyValuesData->GetInt("ExtraItem", 0);	
	m_bTech = !!pKeyValuesData->GetInt("RequiresTech", 0);	
	m_bFirstAid = !!pKeyValuesData->GetInt("RequiresFirstAid", 0);	
	m_bSpecialWeapons = !!pKeyValuesData->GetInt("RequiresSpecialWeapons", 0);	
	m_bTracker = !!pKeyValuesData->GetInt("RequiresTracker", 0);	
	m_bSapper = !!pKeyValuesData->GetInt("RequiresSapper", 0);	
	m_bSarge = !!pKeyValuesData->GetInt("RequiresSarge", 0);	
	m_bUnique = !!pKeyValuesData->GetInt("Unique", 0);
	m_bOffhandActivate = !!pKeyValuesData->GetInt("OffhandActivate", 0);
	m_bHUDPlayerActivate = !!pKeyValuesData->GetInt("HUDPlayerActivate", 0);
	m_bOffhandSwitch = !!pKeyValuesData->GetInt("OffhandSwitch", 0);
	m_nOffhandOrderType = OffhandOrderTypeFromString( pKeyValuesData->GetString( "OffhandOrderType", "" ) );
	m_bZoomHotbarIcon = pKeyValuesData->GetBool( "ZoomHotbarIcon", false );

	m_fFlinchChance = pKeyValuesData->GetFloat("FlinchChance");
	m_fStoppingPowerFlinchBonus = pKeyValuesData->GetFloat("StoppingPowerFlinchBonus");

	m_bShowCharges = !!pKeyValuesData->GetInt("ShowCharges", 0);
	m_bShowBatteryCharge = pKeyValuesData->GetBool("ShowBatteryCharge", 0);
	m_bShowMultiplayerHotkey = !!pKeyValuesData->GetInt("ShowMultiplayerHotkey", 0);
	m_bShowLocalPlayerHotkey = pKeyValuesData->GetBool( "ShowLocalPlayerHotkey", 1 );

	m_iHUDIconOffsetX = pKeyValuesData->GetInt("HUDIconOffsetX", 0);
	m_iHUDIconOffsetY = pKeyValuesData->GetInt("HUDIconOffsetY", 0);
	m_iHUDNumberOffsetX = pKeyValuesData->GetInt("HUDNumberOffsetX", 0);
	m_iHUDNumberOffsetY = pKeyValuesData->GetInt("HUDNumberOffsetY", 0);

	m_iShowBulletsOnHUD = pKeyValuesData->GetInt("ShowBulletsOnHUD", 0);
	m_iShowClipsOnHUD = pKeyValuesData->GetInt("ShowClipsOnHUD", 0);
	m_iShowGrenadesOnHUD = pKeyValuesData->GetInt("ShowGrenadesOnHUD", 0);
	m_bOrientToLaser = pKeyValuesData->GetBool( "OrientToLaser", 1 );
	m_bShowClipsInWeaponDetail = pKeyValuesData->GetBool( "ShowClipsInWeaponDetail", 1 );
	m_bShowClipsDoubled = pKeyValuesData->GetBool( "ShowClipsDoubled", 0 );


	m_iSquadEmote = pKeyValuesData->GetInt("SquadEmote", -1);
}


