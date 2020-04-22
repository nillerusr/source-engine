//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#ifndef BASE_CONTROL_H
#define BASE_CONTROL_H

#pragma warning( disable : 4530 )					// STL uses exceptions, but we are not compiling with them - ignore warning

extern float g_BotUpkeepInterval;					///< duration between bot upkeeps
extern float g_BotUpdateInterval;					///< duration between bot updates
const int g_BotUpdateSkipCount = 2;					///< number of upkeep periods to skip update

class CNavArea;

/// TODO: move CS-specific defines into CSBot files
enum
{
	SmokeGrenadeRadius = 155,
	FlashbangGrenadeRadius = 115,
	HEGrenadeRadius = 115,
};

//--------------------------------------------------------------------------------------------------------------
class CBaseGrenade;

/**
 * An ActiveGrenade is a representation of a grenade in the world
 * NOTE: Currently only used for smoke grenade line-of-sight testing
 * @todo Use system allow bots to avoid HE and Flashbangs
 */
class ActiveGrenade
{
public:
	ActiveGrenade( CBaseGrenade *grenadeEntity );

	void OnEntityGone( void );								///< called when the grenade in the world goes away
	void Update( void );									///< called every frame
	bool IsValid( void ) const	;							///< return true if this grenade is valid
	
	bool IsEntity( CBaseGrenade *grenade ) const		{ return (grenade == m_entity) ? true : false; }
	CBaseGrenade *GetEntity( void ) const				{ return m_entity; }

	const Vector &GetDetonationPosition( void ) const	{ return m_detonationPosition; }
	const Vector &GetPosition( void ) const;
	bool IsSmoke( void ) const							{ return m_isSmoke; }
	bool IsFlashbang( void ) const						{ return m_isFlashbang; }
	CBaseGrenade *GetGrenade( void ) { return m_entity; }
	float GetRadius( void ) const { return m_radius; }
	void SetRadius( float radius ) { m_radius = radius; }

private:
	CBaseGrenade *m_entity;									///< the entity
	Vector m_detonationPosition;							///< the location where the grenade detonated (smoke)
	float m_dieTimestamp;									///< time this should go away after m_entity is NULL
	bool m_isSmoke;											///< true if this is a smoke grenade
	bool m_isFlashbang;										///< true if this is a flashbang grenade
	float m_radius;
};

typedef CUtlLinkedList<ActiveGrenade *> ActiveGrenadeList;


//--------------------------------------------------------------------------------------------------------------
/**
 * This class manages all active bots, propagating events to them and updating them.
 */
class CBotManager
{
public:
	CBotManager();
	virtual ~CBotManager();

	CBasePlayer *AllocateAndBindBotEntity( edict_t *ed );			///< allocate the appropriate entity for the bot and bind it to the given edict
	virtual CBasePlayer *AllocateBotEntity( void ) = 0;				///< factory method to allocate the appropriate entity for the bot 

	virtual void ClientDisconnect( CBaseEntity *entity ) = 0;
	virtual bool ClientCommand( CBasePlayer *player, const CCommand &args ) = 0;

	virtual void ServerActivate( void ) = 0;
	virtual void ServerDeactivate( void ) = 0;
	virtual bool ServerCommand( const char * pcmd ) = 0;

	virtual void RestartRound( void );							///< (EXTEND) invoked when a new round begins
	virtual void StartFrame( void );							///< (EXTEND) called each frame

	virtual unsigned int GetPlayerPriority( CBasePlayer *player ) const = 0;	///< return priority of player (0 = max pri)
	

	void AddGrenade( CBaseGrenade *grenade );					///< add an active grenade to the bot's awareness
	void RemoveGrenade( CBaseGrenade *grenade );				///< the grenade entity in the world is going away
	void SetGrenadeRadius( CBaseGrenade *grenade, float radius );	///< the radius of the grenade entity (or associated smoke cloud)
	void ValidateActiveGrenades( void );						///< destroy any invalid active grenades
	void DestroyAllGrenades( void );
	bool IsLineBlockedBySmoke( const Vector &from, const Vector &to, float grenadeBloat = 1.0f );	///< return true if line intersects smoke volume, with grenade radius increased by the grenadeBloat factor
	bool IsInsideSmokeCloud( const Vector *pos );				///< return true if position is inside a smoke cloud

	//
	// Invoke functor on all active grenades.
	// If any functor call return false, return false.  Otherwise, return true.
	//
	template < typename T >
	bool ForEachGrenade( T &func )
	{
		int it = m_activeGrenadeList.Head();

		while( it != m_activeGrenadeList.InvalidIndex() )
		{
			ActiveGrenade *ag = m_activeGrenadeList[ it ];

			int current = it;
			it = m_activeGrenadeList.Next( it );

			// lazy validation
			if (!ag->IsValid())
			{
				m_activeGrenadeList.Remove( current );
				delete ag;
				continue;
			}
			else
			{
				if (func( ag ) == false)
				{
					return false;
				}
			}
		}

		return true;
	}

	enum { MAX_DBG_MSG_SIZE = 1024 };
	struct DebugMessage
	{
		char m_string[ MAX_DBG_MSG_SIZE ];
		IntervalTimer m_age;
	};

	// debug message history -------------------------------------------------------------------------------
	int GetDebugMessageCount( void ) const;						///< get number of debug messages in history
	const DebugMessage *GetDebugMessage( int which = 0 ) const;	///< return the debug message emitted by the bot (0 = most recent)
	void ClearDebugMessages( void );
	void AddDebugMessage( const char *msg );


private:
	ActiveGrenadeList m_activeGrenadeList;///< the list of active grenades the bots are aware of

	enum { MAX_DBG_MSGS = 6 };
	DebugMessage m_debugMessage[ MAX_DBG_MSGS ];				///< debug message history
	int m_debugMessageCount;
	int m_currentDebugMessage;

	IntervalTimer m_frameTimer;									///< for measuring each frame's duration
};


inline CBasePlayer *CBotManager::AllocateAndBindBotEntity( edict_t *ed )
{
	CBasePlayer::s_PlayerEdict = ed;
	return AllocateBotEntity();
}

inline int CBotManager::GetDebugMessageCount( void ) const
{
	return m_debugMessageCount;
}

inline const CBotManager::DebugMessage *CBotManager::GetDebugMessage( int which ) const
{
	if (which >= m_debugMessageCount)
		return NULL;

	int i = m_currentDebugMessage - which;
	if (i < 0)
		i += MAX_DBG_MSGS;

	return &m_debugMessage[ i ];
}





// global singleton to create and control bots
extern CBotManager *TheBots;


#endif
