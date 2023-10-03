#ifndef _DEFINED_C_ASW_DOOR_H
#define _DEFINED_C_ASW_DOOR_H

#include "c_props.h"
#include "asw_shareddefs.h"

class C_ASW_Door : public C_BasePropDoor
{
	DECLARE_CLASS( C_ASW_Door, C_BasePropDoor );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Door();
	virtual ~C_ASW_Door();
	float GetSealAmount();	// returns how sealed this door is, from 0 to 1.0
	int GetSealedIconTextureID();
	int GetFullySealedIconTextureID();
	const char* GetSealedIconText();
	const char* GetUnsealedIconText() { return "#asw_door_unsealed"; }
	bool IsOpen();
	bool	IsMoving();
	virtual int	GetHealth() const { return m_iHealth; }
	virtual float GetHealthFraction(int &iDoorType) const;
	virtual void OnDataChanged( DataUpdateType_t type );
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_DOOR; }

	virtual void					ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName );

	Vector GetWeldFacingPoint( C_BaseEntity* pOther );	// the point a marine should look to weld this door
	Vector GetSparkNormal( C_BaseEntity* pOther );	// the angle sparks should shoot out when welding

	bool IsRecommendedSeal( void ) { return m_bRecommendedSeal; }
	CNetworkVar(bool, m_bSkillMarineHelping);	// is an engineer helping a weld on this door currently?

	// sent from server
	float m_flTotalSealTime;
	float m_flCurrentSealTime;
	float m_flOldSealTime;
	bool m_bUnwelding;
	int m_iDoorStrength;
	int m_iDoorType;

	bool m_bAutoOpen;
	bool m_bBashable;
	bool m_bShootable;
	bool m_bCanCloseToWeld;
	bool m_bRecommendedSeal;
	bool m_bWasWeldedByMarine;
	float m_fLastMomentFlipDamage;
	Vector m_vecClosedPosition;

	float m_fLastWeldedTime;

	// checks the client door list for a door near this position
	static C_ASW_Door* GetDoorNear(Vector vecSrc);

protected:
	C_ASW_Door( const C_ASW_Door & ); // not defined, not accessible	

	char m_szSealedIconTexture[96];

	static bool s_bLoadedSealedIconTexture, s_bLoadedFullySealedIconTexture;
	static int s_nSealedIconTextureID;
	static int s_nFullySealedIconTextureID;
};

extern CUtlVector<C_ASW_Door*> g_ClientDoorList;

#endif /* _DEFINED_C_ASW_DOOR_H */
