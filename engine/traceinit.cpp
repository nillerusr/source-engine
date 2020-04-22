//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "basetypes.h"
#include "traceinit.h"
#include "utlvector.h"
#include "sys.h"
#include "common.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CInitTracker
{
public:
	enum
	{
		NUM_LISTS = 4,
	};

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	class InitFunc
	{
	public:
		const char	*initname;
		const char	*shutdownname;
		int			referencecount;
		int			sequence;
		bool		warningprinted;
		// Profiling data
		float		inittime;
		float		shutdowntime;
	};

								CInitTracker( void );
								~CInitTracker( void );

	void						Init( const char *init, const char *shutdown, int listnum );
	void						Shutdown( const char *shutdown, int listnum );

private:
	int							m_nNumFuncs[ NUM_LISTS ];
	CUtlVector < InitFunc * >	m_Funcs[ NUM_LISTS ];
};

static CInitTracker g_InitTracker;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInitTracker::CInitTracker( void )
{
	for ( int l = 0; l < NUM_LISTS; l++ )
	{
		m_nNumFuncs[ l ] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInitTracker::~CInitTracker( void )
{
	for ( int l = 0; l < NUM_LISTS; l++ )
	{
		//assert( m_nNumFuncs[l] == 0 );
		for ( int i = 0; i < m_nNumFuncs[l]; i++ )
		{
			InitFunc *f = m_Funcs[ l ][ i ];
			if ( f->referencecount )
			{
				Msg( "Missing shutdown function for %s : %s\n", f->initname, f->shutdownname );
			}
			delete f;
		}
		m_Funcs[ l ].RemoveAll();
		m_nNumFuncs[ l ] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *init - 
//			*shutdown - 
//-----------------------------------------------------------------------------
void CInitTracker::Init( const char *init, const char *shutdown, int listnum )
{
	InitFunc *f = new InitFunc;
	Assert( f );
	f->initname			= init;
	f->shutdownname		= shutdown;
	f->sequence			= m_nNumFuncs[ listnum ];
	f->referencecount	= 1;
	f->warningprinted	= false;
	f->inittime			= 0.0; //Sys_FloatTime();
	f->shutdowntime		= 0.0;

	m_Funcs[ listnum ].AddToHead( f );
	m_nNumFuncs[ listnum ]++;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *shutdown - 
//-----------------------------------------------------------------------------
void CInitTracker::Shutdown( const char *shutdown, int listnum )
{
	if( !m_nNumFuncs[ listnum ] )
	{
		Msg( "Mismatched shutdown function %s\n", shutdown );
		return;
	}
	
	int i = 0;
	InitFunc *f = NULL;
	for ( i = 0; i < m_nNumFuncs[ listnum ]; i++ )
	{
		f = m_Funcs[ listnum ][ i ];
		if ( f->referencecount )
			break;
	}
	
	if ( f && f->referencecount && stricmp( f->shutdownname, shutdown ) )
	{
		if ( !f->warningprinted )
		{
			f->warningprinted = true;
			//Msg( "Shutdown function %s called out of order, expecting %s\n", shutdown, f->shutdownname );
		}
	}

	for ( i = 0; i < m_nNumFuncs[ listnum ]; i++ )
	{
		f = m_Funcs[ listnum ][ i ];

		if ( !stricmp( f->shutdownname, shutdown ) )
		{
			Assert( f->referencecount );
			//f->shutdowntime = Sys_FloatTime();
			f->referencecount--;
			return;
		}
	}

	Msg( "Shutdown function %s not in list!!!\n", shutdown );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *i - 
//			*s - 
//-----------------------------------------------------------------------------
void TraceInit( const char *i, const char *s, int listnum )
{
	g_InitTracker.Init( i, s, listnum );

	COM_TimestampedLog( "%s", i );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//-----------------------------------------------------------------------------
void TraceShutdown( const char *s, int listnum )
{
	g_InitTracker.Shutdown( s, listnum );
}