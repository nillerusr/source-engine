#ifndef _INCLUDED_ASW_EQUIP_REQ_H
#define _INCLUDED_ASW_EQUIP_REQ_H
#ifdef _WIN32
#pragma once
#endif

// Mappers can place this class to add an equipment requirement to the mission.
// Players will not be able to start the mission until the squad is carrying at
// least one of each of the named pieces of equipment.

class CASW_Equip_Req : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Equip_Req, CBaseEntity );
	DECLARE_NETWORKCLASS();	

	enum { ASW_MAX_EQUIP_REQ_CLASSES = 3 };

	bool AreRequirementsMet( int arrEquippedReqClasses[ASW_MAX_EQUIP_REQ_CLASSES] = NULL );
	const char* GetEquipClass(int i);

#ifndef CLIENT_DLL
	DECLARE_DATADESC(); 

	virtual void Spawn();	
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	void ReportMissingEquipment();

	CNetworkString( m_szEquipClass1, 255 );		// TODO: please make these into arrays of [ ASW_MAX_EQUIP_REQ_CLASSES ] size
	CNetworkString( m_szEquipClass2, 255 );
	CNetworkString( m_szEquipClass3, 255 );
#else
	CASW_Equip_Req();

	static CASW_Equip_Req* FindEquipReq();
	static bool ForceWeaponUnlocked( const char *szWeaponClass );			// if a weapon is required for this mission, we force it to be unlocked
	
	char		m_szEquipClass1[255];	// TODO: please make these into arrays of [ ASW_MAX_EQUIP_REQ_CLASSES ] size
	char		m_szEquipClass2[255];
	char		m_szEquipClass3[255];
#endif		
};

#endif /* _INCLUDED_ASW_EQUIP_REQ_H */