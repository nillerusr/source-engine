//=============================================================================//
//
// Purpose: A prototype portal gun left behind by Aperture at Arbeit 1
//
//	For Bad Cop's purposes, it's either a stun weapon, a puzzle solving tool,
// or a bag of holding.
//
// The first shot fires a 'blue portal' which consumes an NPC or object.
// Firing again shoots the 'red portal' which spits that object back out.
//
// Holding onto an object for more than 5 seconds will cause it to spawn
// back into its original position.
//
// Author: 1upD
//
//=============================================================================//

#include "basehlcombatweapon.h"
#include "grenade_hopwire.h"

//---------------------
// Aperture pistol
//---------------------
class CDisplacerPistol :public CBaseHLCombatWeapon, public CDisplacerSink
{
public:
	DECLARE_CLASS( CDisplacerPistol, CBaseHLCombatWeapon );

	DECLARE_SERVERCLASS();

	DECLARE_DATADESC();

	void Precache( void );

	void Equip( CBaseCombatCharacter *pOwner );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void HandleFireOnEmpty();					// Called when they have the attack button down
	//virtual bool Reload( void );

	virtual bool HasAnyAmmo( void ); // Returns true is weapon has ammo - having an active displaced entity counts as having ammo
	virtual bool UsesClipsForAmmo1( void ) const { return false; } // Don't use clips!

	bool DispaceEntity( CBaseEntity * pEnt );
	void ReleaseThink();
	bool ReleaseEntity( CBaseEntity * pCollidedEntity );
	bool Telefrag( CBaseEntity * pVictim, CBaseEntity * pAttacker );

	void BluePortalEffects();
	void RedPortalEffects();

	CBaseEntity *GetDisplacedEntity() { return m_hDisplacedEntity; }
	CBaseEntity *GetTargetPosition() { return m_hTargetPosition; }
	CBaseEntity *GetLastDisplacePosition() { return m_hLastDisplacePosition; }

	bool	ShouldDisplayHUDHint() { return true; } // This weapon will need some explanation
protected:
	void PortalEffects( const char * pSpriteNam );

	void MoveTargetPosition( const Vector &vecOrigin, const QAngle &vecAngles);
	void MoveLastDisplacedPosition( const Vector &vecOrigin, const QAngle &vecAngles );
	CBaseEntity * MovePosition( CBaseEntity * pTarget, const Vector &vecOrigin, const QAngle &vecAngles );

private:
	CHandle<CBaseEntity>	m_hDisplacedEntity;
	CHandle<CBaseEntity>	m_hTargetPosition;
	CHandle<CBaseEntity>	m_hLastDisplacePosition;
};