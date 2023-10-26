#ifndef _INCLUDED_ASW_EQUIPMENT_LIST_H
#define _INCLUDED_ASW_EQUIPMENT_LIST_H
#pragma once

class CASW_WeaponInfo;

// single entry in the equipment list

class CASW_EquipItem
{
public:
	DECLARE_CLASS_NOBASE( CASW_EquipItem );

					CASW_EquipItem();
	virtual			~CASW_EquipItem();

	// the items index in the list of equipment
	int			m_iItemIndex;

	bool m_bSelectableInBriefing;			// if false, this item won't show up on the loadout screen

	string_t	m_EquipClass;

	bool SupportedInSingleplayer();
};

// This class describes the fixed data about the weapons available on the loadout screen

class CASW_EquipmentList
{
public:
	DECLARE_CLASS_NOBASE( CASW_EquipmentList );

	CASW_EquipmentList();
	virtual ~CASW_EquipmentList();
	
	// grab all the weapon datas
	void FindWeaponData();
	// read in the equipment classes from the res file
	void LoadEquipmentList();

	// return info on a particular weapon data
	CASW_WeaponInfo* GetWeaponDataFor(const char* szWeaponClass);

	// find the index of a particular item
	int GetRegularIndex(const char* szWeaponClass);
	int GetExtraIndex(const char* szWeaponClass);

	// Automatically returns a regular or extra item based on wpnslot
	int GetIndexForSlot( int iWpnSlot, const char* szWeaponClass );

	CASW_EquipItem* GetRegular(int index);	
	int GetNumRegular(bool bIncludeHidden);
	CASW_EquipItem* GetExtra(int index);
	int GetNumExtra(bool bIncludeHidden);

	// Automatically returns a regular or extra item based on wpnslot
	CASW_EquipItem* GetItemForSlot( int iWpnSlot, int index );

#ifdef CLIENT_DLL
	void LoadTextures();
	int GetEquipIconTexture(bool bRegular, int iIndex);
	bool m_bLoadedTextures;
	CUtlVector<int> m_iRegularTexture;
	CUtlVector<int> m_iExtraTexture;
#endif

private:
	CUtlVector<CASW_EquipItem*> m_Regular;
	CUtlVector<CASW_EquipItem*> m_Extra;

	// equip counts not including hidden equip
	int m_iNumRegular, m_iNumExtra;

	bool m_bFoundWeaponData;
};

extern CASW_EquipmentList* g_pASWEquipmentList;

extern CASW_EquipmentList* ASWEquipmentList();

#endif /* _INCLUDED_ASW_EQUIPMENT_LIST_H */