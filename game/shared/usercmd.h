//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined( USERCMD_H )
#define USERCMD_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "utlvector.h"
#include "imovehelper.h"
#include "checksum_crc.h"


class bf_read;
class bf_write;

class CEntityGroundContact
{
public:
	int					entindex;
	float				minheight;
	float				maxheight;
};

class CUserCmd
{
public:
	CUserCmd()
	{
		Reset();
	}

	virtual ~CUserCmd() { };

	void Reset()
	{
		command_number = 0;
		tick_count = 0;
		viewangles.Init();
		forwardmove = 0.0f;
		sidemove = 0.0f;
		upmove = 0.0f;
		buttons = 0;
		impulse = 0;
		weaponselect = 0;
		weaponsubtype = 0;
		random_seed = 0;
		mousedx = 0;
		mousedy = 0;

		hasbeenpredicted = false;
#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
		entitygroundcontact.RemoveAll();
#endif



		// TrackIR
		headangles.Init();
		headoffset.Init();
		// TrackIR

#if defined( INFESTED_DLL )
		crosshairtrace = vec3_origin;
#endif

#ifdef INFESTED_DLL
		crosshair_entity = 0;
		forced_action = 0;
		sync_kill_ent = 0;
		skill_dest.Init();
		skill_dest_ent = 0;
#endif

	}

	CUserCmd& operator =( const CUserCmd& src )
	{
		if ( this == &src )
			return *this;

		command_number		= src.command_number;
		tick_count			= src.tick_count;
		viewangles			= src.viewangles;
		forwardmove			= src.forwardmove;
		sidemove			= src.sidemove;
		upmove				= src.upmove;
		buttons				= src.buttons;
		impulse				= src.impulse;
		weaponselect		= src.weaponselect;
		weaponsubtype		= src.weaponsubtype;
		random_seed			= src.random_seed;
		mousedx				= src.mousedx;
		mousedy				= src.mousedy;

		hasbeenpredicted	= src.hasbeenpredicted;

#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
		entitygroundcontact			= src.entitygroundcontact;
#endif



		// TrackIR
		headangles			= src.headangles;
		headoffset			= src.headoffset;
		// TrackIR

#if defined( INFESTED_DLL )
		crosshairtrace		= src.crosshairtrace;
#endif

#ifdef INFESTED_DLL
		crosshair_entity			= src.crosshair_entity;
		forced_action				= src.forced_action;
		sync_kill_ent				= src.sync_kill_ent;
		skill_dest					= src.skill_dest;
		skill_dest_ent				= src.skill_dest_ent;
#endif

		return *this;
	}

	CUserCmd( const CUserCmd& src )
	{
		*this = src;
	}

	CRC32_t GetChecksum( void ) const
	{
		CRC32_t crc;

		CRC32_Init( &crc );
		CRC32_ProcessBuffer( &crc, &command_number, sizeof( command_number ) );
		CRC32_ProcessBuffer( &crc, &tick_count, sizeof( tick_count ) );
		CRC32_ProcessBuffer( &crc, &viewangles, sizeof( viewangles ) );    
		CRC32_ProcessBuffer( &crc, &forwardmove, sizeof( forwardmove ) );   
		CRC32_ProcessBuffer( &crc, &sidemove, sizeof( sidemove ) );      
		CRC32_ProcessBuffer( &crc, &upmove, sizeof( upmove ) );         
		CRC32_ProcessBuffer( &crc, &buttons, sizeof( buttons ) );		
		CRC32_ProcessBuffer( &crc, &impulse, sizeof( impulse ) );        
		CRC32_ProcessBuffer( &crc, &weaponselect, sizeof( weaponselect ) );	
		CRC32_ProcessBuffer( &crc, &weaponsubtype, sizeof( weaponsubtype ) );
		CRC32_ProcessBuffer( &crc, &random_seed, sizeof( random_seed ) );
#ifndef INFESTED_DLL		// alien swarm doesn't need these
		CRC32_ProcessBuffer( &crc, &mousedx, sizeof( mousedx ) );
		CRC32_ProcessBuffer( &crc, &mousedy, sizeof( mousedy ) );
#endif

#if defined( INFESTED_DLL )
		CRC32_ProcessBuffer( &crc, &crosshairtrace, sizeof( crosshairtrace ) );
#endif



#ifdef INFESTED_DLL
		CRC32_ProcessBuffer( &crc, &crosshair_entity, sizeof( crosshair_entity ) );
		CRC32_ProcessBuffer( &crc, &forced_action, sizeof( forced_action ) );
		CRC32_ProcessBuffer( &crc, &sync_kill_ent, sizeof( sync_kill_ent ) );
		CRC32_ProcessBuffer( &crc, &skill_dest, sizeof( skill_dest ) );
		CRC32_ProcessBuffer( &crc, &skill_dest_ent, sizeof( skill_dest_ent ) );
#endif
		CRC32_Final( &crc );

		return crc;
	}

	// For matching server and client commands for debugging
	int		command_number;
	
	// the tick the client created this command
	int		tick_count;
	
	// Player instantaneous view angles.
	QAngle	viewangles;     
	// Intended velocities
	//	forward velocity.
	float	forwardmove;   
	//  sideways velocity.
	float	sidemove;      
	//  upward velocity.
	float	upmove;         
	// Attack button states
	int		buttons;		
	// Impulse command issued.
	byte    impulse;        
	// Current weapon id
	int		weaponselect;	
	int		weaponsubtype;

	int		random_seed;	// For shared random functions

	short	mousedx;		// mouse accum in x from create move
	short	mousedy;		// mouse accum in y from create move

	// Client only, tracks whether we've predicted this command at least once
	bool	hasbeenpredicted;

	// Back channel to communicate IK state
#if defined( HL2_DLL ) || defined( HL2_CLIENT_DLL )
	CUtlVector< CEntityGroundContact > entitygroundcontact;
#endif


	// TrackIR
	QAngle headangles;
	Vector headoffset;
	// TrackIR

#if defined( INFESTED_DLL )
	Vector crosshairtrace;		// world location directly beneath the player's crosshair
#endif

#ifdef INFESTED_DLL
	short crosshair_entity;			// index of the entity under the player's crosshair
	byte forced_action;
	short sync_kill_ent;
	Vector skill_dest;
	short skill_dest_ent;
#endif
};

void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from );
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from );

#endif // USERCMD_H
