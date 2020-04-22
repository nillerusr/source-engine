//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


#ifndef ITEMTESTAPP_H
#define ITEMTESTAPP_H

#if COMPILER_MSVC
#pragma once
#endif


// Valve includes
#include "appframework/tier3app.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CAsset;


//=============================================================================
//
//=============================================================================
class CItemTestApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool PreInit();
	virtual void Destroy() {}
	virtual void PostShutdown();

	static void PrintHelp();

protected:
	enum Flags_t	// Should match up to s_pszFlags
	{
		kHelp,
		kSteamId,
		kBatch,
		kClass,
		kName,
		kDev,
		kNoP4,
		kAutoSkin,
		kLod,
		kListMats,
		kMat,
		kMatType,
		kTex,
		kTexType,
		kAlphaType,
		kView,
		kExplore,
		kOutput,
		kFlagsCount	// For compile time assert
	};

	static void DoDevMode();
	static bool ProcessCommandLine( CAsset *pAsset, bool bDoListMats );
	static int FindParam( Flags_t nFlag );
	static const char *ParmValue( Flags_t nFlag );
	static const char *GetParm( int nIndex );

private:
	enum FlagData_t
	{
		kShortFlag,
		kLongFlag,
		kArgDesc,
		kFlagDesc
	};

	static char s_szName[];
	static char s_szDesc[];
	static char s_szExample[];
	static char *s_pszFlags[][4];

};


#endif // ITEMTESTAPP_H
