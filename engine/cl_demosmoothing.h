//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOSMOOTHING_H
#define CL_DEMOSMOOTHING_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "quakedef.h"
#include "tier0/platform.h"

#define FDEMO_NORMAL		0
#define FDEMO_USE_ORIGIN2	(1<<0)
#define FDEMO_USE_ANGLES2	(1<<1)
#define FDEMO_NOINTERP		(1<<2)	// don't interpolate between this an last view

struct democmdinfo_t
{
	// Default constructor
	democmdinfo_t()
	{
		flags = FDEMO_NORMAL;
		viewOrigin.Init();
		viewAngles.Init();
		localViewAngles.Init();

		// Resampled origin/angles
		viewOrigin2.Init();
		viewAngles2.Init();
		localViewAngles2.Init();
	}

	// Copy constructor
	// Assignment
	democmdinfo_t&	operator=(const democmdinfo_t& src )
	{
		if ( this == &src )
			return *this;

		flags = src.flags;
		viewOrigin = src.viewOrigin;
		viewAngles = src.viewAngles;
		localViewAngles = src.localViewAngles;
		viewOrigin2 = src.viewOrigin2;
		viewAngles2 = src.viewAngles2;
		localViewAngles2 = src.localViewAngles2;

		return *this;
	}

	const Vector& GetViewOrigin()
	{
		if ( flags & FDEMO_USE_ORIGIN2 )
		{
			return viewOrigin2;
		}
		return viewOrigin;
	}

	const QAngle& GetViewAngles()
	{
		if ( flags & FDEMO_USE_ANGLES2 )
		{
			return viewAngles2;
		}
		return viewAngles;
	}
	const QAngle& GetLocalViewAngles()
	{
		if ( flags & FDEMO_USE_ANGLES2 )
		{
			return localViewAngles2;
		}
		return localViewAngles;
	}

	void Reset( void )
	{
		flags = 0;
		viewOrigin2 = viewOrigin;
		viewAngles2 = viewAngles;
		localViewAngles2 = localViewAngles;
	}

	int			flags;

	// original origin/viewangles
	Vector		viewOrigin;
	QAngle		viewAngles;
	QAngle		localViewAngles;

	// Resampled origin/viewangles
	Vector		viewOrigin2;
	QAngle		viewAngles2;
	QAngle		localViewAngles2;
};

struct demosmoothing_t
{
	demosmoothing_t()
	{
		file_offset = 0;
		frametick = 0;
		selected = false;
		samplepoint = false;

		vecmoved.Init();
		angmoved.Init();

		targetpoint = false;
		vectarget.Init();
	}

	demosmoothing_t&	operator=(const demosmoothing_t& src )
	{
		if ( this == &src )
			return *this;

		file_offset = src.file_offset;
		frametick = src.frametick;
		selected = src.selected;
		samplepoint = src.samplepoint;
		vecmoved = src.vecmoved;
		angmoved = src.angmoved;

		targetpoint = src.targetpoint;
		vectarget = src.vectarget;

		info = src.info;

		return *this;
	}

	int					file_offset;

	int					frametick;

	bool				selected;

	// For moved sample points
	bool				samplepoint;
	Vector				vecmoved;
	QAngle				angmoved;

	bool				targetpoint;
	Vector				vectarget;

	democmdinfo_t		info;
};

struct CSmoothingContext
{
	CSmoothingContext()
	{
		active = false;
		filename[ 0 ] = 0;
	}

	CSmoothingContext&	operator=(const CSmoothingContext& src )
	{
		if ( this == &src )
			return *this;

		active = src.active;
		Q_strncpy( filename, src.filename, sizeof( filename ) );

		smooth.RemoveAll();
		int c = src.smooth.Count();
		int i;
		for ( i = 0; i < c; i++ )
		{
			demosmoothing_t newitem;
			newitem = src.smooth[ i ];
			smooth.AddToTail( newitem );
		}

		return *this;
	}

	bool							active;
	char							filename[ MAX_OSPATH ];
	CUtlVector< demosmoothing_t >	smooth;
};

#endif // CL_DEMOSMOOTHING_H
