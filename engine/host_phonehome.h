//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HOST_PHONEHOME_H
#define HOST_PHONEHOME_H
#ifdef _WIN32
#pragma once
#endif

abstract_class IPhoneHome
{
public:
	enum
	{
		PHONE_MSG_UNKNOWN	= 0,
		PHONE_MSG_ENGINESTART,
		PHONE_MSG_ENGINEEND,
		PHONE_MSG_MAPSTART,
		PHONE_MSG_MAPEND
	};

	virtual void Init( void ) = 0;
	virtual void Shutdown() = 0;
	virtual void Message( byte msgtype, char const *mapname ) = 0;
	virtual bool IsExternalBuild() = 0;
};

extern IPhoneHome *phonehome;

#endif // HOST_PHONEHOME_H
