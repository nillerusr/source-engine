//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// filter.h
#if !defined( FILTER_H )
#define FILTER_H
#ifdef _WIN32
#pragma once
#endif

#include "userid.h"

typedef struct
{
	unsigned	mask;
	unsigned	compare;
	float       banEndTime; // 0 for permanent ban
	float       banTime;
} ipfilter_t;

typedef struct
{
	USERID_t userid;
	float	banEndTime;
	float	banTime;
} userfilter_t;

#define	MAX_IPFILTERS	    32768
#define	MAX_USERFILTERS	    32768

#endif // FILTER_H
