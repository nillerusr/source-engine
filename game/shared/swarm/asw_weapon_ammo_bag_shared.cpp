#include "cbase.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "in_buttons.h"
#include "ammodef.h"
#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "asw_hud_crosshair.h"
#include "asw_input.h"
#define CASW_Marine_Resource C_ASW_Marine_Resource
#else
#include "asw_pickup.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_game_resource.h"
#include "npcevent.h"
#include "asw_sentry_base.h"
#include "world.h"
#include "te_effect_dispatch.h"
#include "asw_util_shared.h"
#include "asw_marine_profile.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Ammo_Bag, DT_ASW_Weapon_Ammo_Bag )

BEGIN_NETWORK_TABLE( CASW_Weapon_Ammo_Bag, DT_ASW_Weapon_Ammo_Bag )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropArray3		( RECVINFO_ARRAY(m_AmmoCount), RecvPropInt( RECVINFO(m_AmmoCount[0])) ),
#else
	// sendprops
	SendPropArray3	( SENDINFO_ARRAY3(m_AmmoCount), SendPropInt( SENDINFO_ARRAY(m_AmmoCount), ASW_AMMO_BAG_SLOTS, SPROP_UNSIGNED ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Ammo_Bag )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_ammo_bag, CASW_Weapon_Ammo_Bag );
PRECACHE_WEAPON_REGISTER(asw_weapon_ammo_bag);

#define THROW_INTERVAL 0.5

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Ammo_Bag )
	DEFINE_ARRAY( m_AmmoCount, FIELD_INTEGER, ASW_AMMO_BAG_SLOTS ),
	DEFINE_FIELD( m_fBurnDamage, FIELD_FLOAT ),
END_DATADESC()

ConVar asw_ammo_bag_damage("asw_ammo_bag_damage", "150", FCVAR_CHEAT, "Damage from ammo bag explosion");
ConVar asw_ammo_bag_damage_radius("asw_ammo_bag_damage_radius", "400", FCVAR_CHEAT, "Radius from ammo bag explosion");
ConVar asw_ammo_bag_burn_limit("asw_ammo_bag_burn_limit", "30", FCVAR_CHEAT, "Amount of burn damage needed to make ammo bag explode");

extern int	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud

#endif /* not client */

CASW_Weapon_Ammo_Bag::CASW_Weapon_Ammo_Bag()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 2048;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;

	// set default ammo types
	m_iAmmoType[ASW_BAG_SLOT_RIFLE] = GetAmmoDef()->Index("ASW_R");
	m_iAmmoType[ASW_BAG_SLOT_AUTOGUN] = GetAmmoDef()->Index("ASW_AG");
	m_iAmmoType[ASW_BAG_SLOT_SHOTGUN] = GetAmmoDef()->Index("ASW_SG");
	m_iAmmoType[ASW_BAG_SLOT_ASSAULT_SHOTGUN] = GetAmmoDef()->Index("ASW_ASG");
	m_iAmmoType[ASW_BAG_SLOT_FLAMER] = GetAmmoDef()->Index("ASW_F");
	//m_iAmmoType[ASW_BAG_SLOT_RAILGUN] = GetAmmoDef()->Index("ASW_RG");
	m_iAmmoType[ASW_BAG_SLOT_PDW] = GetAmmoDef()->Index("ASW_PDW");
	m_iAmmoType[ASW_BAG_SLOT_PISTOL] = GetAmmoDef()->Index("ASW_P");

	// set ammo defaults	
	m_AmmoCount.Set(ASW_BAG_SLOT_RIFLE, 5);
	m_AmmoCount.Set(ASW_BAG_SLOT_AUTOGUN, 1);
	m_AmmoCount.Set(ASW_BAG_SLOT_SHOTGUN, 10);
	m_AmmoCount.Set(ASW_BAG_SLOT_ASSAULT_SHOTGUN, 5);	
	m_AmmoCount.Set(ASW_BAG_SLOT_FLAMER, 5);
	//m_AmmoCount.Set(ASW_BAG_SLOT_RAILGUN, 5);
	m_AmmoCount.Set(ASW_BAG_SLOT_PDW, 5);
	m_AmmoCount.Set(ASW_BAG_SLOT_PISTOL, 5);

	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		m_MaxAmmoCount[i] = AmmoCount(i);
	}

	// set default weapon classes
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_RIFLE], sizeof(m_szWeaponClass[ASW_BAG_SLOT_RIFLE]), "asw_weapon_rifle");
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_AUTOGUN], sizeof(m_szWeaponClass[ASW_BAG_SLOT_AUTOGUN]), "asw_weapon_autogun");
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_SHOTGUN], sizeof(m_szWeaponClass[ASW_BAG_SLOT_SHOTGUN]), "asw_weapon_shotgun");
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_ASSAULT_SHOTGUN], sizeof(m_szWeaponClass[ASW_BAG_SLOT_ASSAULT_SHOTGUN]), "asw_weapon_vindicator");	
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_FLAMER], sizeof(m_szWeaponClass[ASW_BAG_SLOT_FLAMER]), "asw_weapon_flamer");
	//Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_RAILGUN], sizeof(m_szWeaponClass[ASW_BAG_SLOT_RAILGUN]), "asw_weapon_railgun");
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_PDW], sizeof(m_szWeaponClass[ASW_BAG_SLOT_PDW]), "asw_weapon_pdw");
	Q_snprintf(m_szWeaponClass[ASW_BAG_SLOT_PISTOL], sizeof(m_szWeaponClass[ASW_BAG_SLOT_PISTOL]), "asw_weapon_pistol");

	// set default ammo pickup classes
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_RIFLE], sizeof(m_szAmmoClass[ASW_BAG_SLOT_RIFLE]), "asw_ammo_rifle");
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_AUTOGUN], sizeof(m_szAmmoClass[ASW_BAG_SLOT_AUTOGUN]), "asw_ammo_autogun");
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_SHOTGUN], sizeof(m_szAmmoClass[ASW_BAG_SLOT_SHOTGUN]), "asw_ammo_shotgun");
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_ASSAULT_SHOTGUN], sizeof(m_szAmmoClass[ASW_BAG_SLOT_ASSAULT_SHOTGUN]), "asw_ammo_vindicator");	
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_FLAMER], sizeof(m_szAmmoClass[ASW_BAG_SLOT_FLAMER]), "asw_ammo_flamer");
	//Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_RAILGUN], sizeof(m_szAmmoClass[ASW_BAG_SLOT_RAILGUN]), "asw_ammo_railgun");
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_PDW], sizeof(m_szAmmoClass[ASW_BAG_SLOT_PDW]), "asw_ammo_pdw");
	Q_snprintf(m_szAmmoClass[ASW_BAG_SLOT_PISTOL], sizeof(m_szAmmoClass[ASW_BAG_SLOT_PISTOL]), "asw_ammo_pistol");
}


Activity CASW_Weapon_Ammo_Bag::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

void CASW_Weapon_Ammo_Bag::PrimaryAttack( void )
{
	ThrowAmmo();
}

// checks if we're highlighting a fellow marine, if so we chuck him some ammo
void CASW_Weapon_Ammo_Bag::ThrowAmmo()
{
#ifndef CLIENT_DLL
	CASW_Player *pOwner = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	CASW_Marine *pTargetMarine = NULL;
	float flMaxDist = FLT_MAX;

	if ( pOwner->GetMarine() == pMarine )
	{
		// commander throwing ammo
		pTargetMarine = dynamic_cast< CASW_Marine* >( pOwner->GetHighlightEntity() );
		flMaxDist = CASW_Weapon_Ammo_Bag::GetPlayerMaxAmmoGiveDistance();
	}
	else
	{
		// AI throwing ammo
		pTargetMarine = pMarine->m_hGiveAmmoTarget;
		// fudge factor in case ammo give target moved in the moment between AI move and ammo give tasks
		flMaxDist = CASW_Weapon_Ammo_Bag::GetAIMaxAmmoGiveDistance() * 3.0f;
	}

	if ( pTargetMarine && pTargetMarine != pMarine )
	{
		int iType = SelectAmmoTypeToGive( pTargetMarine );
		if ( iType != -1 )
		{
			// check he's near enough
			float flDist = ( pMarine->GetAbsOrigin() - pTargetMarine->GetAbsOrigin() ).Length2D();
			if ( flDist <= flMaxDist ) 
			{
				// give the ammo
				GiveClipTo( pTargetMarine, iType );
				pMarine->OnWeaponFired( this, 1 );
			}
		}			
	}
#else
	// Do a cool client effect/animation here!
#endif
}

#ifdef CLIENT_DLL

	// if the player has his mouse over another marine, highlight it, cos he's the one we can throw ammo to
	void CASW_Weapon_Ammo_Bag::MouseOverEntity(C_BaseEntity *pEnt, Vector vecWorldCursor)
	{
		C_ASW_Marine *pOtherMarine = C_ASW_Marine::AsMarine( pEnt );
		CASW_Player *pOwner = GetCommander();
		CASW_Marine *pMarine = GetMarine();
		if ( !pOwner || !pMarine )
			return;

		if ( !pOtherMarine )
		{			
			C_ASW_Game_Resource *pGameResource = ASWGameResource();
			if ( pGameResource )
			{
				// find marine closest to world cursor
				const float fMustBeThisClose = 70;
				const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t &info = pGameResource->GetMarineCrosshairCache()->GetClosestMarine();
				if ( info.m_fDistToCursor <= fMustBeThisClose )
				{
					pOtherMarine = info.m_hMarine.Get();
				}
			}
		}

		// if the marine our cursor is over is near enough, highlight him
		if ( pOtherMarine && pOtherMarine != pMarine )
		{
			float flDist = ( pMarine->GetAbsOrigin() - pOtherMarine->GetAbsOrigin() ).Length2D();
			if ( flDist <= CASW_Weapon_Ammo_Bag::GetPlayerMaxAmmoGiveDistance() ) 
			{
				// we're mousing over a marine, decide which type of ammo we're going to give him and show it on the tooltip
				CASWHudCrosshair *pCrosshair = GET_HUDELEMENT( CASWHudCrosshair );
				bool bCanGiveAmmo = false;
				if ( pCrosshair )
				{
					int iAmmoType = SelectAmmoTypeToGive(pOtherMarine);
					if (iAmmoType != -1)
					{
						pCrosshair->SetShowGiveAmmo(true, iAmmoType);
						bCanGiveAmmo = true;
					}
				}
				ASWInput()->SetHighlightEntity( pOtherMarine, bCanGiveAmmo );
			}
		}
	}
#else

void CASW_Weapon_Ammo_Bag::GiveClipTo( CASW_Marine *pTargetMarine, int iAmmoType, bool bSuppressSound )
{
	if ( !pTargetMarine )
		return;

	int iBagIndex = FindBagIndexForAmmoType(iAmmoType);
	if ( iBagIndex == -1 )
		return;

	int iCount = m_AmmoCount[iBagIndex];
	if ( iCount <= 0 )
		return;

	// find how much ammo per clip for this type
	CASW_WeaponInfo *pWeaponData = ASWEquipmentList()->GetWeaponDataFor(m_szWeaponClass[iBagIndex]);
	if (!pWeaponData)
		return;

	int iAmmoPerClip = pWeaponData->iMaxClip1;	
	//Msg(" ammo per clip %d\n", iAmmoPerClip);
	if (iAmmoPerClip <= 0)
		return;
	
	
	// give ammo to the target marine
	if ( pTargetMarine->GiveAmmo( iAmmoPerClip, iAmmoType, bSuppressSound ) <= 0 )
	{
		if ( pTargetMarine->GiveAmmoToAmmoBag( iAmmoPerClip, iAmmoType, bSuppressSound ) <= 0 )
			return;
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_give_ammo" );
	if ( event )
	{
		CASW_Marine *pMarine = GetMarine();
		CASW_Player *pPlayer = NULL;

		if ( pMarine )
		{
			pPlayer = pMarine->GetCommander();
		}

		event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
		event->SetInt( "entindex", pTargetMarine->entindex() );

		gameeventmanager->FireEvent( event );
	}

	m_AmmoCount.Set(iBagIndex, iCount - 1);
}

// marine was hurt when holding this weapon
void CASW_Weapon_Ammo_Bag::OnMarineDamage(const CTakeDamageInfo &info)
{
	BaseClass::OnMarineDamage(info);
	
	if (info.GetDamageType() & DMG_BURN)
	{
		m_fBurnDamage += info.GetDamage();
		// check if the ammo bag has taken too much burn damage and should now explode
		if (m_fBurnDamage > asw_ammo_bag_burn_limit.GetInt())
		{			
			CASW_Marine *pMarine = GetMarine();
			if (pMarine)
			{
				pMarine->Weapon_Detach(this);

				// switch marine's weapon away from this, if he's holding it
				if (pMarine->GetActiveASWWeapon() == this)
					pMarine->SwitchToNextBestWeapon(NULL);

				CASW_Marine_Resource *pMR = pMarine->GetMarineResource();
				if ( pMR )
				{
					char szName[ 256 ];
					pMR->GetDisplayName( szName, sizeof( szName ) );
					UTIL_ClientPrintAll( HUD_PRINTTALK, "#asw_ammo_bag_exploded", szName );
				}
					
				// explosion effect
				CPASFilter filter( pMarine->GetAbsOrigin() );

				te->Explosion( filter, 0.0,
					&GetAbsOrigin(), 
					g_sModelIndexFireball,
					2.0, 
					15,
					TE_EXPLFLAG_NONE,
					asw_ammo_bag_damage_radius.GetInt(),
					asw_ammo_bag_damage.GetInt() );

				Vector vecForward = Vector(0, 0, -1);				
				trace_t		tr;
				UTIL_TraceLine ( pMarine->GetAbsOrigin(), pMarine->GetAbsOrigin() + 60*vecForward, MASK_SHOT, 
					pMarine, COLLISION_GROUP_NONE, &tr);


				if ((tr.m_pEnt != GetWorldEntity()) || (tr.hitbox != 0))
				{
					// non-world needs smaller decals
					if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
					{
						UTIL_DecalTrace( &tr, "SmallScorch" );
					}
				}
				else
				{
					UTIL_DecalTrace( &tr, "Scorch" );
				}

				UTIL_ASW_ScreenShake( pMarine->GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );
				
				RadiusDamage ( CTakeDamageInfo( this, info.GetAttacker(), asw_ammo_bag_damage.GetInt(), DMG_BLAST ), pMarine->GetAbsOrigin(), asw_ammo_bag_damage_radius.GetInt(), CLASS_NONE, NULL );				
			}
			
			// remove the ammo bag
			Kill();
		}
	}
}

int CASW_Weapon_Ammo_Bag::AddAmmo(int iBullets, int iAmmoIndex)
{
	int iBagSlot = FindBagIndexForAmmoType(iAmmoIndex);
	if (iBagSlot == -1)	// ammo type not carried by the ammo bag
		return 0;

	if (!HasRoomForAmmo(iAmmoIndex))
		return 0;

	CASW_WeaponInfo *pWeaponData = ASWEquipmentList()->GetWeaponDataFor(m_szWeaponClass[iBagSlot]);
	if (!pWeaponData)
		return 0;

	int iBulletsPerClip = pWeaponData->iMaxClip1;
	int clips = iBullets / iBulletsPerClip;

	m_AmmoCount.Set(iBagSlot, AmmoCount(iBagSlot) + clips);
	return clips * iBulletsPerClip;
}

bool CASW_Weapon_Ammo_Bag::DropAmmoPickup(int iBagSlot)
{
	CASW_Marine* pMarine = GetMarine();
	if (!pMarine)
		return false;

	if (iBagSlot < 0 || iBagSlot >= ASW_AMMO_BAG_SLOTS)
		return false;

	int iDropped = 1;
	if (iBagSlot == ASW_BAG_SLOT_SHOTGUN)
	{
		iDropped = 2;
	}

	if (AmmoCount(iBagSlot) < iDropped)
		return false;

	// create a pickup of this ammo's class
	CASW_Pickup* pPickup = static_cast<CASW_Pickup*>(
		Create( m_szAmmoClass[iBagSlot], pMarine->WorldSpaceCenter(), pMarine->GetAbsAngles(), pMarine ) );
		
	if (!pPickup)
		return false;	
		
	Vector vecForward = pMarine->BodyDirection2D() * 100;
	pPickup->SetAbsVelocity(vecForward);
	if (pPickup->VPhysicsGetObject())
		pPickup->VPhysicsGetObject()->SetVelocity( &vecForward, &vec3_origin );
	pPickup->SetOwnerEntity(NULL);

	
	// for shotgun ammo we try to drop two
	m_AmmoCount.Set(iBagSlot, AmmoCount(iBagSlot) - iDropped);
	return true;
}
#endif

int CASW_Weapon_Ammo_Bag::AmmoCount(int index)
{
	if (index < 0 || index >= ASW_AMMO_BAG_SLOTS)
		return 0;

	return m_AmmoCount.Get(index);
}

int CASW_Weapon_Ammo_Bag::MaxAmmoCount(int index)
{
	if (index < 0 || index >= ASW_AMMO_BAG_SLOTS)
		return 0;

	return m_MaxAmmoCount[index];
}

int CASW_Weapon_Ammo_Bag::FindBagIndexForAmmoType(int iAmmoType)
{
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		if (m_iAmmoType[i] == iAmmoType)
			return i;
	}
	return -1;
}

int CASW_Weapon_Ammo_Bag::SelectAmmoTypeToGive(CASW_Marine *pOtherMarine)
{
	CASW_Weapon *pCurrent = pOtherMarine->GetActiveASWWeapon();
	int iType = -1;
	if (pCurrent && CanGiveAmmoToWeapon(pCurrent))
		iType = pCurrent->GetPrimaryAmmoType();
	else
	{
		// go through marine's inventory
		for (int i=0;i<3;i++)
		{
			CASW_Weapon *pWeapon = pOtherMarine->GetASWWeapon(i);
			if (pWeapon != pCurrent && pWeapon && CanGiveAmmoToWeapon(pWeapon))
			{
				iType = pWeapon->GetPrimaryAmmoType();
			}
		}
	}
	// no weapons to give ammo to, so let's check for giving into an ammo bag
	if (iType == -1)
	{
		CASW_Weapon_Ammo_Bag* pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pOtherMarine->GetWeapon(0));
		if (pBag)
		{
			for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
			{
				int iAmmoType = pBag->m_iAmmoType[i];
				if (pBag->HasRoomForAmmo(iAmmoType) && BagHasAmmo(iAmmoType))
					return iAmmoType;
			}
		}
		pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pOtherMarine->GetWeapon(1));
		if (pBag)
		{
			for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
			{
				int iAmmoType = pBag->m_iAmmoType[i];
				if (pBag->HasRoomForAmmo(iAmmoType) && BagHasAmmo(iAmmoType))
					return iAmmoType;
			}
		}
	}
	return iType;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon has some ammo loaded in the primary clip
//
// Useful because some weapons (like the ammo bag) may have special conditions
// other than how many bullets are in the clip
//-----------------------------------------------------------------------------
bool CASW_Weapon_Ammo_Bag::PrimaryAmmoLoaded( void )
{
	// Needed to allow the primary attack to fire, since it doesn't have 
	// the same concept of a clip as other weapons
	return true;
}

bool CASW_Weapon_Ammo_Bag::BagHasAmmo(int iAmmoType)
{
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		if (m_iAmmoType[i] == iAmmoType && m_AmmoCount[i] > 0)
		{
			return true;
		}
	}
	return false;
}

bool CASW_Weapon_Ammo_Bag::CanGiveAmmoToWeapon(CASW_Weapon *pWeapon)
{
	if (!pWeapon)
		return false;

	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(pWeapon->GetOwner());
	if (!pMarine)
		return false;

	// grab the ammo type of the weapon
	int iAmmoType = pWeapon->GetPrimaryAmmoType();
	// check if we have that ammo here
	if (!BagHasAmmo(iAmmoType))
		return false;

	int iGuns = pMarine->GetNumberOfWeaponsUsingAmmo(iAmmoType);

	// check if the gun is full
	int ammo = pMarine->GetAmmoCount(pWeapon->GetPrimaryAmmoType());   // ammo the marine is carrying outside the gun
	if (ammo >= GetAmmoDef()->MaxCarry(iAmmoType,pMarine) * iGuns)
		return false;

	return true;
}

int CASW_Weapon_Ammo_Bag::NumClipsForWeapon(CASW_Weapon *pWeapon)
{
	if (!pWeapon)
		return 0;

	// grab the ammo type of the weapon
	int iAmmoType = pWeapon->GetPrimaryAmmoType();
	// check if we have that ammo here
	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		if (m_iAmmoType[i] == iAmmoType && m_AmmoCount[i] > 0)
		{
			return m_AmmoCount[i];
		}
	}
	
	return 0;
}

#ifndef CLIENT_DLL
void asw_ammo_bag_report_f()
{
	if ( !ASWGameResource() )
		return;

	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;
		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if (!pMarine)
			continue;
		CASW_Weapon_Ammo_Bag *pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
		if (pBag)
			pBag->DebugContents();

		pBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
		if (pBag)
			pBag->DebugContents();
	}
}
ConCommand asw_ammo_bag_report( "asw_ammo_bag_report", asw_ammo_bag_report_f, "List contents of ammo bag", FCVAR_CHEAT );
#else
void asw_ammo_bag_report_client_f()
{
	if ( !ASWGameResource() )
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (!pMR)
			continue;
		C_ASW_Marine *pMarine = pMR->GetMarineEntity();
		if (!pMarine)
			continue;
		C_ASW_Weapon_Ammo_Bag *pBag = dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
		if (pBag)
			pBag->DebugContents();

		pBag = dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
		if (pBag)
			pBag->DebugContents();
	}
}
ConCommand asw_ammo_bag_report_client( "asw_ammo_bag_report_client", asw_ammo_bag_report_client_f, "List contents of ammo bag in client.dll", FCVAR_CHEAT );
#endif

void CASW_Weapon_Ammo_Bag::DebugContents()
{
	Msg("Ammo bag %d on %s\n", entindex(),
#ifdef CLIENT_DLL
		"client"
#else
		"server"
#endif
		);

	for (int i=0;i<ASW_AMMO_BAG_SLOTS;i++)
	{
		Msg(" Slot [%d:%s] has %d clips\n", i, m_szWeaponClass[i],
				AmmoCount(i));
	}
}

bool CASW_Weapon_Ammo_Bag::HasRoomForAmmo(int iAmmoType)
{
	int iBagSlot = FindBagIndexForAmmoType(iAmmoType);
	if (iBagSlot == -1)	// ammo type not carried by the ammo bag
		return false;

	return (AmmoCount(iBagSlot) < MaxAmmoCount(iBagSlot));
}