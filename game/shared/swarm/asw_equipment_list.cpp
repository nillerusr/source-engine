#include "cbase.h"
#include "asw_equipment_list.h"
#include "filesystem.h"
#include "weapon_parse.h"
#include <KeyValues.h>
#ifdef CLIENT_DLL
	#include <vgui/ISurface.h>
	#include <vgui/ISystem.h>
	#include <vgui_controls/Panel.h>
#endif
#include "asw_weapon_parse.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------
//      EquipItems
// ------------------

CASW_EquipItem::CASW_EquipItem()
{
	m_bSelectableInBriefing = true;
}

CASW_EquipItem::~CASW_EquipItem()
{
}

bool CASW_EquipItem::SupportedInSingleplayer()
{
	if ( !Q_strcmp( STRING( m_EquipClass ), "asw_weapon_medical_satchel" ) ||
		!Q_strcmp( STRING( m_EquipClass ), "asw_weapon_t75" ) ||
		!Q_strcmp( STRING( m_EquipClass ), "asw_weapon_laser_mines" ) )
	{
		//Msg( "Not supportedin singleplayer %s\n", STRING( m_EquipClass ) );
		return false;
	}
	return true;
}

// ------------------
//    EquipmentList
// ------------------

CASW_EquipmentList::CASW_EquipmentList()
{
	Assert( !g_pASWEquipmentList );
	g_pASWEquipmentList = this;

	m_bFoundWeaponData = false;
	m_iNumRegular = 0;
	m_iNumExtra = 0;
	LoadEquipmentList();

#ifdef CLIENT_DLL
	m_bLoadedTextures = false;
#endif CLIENT_DLL
}

void CASW_EquipmentList::LoadEquipmentList()
{
	m_iNumRegular = 0;
	m_iNumExtra = 0;

	KeyValues *kv = new KeyValues("Equipment");
	// load equipment
	if (kv->LoadFromFile(filesystem, "resource/Equipment.res"))
	{		
		int iNumEquip = 0;
		KeyValues *pKeys = kv;
		while ( pKeys )
		{
			for (KeyValues *details = pKeys->GetFirstSubKey(); details; details = details->GetNextKey())
			{			
				if ( FStrEq( details->GetName(), "Regular" ) )
				{
					//Msg("adding regular equip %s\n", MAKE_STRING( details->GetString() ));
					CASW_EquipItem* equip = new CASW_EquipItem();
					equip->m_EquipClass = MAKE_STRING( details->GetString() );
					equip->m_iItemIndex = iNumEquip;
					equip->m_bSelectableInBriefing = true;
					m_Regular.AddToTail(equip);	
					m_iNumRegular++;
					iNumEquip++;
				}
				else if ( FStrEq( details->GetName(), "Extra" ) )
				{
					//Msg("adding extra equip %s\n", MAKE_STRING( details->GetString() ));
					CASW_EquipItem* equip = new CASW_EquipItem();
					equip->m_EquipClass = MAKE_STRING( details->GetString() );
					equip->m_iItemIndex = iNumEquip;
					equip->m_bSelectableInBriefing = true;
					m_Extra.AddToTail(equip);
					m_iNumExtra++;
					iNumEquip++;							
				}
				// hidden equip
				else if ( FStrEq( details->GetName(), "RegularOther" ) )
				{
					//Msg("adding regular equip %s\n", MAKE_STRING( details->GetString() ));
					CASW_EquipItem* equip = new CASW_EquipItem();
					equip->m_EquipClass = MAKE_STRING( details->GetString() );
					equip->m_iItemIndex = iNumEquip;
					equip->m_bSelectableInBriefing = false;
					m_Regular.AddToTail(equip);	
					iNumEquip++;
				}
				else if ( FStrEq( details->GetName(), "ExtraOther" ) )
				{
					//Msg("adding extra equip %s\n", MAKE_STRING( details->GetString() ));
					CASW_EquipItem* equip = new CASW_EquipItem();
					equip->m_EquipClass = MAKE_STRING( details->GetString() );
					equip->m_iItemIndex = iNumEquip;
					equip->m_bSelectableInBriefing = false;
					m_Extra.AddToTail(equip);
					iNumEquip++;							
				}
			}
			pKeys = pKeys->GetNextKey();
		}							
	}	
}

void CASW_EquipmentList::FindWeaponData()
{	
	m_bFoundWeaponData = true;
}

CASW_EquipmentList::~CASW_EquipmentList()
{
	Assert( g_pASWEquipmentList == this );
	g_pASWEquipmentList = NULL;

	// delete all the equip items too
	for (int i=0;i<m_Regular.Count();i++)
	{
		delete m_Regular[i];
	}
	for (int i=0;i<m_Extra.Count();i++)
	{
		delete m_Extra[i];
	}
}

CASW_EquipmentList *g_pASWEquipmentList = NULL;

CASW_EquipmentList* ASWEquipmentList()
{
	if (g_pASWEquipmentList == NULL)
	{
		g_pASWEquipmentList = new CASW_EquipmentList();
	}
	return g_pASWEquipmentList;
}

// gets the weapondata for a particular class of weapon (assumes the weapon has been precached already)
CASW_WeaponInfo* CASW_EquipmentList::GetWeaponDataFor(const char* szWeaponClass)
{	
	WEAPON_FILE_INFO_HANDLE hWeaponFileInfo;
	ReadWeaponDataFromFileForSlot( filesystem, szWeaponClass, &hWeaponFileInfo, ASWGameRules()->GetEncryptionKey() );
	return dynamic_cast<CASW_WeaponInfo*>(GetFileWeaponInfoFromHandle(hWeaponFileInfo));
}

CASW_EquipItem* CASW_EquipmentList::GetRegular(int index)
{
	if (index < 0 || index >= m_Regular.Count())
		return NULL;
	return m_Regular[index];
}

int CASW_EquipmentList::GetNumRegular(bool bIncludeHidden)
{
	if (bIncludeHidden)
		return m_Regular.Count();

	return m_iNumRegular;
}

CASW_EquipItem* CASW_EquipmentList::GetExtra(int index)
{
	if (index < 0 || index >= m_Extra.Count())
		return NULL;
	return m_Extra[index];
}

int CASW_EquipmentList::GetNumExtra(bool bIncludeHidden)
{
	if (bIncludeHidden)
		return m_Extra.Count();

	return m_iNumExtra;
}

CASW_EquipItem* CASW_EquipmentList::GetItemForSlot( int iWpnSlot, int index )
{
	if ( iWpnSlot > 1 )
		return GetExtra( index );
	else
		return GetRegular( index );
}

int CASW_EquipmentList::GetRegularIndex(const char* szWeaponClass)
{
	for (int i=0;i<m_Regular.Count();i++)
	{
		//Msg("GetRegularIndex  %d Comparing %s to %s\n", i, STRING(m_Regular[i]->m_EquipClass), szWeaponClass);
		const char *szListClass = STRING( m_Regular[i]->m_EquipClass );
		if ( !Q_stricmp( szListClass, szWeaponClass ) )
			return i;
	}
	return -1;
}

int CASW_EquipmentList::GetExtraIndex(const char* szWeaponClass)
{
	for (int i=0;i<m_Extra.Count();i++)
	{
		const char *szListClass = STRING( m_Extra[i]->m_EquipClass );
		if ( !Q_stricmp( szListClass, szWeaponClass ) )
			return i;
	}
	return -1;
}

int CASW_EquipmentList::GetIndexForSlot( int iWpnSlot, const char* szWeaponClass )
{
	if ( iWpnSlot > 1 )
		return GetExtraIndex( szWeaponClass );
	else
		return GetRegularIndex( szWeaponClass );
}

// loads the weapon icon textures for all our equipment
#ifdef CLIENT_DLL
void CASW_EquipmentList::LoadTextures()
{
	if (m_bLoadedTextures)
		return;

	m_bLoadedTextures = true;
	char buffer[256];
	for (int i=0;i<m_Regular.Count();i++)
	{
		int t = vgui::surface()->CreateNewTextureID();
		CASW_EquipItem* pItem = GetRegular(i);
		if (pItem)
		{
			CASW_WeaponInfo* pWeaponData = GetWeaponDataFor(pItem->m_EquipClass);
			gWR.LoadWeaponSprites(LookupWeaponInfoSlot(pItem->m_EquipClass));	// make sure we load in it's .txt + sprites now too
			Q_snprintf(buffer, 256, "vgui/%s", pWeaponData->szEquipIcon);
			vgui::surface()->DrawSetTextureFile( t, buffer, true, false);
		}
		else
		{
			// need to load an error texture?
		}
		m_iRegularTexture.AddToTail(t);
	}
	for (int i=0;i<m_Extra.Count();i++)
	{
		int t = vgui::surface()->CreateNewTextureID();
		CASW_EquipItem* pItem = GetExtra(i);
		if (pItem)
		{
			CASW_WeaponInfo* pWeaponData = GetWeaponDataFor(pItem->m_EquipClass);
			gWR.LoadWeaponSprites(LookupWeaponInfoSlot(pItem->m_EquipClass));	// make sure we load in it's .txt + sprites now too
			Q_snprintf(buffer, 256, "vgui/%s", pWeaponData->szEquipIcon);
			vgui::surface()->DrawSetTextureFile( t, buffer, true, false);
		}
		else
		{
			// need to load an error texture?
		}
		m_iExtraTexture.AddToTail(t);
	}
}

int CASW_EquipmentList::GetEquipIconTexture(bool bRegular, int iIndex)
{
	if (!m_bLoadedTextures)
		LoadTextures();
	if (iIndex < 0)
		return -1;
	if (bRegular)
	{
		if (iIndex > m_iRegularTexture.Count())
			return -1;
		return m_iRegularTexture[iIndex];
	}
	else
	{
		if (iIndex > m_iExtraTexture.Count())
			return -1;
		return m_iExtraTexture[iIndex];
	}
}
#endif