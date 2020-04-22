//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

//
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003
//
// NOTE: The CS Bot code uses Doxygen-style comments. If you run Doxygen over this code, it will 
// auto-generate documentation.  Visit www.doxygen.org to download the system for free.
//

#ifndef _CS_BOT_H_
#define _CS_BOT_H_

#include "bot/bot.h"
#include "bot/cs_bot_manager.h"
#include "bot/cs_bot_chatter.h"
#include "cs_gamestate.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "cs_nav_pathfind.h"
#include "cs_nav_area.h"

class CBaseDoor;
class CBasePropDoor;
class CCSBot;
class CPushAwayEnumerator;

//--------------------------------------------------------------------------------------------------------------
/**
 * For use with player->m_rgpPlayerItems[]
 */
enum InventorySlotType
{
	PRIMARY_WEAPON_SLOT = 1,
	PISTOL_SLOT,
	KNIFE_SLOT,
	GRENADE_SLOT,
	C4_SLOT
};


//--------------------------------------------------------------------------------------------------------------
/**
 * The definition of a bot's behavior state.  One or more finite state machines 
 * using these states implement a bot's behaviors.
 */
class BotState
{
public:
	virtual void OnEnter( CCSBot *bot ) { }				///< when state is entered
	virtual void OnUpdate( CCSBot *bot ) { }			///< state behavior
	virtual void OnExit( CCSBot *bot ) { }				///< when state exited
	virtual const char *GetName( void ) const = 0;		///< return state name
};


//--------------------------------------------------------------------------------------------------------------
/**
 * The state is invoked when a bot has nothing to do, or has finished what it was doing.
 * A bot never stays in this state - it is the main action selection mechanism.
 */
class IdleState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "Idle"; }
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is actively searching for an enemy.
 */
class HuntState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "Hunt"; }

	void ClearHuntArea( void )						{ m_huntArea = NULL; }

private:
	CNavArea *m_huntArea;										///< "far away" area we are moving to
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot has an enemy and is attempting to kill it
 */
class AttackState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "Attack"; }
	
	void SetCrouchAndHold( bool crouch )			{ m_crouchAndHold = crouch; }

protected:
	enum DodgeStateType
	{
		STEADY_ON,
		SLIDE_LEFT,
		SLIDE_RIGHT,
		JUMP,

		NUM_ATTACK_STATES
	};
	DodgeStateType m_dodgeState;
	float m_nextDodgeStateTimestamp;

	CountdownTimer m_repathTimer;
	float m_scopeTimestamp;

	bool m_haveSeenEnemy;										///< false if we haven't yet seen the enemy since we started this attack (told by a friend, etc)
	bool m_isEnemyHidden;										///< true we if we have lost line-of-sight to our enemy
	float m_reacquireTimestamp;									///< time when we can fire again, after losing enemy behind cover
	float m_shieldToggleTimestamp;								///< time to toggle shield deploy state
	bool m_shieldForceOpen;										///< if true, open up and shoot even if in danger

	float m_pinnedDownTimestamp;								///< time when we'll consider ourselves "pinned down" by the enemy

	bool m_crouchAndHold;
	bool m_didAmbushCheck;
	bool m_shouldDodge;
	bool m_firstDodge;

	bool m_isCoward;											///< if true, we'll retreat if outnumbered during this fight
	CountdownTimer m_retreatTimer;

	void StopAttacking( CCSBot *bot );
	void Dodge( CCSBot *bot );									///< do dodge behavior
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot has heard an enemy noise and is moving to find out what it was.
 */
class InvestigateNoiseState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "InvestigateNoise"; }

private:
	void AttendCurrentNoise( CCSBot *bot );						///< move towards currently heard noise
	Vector m_checkNoisePosition;								///< the position of the noise we're investigating
	CountdownTimer m_minTimer;									///< minimum time we will investigate our current noise
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is buying equipment at the start of a round.
 */
class BuyState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "Buy"; }

private:
	bool m_isInitialDelay;
	int m_prefRetries;											///< for retrying buying preferred weapon at current index
	int m_prefIndex;											///< where are we in our list of preferred weapons

	int m_retries;
	bool m_doneBuying;
	bool m_buyDefuseKit;
	bool m_buyGrenade;
	bool m_buyShield;
	bool m_buyPistol;
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is moving to a potentially far away position in the world.
 */
class MoveToState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "MoveTo"; }
	void SetGoalPosition( const Vector &pos )		{ m_goalPosition = pos; }
	void SetRouteType( RouteType route )			{ m_routeType = route; }

private:
	Vector m_goalPosition;										///< goal position of move
	RouteType m_routeType;										///< the kind of route to build
	bool m_radioedPlan;
	bool m_askedForCover;
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a Terrorist bot is moving to pick up a dropped bomb.
 */
class FetchBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual const char *GetName( void ) const	{ return "FetchBomb"; }
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a Terrorist bot is actually planting the bomb.
 */
class PlantBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const	{ return "PlantBomb"; }
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a CT bot is actually defusing a live bomb.
 */
class DefuseBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const	{ return "DefuseBomb"; }
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is hiding in a corner.
 * NOTE: This state also includes MOVING TO that hiding spot, which may be all the way
 * across the map!
 */
class HideState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const	{ return "Hide"; }

	void SetHidingSpot( const Vector &pos )		{ m_hidingSpot = pos; }
	const Vector &GetHidingSpot( void ) const	{ return m_hidingSpot; }

	void SetSearchArea( CNavArea *area )		{ m_searchFromArea = area; }
	void SetSearchRange( float range )			{ m_range = range; }
	void SetDuration( float time )				{ m_duration = time; }
	void SetHoldPosition( bool hold )			{ m_isHoldingPosition = hold; }

	bool IsAtSpot( void ) const					{ return m_isAtSpot; }

	float GetHideTime( void ) const
	{
		if (IsAtSpot())
		{
			return m_duration - m_hideTimer.GetRemainingTime();
		}

		return 0.0f;
	}

private:
	CNavArea *m_searchFromArea;
	float m_range;

	Vector m_hidingSpot;
	bool m_isLookingOutward;
	bool m_isAtSpot;
	float m_duration;
	CountdownTimer m_hideTimer;								///< how long to hide

	bool m_isHoldingPosition;
	float m_holdPositionTime;								///< how long to hold our position after we hear nearby enemy noise

	bool m_heardEnemy;										///< set to true when we first hear an enemy
	float m_firstHeardEnemyTime;							///< when we first heard the enemy

	int m_retry;											///< counter for retrying hiding spot

	Vector m_leaderAnchorPos;								///< the position of our follow leader when we decided to hide

	bool m_isPaused;										///< if true, we have paused in our retreat for a moment
	CountdownTimer m_pauseTimer;							///< for stoppping and starting our pauses while we retreat
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is attempting to flee from a bomb that is about to explode.
 */
class EscapeFromBombState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "EscapeFromBomb"; }
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is following another player.
 */
class FollowState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "Follow"; }

	void SetLeader( CCSPlayer *player )				{ m_leader = player; }

private:
	CHandle< CCSPlayer > m_leader;								///< the player we are following
	Vector m_lastLeaderPos;										///< where the leader was when we computed our follow path
	bool m_isStopped;
	float m_stoppedTimestamp;

	enum LeaderMotionStateType
	{
		INVALID,
		STOPPED,
		WALKING,
		RUNNING
	};
	LeaderMotionStateType m_leaderMotionState;
	IntervalTimer m_leaderMotionStateTime;

	bool m_isSneaking;
	float m_lastSawLeaderTime;
	CountdownTimer m_repathInterval;

	IntervalTimer m_walkTime;
	bool m_isAtWalkSpeed;

	float m_waitTime;
	CountdownTimer m_idleTimer;

	void ComputeLeaderMotionState( float leaderSpeed );
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is actually using another entity (ie: facing towards it and pressing the use key)
 */
class UseEntityState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const			{ return "UseEntity"; }

	void SetEntity( CBaseEntity *entity )				{ m_entity = entity; }

private:
	EHANDLE m_entity;											///< the entity we will use
};


//--------------------------------------------------------------------------------------------------------------
/**
 * When a bot is opening a door
 */
class OpenDoorState : public BotState
{
public:
	virtual void OnEnter( CCSBot *bot );
	virtual void OnUpdate( CCSBot *bot );
	virtual void OnExit( CCSBot *bot );
	virtual const char *GetName( void ) const		{ return "OpenDoor"; }

	void SetDoor( CBaseEntity *door );

	bool IsDone( void ) const						{ return m_isDone; }	///< return true if behavior is done

private:
	CHandle< CBaseDoor > m_funcDoor;									///< the func_door we are opening
	CHandle< CBasePropDoor > m_propDoor;								///< the prop_door we are opening
	bool m_isDone;
	CountdownTimer m_timeout;
};


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
/**
 * The Counter-strike Bot
 */
class CCSBot : public CBot< CCSPlayer >
{
public:
	DECLARE_CLASS( CCSBot, CBot< CCSPlayer > );
	DECLARE_DATADESC();

	CCSBot( void );												///< constructor initializes all values to zero
	virtual ~CCSBot();
	virtual bool Initialize( const BotProfile *profile, int team );		///< (EXTEND) prepare bot for action

	virtual void Spawn( void );									///< (EXTEND) spawn the bot into the game
	virtual void Touch( CBaseEntity *other );					///< (EXTEND) when touched by another entity

	virtual void Upkeep( void );								///< lightweight maintenance, invoked frequently
	virtual void Update( void );								///< heavyweight algorithms, invoked less often
	virtual void BuildUserCmd( CUserCmd& cmd, const QAngle& viewangles, float forwardmove, float sidemove, float upmove, int buttons, byte impulse );
	virtual float GetMoveSpeed( void );							///< returns current movement speed (for walk/run)

	virtual void Walk( void );
	virtual bool Jump( bool mustJump = false );					///< returns true if jump was started

	//- behavior properties ------------------------------------------------------------------------------------------
	float GetCombatRange( void ) const;
	bool IsRogue( void ) const;									///< return true if we dont listen to teammates or pursue scenario goals
	void SetRogue( bool rogue );
	bool IsHurrying( void ) const;								///< return true if we are in a hurry 
	void Hurry( float duration );								///< force bot to hurry
	bool IsSafe( void ) const;									///< return true if we are in a safe region
	bool IsWellPastSafe( void ) const;							///< return true if it is well past the early, "safe", part of the round
	bool IsEndOfSafeTime( void ) const;							///< return true if we were in the safe time last update, but not now
	float GetSafeTimeRemaining( void ) const;					///< return the amount of "safe time" we have left
	float GetSafeTime( void ) const;							///< return what we think the total "safe time" for this map is
	virtual void Blind( float holdTime, float fadeTime, float startingAlpha = 255 );	// player blinded by a flashbang
	bool IsUnhealthy( void ) const;								///< returns true if bot is low on health
	
	bool IsAlert( void ) const;									///< return true if bot is in heightened "alert" mode
	void BecomeAlert( void );									///< bot becomes "alert" for immediately nearby enemies

	bool IsSneaking( void ) const;								///< return true if bot is sneaking
	void Sneak( float duration );								///< sneak for given duration

	//- behaviors ---------------------------------------------------------------------------------------------------
	void Idle( void );

	void Hide( CNavArea *searchFromArea = NULL, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false );	///< DEPRECATED: Use TryToHide() instead
	#define USE_NEAREST true
	bool TryToHide( CNavArea *searchFromArea = NULL, float duration = -1.0f, float hideRange = 750.0f, bool holdPosition = false, bool useNearest = false );	///< try to hide nearby, return false if cannot
	void Hide( const Vector &hidingSpot, float duration = -1.0f, bool holdPosition = false );	///< move to the given hiding place
	bool IsHiding( void ) const;								///< returns true if bot is currently hiding
	bool IsAtHidingSpot( void ) const;							///< return true if we are hiding and at our hiding spot
	float GetHidingTime( void ) const;							///< return number of seconds we have been at our current hiding spot

	bool MoveToInitialEncounter( void );						///< move to a hiding spot and wait for initial encounter with enemy team (return false if no spots are available)

	bool TryToRetreat( float maxRange = 1000.0f, float duration = -1.0f );	///< retreat to a nearby hiding spot, away from enemies

	void Hunt( void );
	bool IsHunting( void ) const;								///< returns true if bot is currently hunting

	void Attack( CCSPlayer *victim );
	void FireWeaponAtEnemy( void );								///< fire our active weapon towards our current enemy
	void StopAttacking( void );
	bool IsAttacking( void ) const;								///< returns true if bot is currently engaging a target

	void MoveTo( const Vector &pos, RouteType route = SAFEST_ROUTE );	///< move to potentially distant position
	bool IsMovingTo( void ) const;								///< return true if we are in the MoveTo state

	void PlantBomb( void );

	void FetchBomb( void );										///< bomb has been dropped - go get it
	bool NoticeLooseBomb( void ) const;							///< return true if we noticed the bomb on the ground or on radar
	bool CanSeeLooseBomb( void ) const;							///< return true if we directly see the loose bomb

	void DefuseBomb( void );
	bool IsDefusingBomb( void ) const;							///< returns true if bot is currently defusing the bomb
	bool CanSeePlantedBomb( void ) const;						///< return true if we directly see the planted bomb

	void EscapeFromBomb( void );
	bool IsEscapingFromBomb( void ) const;						///< return true if we are escaping from the bomb

	void RescueHostages( void );								///< begin process of rescuing hostages

	void UseEntity( CBaseEntity *entity );						///< use the entity

	void OpenDoor( CBaseEntity *door );							///< open the door (assumes we are right in front of it)
	bool IsOpeningDoor( void ) const;							///< return true if we are in the process of opening a door

	void Buy( void );											///< enter the buy state
	bool IsBuying( void ) const;

	void Panic( void );											///< look around in panic
	bool IsPanicking( void ) const;								///< return true if bot is panicked
	void StopPanicking( void );									///< end our panic
	void UpdatePanicLookAround( void );							///< do panic behavior

	void TryToJoinTeam( int team );								///< try to join the given team

	void Follow( CCSPlayer *player );							///< begin following given Player
	void ContinueFollowing( void );								///< continue following our leader after finishing what we were doing
	void StopFollowing( void );									///< stop following
	bool IsFollowing( void ) const;								///< return true if we are following someone (not necessarily in the follow state)
	CCSPlayer *GetFollowLeader( void ) const;					///< return the leader we are following
	float GetFollowDuration( void ) const;						///< return how long we've been following our leader
	bool CanAutoFollow( void ) const;							///< return true if we can auto-follow

	bool IsNotMoving( float minDuration = 0.0f ) const;			///< return true if we are currently standing still and have been for minDuration

	void AimAtEnemy( void );									///< point our weapon towards our enemy
	void StopAiming( void );									///< stop aiming at enemy
	bool IsAimingAtEnemy( void ) const;							///< returns true if we are trying to aim at an enemy

	float GetStateTimestamp( void ) const;						///< get time current state was entered

	bool IsDoingScenario( void ) const;							///< return true if we will do scenario-related tasks

	//- scenario / gamestate -----------------------------------------------------------------------------------------
	CSGameState *GetGameState( void );							///< return an interface to this bot's gamestate
	const CSGameState *GetGameState( void ) const;				///< return an interface to this bot's gamestate

	bool IsAtBombsite( void );									///< return true if we are in a bomb planting zone
	bool GuardRandomZone( float range = 500.0f );				///< pick a random zone and hide near it

	bool IsBusy( void ) const;									///< return true if we are busy doing something important

	//- high-level tasks ---------------------------------------------------------------------------------------------
	enum TaskType
	{
		SEEK_AND_DESTROY,
		PLANT_BOMB,
		FIND_TICKING_BOMB,
		DEFUSE_BOMB,
		GUARD_TICKING_BOMB,
		GUARD_BOMB_DEFUSER,
		GUARD_LOOSE_BOMB,
		GUARD_BOMB_ZONE,
		GUARD_INITIAL_ENCOUNTER,
		ESCAPE_FROM_BOMB,
		HOLD_POSITION,
		FOLLOW,
		VIP_ESCAPE,
		GUARD_VIP_ESCAPE_ZONE,
		COLLECT_HOSTAGES,
		RESCUE_HOSTAGES,
		GUARD_HOSTAGES,
		GUARD_HOSTAGE_RESCUE_ZONE,
		MOVE_TO_LAST_KNOWN_ENEMY_POSITION,
		MOVE_TO_SNIPER_SPOT,
		SNIPING,

		NUM_TASKS
	};
	void SetTask( TaskType task, CBaseEntity *entity = NULL );	///< set our current "task"
	TaskType GetTask( void ) const;
	CBaseEntity *GetTaskEntity( void );
	const char *GetTaskName( void ) const;						///< return string describing current task

	//- behavior modifiers ------------------------------------------------------------------------------------------
	enum DispositionType
	{
		ENGAGE_AND_INVESTIGATE,								///< engage enemies on sight and investigate enemy noises
		OPPORTUNITY_FIRE,									///< engage enemies on sight, but only look towards enemy noises, dont investigate
		SELF_DEFENSE,										///< only engage if fired on, or very close to enemy
		IGNORE_ENEMIES,										///< ignore all enemies - useful for ducking around corners, running away, etc

		NUM_DISPOSITIONS
	};
	void SetDisposition( DispositionType disposition );		///< define how we react to enemies
	DispositionType GetDisposition( void ) const;
	const char *GetDispositionName( void ) const;			///< return string describing current disposition

	void IgnoreEnemies( float duration );					///< ignore enemies for a short duration

	enum MoraleType
	{
		TERRIBLE = -3,
		BAD = -2,
		NEGATIVE = -1,
		NEUTRAL = 0,
		POSITIVE = 1,
		GOOD = 2,
		EXCELLENT = 3,
	};
	MoraleType GetMorale( void ) const;
	const char *GetMoraleName( void ) const;				///< return string describing current morale
	void IncreaseMorale( void );
	void DecreaseMorale( void );

	void Surprise( float duration );						///< become "surprised" - can't attack
	bool IsSurprised( void ) const;							///< return true if we are "surprised"


	//- listening for noises ----------------------------------------------------------------------------------------
	bool IsNoiseHeard( void ) const;							///< return true if we have heard a noise
	bool HeardInterestingNoise( void );							///< return true if we heard an enemy noise worth checking in to
	void InvestigateNoise( void );								///< investigate recent enemy noise
	bool IsInvestigatingNoise( void ) const;					///< return true if we are investigating a noise
	const Vector *GetNoisePosition( void ) const;				///< return position of last heard noise, or NULL if none heard
	CNavArea *GetNoiseArea( void ) const;						///< return area where noise was heard
	void ForgetNoise( void );									///< clear the last heard noise
	bool CanSeeNoisePosition( void ) const;						///< return true if we directly see where we think the noise came from
	float GetNoiseRange( void ) const;							///< return approximate distance to last noise heard

	bool CanHearNearbyEnemyGunfire( float range = -1.0f ) const;///< return true if we hear nearby threatening enemy gunfire within given range (-1 == infinite)
	PriorityType GetNoisePriority( void ) const;				///< return priority of last heard noise

	//- radio and chatter--------------------------------------------------------------------------------------------
	void SendRadioMessage( RadioType event );					///< send a radio message
	void SpeakAudio( const char *voiceFilename, float duration, int pitch );	///< send voice chatter
	BotChatterInterface *GetChatter( void );					///< return an interface to this bot's chatter system
	bool RespondToHelpRequest( CCSPlayer *player, Place place, float maxRange = -1.0f );	///< decide if we should move to help the player, return true if we will
	bool IsUsingVoice() const;									///< new-style "voice" chatter gets voice feedback


	//- enemies ------------------------------------------------------------------------------------------------------
	// BOTPORT: GetEnemy() collides with GetEnemy() in CBaseEntity - need to use different nomenclature
	void SetBotEnemy( CCSPlayer *enemy );						///< set given player as our current enemy
	CCSPlayer *GetBotEnemy( void ) const;
	int GetNearbyEnemyCount( void ) const;						///< return max number of nearby enemies we've seen recently
	unsigned int GetEnemyPlace( void ) const;					///< return location where we see the majority of our enemies
	bool CanSeeBomber( void ) const;							///< return true if we can see the bomb carrier
	CCSPlayer *GetBomber( void ) const;

	int GetNearbyFriendCount( void ) const;						///< return number of nearby teammates
	CCSPlayer *GetClosestVisibleFriend( void ) const;			///< return the closest friend that we can see
	CCSPlayer *GetClosestVisibleHumanFriend( void ) const;		///< return the closest human friend that we can see

	bool IsOutnumbered( void ) const;							///< return true if we are outnumbered by enemies
	int OutnumberedCount( void ) const;							///< return number of enemies we are outnumbered by

	#define ONLY_VISIBLE_ENEMIES true
	CCSPlayer *GetImportantEnemy( bool checkVisibility = false ) const;	///< return the closest "important" enemy for the given scenario (bomb carrier, VIP, hostage escorter)

	void UpdateReactionQueue( void );							///< update our reaction time queue
	CCSPlayer *GetRecognizedEnemy( void );						///< return the most dangerous threat we are "conscious" of
	bool IsRecognizedEnemyReloading( void );					///< return true if the enemy we are "conscious" of is reloading
	bool IsRecognizedEnemyProtectedByShield( void );			///< return true if the enemy we are "conscious" of is hiding behind a shield
	float GetRangeToNearestRecognizedEnemy( void );				///< return distance to closest enemy we are "conscious" of

	CCSPlayer *GetAttacker( void ) const;						///< return last enemy that hurt us
	float GetTimeSinceAttacked( void ) const;					///< return duration since we were last injured by an attacker
	float GetFirstSawEnemyTimestamp( void ) const;				///< time since we saw any enemies
	float GetLastSawEnemyTimestamp( void ) const;
	float GetTimeSinceLastSawEnemy( void ) const;
	float GetTimeSinceAcquiredCurrentEnemy( void ) const;
	bool HasNotSeenEnemyForLongTime( void ) const;				///< return true if we haven't seen an enemy for "a long time"
	const Vector &GetLastKnownEnemyPosition( void ) const;
	bool IsEnemyVisible( void ) const;							///< is our current enemy visible
	float GetEnemyDeathTimestamp( void ) const;
	bool IsFriendInLineOfFire( void );							///< return true if a friend is in our weapon's way
	bool IsAwareOfEnemyDeath( void ) const;						///< return true if we *noticed* that our enemy died
	int GetLastVictimID( void ) const;							///< return the ID (entindex) of the last victim we killed, or zero

	bool CanSeeSniper( void ) const;							///< return true if we can see an enemy sniper
	bool HasSeenSniperRecently( void ) const;					///< return true if we have seen a sniper recently

	float GetTravelDistanceToPlayer( CCSPlayer *player ) const;	///< return shortest path travel distance to this player	
	bool DidPlayerJustFireWeapon( const CCSPlayer *player ) const;	///< return true if the given player just fired their weapon

	//- navigation --------------------------------------------------------------------------------------------------
	bool HasPath( void ) const;
	void DestroyPath( void );

	float GetFeetZ( void ) const;								///< return Z of bottom of feet

	enum PathResult
	{
		PROGRESSING,		///< we are moving along the path
		END_OF_PATH,		///< we reached the end of the path
		PATH_FAILURE		///< we failed to reach the end of the path
	};
	#define NO_SPEED_CHANGE false
	PathResult UpdatePathMovement( bool allowSpeedChange = true );	///< move along our computed path - if allowSpeedChange is true, bot will walk when near goal to ensure accuracy

	//bool AStarSearch( CNavArea *startArea, CNavArea *goalArea );	///< find shortest path from startArea to goalArea - don't actually buid the path
	bool ComputePath( const Vector &goal, RouteType route = SAFEST_ROUTE );	///< compute path to goal position
	bool StayOnNavMesh( void );
	CNavArea *GetLastKnownArea( void ) const;						///< return the last area we know we were inside of
	const Vector &GetPathEndpoint( void ) const;					///< return final position of our current path
	float GetPathDistanceRemaining( void ) const;					///< return estimated distance left to travel along path
	void ResetStuckMonitor( void );
	bool IsAreaVisible( const CNavArea *area ) const;				///< is any portion of the area visible to this bot
	const Vector &GetPathPosition( int index ) const;
	bool GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal = NULL );	///< find "simple" ground height, treating current nav area as part of the floor
	void BreakablesCheck( void );
	void DoorCheck( void );											///< Check for any doors along our path that need opening

	virtual void PushawayTouch( CBaseEntity *pOther );

	Place GetPlace( void ) const;									///< get our current radio chatter place

	bool IsUsingLadder( void ) const;								///< returns true if we are in the process of negotiating a ladder
	void GetOffLadder( void );										///< immediately jump off of our ladder, if we're on one

	void SetGoalEntity( CBaseEntity *entity );
	CBaseEntity *GetGoalEntity( void );

	bool IsNearJump( void ) const;									///< return true if nearing a jump in the path
	float GetApproximateFallDamage( float height ) const;			///< return how much damage will will take from the given fall height

	void ForceRun( float duration );								///< force the bot to run if it moves for the given duration
	virtual bool IsRunning( void ) const;

	void Wait( float duration );									///< wait where we are for the given duration
	bool IsWaiting( void ) const;									///< return true if we are waiting
	void StopWaiting( void );										///< stop waiting

	void Wiggle( void );											///< random movement, for getting un-stuck

	bool IsFriendInTheWay( const Vector &goalPos );					///< return true if a friend is between us and the given position
	void FeelerReflexAdjustment( Vector *goalPosition );			///< do reflex avoidance movements if our "feelers" are touched

	bool HasVisitedEnemySpawn( void ) const;						///< return true if we have visited enemy spawn at least once
	bool IsAtEnemySpawn( void ) const;								///< return true if we are at the/an enemy spawn right now

	//- looking around ----------------------------------------------------------------------------------------------

	// BOTPORT: EVIL VILE HACK - why is EyePosition() not const?!?!?
	const Vector &EyePositionConst( void ) const;
	
	void SetLookAngles( float yaw, float pitch );					///< set our desired look angles
	void UpdateLookAngles( void );									///< move actual view angles towards desired ones
	void UpdateLookAround( bool updateNow = false );				///< update "looking around" mechanism
	void InhibitLookAround( float duration );						///< block all "look at" and "looking around" behavior for given duration - just look ahead

	/// @todo Clean up notion of "forward angle" and "look ahead angle"
	void SetForwardAngle( float angle );							///< define our forward facing
	void SetLookAheadAngle( float angle );							///< define default look ahead angle

	/// look at the given point in space for the given duration (-1 means forever)
	void SetLookAt( const char *desc, const Vector &pos, PriorityType pri, float duration = -1.0f, bool clearIfClose = false, float angleTolerance = 5.0f, bool attack = false );
	void ClearLookAt( void );										///< stop looking at a point in space and just look ahead
	bool IsLookingAtSpot( PriorityType pri = PRIORITY_LOW ) const;	///< return true if we are looking at spot with equal or higher priority
	bool IsViewMoving( float angleVelThreshold = 1.0f ) const;		///< returns true if bot's view angles are rotating (not still)
	bool HasViewBeenSteady( float duration ) const;					///< how long has our view been "steady" (ie: not moving) for given duration

	bool HasLookAtTarget( void ) const;								///< return true if we are in the process of looking at a target

	enum VisiblePartType
	{
		NONE		= 0x00,
		GUT			= 0x01,
		HEAD		= 0x02,
		LEFT_SIDE	= 0x04,			///< the left side of the object from our point of view (not their left side)
		RIGHT_SIDE	= 0x08,			///< the right side of the object from our point of view (not their right side)
		FEET		= 0x10
	};

	#define CHECK_FOV true
	bool IsVisible( const Vector &pos, bool testFOV = false, const CBaseEntity *ignore = NULL ) const;	///< return true if we can see the point
	bool IsVisible( CCSPlayer *player, bool testFOV = false, unsigned char *visParts = NULL ) const;	///< return true if we can see any part of the player

	bool IsNoticable( const CCSPlayer *player, unsigned char visibleParts ) const;	///< return true if we "notice" given player 

	bool IsEnemyPartVisible( VisiblePartType part ) const;			///< if enemy is visible, return the part we see for our current enemy
	const Vector &GetPartPosition( CCSPlayer *player, VisiblePartType part ) const;	///< return world space position of given part on player

	float ComputeWeaponSightRange( void );							///< return line-of-sight distance to obstacle along weapon fire ray

	bool IsAnyVisibleEnemyLookingAtMe( bool testFOV = false ) const;///< return true if any enemy I have LOS to is looking directly at me

	bool IsSignificantlyCloser( const CCSPlayer *testPlayer, const CCSPlayer *referencePlayer ) const;	///< return true if testPlayer is significantly closer than referencePlayer

	//- approach points ---------------------------------------------------------------------------------------------
	void ComputeApproachPoints( void );								///< determine the set of "approach points" representing where the enemy can enter this region
	void UpdateApproachPoints( void );								///< recompute the approach point set if we have moved far enough to invalidate the current ones
	void ClearApproachPoints( void );
	void DrawApproachPoints( void ) const;							///< for debugging
	float GetHidingSpotCheckTimestamp( HidingSpot *spot ) const;	///< return time when given spot was last checked
	void SetHidingSpotCheckTimestamp( HidingSpot *spot );			///< set the timestamp of the given spot to now

	const CNavArea *GetInitialEncounterArea( void ) const;			///< return area where we think we will first meet the enemy
	void SetInitialEncounterArea( const CNavArea *area );

	//- weapon query and equip --------------------------------------------------------------------------------------
	#define MUST_EQUIP true
	void EquipBestWeapon( bool mustEquip = false );					///< equip the best weapon we are carrying that has ammo
	void EquipPistol( void );										///< equip our pistol
	void EquipKnife( void );										///< equip the knife

	#define DONT_USE_SMOKE_GRENADE true
	bool EquipGrenade( bool noSmoke = false );						///< equip a grenade, return false if we cant

	bool IsUsingKnife( void ) const;								///< returns true if we have knife equipped
	bool IsUsingPistol( void ) const;								///< returns true if we have pistol equipped
	bool IsUsingGrenade( void ) const;								///< returns true if we have grenade equipped
	bool IsUsingSniperRifle( void ) const;							///< returns true if using a "sniper" rifle
	bool IsUsing( CSWeaponID weapon ) const;						///< returns true if using the specific weapon
	bool IsSniper( void ) const;									///< return true if we have a sniper rifle in our inventory
	bool IsSniping( void ) const;									///< return true if we are actively sniping (moving to sniper spot or settled in)
	bool IsUsingShotgun( void ) const;								///< returns true if using a shotgun
	bool IsUsingMachinegun( void ) const;							///< returns true if using the big 'ol machinegun
	void ThrowGrenade( const Vector &target );						///< begin the process of throwing the grenade
	bool IsThrowingGrenade( void ) const;							///< return true if we are in the process of throwing a grenade
	bool HasGrenade( void ) const;									///< return true if we have a grenade in our inventory
	void AvoidEnemyGrenades( void );								///< react to enemy grenades we see
	bool IsAvoidingGrenade( void ) const;							///< return true if we are in the act of avoiding a grenade
	bool DoesActiveWeaponHaveSilencer( void ) const;				///< returns true if we are using a weapon with a removable silencer
	bool CanActiveWeaponFire( void ) const;							///< returns true if our current weapon can attack
	CWeaponCSBase *GetActiveCSWeapon( void ) const;					///< get our current Counter-Strike weapon

	void GiveWeapon( const char *weaponAlias );						///< Debug command to give a named weapon

	virtual void PrimaryAttack( void );								///< presses the fire button, unless we're holding a pistol that can't fire yet (so we can just always call PrimaryAttack())

	enum ZoomType { NO_ZOOM, LOW_ZOOM, HIGH_ZOOM };
	ZoomType GetZoomLevel( void );									///< return the current zoom level of our weapon

	bool AdjustZoom( float range );									///< change our zoom level to be appropriate for the given range
	bool IsWaitingForZoom( void ) const;							///< return true if we are reacquiring after our zoom

	bool IsPrimaryWeaponEmpty( void ) const;						///< return true if primary weapon doesn't exist or is totally out of ammo
	bool IsPistolEmpty( void ) const;								///< return true if pistol doesn't exist or is totally out of ammo

	int GetHostageEscortCount( void ) const;						///< return the number of hostages following me
	void IncreaseHostageEscortCount( void );
	float GetRangeToFarthestEscortedHostage( void ) const;			///< return euclidean distance to farthest escorted hostage
	void ResetWaitForHostagePatience( void );

	//------------------------------------------------------------------------------------
	// Event hooks
	//

	/// invoked when injured by something (EXTEND) - returns the amount of damage inflicted
	virtual int OnTakeDamage( const CTakeDamageInfo &info );

	/// invoked when killed (EXTEND)
	virtual void Event_Killed( const CTakeDamageInfo &info );

	virtual bool BumpWeapon( CBaseCombatWeapon *pWeapon );		///< invoked when in contact with a CWeaponBox


	/// invoked when event occurs in the game (some events have NULL entity)
	void OnPlayerFootstep( IGameEvent *event );
	void OnPlayerRadio( IGameEvent *event );
	void OnPlayerDeath( IGameEvent *event );
	void OnPlayerFallDamage( IGameEvent *event );

	void OnBombPickedUp( IGameEvent *event );
	void OnBombPlanted( IGameEvent *event );
	void OnBombBeep( IGameEvent *event );
	void OnBombDefuseBegin( IGameEvent *event );
	void OnBombDefused( IGameEvent *event );
	void OnBombDefuseAbort( IGameEvent *event );
	void OnBombExploded( IGameEvent *event );

	void OnRoundEnd( IGameEvent *event );
	void OnRoundStart( IGameEvent *event );

	void OnDoorMoving( IGameEvent *event );

	void OnBreakProp( IGameEvent *event );
	void OnBreakBreakable( IGameEvent *event );

	void OnHostageFollows( IGameEvent *event );
	void OnHostageRescuedAll( IGameEvent *event );

	void OnWeaponFire( IGameEvent *event );
	void OnWeaponFireOnEmpty( IGameEvent *event );
	void OnWeaponReload( IGameEvent *event );
	void OnWeaponZoom( IGameEvent *event );

	void OnBulletImpact( IGameEvent *event );

	void OnHEGrenadeDetonate( IGameEvent *event );
	void OnFlashbangDetonate( IGameEvent *event );
	void OnSmokeGrenadeDetonate( IGameEvent *event );
	void OnGrenadeBounce( IGameEvent *event );

	void OnNavBlocked( IGameEvent *event );

	void OnEnteredNavArea( CNavArea *newArea );						///< invoked when bot enters a nav area

private:
	#define IS_FOOTSTEP true
	void OnAudibleEvent( IGameEvent *event, CBasePlayer *player, float range, PriorityType priority, bool isHostile, bool isFootstep = false, const Vector *actualOrigin = NULL );	///< Checks if the bot can hear the event

private:
	friend class CCSBotManager;

	/// @todo Get rid of these
	friend class AttackState;
	friend class BuyState;

	// BOTPORT: Remove this vile hack
	Vector m_eyePosition;

	void ResetValues( void );										///< reset internal data to initial state
	void BotDeathThink( void );

	char m_name[64];												///< copied from STRING(pev->netname) for debugging
	void DebugDisplay( void ) const;								///< render bot debug info

	//- behavior properties ------------------------------------------------------------------------------------------
	float m_combatRange;											///< desired distance between us and them during gunplay
	mutable bool m_isRogue;											///< if true, the bot is a "rogue" and listens to no-one
	mutable CountdownTimer m_rogueTimer;
	MoraleType m_morale;											///< our current morale, based on our win/loss history
	bool m_diedLastRound;											///< true if we died last round
	float m_safeTime;												///< duration at the beginning of the round where we feel "safe"
	bool m_wasSafe;													///< true if we were in the safe time last update
	void AdjustSafeTime( void );									///< called when enemy seen to adjust safe time for this round
	NavRelativeDirType m_blindMoveDir;								///< which way to move when we're blind
	bool m_blindFire;												///< if true, fire weapon while blinded
	CountdownTimer m_surpriseTimer;									///< when we were surprised

	bool m_isFollowing;												///< true if we are following someone
	CHandle< CCSPlayer > m_leader;									///< the ID of who we are following
	float m_followTimestamp;										///< when we started following
	float m_allowAutoFollowTime;									///< time when we can auto follow

	CountdownTimer m_hurryTimer;									///< if valid, bot is in a hurry
	CountdownTimer m_alertTimer;									///< if valid, bot is alert
	CountdownTimer m_sneakTimer;									///< if valid, bot is sneaking
	CountdownTimer m_panicTimer;									///< if valid, bot is panicking


	// instances of each possible behavior state, to avoid dynamic memory allocation during runtime
	IdleState				m_idleState;
	HuntState				m_huntState;
	AttackState				m_attackState;
	InvestigateNoiseState	m_investigateNoiseState;
	BuyState				m_buyState;
	MoveToState				m_moveToState;
	FetchBombState			m_fetchBombState;
	PlantBombState			m_plantBombState;
	DefuseBombState			m_defuseBombState;
	HideState				m_hideState;
	EscapeFromBombState		m_escapeFromBombState;
	FollowState				m_followState;
	UseEntityState			m_useEntityState;
	OpenDoorState			m_openDoorState;

	/// @todo Allow multiple simultaneous state machines (look around, etc)	
	void SetState( BotState *state );								///< set the current behavior state
	BotState *m_state;												///< current behavior state
	float m_stateTimestamp;											///< time state was entered
	bool m_isAttacking;												///< if true, special Attack state is overriding the state machine
	bool m_isOpeningDoor;											///< if true, special OpenDoor state is overriding the state machine

	TaskType m_task;												///< our current task
	EHANDLE m_taskEntity;											///< an entity used for our task

	//- navigation ---------------------------------------------------------------------------------------------------
	Vector m_goalPosition;
	EHANDLE m_goalEntity;
	void MoveTowardsPosition( const Vector &pos );					///< move towards position, independant of view angle
	void MoveAwayFromPosition( const Vector &pos );					///< move away from position, independant of view angle
	void StrafeAwayFromPosition( const Vector &pos );				///< strafe (sidestep) away from position, independant of view angle
	void StuckCheck( void );										///< check if we have become stuck
	CCSNavArea *m_currentArea;										///< the nav area we are standing on
	CCSNavArea *m_lastKnownArea;										///< the last area we were in
	EHANDLE m_avoid;												///< higher priority player we need to make way for
	float m_avoidTimestamp;
	bool m_isStopping;												///< true if we're trying to stop because we entered a 'stop' nav area
	bool m_hasVisitedEnemySpawn;									///< true if we have been at the enemy spawn
	IntervalTimer m_stillTimer;										///< how long we have been not moving

	//- path navigation data ----------------------------------------------------------------------------------------
	enum { MAX_PATH_LENGTH = 256 };
	struct ConnectInfo
	{
		CNavArea *area;												///< the area along the path
		NavTraverseType how;										///< how to enter this area from the previous one
		Vector pos;													///< our movement goal position at this point in the path
		const CNavLadder *ladder;									///< if "how" refers to a ladder, this is it
	}
	m_path[ MAX_PATH_LENGTH ];
	int m_pathLength;
	int m_pathIndex;												///< index of next area on path
	float m_areaEnteredTimestamp;
	void BuildTrivialPath( const Vector &goal );					///< build trivial path to goal, assuming we are already in the same area

	CountdownTimer m_repathTimer;									///< must have elapsed before bot can pathfind again

	bool ComputePathPositions( void );								///< determine actual path positions bot will move between along the path
	void SetupLadderMovement( void );
	void SetPathIndex( int index );									///< set the current index along the path
	void DrawPath( void );
	int FindOurPositionOnPath( Vector *close, bool local = false ) const;	///< compute the closest point to our current position on our path
	int FindPathPoint( float aheadRange, Vector *point, int *prevIndex = NULL );	///< compute a point a fixed distance ahead along our path.
	bool FindClosestPointOnPath( const Vector &pos, int startIndex, int endIndex, Vector *close ) const;	///< compute closest point on path to given point
	bool IsStraightLinePathWalkable( const Vector &goal ) const;	///< test for un-jumpable height change, or unrecoverable fall
	void ComputeLadderAngles( float *yaw, float *pitch );			///< computes ideal yaw/pitch for traversing the current ladder on our path

	mutable CountdownTimer m_avoidFriendTimer;						///< used to throttle how often we check for friends in our path
	mutable bool m_isFriendInTheWay;								///< true if a friend is blocking our path
	CountdownTimer m_politeTimer;									///< we'll wait for friend to move until this runs out
	bool m_isWaitingBehindFriend;									///< true if we are waiting for a friend to move

	#define ONLY_JUMP_DOWN true
	bool DiscontinuityJump( float ground, bool onlyJumpDown = false, bool mustJump = false ); ///< check if we need to jump due to height change

	enum LadderNavState
	{
		APPROACH_ASCENDING_LADDER,									///< prepare to scale a ladder
		APPROACH_DESCENDING_LADDER,									///< prepare to go down ladder 
		FACE_ASCENDING_LADDER,
		FACE_DESCENDING_LADDER,
		MOUNT_ASCENDING_LADDER,										///< move toward ladder until "on" it
		MOUNT_DESCENDING_LADDER,									///< move toward ladder until "on" it
		ASCEND_LADDER,												///< go up the ladder
		DESCEND_LADDER,												///< go down the ladder
		DISMOUNT_ASCENDING_LADDER,									///< get off of the ladder
		DISMOUNT_DESCENDING_LADDER,									///< get off of the ladder
		MOVE_TO_DESTINATION,										///< dismount ladder and move to destination area
	}
	m_pathLadderState;
	bool m_pathLadderFaceIn;										///< if true, face towards ladder, otherwise face away
	const CNavLadder *m_pathLadder;									///< the ladder we need to use to reach the next area
	bool UpdateLadderMovement( void );								///< called by UpdatePathMovement()
	NavRelativeDirType m_pathLadderDismountDir;						///< which way to dismount
	float m_pathLadderDismountTimestamp;							///< time when dismount started
	float m_pathLadderEnd;											///< if ascending, z of top, if descending z of bottom
	void ComputeLadderEndpoint( bool ascending );
	float m_pathLadderTimestamp;									///< time when we started using ladder - for timeout check

	CountdownTimer m_mustRunTimer;									///< if nonzero, bot cannot walk
	CountdownTimer m_waitTimer;										///< if nonzero, we are waiting where we are

	void UpdateTravelDistanceToAllPlayers( void );					///< periodically compute shortest path distance to each player
	CountdownTimer m_updateTravelDistanceTimer;						///< for throttling travel distance computations
	float m_playerTravelDistance[ MAX_PLAYERS ];					///< current distance from this bot to each player
	unsigned char m_travelDistancePhase;							///< a counter for optimizing when to compute travel distance

	//- game scenario mechanisms -------------------------------------------------------------------------------------
	CSGameState m_gameState;										///< our current knowledge about the state of the scenario

	byte m_hostageEscortCount;										///< the number of hostages we're currently escorting
	void UpdateHostageEscortCount( void );							///< periodic check of hostage count in case we lost some
	float m_hostageEscortCountTimestamp;

	int m_desiredTeam;												///< the team we want to be on
	bool m_hasJoined;												///< true if bot has actually joined the game

	bool m_isWaitingForHostage;
	CountdownTimer m_inhibitWaitingForHostageTimer;					///< if active, inhibits us waiting for lagging hostages
	CountdownTimer m_waitForHostageTimer;							///< stops us waiting too long

	//- listening mechanism ------------------------------------------------------------------------------------------
	Vector m_noisePosition;											///< position we last heard non-friendly noise
	float m_noiseTravelDistance;									///< the travel distance to the noise
	float m_noiseTimestamp;											///< when we heard it (can get zeroed)
	CNavArea *m_noiseArea;											///< the nav area containing the noise
	PriorityType m_noisePriority;									///< priority of currently heard noise
	bool UpdateLookAtNoise( void );									///< return true if we decided to look towards the most recent noise source
	CountdownTimer m_noiseBendTimer;								///< for throttling how often we bend our line of sight to the noise location
	Vector m_bentNoisePosition;										///< the last computed bent line of sight
	bool m_bendNoisePositionValid;

	//- "looking around" mechanism -----------------------------------------------------------------------------------
	float m_lookAroundStateTimestamp;								///< time of next state change
	float m_lookAheadAngle;											///< our desired forward look angle
	float m_forwardAngle;											///< our current forward facing direction
	float m_inhibitLookAroundTimestamp;								///< time when we can look around again

	enum LookAtSpotState
	{
		NOT_LOOKING_AT_SPOT,			///< not currently looking at a point in space
		LOOK_TOWARDS_SPOT,				///< in the process of aiming at m_lookAtSpot
		LOOK_AT_SPOT,					///< looking at m_lookAtSpot
		NUM_LOOK_AT_SPOT_STATES
	}
	m_lookAtSpotState;
	Vector m_lookAtSpot;											///< the spot we're currently looking at
	PriorityType m_lookAtSpotPriority;
	float m_lookAtSpotDuration;										///< how long we need to look at the spot
	float m_lookAtSpotTimestamp;									///< when we actually began looking at the spot
	float m_lookAtSpotAngleTolerance;								///< how exactly we must look at the spot
	bool m_lookAtSpotClearIfClose;									///< if true, the look at spot is cleared if it gets close to us
	bool m_lookAtSpotAttack;										///< if true, the look at spot should be attacked
	const char *m_lookAtDesc;										///< for debugging
	void UpdateLookAt( void );
	void UpdatePeripheralVision();									///< update enounter spot timestamps, etc
	float m_peripheralTimestamp;

	enum { MAX_APPROACH_POINTS = 16 };
	struct ApproachPoint
	{
		Vector m_pos;
		CNavArea *m_area;
	};

	ApproachPoint m_approachPoint[ MAX_APPROACH_POINTS ];
	unsigned char m_approachPointCount;
	Vector m_approachPointViewPosition;								///< the position used when computing current approachPoint set

	CBaseEntity * FindEntitiesOnPath( float distance, CPushAwayEnumerator *enumerator, bool checkStuck );

	IntervalTimer m_viewSteadyTimer;								///< how long has our view been "steady" (ie: not moving)

	bool BendLineOfSight( const Vector &eye, const Vector &target, Vector *bend, float angleLimit = 135.0f ) const;		///< "bend" our line of sight until we can see the target point. Return bend point, false if cant bend.
	bool FindApproachPointNearestPath( Vector *pos );				///< find the approach point that is nearest to our current path, ahead of us
	bool FindGrenadeTossPathTarget( Vector *pos );					///< find spot to throw grenade ahead of us and "around the corner" along our path
	enum GrenadeTossState
	{
		NOT_THROWING,				///< not yet throwing
		START_THROW,				///< lining up throw
		THROW_LINED_UP,				///< pause for a moment when on-line
		FINISH_THROW,				///< throwing
	};
	GrenadeTossState m_grenadeTossState;
	CountdownTimer m_tossGrenadeTimer;								///< timeout timer for grenade tossing
	const CNavArea *m_initialEncounterArea;							///< area where we think we will initially encounter the enemy
	void LookForGrenadeTargets( void );								///< look for grenade throw targets and throw our grenade at them
	void UpdateGrenadeThrow( void );								///< process grenade throwing
	CountdownTimer m_isAvoidingGrenade;								///< if nonzero we are in the act of avoiding a grenade


	SpotEncounter *m_spotEncounter;									///< the spots we will encounter as we move thru our current area
	float m_spotCheckTimestamp;										///< when to check next encounter spot

	/// @todo Add timestamp for each possible client to hiding spots
	enum { MAX_CHECKED_SPOTS = 64 };
	struct HidingSpotCheckInfo
	{
		HidingSpot *spot;
		float timestamp;
	}
	m_checkedHidingSpot[ MAX_CHECKED_SPOTS ];
	int m_checkedHidingSpotCount;

	//- view angle mechanism -----------------------------------------------------------------------------------------
	float m_lookPitch;												///< our desired look pitch angle
	float m_lookPitchVel;
	float m_lookYaw;												///< our desired look yaw angle
	float m_lookYawVel;

	//- aim angle mechanism -----------------------------------------------------------------------------------------
	Vector m_aimOffset;												///< current error added to victim's position to get actual aim spot
	Vector m_aimOffsetGoal;											///< desired aim offset
	float m_aimOffsetTimestamp;										///< time of next offset adjustment
	float m_aimSpreadTimestamp;										///< time used to determine max spread as it begins to tighten up
	void SetAimOffset( float accuracy );							///< set the current aim offset
	void UpdateAimOffset( void );									///< wiggle aim error based on m_accuracy
	Vector m_aimSpot;												///< the spot we are currently aiming to fire at

	struct PartInfo
	{
		Vector m_headPos;											///< current head position
		Vector m_gutPos;											///< current gut position
		Vector m_feetPos;											///< current feet position
		Vector m_leftSidePos;										///< current left side position
		Vector m_rightSidePos;										///< current right side position
		int m_validFrame;											///< frame of last computation (for lazy evaluation)
	};
	static PartInfo m_partInfo[ MAX_PLAYERS ];						///< part positions for each player
	void ComputePartPositions( CCSPlayer *player );					///< compute part positions from bone location

	//- attack state data --------------------------------------------------------------------------------------------
	DispositionType m_disposition;									///< how we will react to enemies
	CountdownTimer m_ignoreEnemiesTimer;							///< how long will we ignore enemies
	mutable CHandle< CCSPlayer > m_enemy;							///< our current enemy
	bool m_isEnemyVisible;											///< result of last visibility test on enemy
	unsigned char m_visibleEnemyParts;								///< which parts of the visible enemy do we see
	Vector m_lastEnemyPosition;										///< last place we saw the enemy
	float m_lastSawEnemyTimestamp;
	float m_firstSawEnemyTimestamp;
	float m_currentEnemyAcquireTimestamp;
	float m_enemyDeathTimestamp;									///< if m_enemy is dead, this is when he died
	float m_friendDeathTimestamp;									///< time since we saw a friend die
	bool m_isLastEnemyDead;											///< true if we killed or saw our last enemy die
	int m_nearbyEnemyCount;											///< max number of enemies we've seen recently
	unsigned int m_enemyPlace;										///< the location where we saw most of our enemies

	struct WatchInfo
	{
		float timestamp;											///< time we last saw this player, zero if never seen
		bool isEnemy;
	}
	m_watchInfo[ MAX_PLAYERS ];
	mutable CHandle< CCSPlayer > m_bomber;							///< points to bomber if we can see him

	int m_nearbyFriendCount;										///< number of nearby teammates
	mutable CHandle< CCSPlayer > m_closestVisibleFriend;			///< the closest friend we can see
	mutable CHandle< CCSPlayer > m_closestVisibleHumanFriend;		///< the closest human friend we can see

	IntervalTimer m_attentionInterval;								///< time between attention checks

	mutable CHandle< CCSPlayer > m_attacker;						///< last enemy that hurt us (may not be same as m_enemy)
	float m_attackedTimestamp;										///< when we were hurt by the m_attacker

	int m_lastVictimID;												///< the entindex of the last victim we killed, or zero
	bool m_isAimingAtEnemy;											///< if true, we are trying to aim at our enemy
	bool m_isRapidFiring;											///< if true, RunUpkeep() will toggle our primary attack as fast as it can
	IntervalTimer m_equipTimer;										///< how long have we had our current weapon equipped
	CountdownTimer m_zoomTimer;										///< for delaying firing immediately after zoom
	bool DoEquip( CWeaponCSBase *gun );								///< equip the given item

	void ReloadCheck( void );										///< reload our weapon if we must
	void SilencerCheck( void );										///< use silencer

	float m_fireWeaponTimestamp;

	bool m_isEnemySniperVisible;									///< do we see an enemy sniper right now
	CountdownTimer m_sawEnemySniperTimer;							///< tracking time since saw enemy sniper
	
	//- reaction time system -----------------------------------------------------------------------------------------
	enum { MAX_ENEMY_QUEUE = 20 };
	struct ReactionState
	{
		// NOTE: player position & orientation is not currently stored separately
		CHandle<CCSPlayer> player;
		bool isReloading;
		bool isProtectedByShield;
	}
	m_enemyQueue[ MAX_ENEMY_QUEUE ];								///< round-robin queue for simulating reaction times
	byte m_enemyQueueIndex;
	byte m_enemyQueueCount;
	byte m_enemyQueueAttendIndex;									///< index of the timeframe we are "conscious" of

	CCSPlayer *FindMostDangerousThreat( void );						///< return most dangerous threat in my field of view (feeds into reaction time queue)


	//- stuck detection ---------------------------------------------------------------------------------------------
	bool m_isStuck;
	float m_stuckTimestamp;											///< time when we got stuck
	Vector m_stuckSpot;												///< the location where we became stuck
	NavRelativeDirType m_wiggleDirection;
	CountdownTimer m_wiggleTimer;
	CountdownTimer m_stuckJumpTimer;								///< time for next jump when stuck

	enum { MAX_VEL_SAMPLES = 10 };	
	float m_avgVel[ MAX_VEL_SAMPLES ];
	int m_avgVelIndex;
	int m_avgVelCount;
	Vector m_lastOrigin;

	//- radio --------------------------------------------------------------------------------------------------------
	RadioType m_lastRadioCommand;									///< last radio command we recieved
	float m_lastRadioRecievedTimestamp;								///< time we recieved a radio message
	float m_lastRadioSentTimestamp;									///< time when we send a radio message
	CHandle< CCSPlayer > m_radioSubject;							///< who issued the radio message
	Vector m_radioPosition;											///< position referred to in radio message
	void RespondToRadioCommands( void );
	bool IsRadioCommand( RadioType event ) const;					///< returns true if the radio message is an order to do something

	/// new-style "voice" chatter gets voice feedback
	float m_voiceEndTimestamp;

	BotChatterInterface m_chatter;									///< chatter mechanism
};


//
// Inlines
//

inline float CCSBot::GetFeetZ( void ) const
{
	return GetAbsOrigin().z;
}

inline const Vector *CCSBot::GetNoisePosition( void ) const
{
	if (m_noiseTimestamp > 0.0f)
		return &m_noisePosition;

	return NULL;
}

inline bool CCSBot::IsAwareOfEnemyDeath( void ) const
{
	if (GetEnemyDeathTimestamp() == 0.0f)
		return false;

	if (m_enemy == NULL)
		return true;

	if (!m_enemy->IsAlive() && gpGlobals->curtime - GetEnemyDeathTimestamp() > (1.0f - 0.8f * GetProfile()->GetSkill()))
		return true;

	return false;
}

inline void CCSBot::Panic( void )
{
	// we are stunned for a moment
	Surprise( RandomFloat( 0.2f, 0.3f ) );

	const float panicTime = 3.0f;
	m_panicTimer.Start( panicTime );

	const float panicRetreatRange = 300.0f;
	TryToRetreat( panicRetreatRange, 0.0f );

	PrintIfWatched( "*** PANIC ***\n" );
}

inline bool CCSBot::IsPanicking( void ) const
{
	return !m_panicTimer.IsElapsed();
}

inline void CCSBot::StopPanicking( void )
{
	m_panicTimer.Invalidate();
}

inline bool CCSBot::IsNotMoving( float minDuration ) const
{
	return (m_stillTimer.HasStarted() && m_stillTimer.GetElapsedTime() >= minDuration);
}

inline CWeaponCSBase *CCSBot::GetActiveCSWeapon( void ) const
{
	return reinterpret_cast<CWeaponCSBase *>( GetActiveWeapon() );
}


inline float CCSBot::GetCombatRange( void ) const
{ 
	return m_combatRange; 
}

inline void CCSBot::SetRogue( bool rogue )
{ 
	m_isRogue = rogue;
}

inline void CCSBot::Hurry( float duration )
{ 
	m_hurryTimer.Start( duration ); 
}

inline float CCSBot::GetSafeTime( void ) const
{
	return m_safeTime;
}

inline bool CCSBot::IsUnhealthy( void ) const
{
	return (GetHealth() <= 40);
}

inline bool CCSBot::IsAlert( void ) const
{
	return !m_alertTimer.IsElapsed();
}

inline void CCSBot::BecomeAlert( void )
{
	const float alertCooldownTime = 10.0f;
	m_alertTimer.Start( alertCooldownTime );
}

inline bool CCSBot::IsSneaking( void ) const
{
	return !m_sneakTimer.IsElapsed();
}

inline void CCSBot::Sneak( float duration )
{
	m_sneakTimer.Start( duration );
}

inline bool CCSBot::IsFollowing( void ) const
{ 
	return m_isFollowing;
}

inline CCSPlayer *CCSBot::GetFollowLeader( void ) const
{ 
	return m_leader;
}

inline float CCSBot::GetFollowDuration( void ) const
{
	return gpGlobals->curtime - m_followTimestamp;
}

inline bool CCSBot::CanAutoFollow( void ) const
{ 
	return (gpGlobals->curtime > m_allowAutoFollowTime);
}

inline void CCSBot::AimAtEnemy( void )
{ 
	m_isAimingAtEnemy = true;
}

inline void CCSBot::StopAiming( void )
{ 
	m_isAimingAtEnemy = false;
}

inline bool CCSBot::IsAimingAtEnemy( void ) const
{
	return m_isAimingAtEnemy;
}

inline float CCSBot::GetStateTimestamp( void ) const
{
	return m_stateTimestamp;
}

inline CSGameState *CCSBot::GetGameState( void )
{
	return &m_gameState;
}

inline const CSGameState *CCSBot::GetGameState( void ) const
{
	return &m_gameState;
}

inline bool CCSBot::IsAtBombsite( void )
{
	return m_bInBombZone;
}

inline void CCSBot::SetTask( TaskType task, CBaseEntity *entity )
{
	m_task = task;
	m_taskEntity = entity;
}

inline CCSBot::TaskType CCSBot::GetTask( void ) const
{
	return m_task;
}

inline CBaseEntity *CCSBot::GetTaskEntity( void )
{
	return static_cast<CBaseEntity *>( m_taskEntity );
}

inline CCSBot::MoraleType CCSBot::GetMorale( void ) const
{
	return m_morale;
}

inline void CCSBot::Surprise( float duration )
{
	m_surpriseTimer.Start( duration );
}

inline bool CCSBot::IsSurprised( void ) const
{
	return !m_surpriseTimer.IsElapsed();
}

inline CNavArea *CCSBot::GetNoiseArea( void ) const
{
	return m_noiseArea;
}

inline void CCSBot::ForgetNoise( void )
{
	m_noiseTimestamp = 0.0f;
}

inline float CCSBot::GetNoiseRange( void ) const
{
	if (IsNoiseHeard())
		return m_noiseTravelDistance;

	return 999999999.9f;
}

inline PriorityType CCSBot::GetNoisePriority( void ) const
{ 
	return m_noisePriority;
}

inline BotChatterInterface *CCSBot::GetChatter( void )
{
	return &m_chatter;
}

inline CCSPlayer *CCSBot::GetBotEnemy( void ) const
{
	return m_enemy;
}

inline int CCSBot::GetNearbyEnemyCount( void ) const
{ 
	return MIN( GetEnemiesRemaining(), m_nearbyEnemyCount );
}

inline unsigned int CCSBot::GetEnemyPlace( void ) const
{
	return m_enemyPlace;
}

inline bool CCSBot::CanSeeBomber( void ) const
{
	return (m_bomber == NULL) ? false : true;
}

inline CCSPlayer *CCSBot::GetBomber( void ) const
{
	return m_bomber;
}

inline int CCSBot::GetNearbyFriendCount( void ) const
{
	return MIN( GetFriendsRemaining(), m_nearbyFriendCount );
}

inline CCSPlayer *CCSBot::GetClosestVisibleFriend( void ) const
{
	return m_closestVisibleFriend;
}

inline CCSPlayer *CCSBot::GetClosestVisibleHumanFriend( void ) const
{
	return m_closestVisibleHumanFriend;
}

inline float CCSBot::GetTimeSinceAttacked( void ) const
{
	return gpGlobals->curtime - m_attackedTimestamp;
}

inline float CCSBot::GetFirstSawEnemyTimestamp( void ) const
{
	return m_firstSawEnemyTimestamp;
}

inline float CCSBot::GetLastSawEnemyTimestamp( void ) const
{
	return m_lastSawEnemyTimestamp;
}

inline float CCSBot::GetTimeSinceLastSawEnemy( void ) const
{
	return gpGlobals->curtime - m_lastSawEnemyTimestamp;
}

inline float CCSBot::GetTimeSinceAcquiredCurrentEnemy( void ) const
{
	return gpGlobals->curtime - m_currentEnemyAcquireTimestamp;
}

inline const Vector &CCSBot::GetLastKnownEnemyPosition( void ) const
{
	return m_lastEnemyPosition;
}

inline bool CCSBot::IsEnemyVisible( void ) const			
{
	return m_isEnemyVisible;
}

inline float CCSBot::GetEnemyDeathTimestamp( void ) const	
{
	return m_enemyDeathTimestamp;
}

inline int CCSBot::GetLastVictimID( void ) const
{
	return m_lastVictimID;
}

inline bool CCSBot::CanSeeSniper( void ) const
{
	return m_isEnemySniperVisible;
}

inline bool CCSBot::HasSeenSniperRecently( void ) const
{
	return !m_sawEnemySniperTimer.IsElapsed();
}

inline float CCSBot::GetTravelDistanceToPlayer( CCSPlayer *player ) const
{
	if (player == NULL)
		return -1.0f;

	if (!player->IsAlive())
		return -1.0f;

	return m_playerTravelDistance[ player->entindex() % MAX_PLAYERS ];
}

inline bool CCSBot::HasPath( void ) const
{
	return (m_pathLength) ? true : false;
}

inline void CCSBot::DestroyPath( void )		
{
	m_isStopping = false;
	m_pathLength = 0;
	m_pathLadder = NULL;
}

inline CNavArea *CCSBot::GetLastKnownArea( void ) const		
{
	return m_lastKnownArea;
}

inline const Vector &CCSBot::GetPathEndpoint( void ) const		
{
	return m_path[ m_pathLength-1 ].pos;
}

inline const Vector &CCSBot::GetPathPosition( int index ) const
{
	return m_path[ index ].pos;
}

inline bool CCSBot::IsUsingLadder( void ) const	
{
	return (m_pathLadder) ? true : false;
}

inline void CCSBot::SetGoalEntity( CBaseEntity *entity )	
{
	m_goalEntity = entity;
}

inline CBaseEntity *CCSBot::GetGoalEntity( void )
{
	return m_goalEntity;
}

inline void CCSBot::ForceRun( float duration )
{
	Run();
	m_mustRunTimer.Start( duration );
}

inline void CCSBot::Wait( float duration )			
{
	m_waitTimer.Start( duration );
}

inline bool CCSBot::IsWaiting( void ) const		
{
	return !m_waitTimer.IsElapsed();
}

inline void CCSBot::StopWaiting( void )			
{
	m_waitTimer.Invalidate();
}

inline bool CCSBot::HasVisitedEnemySpawn( void ) const		
{
	return m_hasVisitedEnemySpawn;
}

inline const Vector &CCSBot::EyePositionConst( void ) const		
{
	return m_eyePosition;
}
	
inline void CCSBot::SetLookAngles( float yaw, float pitch )
{
	m_lookYaw = yaw;
	m_lookPitch = pitch;
}

inline void CCSBot::SetForwardAngle( float angle ) 
{
	m_forwardAngle = angle;
}

inline void CCSBot::SetLookAheadAngle( float angle ) 
{
	m_lookAheadAngle = angle;
}

inline void CCSBot::ClearLookAt( void )
{ 
	//PrintIfWatched( "ClearLookAt()\n" );
	m_lookAtSpotState = NOT_LOOKING_AT_SPOT; 
	m_lookAtDesc = NULL; 
}

inline bool CCSBot::IsLookingAtSpot( PriorityType pri ) const
{ 
	if (m_lookAtSpotState != NOT_LOOKING_AT_SPOT && m_lookAtSpotPriority >= pri)
		return true;

	return false;
}

inline bool CCSBot::IsViewMoving( float angleVelThreshold ) const
{
	if (m_lookYawVel < angleVelThreshold && m_lookYawVel > -angleVelThreshold &&
		m_lookPitchVel < angleVelThreshold && m_lookPitchVel > -angleVelThreshold)
	{
		return false;
	}

	return true;
}

inline bool CCSBot::HasViewBeenSteady( float duration ) const
{
	return (m_viewSteadyTimer.GetElapsedTime() > duration);
}

inline bool CCSBot::HasLookAtTarget( void ) const
{
	return (m_lookAtSpotState != NOT_LOOKING_AT_SPOT);
}

inline bool CCSBot::IsEnemyPartVisible( VisiblePartType part ) const
{ 
	VPROF_BUDGET( "CCSBot::IsEnemyPartVisible", VPROF_BUDGETGROUP_NPCS );

	if (!IsEnemyVisible())
		return false;

	return (m_visibleEnemyParts & part) ? true : false;
}

inline bool CCSBot::IsSignificantlyCloser( const CCSPlayer *testPlayer, const CCSPlayer *referencePlayer ) const
{
	if ( !referencePlayer )
		return true;

	if ( !testPlayer )
		return false;

	float testDist = ( GetAbsOrigin() - testPlayer->GetAbsOrigin() ).Length();
	float referenceDist = ( GetAbsOrigin() - referencePlayer->GetAbsOrigin() ).Length();

	const float significantRangeFraction = 0.7f;
	if ( testDist < referenceDist * significantRangeFraction )
		return true;

	return false;
}

inline void CCSBot::ClearApproachPoints( void )	
{
	m_approachPointCount = 0;
}

inline const CNavArea *CCSBot::GetInitialEncounterArea( void ) const
{
	return m_initialEncounterArea;
}

inline void CCSBot::SetInitialEncounterArea( const CNavArea *area )		
{
	m_initialEncounterArea = area;
}

inline bool CCSBot::IsThrowingGrenade( void ) const		
{
	return m_grenadeTossState != NOT_THROWING;
}

inline bool CCSBot::IsAvoidingGrenade( void ) const
{
	return !m_isAvoidingGrenade.IsElapsed();
}

inline void CCSBot::PrimaryAttack( void )
{
	if ( IsUsingPistol() && !CanActiveWeaponFire() )
		return;

	BaseClass::PrimaryAttack();
}

inline CCSBot::ZoomType CCSBot::GetZoomLevel( void )
{
	if (GetFOV() > 60.0f)
		return NO_ZOOM;
	if (GetFOV() > 25.0f)
		return LOW_ZOOM;
	return HIGH_ZOOM;
}

inline bool CCSBot::IsWaitingForZoom( void ) const		
{
	return !m_zoomTimer.IsElapsed();
}

inline int CCSBot::GetHostageEscortCount( void ) const		
{
	return m_hostageEscortCount;
}

inline void CCSBot::IncreaseHostageEscortCount( void )		
{
	++m_hostageEscortCount;
}

inline void CCSBot::ResetWaitForHostagePatience( void )
{
	m_isWaitingForHostage = false;
	m_inhibitWaitingForHostageTimer.Invalidate();
}


inline bool CCSBot::IsUsingVoice() const
{
	return m_voiceEndTimestamp > gpGlobals->curtime; 
}

inline bool CCSBot::IsOpeningDoor( void ) const
{
	return m_isOpeningDoor;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if the given weapon is a sniper rifle
 */
inline bool IsSniperRifle( CWeaponCSBase *weapon )
{
	if (weapon == NULL)
		return false;

	return weapon->IsKindOf(WEAPONTYPE_SNIPER_RIFLE);
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Functor used with NavAreaBuildPath()
 */
class PathCost
{
public:
	PathCost( CCSBot *bot, RouteType route = SAFEST_ROUTE )
	{
		m_bot = bot;
		m_route = route;
	}

	// HPE_TODO[pmf]: check that these new parameters are okay to be ignored
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
		float baseDangerFactor = 100.0f;	// 100

		// respond to the danger modulated by our aggression (even super-aggressives pay SOME attention to danger)
		float dangerFactor = (1.0f - (0.95f * m_bot->GetProfile()->GetAggression())) * baseDangerFactor;

		if (fromArea == NULL)
		{
			if (m_route == FASTEST_ROUTE)
				return 0.0f;

			// first area in path, cost is just danger
			return dangerFactor * area->GetDanger( m_bot->GetTeamNumber() );
		}
		else if ((fromArea->GetAttributes() & NAV_MESH_JUMP) && (area->GetAttributes() & NAV_MESH_JUMP))
		{
			// cannot actually walk in jump areas - disallow moving from jump area to jump area
			return -1.0f;
		}
		if ( area->GetAttributes() & NAV_MESH_NO_HOSTAGES && m_bot->GetHostageEscortCount() )
		{
			// if we're leading hostages, don't try to go where they can't
			return -1.0f;
		}
		else
		{
			// compute distance from previous area to this area
			float dist;
			if (ladder)
			{
				// ladders are slow to use
				const float ladderPenalty = 1.0f; // 3.0f;
				dist = ladderPenalty * ladder->m_length;

				// if we are currently escorting hostages, avoid ladders (hostages are confused by them)
				//if (m_bot->GetHostageEscortCount())
				//	dist *= 100.0f;
			}
			else
			{
				dist = (area->GetCenter() - fromArea->GetCenter()).Length();
			}

			// compute distance travelled along path so far
			float cost = dist + fromArea->GetCostSoFar();

			// zombies ignore all path penalties
			if (cv_bot_zombie.GetBool())
				return cost;

			// add cost of "jump down" pain unless we're jumping into water
			if (!area->IsUnderwater() && area->IsConnected( fromArea, NUM_DIRECTIONS ) == false)
			{
				// this is a "jump down" (one way drop) transition - estimate damage we will take to traverse it
				float fallDistance = -fromArea->ComputeGroundHeightChange( area );

				// if it's a drop-down ladder, estimate height from the bottom of the ladder to the lower area
				if ( ladder && ladder->m_bottom.z < fromArea->GetCenter().z && ladder->m_bottom.z > area->GetCenter().z )
				{
					fallDistance = ladder->m_bottom.z - area->GetCenter().z;
				}

				float fallDamage = m_bot->GetApproximateFallDamage( fallDistance );

				if (fallDamage > 0.0f)
				{
					// if the fall would kill us, don't use it
					const float deathFallMargin = 10.0f;
					if (fallDamage + deathFallMargin >= m_bot->GetHealth())
						return -1.0f;

					// if we need to get there in a hurry, ignore minor pain
					const float painTolerance = 15.0f * m_bot->GetProfile()->GetAggression() + 10.0f;
					if (m_route != FASTEST_ROUTE || fallDamage > painTolerance)
					{
						// cost is proportional to how much it hurts when we fall
						// 10 points - not a big deal, 50 points - ouch!
						cost += 100.0f * fallDamage * fallDamage;
					}
				}
			}

			// if this is a "crouch" or "walk" area, add penalty
			if (area->GetAttributes() & (NAV_MESH_CROUCH | NAV_MESH_WALK))
			{
				// these areas are very slow to move through
				float penalty = (m_route == FASTEST_ROUTE) ? 20.0f : 5.0f;

				// avoid crouch areas if we are rescuing hostages 
				if ((area->GetAttributes() & NAV_MESH_CROUCH) && m_bot->GetHostageEscortCount())
				{
					penalty *= 3.0f;
				}

				cost += penalty * dist;
			}

			// if this is a "jump" area, add penalty
			if (area->GetAttributes() & NAV_MESH_JUMP)
			{
				// jumping can slow you down
				//const float jumpPenalty = (m_route == FASTEST_ROUTE) ? 100.0f : 0.5f;
				const float jumpPenalty = 1.0f;
				cost += jumpPenalty * dist;
			}

			// if this is an area to avoid, add penalty
			if (area->GetAttributes() & NAV_MESH_AVOID)
			{
				const float avoidPenalty = 20.0f;
				cost += avoidPenalty * dist;
			}

			if (m_route == SAFEST_ROUTE)
			{
				// add in the danger of this path - danger is per unit length travelled
				cost += dist * dangerFactor * area->GetDanger( m_bot->GetTeamNumber() );
			}

			if (!m_bot->IsAttacking())
			{
				// add in cost of teammates in the way

				// approximate density of teammates based on area
				float size = (area->GetSizeX() + area->GetSizeY())/2.0f;

				// degenerate check
				if (size >= 1.0f)
				{
					// cost is proportional to the density of teammates in this area
					const float costPerFriendPerUnit = 50000.0f;
					cost += costPerFriendPerUnit * (float)area->GetPlayerCount( m_bot->GetTeamNumber() ) / size;
				}
			}

			return cost;
		}
	}

private:
	CCSBot *m_bot;
	RouteType m_route;
};


//--------------------------------------------------------------------------------------------------------------
//
// Prototypes
//
extern int GetBotFollowCount( CCSPlayer *leader );
extern const Vector *FindNearbyRetreatSpot( CCSBot *me, float maxRange = 250.0f );
extern const HidingSpot *FindInitialEncounterSpot( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float maxRange, bool isSniper );


#endif	// _CS_BOT_H_

