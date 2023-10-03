#ifndef _DEFINED_ASW_FIREWALL_PIECE_H
#define _DEFINED_ASW_FIREWALL_PIECE_H
#pragma once

class CASW_Firewall_Piece : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Firewall_Piece, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Firewall_Piece();
	virtual ~CASW_Firewall_Piece();
	void	Spawn( void );
	void	FireThink( void );
	void	FireTouch( CBaseEntity *pOther );
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();
	void	SetSideFire(int iLeft, int iRight) { m_iLeftFire = iLeft; m_iRightFire = iRight; }
	CASW_Firewall_Piece* CreateAnotherPiece(bool bRight);
	void	SetDuration(float fDuration) { m_fFireDuration = fDuration; }

	int m_iLeftFire;
	int m_iRightFire;
	float m_fSpawnTime;
	float m_fFireDuration;
	EHANDLE m_hCreatorWeapon;
};


#endif /* _DEFINED_ASW_FIREWALL_PIECE_H */