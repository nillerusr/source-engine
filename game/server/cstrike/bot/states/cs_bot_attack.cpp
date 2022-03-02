//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin attacking
 */
void AttackState::OnEnter( CCSBot *me )
{
	CBasePlayer *enemy = me->GetBotEnemy();

	// store our posture when the attack began
	me->PushPostureContext();

	me->DestroyPath();

	// if we are using a knife, try to sneak up on the enemy
	if (enemy && me->IsUsingKnife() && !me->IsPlayerFacingMe( enemy ))
		me->Walk();
	else
		me->Run();

	me->GetOffLadder();
	me->ResetStuckMonitor();

	m_repathTimer.Invalidate();
	m_haveSeenEnemy = me->IsEnemyVisible();
	m_nextDodgeStateTimestamp = 0.0f;
	m_firstDodge = true;
	m_isEnemyHidden = false;
	m_reacquireTimestamp = 0.0f;

	m_pinnedDownTimestamp = gpGlobals->curtime + RandomFloat( 7.0f, 10.0f );

	m_shieldToggleTimestamp = gpGlobals->curtime + RandomFloat( 2.0f, 10.0f );
	m_shieldForceOpen = false;

	// if we encountered someone while escaping, grab our weapon and fight!
	if (me->IsEscapingFromBomb())
		me->EquipBestWeapon();

	if (me->IsUsingKnife())
	{
		// can't crouch and hold with a knife
		m_crouchAndHold = false;
		me->StandUp();
	}
	else if (me->CanSeeSniper() && !me->IsSniper())
	{
		// don't sit still if we see a sniper!
		m_crouchAndHold = false;
		me->StandUp();
	}
	else
	{
		// decide whether to crouch where we are, or run and gun (if we havent already - see CCSBot::Attack())
		if (!m_crouchAndHold)
		{
			if (enemy)
			{
				const float crouchFarRange = 750.0f;
				float crouchChance;
				
				// more likely to crouch if using sniper rifle or if enemy is far away
				if (me->IsUsingSniperRifle())
					crouchChance = 50.0f;
				else if ((GetCentroid( me ) - GetCentroid( enemy )).IsLengthGreaterThan( crouchFarRange ))
					crouchChance = 50.0f;
				else
					crouchChance = 20.0f * (1.0f - me->GetProfile()->GetAggression());

				if (RandomFloat( 0.0f, 100.0f ) < crouchChance)
				{
					// make sure we can still see if we crouch
					trace_t result;

					Vector origin = GetCentroid( me );
					if (!me->IsCrouching())
					{
						// we are standing, adjust for lower crouch origin
						origin.z -= 20.0f;
					}

					UTIL_TraceLine( origin, enemy->EyePosition(), MASK_PLAYERSOLID, me, COLLISION_GROUP_NONE, &result );

					if (result.fraction == 1.0f)
					{
						m_crouchAndHold = true;
					}
				}
			}
		}

		if (m_crouchAndHold)
		{
			me->Crouch();
			me->PrintIfWatched( "Crouch and hold attack!\n" ); 
		}
	}

	m_scopeTimestamp = 0;
	m_didAmbushCheck = false;

	float skill = me->GetProfile()->GetSkill();

	// tendency to dodge is proportional to skill
	float dodgeChance = 80.0f * skill;

	// high skill bots always dodge if outnumbered, or they see a sniper
	if (skill > 0.5f && (me->IsOutnumbered() || me->CanSeeSniper()))
	{
		dodgeChance = 100.0f;
	}

	m_shouldDodge = (RandomFloat( 0, 100 ) <= dodgeChance);


	// decide whether we might bail out of this fight
	m_isCoward = (RandomFloat( 0, 100 ) > 100.0f * me->GetProfile()->GetAggression());
}

//--------------------------------------------------------------------------------------------------------------
/**
 * When we are done attacking, this is invoked
 */
void AttackState::StopAttacking( CCSBot *me )
{
	if (me->GetTask() == CCSBot::SNIPING)
	{
		// stay in our hiding spot
		me->Hide( me->GetLastKnownArea(), -1.0f, 50.0f );
	}
	else
	{
		me->StopAttacking();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Do dodge behavior
 */
void AttackState::Dodge( CCSBot *me )
{
	//
	// Dodge.
	// If sniping or crouching, stand still.
	//
	if (m_shouldDodge && !me->IsUsingSniperRifle() && !m_crouchAndHold)
	{
		CBasePlayer *enemy = me->GetBotEnemy();
		if (enemy == NULL)
		{
			return;
		}

		Vector toEnemy = enemy->GetAbsOrigin() - me->GetAbsOrigin();
		float range = toEnemy.Length();

		const float hysterisRange = 125.0f;		// (+/-) m_combatRange

		float minRange = me->GetCombatRange() - hysterisRange;
		float maxRange = me->GetCombatRange() + hysterisRange;

		if (me->IsUsingKnife())
		{
			// dodge when far away if armed only with a knife
			maxRange = 999999.9f;
		}

		// move towards (or away from) enemy if we are using a knife, behind a corner, or we aren't very skilled
		if (me->GetProfile()->GetSkill() < 0.66f || !me->IsEnemyVisible())
		{
			if (range > maxRange)
				me->MoveForward();
			else if (range < minRange)
				me->MoveBackward();
		}

		// don't dodge if enemy is facing away
		const float dodgeRange = 2000.0f;
		if (!me->CanSeeSniper() && (range > dodgeRange || !me->IsPlayerFacingMe( enemy )))
		{
			m_dodgeState = STEADY_ON;
			m_nextDodgeStateTimestamp = 0.0f;
		}
		else if (gpGlobals->curtime >= m_nextDodgeStateTimestamp)
		{
			int next;

			// high-skill bots keep moving and don't jump if they see a sniper
			if (me->GetProfile()->GetSkill() > 0.5f && me->CanSeeSniper())
			{
				// juke back and forth
				if (m_firstDodge)
				{
					next = (RandomInt( 0, 100 ) < 50) ? SLIDE_RIGHT : SLIDE_LEFT;
				}
				else
				{
					next = (m_dodgeState == SLIDE_LEFT) ? SLIDE_RIGHT : SLIDE_LEFT;
				}
			}
			else
			{
				// select next dodge state that is different that our current one
				do
				{
					// low-skill bots may jump when first engaging the enemy (if they are moving)
					const float jumpChance = 33.3f;
					if (m_firstDodge && me->GetProfile()->GetSkill() < 0.5f && RandomFloat( 0, 100 ) < jumpChance && !me->IsNotMoving())
						next = RandomInt( 0, NUM_ATTACK_STATES-1 );
					else
						next = RandomInt( 0, NUM_ATTACK_STATES-2 );
				}
				while( !m_firstDodge && next == m_dodgeState );
			}

			m_dodgeState = (DodgeStateType)next;
			m_nextDodgeStateTimestamp = gpGlobals->curtime + RandomFloat( 0.3f, 1.0f );
			m_firstDodge = false;
		}


		Vector forward, right;
		me->EyeVectors( &forward, &right );

		const float lookAheadRange = 30.0f;
		float ground;

		switch( m_dodgeState )
		{
			case STEADY_ON:
			{
				break;
			}

			case SLIDE_LEFT:
			{
				// don't move left if we will fall
				Vector pos = me->GetAbsOrigin() - (lookAheadRange * right);

				if (me->GetSimpleGroundHeightWithFloor( pos, &ground ))
				{
					if (me->GetAbsOrigin().z - ground < StepHeight)
					{
						me->StrafeLeft();
					}
				}
				break;
			}

			case SLIDE_RIGHT:
			{
				// don't move left if we will fall
				Vector pos = me->GetAbsOrigin() + (lookAheadRange * right);

				if (me->GetSimpleGroundHeightWithFloor( pos, &ground ))
				{
					if (me->GetAbsOrigin().z - ground < StepHeight)
					{
						me->StrafeRight();
					}
				}
				break;
			}

			case JUMP:
			{
				if (me->m_isEnemyVisible)
				{
					me->Jump();
				}
				break;
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Perform attack behavior
 */
void AttackState::OnUpdate( CCSBot *me )
{
	// can't be stuck while attacking
	me->ResetStuckMonitor();

	// if we somehow ended up with the C4 or a grenade in our hands, grab our weapon!
	CWeaponCSBase *weapon = me->GetActiveCSWeapon();
	if (weapon)
	{
		if (weapon->GetWeaponID() == WEAPON_C4 || 
			weapon->GetWeaponID() == WEAPON_HEGRENADE || 
			weapon->GetWeaponID() == WEAPON_FLASHBANG ||
			weapon->GetWeaponID() == WEAPON_SMOKEGRENADE)
		{
			me->EquipBestWeapon();
		}
	}

	CBasePlayer *enemy = me->GetBotEnemy();
	if (enemy == NULL)
	{
		StopAttacking( me );
		return;
	}

	Vector myOrigin = GetCentroid( me );
	Vector enemyOrigin = GetCentroid( enemy );

	// keep track of whether we have seen our enemy at least once yet
	if (!m_haveSeenEnemy)
		m_haveSeenEnemy = me->IsEnemyVisible();


	//
	// Retreat check
	// Do not retreat if the enemy is too close
	//
	if (m_retreatTimer.IsElapsed())
	{
		// If we've been fighting this battle for awhile, we're "pinned down" and
		// need to do something else.
		// If we are outnumbered, retreat.
		// If we see a sniper and we aren't a sniper, retreat.

		bool isPinnedDown = (gpGlobals->curtime > m_pinnedDownTimestamp);

		if (isPinnedDown || 
			(me->CanSeeSniper() && !me->IsSniper()) ||
			(me->IsOutnumbered() && m_isCoward) || 
			(me->OutnumberedCount() >= 2 && me->GetProfile()->GetAggression() < 1.0f))
		{
			// only retreat if at least one of them is aiming at me
			if (me->IsAnyVisibleEnemyLookingAtMe( CHECK_FOV ))
			{
				// tell our teammates our plight
				if (isPinnedDown)
					me->GetChatter()->PinnedDown();
				else if (!me->CanSeeSniper())
					me->GetChatter()->Scared();

				m_retreatTimer.Start( RandomFloat( 3.0f, 15.0f ) );

				// try to retreat
				if (me->TryToRetreat())
				{
					// if we are a sniper, equip our pistol so we can fire while retreating
					/*
					if (me->IsUsingSniperRifle())
					{
						// wait a moment to allow one last shot
						me->Wait( 0.5f );
						//me->EquipPistol();
					}
					*/

					// request backup if outnumbered
					if (me->IsOutnumbered())
					{
						me->GetChatter()->NeedBackup();
					}
				}
				else	
				{
					me->PrintIfWatched( "I want to retreat, but no safe spots nearby!\n" );
				}
			}
		}
	}

	//
	// Knife fighting
	// We need to pathfind right to the enemy to cut him
	//
	if (me->IsUsingKnife())
	{
		// can't crouch and hold with a knife
		m_crouchAndHold = false;
		me->StandUp();

		// if we are using a knife and our prey is looking towards us, run at him
		if (me->IsPlayerFacingMe( enemy ))
		{
			me->ForceRun( 5.0f );
			me->Hurry( 10.0f );
		}

		// slash our victim
		me->FireWeaponAtEnemy();

		// if toe to toe with our enemy, don't dodge, just slash
		const float slashRange = 70.0f;
		if ((enemy->GetAbsOrigin() - me->GetAbsOrigin()).IsLengthGreaterThan( slashRange ))
		{
			const float repathInterval = 0.5f;

			// if our victim has moved, repath
			bool repath = false;
			if (me->HasPath())
			{
				const float repathRange = 100.0f;		// 50
				if ((me->GetPathEndpoint() - enemy->GetAbsOrigin()).IsLengthGreaterThan( repathRange ))
				{
					repath = true;
				}
			}
			else
			{
				repath = true;
			}

			if (repath && m_repathTimer.IsElapsed())
			{
				Vector enemyPos = enemy->GetAbsOrigin() + Vector( 0, 0, HalfHumanHeight );
				me->ComputePath( enemyPos, FASTEST_ROUTE );
				m_repathTimer.Start( repathInterval );
			}

			// move towards victim
			if (me->UpdatePathMovement( NO_SPEED_CHANGE ) != CCSBot::PROGRESSING)
			{
				me->DestroyPath();
			}
		}

		return;
	}



	//
	// Simple shield usage
	//
	if (me->HasShield())
	{
		if (me->IsEnemyVisible() && !m_shieldForceOpen)
		{
			if (!me->IsRecognizedEnemyReloading() && !me->IsReloading() && me->IsPlayerLookingAtMe( enemy ))
			{
				// close up - enemy is pointing his gun at us
				if (!me->IsProtectedByShield())
					me->SecondaryAttack();
			}
			else
			{
				// enemy looking away or reloading his weapon - open up and shoot him
				if (me->IsProtectedByShield())
					me->SecondaryAttack();
			}
		}
		else
		{
			// can't see enemy, open up
			if (me->IsProtectedByShield())
				me->SecondaryAttack();
		}

		if (gpGlobals->curtime > m_shieldToggleTimestamp)
		{
			m_shieldToggleTimestamp = gpGlobals->curtime + RandomFloat( 0.5, 2.0f );

			// toggle shield force open
			m_shieldForceOpen = !m_shieldForceOpen;
		}
	}


	// check if our weapon range is bad and we should switch to pistol
	if (me->IsUsingSniperRifle())
	{
		// if we have a sniper rifle and our enemy is too close, switch to pistol
		const float sniperMinRange = 160.0f;	// NOTE: Must be larger than NO_ZOOM range in AdjustZoom()
		if ((enemyOrigin - myOrigin).IsLengthLessThan( sniperMinRange ))
			me->EquipPistol();
	}
	else if (me->IsUsingShotgun())
	{
		// if we have a shotgun equipped and enemy is too far away, switch to pistol
		const float shotgunMaxRange = 600.0f;
		if ((enemyOrigin - myOrigin).IsLengthGreaterThan( shotgunMaxRange ))
			me->EquipPistol();
	}

	// if we're sniping, look through the scope - need to do this here in case a reload resets our scope
	if (me->IsUsingSniperRifle())
	{
		// for Scouts and AWPs, we need to wait for zoom to resume
		if (me->m_bResumeZoom)
		{
			m_scopeTimestamp = gpGlobals->curtime;
			return;
		}

		Vector toAimSpot3D = me->m_aimSpot - myOrigin;
		float targetRange = toAimSpot3D.Length();

		// dont adjust zoom level if we're already zoomed in - just fire
		if (me->GetZoomLevel() == CCSBot::NO_ZOOM && me->AdjustZoom( targetRange ))
			m_scopeTimestamp = gpGlobals->curtime;
	
		const float waitScopeTime = 0.3f + me->GetProfile()->GetReactionTime();
		if (gpGlobals->curtime - m_scopeTimestamp < waitScopeTime)
		{
			// force us to wait until zoomed in before firing
			return;
		}
	}

	// see if we "notice" that our prey is dead
	if (me->IsAwareOfEnemyDeath())
	{
		// let team know if we killed the last enemy
		if (me->GetLastVictimID() == enemy->entindex() && me->GetNearbyEnemyCount() <= 1)
		{
			me->GetChatter()->KilledMyEnemy( enemy->entindex() );

			// if there are other enemies left, wait a moment - they usually come in groups
			if (me->GetEnemiesRemaining())
			{
				me->Wait( RandomFloat( 1.0f, 3.0f ) );
			}
		}

		StopAttacking( me );
		return;
	}

	float notSeenEnemyTime = gpGlobals->curtime - me->GetLastSawEnemyTimestamp();

	// if we haven't seen our enemy for a moment, continue on if we dont want to fight, or decide to ambush if we do
	if (!me->IsEnemyVisible())
	{
		// attend to nearby enemy gunfire
		if (notSeenEnemyTime > 0.5f && me->CanHearNearbyEnemyGunfire())
		{
			// give up the attack, since we didn't want it in the first place
			StopAttacking( me );

			const Vector *pos = me->GetNoisePosition();
			if (pos)
			{
				me->SetLookAt( "Nearby enemy gunfire", *pos, PRIORITY_HIGH, 0.0f );
				me->PrintIfWatched( "Checking nearby threatening enemy gunfire!\n" );
				return;
			}
		}

		// check if we have lost track of our enemy during combat
		if (notSeenEnemyTime > 0.25f)
		{
			m_isEnemyHidden = true;
		}
					

		if (notSeenEnemyTime > 0.1f)
		{
			if (me->GetDisposition() == CCSBot::ENGAGE_AND_INVESTIGATE)
			{
				// decide whether we should hide and "ambush" our enemy
				if (m_haveSeenEnemy && !m_didAmbushCheck)
				{
					float hideChance = 33.3f;

					if (RandomFloat( 0.0, 100.0f ) < hideChance)
					{
						float ambushTime = RandomFloat( 3.0f, 15.0f );

						// hide in ambush nearby
						/// @todo look towards where we know enemy is
						const Vector *spot = FindNearbyRetreatSpot( me, 200.0f );
						if (spot)
						{
							me->IgnoreEnemies( 1.0f );

							me->Run();
							me->StandUp();
							me->Hide( *spot, ambushTime, true );
							return;
						}
					}

					// don't check again
					m_didAmbushCheck = true;
				}
			}
			else
			{
				// give up the attack, since we didn't want it in the first place
				StopAttacking( me );
				return;
			}
		}
	}
	else
	{
		// we can see the enemy again - reset our ambush check
		m_didAmbushCheck = false;

		// if the enemy is coming out of hiding, we need time to react
		if (m_isEnemyHidden)
		{
			m_reacquireTimestamp = gpGlobals->curtime + me->GetProfile()->GetReactionTime();
			m_isEnemyHidden = false;
		}
	}


	// if we haven't seen our enemy for a long time, chase after them
	float chaseTime = 2.0f + 2.0f * (1.0f - me->GetProfile()->GetAggression());

	// if we are sniping, be very patient
	if (me->IsUsingSniperRifle())
		chaseTime += 3.0f;
	else if (me->IsCrouching())	// if we are crouching, be a little patient
		chaseTime += 1.0f;

	// if we can't see the enemy, and have either seen him but currently lost sight of him, 
	// or haven't yet seen him, chase after him (unless we are a sniper)
	if (!me->IsEnemyVisible() && (notSeenEnemyTime > chaseTime || !m_haveSeenEnemy))
	{
		// snipers don't chase their prey - they wait for their prey to come to them
		if (me->GetTask() == CCSBot::SNIPING)
		{
			StopAttacking( me );
			return;
		}
		else
		{
			// move to last known position of enemy
			me->SetTask( CCSBot::MOVE_TO_LAST_KNOWN_ENEMY_POSITION, enemy );
			me->MoveTo( me->GetLastKnownEnemyPosition() );
			return;
		}
	}


	// if we can't see our enemy at the moment, and were shot by
	// a different visible enemy, engage them instead
	const float hurtRecentlyTime = 3.0f;
	if (!me->IsEnemyVisible() &&
		me->GetTimeSinceAttacked() < hurtRecentlyTime &&
		me->GetAttacker() &&
		me->GetAttacker() != me->GetBotEnemy())
	{
		// if we can see them, attack, otherwise panic
		if (me->IsVisible( me->GetAttacker(), CHECK_FOV ))
		{
			me->Attack( me->GetAttacker() );
			me->PrintIfWatched( "Switching targets to retaliate against new attacker!\n" );
		}
		/*
		 * Rethink this
		else
		{
			me->Panic( me->GetAttacker() );
			me->PrintIfWatched( "Panicking from crossfire while attacking!\n" );
		}
		*/

		return;
	}

	if (true || gpGlobals->curtime > m_reacquireTimestamp)
		me->FireWeaponAtEnemy();


	// do dodge behavior
	Dodge( me );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Finish attack
 */
void AttackState::OnExit( CCSBot *me )
{
	me->PrintIfWatched( "AttackState:OnExit()\n" );

	m_crouchAndHold = false;

	// clear any noises we heard during battle
	me->ForgetNoise();
	me->ResetStuckMonitor();

	// resume our original posture
	me->PopPostureContext();

	// put shield away
	if (me->IsProtectedByShield())
		me->SecondaryAttack();


	//me->StopAiming();
}

