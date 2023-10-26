//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NPCEVENT_H
#define NPCEVENT_H
#ifdef _WIN32
#pragma once
#endif


#include "eventlist.h"


class CBaseAnimating;

struct animevent_t
{
#ifdef CLIENT_DLL
	// see mstudioevent_for_client_server_t comment below
	union
	{
		unsigned short	_event_highword;
		unsigned short	event_newsystem;
	};
	unsigned short	_event_lowword;
#else
	// see mstudioevent_for_client_server_t comment below
	unsigned short	_event_highword;
	union
	{
		unsigned short	_event_lowword;
		unsigned short	event_newsystem;
	};
#endif

	const char		*options;
	float			cycle;
	float			eventtime;
	int				type;
	CBaseAnimating	*pSource;
	bool			m_bHandledByScript;

	// see mstudioevent_for_client_server_t comment below
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};
#define EVENT_SPECIFIC			0
#define EVENT_SCRIPTED			1000		// see scriptevent.h
#define EVENT_SHARED			2000
#define EVENT_WEAPON			3000
#define EVENT_CLIENT			5000

#define NPC_EVENT_BODYDROP_LIGHT	2001
#define NPC_EVENT_BODYDROP_HEAVY	2002

#define NPC_EVENT_SWISHSOUND		2010

#define NPC_EVENT_180TURN			2020

#define NPC_EVENT_ITEM_PICKUP					2040
#define NPC_EVENT_WEAPON_DROP					2041
#define NPC_EVENT_WEAPON_SET_SEQUENCE_NAME		2042
#define NPC_EVENT_WEAPON_SET_SEQUENCE_NUMBER	2043
#define NPC_EVENT_WEAPON_SET_ACTIVITY			2044

#define	NPC_EVENT_LEFTFOOT			2050
#define NPC_EVENT_RIGHTFOOT			2051

#define NPC_EVENT_OPEN_DOOR			2060

// !! DON'T CHANGE TO ORDER OF THESE.  THEY ARE HARD CODED IN THE WEAPON QC FILES (YUCK!) !!
#define EVENT_WEAPON_MELEE_HIT			3001
#define EVENT_WEAPON_SMG1				3002
#define EVENT_WEAPON_MELEE_SWISH		3003
#define EVENT_WEAPON_SHOTGUN_FIRE		3004
#define EVENT_WEAPON_THROW				3005
#define EVENT_WEAPON_AR1				3006
#define EVENT_WEAPON_AR2				3007
#define EVENT_WEAPON_HMG1				3008
#define EVENT_WEAPON_SMG2				3009
#define EVENT_WEAPON_MISSILE_FIRE		3010
#define EVENT_WEAPON_SNIPER_RIFLE_FIRE	3011
#define EVENT_WEAPON_AR2_GRENADE		3012
#define EVENT_WEAPON_THROW2				3013
#define	EVENT_WEAPON_PISTOL_FIRE		3014
#define EVENT_WEAPON_RELOAD				3015
#define EVENT_WEAPON_THROW3				3016
#define EVENT_WEAPON_RELOAD_SOUND		3017		// Use this + EVENT_WEAPON_RELOAD_FILL_CLIP to prevent shooting during the reload animation 
#define EVENT_WEAPON_RELOAD_FILL_CLIP	3018
#define EVENT_WEAPON_SMG1_BURST1		3101		// first round in a 3-round burst
#define EVENT_WEAPON_SMG1_BURSTN		3102		// 2, 3 rounds
#define EVENT_WEAPON_AR2_ALTFIRE		3103

#define EVENT_WEAPON_SEQUENCE_FINISHED	3900

// NOTE: MUST BE THE LAST WEAPON EVENT -- ONLY WEAPON EVENTS BETWEEN EVENT_WEAPON AND THIS
#define EVENT_WEAPON_LAST				3999


// Because the client and server have a shared memory space for event data,
// but use different indexes for the event (servers cache them all upfront, 
// while client creates them as sequences fire) we're going to store the server
// index in the low word and the client index in the high word.
//
// studio.h is a public header, so we'll use this matching struct to do the
// conversion and not force us to macro all the code that checks the event member.
// This struct's data MUST match mstudioevent_t
//
// BUT events that use the old event system use the entire dword which will always
// match between client and server. Instead of wrapping everything in if checks, 
// for ( type & AE_TYPE_NEWEVENTSYSTEM ) we use the Event accessors to set/get 
// while doing the check under the hood.
//
// -Jeep
struct mstudioevent_for_client_server_t
{
	DECLARE_BYTESWAP_DATADESC();
	float				cycle;
#ifdef CLIENT_DLL
	union
	{
		// Client shares with the high word
		unsigned short	_event_highword;
		unsigned short	event_newsystem;
	};
	unsigned short	_event_lowword;		// Placeholder for the low word
#else
	unsigned short	_event_highword;	// Placeholder for the high word
	union
	{
		// Server shares with the low word
		unsigned short	_event_lowword;
		unsigned short	event_newsystem;
	};
#endif

	int					type;
	inline const char * pszOptions( void ) const { return options; }
	char				options[64];

	int					szeventindex;
	inline char * const pszEventName( void ) const { return ((char *)this) + szeventindex; }

	// For setting and getting through Event, check the AE_TYPE_NEWEVENTSYSTEM flag to decide
	// if we should set/get just the short used by the client/server or use the whole int.
	int Event_OldSystem( void ) const { return *static_cast< const int* >( static_cast< const void* >( &_event_highword ) ); }
	void Event_OldSystem( int nEvent ) { *static_cast< int* >( static_cast< void* >( &_event_highword ) ) = nEvent; }
	int Event( void ) const
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
			return event_newsystem;

		return Event_OldSystem();
	}
	void Event( int nEvent )
	{
		if ( type & AE_TYPE_NEWEVENTSYSTEM )
		{
			event_newsystem = nEvent;
		}
		else
		{
			Event_OldSystem( nEvent );
		}
	}
};

#define mstudioevent_t mstudioevent_for_client_server_t


#endif // NPCEVENT_H
