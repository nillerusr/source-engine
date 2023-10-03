//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Client handler implementations for instruction players how to play
//
//=============================================================================//

#include "cbase.h"

#include "c_gameinstructor.h"
#include "c_baselesson.h"

#include "asw_gamerules.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "asw_marine_profile.h"
#include "c_asw_weapon.h"
#include "c_asw_pickup.h"
#include "c_asw_pickup_weapon.h"
#include "asw_weapon_parse.h"
#include "c_asw_sentry_base.h"
#include "asw_hud_master.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern CUtlDict< int, int > g_LessonActionMap;

extern ConVar gameinstructor_verbose;


enum Mod_LessonAction
{
	// Enum starts from end of LessonAction
	LESSON_ACTION_IS_ALLOWED_ITEM = LESSON_ACTION_MOD_START,
	LESSON_ACTION_ITEM_WILL_SWAP,
	LESSON_ACTION_CAN_HACK,
	LESSON_ACTION_CAN_OFFHAND,
	LESSON_ACTION_HAS_SECONDARY,
	LESSON_ACTION_HAS_SECONDARY_EXPLOSIVES,
	LESSON_ACTION_GET_MARINE,
	LESSON_ACTION_IS_BOT,
	LESSON_ACTION_WEAPON_HAS_SPARE,
	LESSON_ACTION_WEAPON_IS_OFFENSIVE,
	LESSON_ACTION_WEAPON_LOCAL_HOTBAR_SLOT,
	LESSON_ACTION_OWNS_HOTBAR_SLOT,
	LESSON_ACTION_SENTRY_WANTS_DISMANTLE,
	LESSON_ACTION_IS_TUTORIAL,
	LESSON_ACTION_IS_SINGLEPLAYER,
	
	LESSON_ACTION_TOTAL
};

void CScriptedIconLesson::Mod_PreReadLessonsFromFile( void )
{
	// Add custom actions to the map
	CScriptedIconLesson::LessonActionMap.Insert( "is allowed item", LESSON_ACTION_IS_ALLOWED_ITEM );
	CScriptedIconLesson::LessonActionMap.Insert( "item will swap", LESSON_ACTION_ITEM_WILL_SWAP );
	CScriptedIconLesson::LessonActionMap.Insert( "can hack", LESSON_ACTION_CAN_HACK );
	CScriptedIconLesson::LessonActionMap.Insert( "can offhand", LESSON_ACTION_CAN_OFFHAND );
	CScriptedIconLesson::LessonActionMap.Insert( "has secondary", LESSON_ACTION_HAS_SECONDARY );
	CScriptedIconLesson::LessonActionMap.Insert( "has secondary explosives", LESSON_ACTION_HAS_SECONDARY_EXPLOSIVES );
	CScriptedIconLesson::LessonActionMap.Insert( "get marine", LESSON_ACTION_GET_MARINE );
	CScriptedIconLesson::LessonActionMap.Insert( "is bot", LESSON_ACTION_IS_BOT );
	CScriptedIconLesson::LessonActionMap.Insert( "weapon has spare", LESSON_ACTION_WEAPON_HAS_SPARE );
	CScriptedIconLesson::LessonActionMap.Insert( "weapon is offensive", LESSON_ACTION_WEAPON_IS_OFFENSIVE );
	CScriptedIconLesson::LessonActionMap.Insert( "weapon local hotbar slot", LESSON_ACTION_WEAPON_LOCAL_HOTBAR_SLOT );
	CScriptedIconLesson::LessonActionMap.Insert( "owns hotbar slot", LESSON_ACTION_OWNS_HOTBAR_SLOT );
	CScriptedIconLesson::LessonActionMap.Insert( "sentry wants dismantle", LESSON_ACTION_SENTRY_WANTS_DISMANTLE );
	CScriptedIconLesson::LessonActionMap.Insert( "is tutorial", LESSON_ACTION_IS_TUTORIAL );
	CScriptedIconLesson::LessonActionMap.Insert( "is singleplayer", LESSON_ACTION_IS_SINGLEPLAYER );
}


bool CScriptedIconLesson::Mod_ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, C_BaseEntity *pParam, const char *pchParam, bool &bModHandled )
{
	// Assume we're going to handle the action
	bModHandled = true;

	C_BaseEntity *pVar = hVar.Get();

	switch ( iAction )
	{
		case LESSON_ACTION_IS_ALLOWED_ITEM:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->AllowedToPickup( [%s] )", pchParamName->String(), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bIsAllowed = false;

			C_ASW_Pickup *pPickup = dynamic_cast< C_ASW_Pickup * >( pParam );
			if ( !pPickup )
			{
				C_ASW_Weapon *pWeapon = dynamic_cast< C_ASW_Weapon * >( pParam );

				if ( !pWeapon )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->AllowedToPickup( [%s] )", pchParamName->String(), pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam handle as ASW_Weapon and ASW_Pickup returned NULL!\n" );
					}

					return false;
				}
				else
				{
					bIsAllowed = pWeapon->AllowedToPickup( pMarine );
				}
			}
			else
			{
				bIsAllowed = pPickup->AllowedToPickup( pMarine );
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->AllowedToPickup( [%s] )", pchParamName->String(), pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsAllowed ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bIsAllowed ) : ( bIsAllowed );
		}

		case LESSON_ACTION_ITEM_WILL_SWAP:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WillSwap( [%s] )", pchParamName->String(), pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bWillSwap = false;

			C_ASW_Pickup_Weapon *pPickup = dynamic_cast< C_ASW_Pickup_Weapon * >( pParam );
			if ( !pPickup )
			{
				C_ASW_Weapon *pWeapon = dynamic_cast< C_ASW_Weapon * >( pParam );

				if ( !pWeapon )
				{
					if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
					{
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WillSwap( [%s] )", pchParamName->String(), pchVarName );
						ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
						ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
						ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam handle as ASW_Weapon and ASW_Pickup returned NULL!\n" );
					}

					return false;
				}
				else
				{
					int nSlot = pMarine->GetWeaponPositionForPickup( pWeapon->GetClassname() );
					bWillSwap = ( pMarine->GetASWWeapon( nSlot ) != NULL );
				}
			}
			else
			{
				int nSlot = pMarine->GetWeaponPositionForPickup( pPickup->GetWeaponClass() );
				bWillSwap = ( pMarine->GetASWWeapon( nSlot ) != NULL );
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WillSwap( [%s] )", pchParamName->String(), pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bWillSwap ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bWillSwap ) : ( bWillSwap );
		}

		case LESSON_ACTION_CAN_HACK:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->CanHack()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bCanHack = pMarine->GetMarineProfile()->CanHack();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->AllowedToPickup()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bCanHack ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bCanHack ) : ( bCanHack );
		}

		case LESSON_ACTION_CAN_OFFHAND:
		{
			C_ASW_Weapon *pWeapon = dynamic_cast< C_ASW_Weapon* >( pVar );
			if ( !pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->CanOffhand()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Weapon returned NULL!\n" );
				}

				return false;
			}

			bool bCanOffhand = pWeapon->GetWeaponInfo()->m_bOffhandActivate;

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->CanOffhand()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bCanOffhand ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bCanOffhand ) : ( bCanOffhand );
		}

		case LESSON_ACTION_HAS_SECONDARY:
		{
			C_ASW_Weapon *pWeapon = dynamic_cast< C_ASW_Weapon* >( pVar );
			if ( !pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasSecondary()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Weapon returned NULL!\n" );
				}

				return false;
			}

			bool bHasSecondary = pWeapon->UsesSecondaryAmmo();

			if ( bHasSecondary )
			{
				bHasSecondary = !( ( pWeapon->UsesClipsForAmmo2() && pWeapon->m_iClip2 <= 0 ) ||
								   ( !pWeapon->UsesClipsForAmmo2() && pWeapon->GetOwner() && pWeapon->GetOwner()->GetAmmoCount( pWeapon->m_iSecondaryAmmoType ) <= 0 ) );
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasSecondary()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasSecondary ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bHasSecondary ) : ( bHasSecondary );
		}

		case LESSON_ACTION_HAS_SECONDARY_EXPLOSIVES:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasAnySecondary()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bHasAnySecondary = false;

			for ( int i = 0; i < ASW_MAX_EQUIP_SLOTS; ++i )
			{
				C_ASW_Weapon *pWeapon = static_cast< C_ASW_Weapon* >( pMarine->GetWeapon( i ) );
				if ( pWeapon && pWeapon->HasSecondaryExplosive() )
				{
					if ( !( ( pWeapon->UsesClipsForAmmo2() && pWeapon->m_iClip2 <= 0 ) || 
							( !pWeapon->UsesClipsForAmmo2() && pWeapon->GetOwner() && pWeapon->GetOwner()->GetAmmoCount( pWeapon->m_iSecondaryAmmoType ) <= 0 ) ) )
					{
						bHasAnySecondary = true;
						break;
					}
				}
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasAnySecondary()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasAnySecondary ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bHasAnySecondary ) : ( bHasAnySecondary );
		}

		case LESSON_ACTION_GET_MARINE:
		{
			int iTemp = static_cast<int>( fParam );

			if ( iTemp <= 0 || iTemp > 2 )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[entityINVALID] = [%s]->GetMarine()\n", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tParam selecting string is out of range!\n" );
				}

				return false;
			}

			CHandle<C_BaseEntity> *pHandle;

			char const *pchParamNameTemp = NULL;

			if ( iTemp == 2 )
			{
				pHandle = &m_hEntity2;
				pchParamNameTemp = "entity2";
			}
			else
			{
				pHandle = &m_hEntity1;
				pchParamNameTemp = "entity1";
			}

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( !pPlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s]->GetMarine()", pchParamNameTemp, pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "...\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Player returned NULL!\n" );
				}

				return false;
			}

			C_ASW_Marine *pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );

			pHandle->Set( pMarine );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s] = [%s]->GetMarine()\n", pchParamNameTemp, pchVarName );
			}

			return true;
		}

		case LESSON_ACTION_IS_BOT:
		{
			C_ASW_Marine *pMarine = dynamic_cast< C_ASW_Marine* >( pVar );

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t![%s]->IsInhabited()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bIsBot = !pMarine->IsInhabited();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t![%s]->IsInhabited()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsBot ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bIsBot ) : ( bIsBot );
		}

		case LESSON_ACTION_WEAPON_HAS_SPARE:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasSpareWeapon()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			int nOffensiveWeaponCount = 0;

			for ( int i = 0; i < ASW_MAX_EQUIP_SLOTS; ++i )
			{
				C_ASW_Weapon *pWeapon = static_cast< C_ASW_Weapon* >( pMarine->GetWeapon( i ) );
				if ( pWeapon && pWeapon->IsOffensiveWeapon() )
				{
					if ( !( ( pWeapon->UsesClipsForAmmo1() && pWeapon->m_iClip1 <= 0 ) || 
						 ( !pWeapon->UsesClipsForAmmo1() && pWeapon->GetOwner() && pWeapon->GetOwner()->GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) <= 0 ) ) )
					{
						nOffensiveWeaponCount++;
					}
				}
			}

			bool bHasSpare = ( nOffensiveWeaponCount > 1 );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->HasSpareWeapon()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bHasSpare ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bHasSpare ) : ( bHasSpare );
		}

		case LESSON_ACTION_WEAPON_IS_OFFENSIVE:
		{
			C_ASW_Marine *pMarine = NULL;

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( pPlayer )
			{
				pMarine = dynamic_cast< C_ASW_Marine* >( pPlayer->GetMarine() );
			}

			if ( !pMarine )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WeaponIsOffensive()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Marine returned NULL!\n" );
				}

				return false;
			}

			bool bIsOffensive = false;

			C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
			if ( pWeapon && pWeapon->IsOffensiveWeapon() )
			{
				if ( !( ( pWeapon->UsesClipsForAmmo1() && pWeapon->m_iClip1 <= 0 ) || 
					( !pWeapon->UsesClipsForAmmo1() && pWeapon->GetOwner() && pWeapon->GetOwner()->GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) <= 0 ) ) )
				{
					bIsOffensive = true;
				}
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WeaponIsOffensive()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsOffensive ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bIsOffensive ) : ( bIsOffensive );
		}

		case LESSON_ACTION_WEAPON_LOCAL_HOTBAR_SLOT:
		{
			CASW_Hud_Master *pHUDMaster = GET_HUDELEMENT( CASW_Hud_Master );
			if ( pHUDMaster )
			{
				m_fOutput = pHUDMaster->GetHotBarSlot( pchParam );

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tm_fOutput = GET_HUDELEMENT( CASW_Hud_Squad_Hotbar )->GetHotBarSlot( [%s] ", pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
				}
			}

			return true;
		}

		case LESSON_ACTION_OWNS_HOTBAR_SLOT:
		{
			int iTemp = static_cast<int>( fParam );

			C_ASW_Player *pPlayer = dynamic_cast< C_ASW_Player* >( pVar );
			if ( !pPlayer )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tGET_HUDELEMENT( CASW_Hud_Squad_Hotbar )->OwnsHotBarSlot( [%s] ", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "..." );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ", [%s] ", pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%i\" ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ") " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Player returned NULL!\n" );
				}

				return false;
			}

			bool bOwnsHotBarSlot = false;

			CASW_Hud_Master *pHUDMaster = GET_HUDELEMENT( CASW_Hud_Master );
			if ( pHUDMaster )
			{
				bOwnsHotBarSlot = pHUDMaster->OwnsHotBarSlot( pPlayer, iTemp );

				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tGET_HUDELEMENT( CASW_Hud_Squad_Hotbar )->OwnsHotBarSlot( [%s], [%s] ", pchVarName, pchParamName->String() );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%i\" ", iTemp );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ") " );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bOwnsHotBarSlot ? "true" : "false" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
				}
			}

			return ( bNot ) ? ( !bOwnsHotBarSlot ) : ( bOwnsHotBarSlot );
		}

		case LESSON_ACTION_SENTRY_WANTS_DISMANTLE:
		{
			C_ASW_Sentry_Base *pSentry = dynamic_cast< C_ASW_Sentry_Base* >( pVar );
			if ( !pSentry )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WantsDismantle()", pchVarName );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "... " );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as ASW_Sentry_Base returned NULL!\n" );
				}

				return false;
			}

			bool bWantsDismantle = pSentry->WantsDismantle();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->WantsDismantle()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bWantsDismantle ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bWantsDismantle ) : ( bWantsDismantle );
		}

		case LESSON_ACTION_IS_TUTORIAL:
		{
			bool bIsTutorial = ASWGameRules()->IsTutorialMap();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tASWGameRules()->IsTutorialMap()" );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsTutorial ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bIsTutorial ) : ( bIsTutorial );
		}

		case LESSON_ACTION_IS_SINGLEPLAYER:
		{
			bool bIsSingleplayer = ASWGameRules()->IsOfflineGame();

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\tASWGameRules()->IsOfflineGame()" );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", ( bIsSingleplayer ? "true" : "false" ) );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "== false\n" ) : ( "== true\n" ) );
			}

			return ( bNot ) ? ( !bIsSingleplayer ) : ( bIsSingleplayer );
		}

		default:
			// Didn't handle this action
			bModHandled = false;
			break;
	}

	return false;
}

bool C_GameInstructor::Mod_HiddenByOtherElements( void )
{
	C_ASW_Marine *pLocalMarine = C_ASW_Marine::GetLocalMarine();
	if ( pLocalMarine && pLocalMarine->IsControllingTurret() )
	{
		return true;
	}

	return false;
}
