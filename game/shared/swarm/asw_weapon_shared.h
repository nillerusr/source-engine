//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ASW_WEAPON_SHARED_H
#define ASW_WEAPON_SHARED_H

#ifdef _WIN32
#pragma once
#endif


// Shared header file for weapons
#if defined( CLIENT_DLL )
	#define CASW_Weapon C_ASW_Weapon
#endif


// Undefine this to remove verbose messages/if statements from public builds
#define ASW_VERBOSE_MESSAGES 1

DECLARE_LOGGING_CHANNEL( LOG_ASW_Weapons );
DECLARE_LOGGING_CHANNEL( LOG_ASW_Damage );

#define ASW_MSG(channel, string, ...)	\
	if( (IsServerDll()) )				\
	{									\
		Log_Msg( ##channel, "[S] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}									\
	else								\
	{									\
		Log_Msg( ##channel, "[C] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}			

#define ASW_MSG_SIMPLE(channel, string, ...) Log_Msg( ##channel, ##string, ##__VA_ARGS__ );

#define ASW_WAR(channel, string, ...)	\
	if( (IsServerDll()) )				\
	{									\
		Log_Warning( ##channel, "[S] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}									\
	else								\
	{									\
		Log_Warning( ##channel, "[C] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}			

#define ASW_ERR(channel, string, ...)	\
	if( (IsServerDll()) )				\
	{									\
		Log_Error( ##channel, "[S] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}									\
	else								\
	{									\
		Log_Error( ##channel, "[C] " __FUNCTION__ "(): " string, ##__VA_ARGS__ );	\
	}		

#define ASW_WPN_MSG_CONVAR(convar, string, ...) \
	if( (##convar##).GetBool() )				\
	{											\
		ASW_MSG( LOG_ASW_Weapons, ##string, ##__VA_ARGS__ )	\
	}											

#define ASW_DMG_MSG_CONVAR(convar, string, ...) \
	if( (##convar##).GetBool() )				\
	{											\
		ASW_MSG( LOG_ASW_Damage, ##string, ##__VA_ARGS__ )	\
	}									

#define ASW_WPN_MSG(string, ...) ASW_MSG( LOG_ASW_Weapons, ##string, ##__VA_ARGS__ )
#define ASW_WPN_WAR(string, ...) ASW_WAR( LOG_ASW_Weapons, ##string, ##__VA_ARGS__ )

#ifdef ASW_VERBOSE_MESSAGES

DECLARE_LOGGING_CHANNEL( LOG_ASW_WeaponDeveloper );
#define ASW_WPN_DEV_MSG(string, ...) ASW_MSG( LOG_ASW_WeaponDeveloper, ##string, ##__VA_ARGS__ )

#else	// #ifdef ASW_VERBOSE_MESSAGES
#define ASW_WPN_DEV_MSG(string, ...)
#define ASW_WPN_MSG_CONVAR(convar, string, ...)
#define ASW_DMG_MSG_CONVAR(convar, string, ...)
#endif	// #ifdef ASW_VERBOSE_MESSAGES

#endif // ASW_WEAPON_SHARED_H
