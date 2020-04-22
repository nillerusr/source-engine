//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//=============================================================================//

#include "client.h"
#include "convar.h"
#include "convar_serverbounded.h"
#include "sys.h"
#include "net.h"


// These are the server cvars that control our cvars.
extern int ClampClientRate( int nRate );

extern ConVar  sv_mincmdrate;
extern ConVar  sv_maxcmdrate;
extern ConVar  sv_minupdaterate;
extern ConVar  sv_maxupdaterate;
extern ConVar  sv_client_cmdrate_difference;

extern ConVar  sv_client_interp;
extern ConVar  sv_client_predict;


// ------------------------------------------------------------------------------------------ //
// rate
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_Rate : public ConVar_ServerBounded
{
public:
	CBoundedCvar_Rate() :
	  ConVar_ServerBounded( 
		  "rate",
#if defined( _X360 )
		  "6000",
#else
		  V_STRINGIFY(DEFAULT_RATE),
#endif
		  FCVAR_ARCHIVE | FCVAR_USERINFO, 
		  "Max bytes/sec the host can receive data" )
	  {
	  }

	virtual float GetFloat() const
	{
		if ( cl.m_nSignonState >= SIGNONSTATE_FULL )
		{
			int nRate = (int)GetBaseFloatValue();
			return (float)ClampClientRate( nRate );
		}
		else
		{
			return GetBaseFloatValue();
		}
	}
};

static CBoundedCvar_Rate cl_rate_var;
ConVar_ServerBounded *cl_rate = &cl_rate_var;


// ------------------------------------------------------------------------------------------ //
// cl_cmdrate
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_CmdRate : public ConVar_ServerBounded
{
public:
	CBoundedCvar_CmdRate() :
	  ConVar_ServerBounded( 
		  "cl_cmdrate", 
		  "30", 
		  FCVAR_ARCHIVE | FCVAR_USERINFO, 
		  "Max number of command packets sent to server per second", true, MIN_CMD_RATE, true, MAX_CMD_RATE )
	{
	}

	virtual float GetFloat() const
	{
		float flCmdRate = GetBaseFloatValue();

		if ( sv_mincmdrate.GetInt() != 0 && cl.m_nSignonState >= SIGNONSTATE_FULL )
		{
			// First, we make it stay within range of cl_updaterate.
			float diff = flCmdRate - cl_updaterate->GetFloat();
			if ( fabs( diff ) > sv_client_cmdrate_difference.GetFloat() )
			{
				if ( diff > 0 )
					flCmdRate = cl_updaterate->GetFloat() + sv_client_cmdrate_difference.GetFloat();
				else
					flCmdRate = cl_updaterate->GetFloat() - sv_client_cmdrate_difference.GetFloat();
			}

			// Then we clamp to the min/max values the server has set.
			return clamp( flCmdRate, sv_mincmdrate.GetFloat(), sv_maxcmdrate.GetFloat() );
		}
		else
		{
			return flCmdRate;
		}
	}
};

static CBoundedCvar_CmdRate cl_cmdrate_var;
ConVar_ServerBounded *cl_cmdrate = &cl_cmdrate_var;



// ------------------------------------------------------------------------------------------ //
// cl_updaterate
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_UpdateRate : public ConVar_ServerBounded
{
public:
	CBoundedCvar_UpdateRate() :
	  ConVar_ServerBounded( 
		  "cl_updaterate",
		  "20", 
		  FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
		  "Number of packets per second of updates you are requesting from the server" )
	{
	}

	virtual float GetFloat() const
	{
		// Clamp to the min/max values the server has set.
		//
		// This cvar only takes effect on the server anyway, and this is done there too,
		// but we have this here so they'll get the **note thing telling them the value 
		// isn't functioning the way they set it.		
		return clamp( GetBaseFloatValue(), sv_minupdaterate.GetFloat(), sv_maxupdaterate.GetFloat() );
	}
};

static CBoundedCvar_UpdateRate cl_updaterate_var;
ConVar_ServerBounded *cl_updaterate = &cl_updaterate_var;
