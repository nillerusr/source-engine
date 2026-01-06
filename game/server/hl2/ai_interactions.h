//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

//==================================================
// Definition for all AI interactions
//==================================================

#ifndef	AI_INTERACTIONS_H
#define	AI_INTERACTIONS_H

#ifdef _WIN32
#pragma once
#endif

//Antlion
extern int	g_interactionAntlionKilled;

//Barnacle
extern int	g_interactionBarnacleVictimDangle;
extern int	g_interactionBarnacleVictimReleased;
extern int	g_interactionBarnacleVictimGrab;

//Bullsquid
extern int	g_interactionBullsquidPlay;
extern int	g_interactionBullsquidThrow;
#ifdef EZ
extern int	g_interactionBullsquidMonch;
#endif

//Combine
extern int	g_interactionCombineBash;
extern int	g_interactionCombineRequestCover;

//Houndeye
//extern int	g_interactionHoundeyeGroupAttack;
//extern int	g_interactionHoundeyeGroupRetreat;
//extern int	g_interactionHoundeyeGroupRalley;

//Scanner
extern int	g_interactionScannerInspect;
extern int	g_interactionScannerInspectBegin;
extern int	g_interactionScannerInspectDone;
extern int	g_interactionScannerInspectHandsUp;
extern int	g_interactionScannerInspectShowArmband;
extern int	g_interactionScannerSupportEntity;
extern int	g_interactionScannerSupportPosition;

//Metrocop
extern int	g_interactionMetrocopPointed;
extern int	g_interactionMetrocopStartedStitch;

//ScriptedTarget
extern int  g_interactionScriptedTarget;

//Stalker
extern int	g_interactionStalkerBurn;

//Vortigaunt
extern int	g_interactionVortigauntStomp;
extern int	g_interactionVortigauntStompFail;
extern int	g_interactionVortigauntStompHit;
extern int	g_interactionVortigauntKick;
extern int	g_interactionVortigauntClaw;

//Floor turret
extern int	g_interactionTurretStillStanding;

// AI Interaction for being hit by a physics object
extern int  g_interactionHitByPlayerThrownPhysObj;

// Alerts vital allies when the player punts a large object (car)
extern int	g_interactionPlayerPuntedHeavyObject;

// Zombie
// Melee attack will land in one second or so.
extern int	g_interactionZombieMeleeWarning;

#ifdef EZ
extern int  g_interactionZombinePullGrenade;
#endif

#ifdef EZ2
// Headcrab
extern int	g_interactionHeadcrabJump;

// Xen Grenade
extern int	g_interactionXenGrenadePull;
extern int	g_interactionXenGrenadeConsume;
extern int	g_interactionXenGrenadeRelease;
extern int	g_interactionXenGrenadeCreate;
extern int	g_interactionXenGrenadeHop;
extern int  g_interactionXenGrenadeRagdoll;

// Bad Cop kick
extern int	g_interactionBadCopKick;
extern int	g_interactionBadCopKickWarn;

// Bad Cop order surrender
extern int	g_interactionBadCopOrderSurrender;

// Melee warning used by all companions via CNPC_PlayerCompanion::OnChangeActivity
extern int g_interactionGenericMeleeWarning;

// Husks
extern int	g_interactionHuskSuspicious;
extern int	g_interactionHuskAngry;
#endif

#endif	//AI_INTERACTIONS_H
