//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox VXConsole Common. Used for public remote access items.
//
//=============================================================================
#pragma once

// sent during connection, used to explicitly guarantee a binary compatibility
#define VXCONSOLE_PROTOCOL_VERSION	103

typedef struct
{
	char		labelString[128];
	COLORREF	color;
} xrProfile_t;

typedef struct
{
	char		messageString[256];
	float		time;
	float		deltaTime;
	int			memory;
	int			deltaMemory;
} xrTimeStamp_t;

typedef struct
{
	char		nameString[256];
	char		shaderString[256];
	int			refCount;
} xrMaterial_t;

typedef struct
{
	char		nameString[256];
	char		groupString[64];
	char		formatString[64];
	int			size;
	int			width;
	int			height;
	int			depth;
	int			numLevels;
	int			binds;
	int			refCount;
	int			sRGB;
	int			edram;
	int			procedural;
	int			fallback;
	int			final;
	int			failed;
} xrTexture_t;

typedef struct
{
	char		nameString[256];
	char		formatString[64];
	int			rate;
	int			bits;
	int			channels;
	int			looped;
	int			dataSize;
	int			numSamples;
	int			streamed;
} xrSound_t;

typedef struct
{
	char		nameString[128];
	char		helpString[256];	
} xrCommand_t;

typedef struct
{
	float	position[3];
	float	angle[3];
	char	mapPath[256];
	char	savePath[256];
	int		build;
	int		skill;
} xrMapInfo_t;

// Types of action taken in response to an rc_Assert() message
enum AssertAction_t
{
	ASSERT_ACTION_BREAK = 0,		//	Break on this Assert
	ASSERT_ACTION_IGNORE_THIS,		//	Ignore this Assert once
	ASSERT_ACTION_IGNORE_ALWAYS,	//	Ignore this Assert from now on
	ASSERT_ACTION_IGNORE_FILE,		//	Ignore all Asserts from this file from now on
	ASSERT_ACTION_IGNORE_ALL,		//	Ignore all Asserts from now on
	ASSERT_ACTION_OTHER				//	A more complex response requiring additional data (e.g. "ignore this Assert 5 times")
};
