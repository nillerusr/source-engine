//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "ifpspanel.h"
#include <vgui_controls/Panel.h>
#include "view.h"
#include <vgui/IVGui.h>
#include "vguimatsurface/imatsystemsurface.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "filesystem.h"
#include "../common/xbox/xboxstubs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_showfps( "cl_showfps", "0", FCVAR_RELEASE, "Draw fps meter at top of screen (1 = fps, 2 = smooth fps, 3 = server MS, 4 = Show FPS and Log to file )" );
static ConVar cl_showpos( "cl_showpos", "0", FCVAR_RELEASE, "Draw current position at top of screen" );

extern bool g_bDisplayParticlePerformance;
int GetParticlePerformance();

#define PERF_HISTOGRAM_BUCKET_SIZE 60

//-----------------------------------------------------------------------------
// Purpose: Framerate indicator panel
//-----------------------------------------------------------------------------
class CFPSPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFPSPanel, vgui::Panel );

public:
	CFPSPanel( vgui::VPANEL parent );
	virtual			~CFPSPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint();
	virtual void	OnTick( void );
	virtual void	DumpStats();

	virtual bool	ShouldDraw( void );


protected:
	MESSAGE_FUNC_INT_INT( OnScreenSizeChanged, "OnScreenSizeChanged", oldwide, oldtall );

private:
	void ComputeSize( void );
	void InitAverages()
	{
		m_AverageFPS = -1;
		m_lastRealTime = -1;
		m_high = -1;
		m_low = -1;
		memset( m_pServerTimes, 0, sizeof(m_pServerTimes) );
	}

	enum { SERVER_TIME_HISTORY = 32 };

	vgui::HFont		m_hFont;
	float			m_AverageFPS;
	float			m_lastRealTime;
	float			m_pServerTimes[SERVER_TIME_HISTORY];
	int				m_nServerTimeIndex;
	int				m_high;
	int				m_low;
	bool			m_bLastDraw;

	int				m_nLinesNeeded;

	TimedEvent		m_tLogTimer;
	FileHandle_t	m_fhLog;
	char			m_szLevelname[32];
	int				m_nNumFramesTotal;
	int				m_nNumFramesBucket[PERF_HISTOGRAM_BUCKET_SIZE];
};

#define FPS_PANEL_WIDTH 300

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CFPSPanel::CFPSPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CFPSPanel" )
{
	memset( m_pServerTimes, 0, sizeof(m_pServerTimes) );
	m_nServerTimeIndex = 0;
	SetParent( parent );
	SetVisible( false );
	SetCursor( null );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );
					    
	m_hFont = 0;
	m_nLinesNeeded = 5;		  

	ComputeSize();

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	m_bLastDraw = false;

	m_tLogTimer.Init( 6 );
	m_fhLog = FILESYSTEM_INVALID_HANDLE;
	m_nNumFramesTotal = 0;
	memset( m_nNumFramesBucket, 0, sizeof(m_nNumFramesBucket) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFPSPanel::~CFPSPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Updates panel to handle the new screen size
//-----------------------------------------------------------------------------
void CFPSPanel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);
	ComputeSize();
}

//-----------------------------------------------------------------------------
// Purpose: Computes panel's desired size and position
//-----------------------------------------------------------------------------
void CFPSPanel::ComputeSize( void )
{
	int wide, tall;
	vgui::ipanel()->GetSize(GetVParent(), wide, tall );

	int x = 0;;
	int y = 0;
	if ( IsX360() )
	{
		x += XBOX_MINBORDERSAFE * wide;
		y += XBOX_MINBORDERSAFE * tall;
	}
	SetPos( x, y );
	SetSize( FPS_PANEL_WIDTH, ( m_nLinesNeeded + 2 ) * vgui::surface()->GetFontTall( m_hFont ) + 4 );
}

void CFPSPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "DefaultFixedOutline" );
	Assert( m_hFont );

	ComputeSize();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFPSPanel::OnTick( void )
{
	SetVisible( ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFPSPanel::ShouldDraw( void )
{
	if ( g_bDisplayParticlePerformance )
		return true;
	if ( ( !cl_showfps.GetInt() || ( gpGlobals->absoluteframetime <= 0 ) ) &&
		 ( !cl_showpos.GetInt() ) )
	{
		m_bLastDraw = false;
		return false;
	}

	if ( !m_bLastDraw )
	{
		m_bLastDraw = true;
		InitAverages();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GetFPSColor( int nFps, unsigned char ucColor[3] )
{
	ucColor[0] = 255; ucColor[1] = 0; ucColor[2] = 0;

	int nFPSThreshold1 = 20;
	int nFPSThreshold2 = 15;
	
	if ( IsPC() && g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 95 )
	{
		nFPSThreshold1 = 60;
		nFPSThreshold2 = 50;
	}
	else
	{
		if ( engine->IsSplitScreenActive() )
		{
			nFPSThreshold1 = 19; //20; // 19 shows up commonly when testing on the 360
			nFPSThreshold2 = 15;
		}
		else
		{
			nFPSThreshold1 = 29; //30; 29 shows up commonly when testing on the 360
			nFPSThreshold2 = 20;
		}
	}

	if ( nFps >= nFPSThreshold1 )
	{
		ucColor[0] = 0; 
		ucColor[1] = 255;
	}
	else if ( nFps >= nFPSThreshold2 )
	{
		ucColor[1] = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFPSPanel::Paint() 
{
	int i = 0;
	int x = 2;

	int lineHeight = vgui::surface()->GetFontTall( m_hFont ) + 1;

	if ( g_bDisplayParticlePerformance )
	{
		int nPerf = GetParticlePerformance();
		if ( nPerf )
		{
			unsigned char ucColor[3]={ 0,255,0 };
			g_pMatSystemSurface->DrawColoredText(
				m_hFont, x, 42,
				ucColor[0], ucColor[1], ucColor[2],
				255, "Particle Performance Metric : %d", (nPerf+50)/100 );
		}
	}
	float realFrameTime = gpGlobals->realtime - m_lastRealTime;

	int nFPSMode = cl_showfps.GetInt();

	if ( nFPSMode == 3 )
	{
		float flServerTime = engine->GetServerSimulationFrameTime();
		if ( flServerTime != 0.0f )
		{
			m_nServerTimeIndex = ( m_nServerTimeIndex + 1 ) & ( SERVER_TIME_HISTORY - 1 );
			m_pServerTimes[ m_nServerTimeIndex ] = flServerTime;
		}

		flServerTime = m_pServerTimes[ m_nServerTimeIndex ];

		float flTotalTime = 0.0f;
		float flPeakTime = 0.0f;
		for ( int i = 0; i < SERVER_TIME_HISTORY; ++i )
		{
			flTotalTime += m_pServerTimes[i];
			if ( flPeakTime < m_pServerTimes[i] )
			{
				flPeakTime = m_pServerTimes[i];
			}
		}
		flTotalTime /= SERVER_TIME_HISTORY;

		unsigned char ucColor[3];
		int nFps = static_cast<int>( 1.0f / ( flServerTime * 0.001f ) );
		GetFPSColor( nFps, ucColor );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, 
			"server %5.1f ms curr, %5.1f ave, %5.1f peak", flServerTime, flTotalTime, flPeakTime );
	}
	else if ( nFPSMode == 4 && m_lastRealTime > 0.0f && realFrameTime > 0.0f && engine->IsInGame() )
	{
		char levelName[32];
		Q_strncpy( levelName, engine->GetLevelNameShort(), sizeof( levelName ) );
		if ( Q_strcmp( m_szLevelname, levelName ) && m_fhLog != FILESYSTEM_INVALID_HANDLE )
		{
			DumpStats();
		}


		float flServerTime = engine->GetServerSimulationFrameTime();
		if ( flServerTime != 0.0f )
		{
			m_nServerTimeIndex = ( m_nServerTimeIndex + 1 ) & ( SERVER_TIME_HISTORY - 1 );
			m_pServerTimes[ m_nServerTimeIndex ] = flServerTime;
		}

		flServerTime = m_pServerTimes[ m_nServerTimeIndex ];

		const float NewWeight  = 0.1f;
		float NewFrame = 1.0f / realFrameTime;

		if ( m_AverageFPS <= 0.0f )
		{
			m_AverageFPS = NewFrame;
		} 
		else
		{				
			m_AverageFPS *= ( 1.0f - NewWeight ) ;
			m_AverageFPS += ( ( NewFrame ) * NewWeight );
		}

		int nAvgFps = static_cast<int>( m_AverageFPS );
		float flAverageMS = 1000.0f / m_AverageFPS;
		float flFrameMS = realFrameTime * 1000.0f;
		int	nFrameFps = static_cast<int>( 1.0f / realFrameTime );
		float flCurTime = gpGlobals->frametime;

		m_nNumFramesTotal++;
		int nBucket = MIN ( PERF_HISTOGRAM_BUCKET_SIZE, MAX ( 0, nFrameFps ) );
		m_nNumFramesBucket[nBucket]++;

		unsigned char ucColor[3];
		GetFPSColor( nAvgFps, ucColor );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, 
			"Avg FPS %3i, Frame MS %5.1f, Frame Server MS %5.1f", nAvgFps, flFrameMS, flServerTime );

		if ( m_fhLog == FILESYSTEM_INVALID_HANDLE )
		{
			Q_strncpy( m_szLevelname, levelName, sizeof( m_szLevelname ) );
			// Maximum 360 file name length
			char fileString[42];
			Q_snprintf( fileString, 42, "prof_%s.csv", m_szLevelname );
			m_fhLog = g_pFullFileSystem->Open( fileString, "w", "GAME" );
			g_pFullFileSystem->FPrintf( m_fhLog, "Time,Player 1 Position,Player 2 Position,Smooth FPS,Frame FPS,Smooth MS,Frame MS,Server Frame MS\n");
		}

		if ( ( m_tLogTimer.NextEvent( flCurTime ) && nFrameFps < 28.0f ) || nFrameFps < 15.0f )
		{
			Vector vecOrigin[MAX_SPLITSCREEN_PLAYERS];
			QAngle angles[MAX_SPLITSCREEN_PLAYERS];
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				vecOrigin[hh] = MainViewOrigin(hh);
				angles[hh]= MainViewAngles(hh);

				C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer(hh);

				if ( pPlayer )
				{
					vecOrigin[hh] = pPlayer->GetAbsOrigin();
					angles[hh] = pPlayer->GetAbsAngles();
				}
			}

			char outputString[256];
			Q_snprintf( outputString, 256, "%5.1f,setpos %0.2f %0.2f %0.2f ; setang %0.2f %0.2f %0.2f,setpos %0.2f %0.2f %0.2f ; setang %0.2f %0.2f %0.2f,%3i,%3i,%4.1f,%4.1f,%5.1f\n", 
				gpGlobals->curtime,vecOrigin[0].x, vecOrigin[0].y, vecOrigin[0].z, angles[0].x, angles[0].y, angles[0].z, 
				vecOrigin[1].x, vecOrigin[1].y, vecOrigin[1].z, angles[1].x, angles[1].y, angles[1].z, nAvgFps, 
				nFrameFps, flAverageMS, flFrameMS, flServerTime );

			if ( m_fhLog != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->FPrintf( m_fhLog, "%s", outputString );
			}
		}

	}
	else if ( nFPSMode && realFrameTime > 0.0 )
	{
		if ( m_lastRealTime != -1.0f )
		{
			int nFps = -1;
			unsigned char ucColor[3];
			if ( nFPSMode == 2 )
			{
				const float NewWeight  = 0.1f;
				float NewFrame = 1.0f / realFrameTime;

				// If we're just below an integer boundary, we're good enough to call ourselves good WRT to coloration
				if ( (int)(NewFrame + 0.05) > (int )( NewFrame ) )
				{
					NewFrame = ceil( NewFrame );
				}

				if ( m_AverageFPS < 0.0f )
				{
					m_AverageFPS = NewFrame;
					m_high = (int)m_AverageFPS;
					m_low = (int)m_AverageFPS;
				} 
				else
				{				
					m_AverageFPS *= ( 1.0f - NewWeight ) ;
					m_AverageFPS += ( ( NewFrame ) * NewWeight );
				}
			
				int NewFrameInt = (int)NewFrame;
				if( NewFrameInt < m_low ) m_low = NewFrameInt;
				if( NewFrameInt > m_high ) m_high = NewFrameInt;	

				nFps = static_cast<int>( m_AverageFPS );
				float averageMS = 1000.0f / m_AverageFPS;
				float frameMS = realFrameTime * 1000.0f;
				GetFPSColor( nFps, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, "%3i fps (%3i, %3i) smth:%4.1f ms frm:%4.1f ms on %s", nFps, m_low, m_high, averageMS, frameMS, engine->GetLevelName() );
			}
			else
			{
				m_AverageFPS = -1;
				float flFps = ( 1.0f / realFrameTime );

				// If we're just below an integer boundary, we're good enough to call ourselves good WRT to coloration
				if ( (int)(flFps + 0.05) > (int )( flFps ) )
				{
					flFps = ceil( flFps );
				}
				nFps = static_cast<int>( flFps );
				GetFPSColor( nFps, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, "%3i fps on %s", nFps, engine->GetLevelName() );
			}
		}
	}
	else if ( m_fhLog != FILESYSTEM_INVALID_HANDLE )
	{
		DumpStats();
	}

	m_lastRealTime = gpGlobals->realtime;

	int nShowPosMode = cl_showpos.GetInt();
	if ( nShowPosMode > 0 )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			Vector vecOrigin = MainViewOrigin(hh);
			QAngle angles = MainViewAngles(hh);
			Vector vel( 0, 0, 0 );

			char szName[ 32 ] = { 0 };

			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer(hh);
			if ( pPlayer )
			{
				Q_strncpy( szName, pPlayer->GetPlayerName(), sizeof( szName ) );
				vel = pPlayer->GetLocalVelocity();
			}

			if ( nShowPosMode == 2 && pPlayer )
			{
				vecOrigin = pPlayer->GetAbsOrigin();
				angles = pPlayer->GetAbsAngles();
			}

			i++;

			g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
													255, 255, 255, 255, 
													"name: %s", szName );

			i++;

			g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2+ i * lineHeight, 
												  255, 255, 255, 255, 
												  "pos:  %.02f %.02f %.02f", 
												  vecOrigin.x, vecOrigin.y, vecOrigin.z );
			i++;

			g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
												  255, 255, 255, 255, 
												  "ang:  %.02f %.02f %.02f", 
												  angles.x, angles.y, angles.z );
			i++;

			g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
												  255, 255, 255, 255, 
												  "vel:  %.2f", 
												  vel.Length() );
		}
	}

	if ( m_nLinesNeeded != i )
	{
	    m_nLinesNeeded = i;
		ComputeSize();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Outputs the frame rate histogram and closes the file
//-----------------------------------------------------------------------------
void CFPSPanel::DumpStats() 
{
	Assert ( m_fhLog != FILESYSTEM_INVALID_HANDLE );
	if ( m_nNumFramesTotal	> 0 )
	{
		g_pFullFileSystem->FPrintf( m_fhLog, "\n\nTotal Frames : %3i\n\n", m_nNumFramesTotal );
		g_pFullFileSystem->FPrintf( m_fhLog, "Frame Rate, Number of Frames, Percent of Frames\n" );

		for ( int i = 0; i <= PERF_HISTOGRAM_BUCKET_SIZE; i++ )
		{
			float flPercent = m_nNumFramesBucket[i];
			flPercent /= m_nNumFramesTotal;
			flPercent *= 100.0f;
			g_pFullFileSystem->FPrintf( m_fhLog, "%3i, %3i, %5.1f\n", i, m_nNumFramesBucket[i], flPercent );
			m_nNumFramesBucket[i] = 0;
		}
	}
	g_pFullFileSystem->Close( m_fhLog );
	m_fhLog = FILESYSTEM_INVALID_HANDLE;
	m_nNumFramesTotal = 0;
}

class CFPS : public IFPSPanel
{
private:
	CFPSPanel *fpsPanel;
public:
	CFPS( void )
	{
		fpsPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		fpsPanel = new CFPSPanel( parent );
	}

	void Destroy( void )
	{
		if ( fpsPanel )
		{
			fpsPanel->SetParent( (vgui::Panel *)NULL );
			delete fpsPanel;
		}
	}
};

static CFPS g_FPSPanel;
IFPSPanel *fps = ( IFPSPanel * )&g_FPSPanel;

#if defined( TRACK_BLOCKING_IO )

static ConVar cl_blocking_threshold( "cl_blocking_threshold", "0.000", 0, "If file ops take more than this amount of time, add to 'spewblocking' history list" );

void ShowBlockingChanged( ConVar *var, char const *pOldString )
{
	filesystem->EnableBlockingFileAccessTracking( var->GetBool() );
}

static ConVar cl_showblocking( "cl_showblocking", "0", 0, "Show blocking i/o on top of fps panel", ShowBlockingChanged );
static ConVar cl_blocking_recentsize( "cl_blocking_recentsize", "40", 0, "Number of items to store in recent spew history." );

//-----------------------------------------------------------------------------
// Purpose: blocking i/o indicator
//-----------------------------------------------------------------------------
class CBlockingFileIOPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	CBlockingFileIOPanel( vgui::VPANEL parent );
	virtual			~CBlockingFileIOPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint();
	virtual void	OnTick( void );

	virtual bool	ShouldDraw( void );

	void			SpewRecent();

private:
	void			DrawIOTime( int x, int y, int w, int h, int slot, char const *label, const Color& clr );

	vgui::HFont		m_hFont;

	struct Graph_t
	{
		float			m_flCurrent;

		float			m_flHistory;
		float			m_flHistorySpike;
		float			m_flLatchTime;
		CUtlSymbol		m_LastFile;
	};

	Graph_t			m_History[ FILESYSTEM_BLOCKING_NUMBINS ];

	struct RecentPeaks_t
	{
		float		time;
		CUtlSymbol	fileName;
		float		elapsed;
		byte		reason;
		byte		ioType;
	};

	CUtlLinkedList< RecentPeaks_t, unsigned short >	m_Recent;

	void			SpewItem( const RecentPeaks_t& item );
};

#define IO_PANEL_WIDTH		400
#define IO_DECAY_FRAC		0.95f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CBlockingFileIOPanel::CBlockingFileIOPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CBlockingFileIOPanel" )
{
	SetParent( parent );
	int wide, tall;
	vgui::ipanel()->GetSize( parent, wide, tall );

	int x = 2;
	int y = 100;
	if ( IsX360() )
	{
		x += XBOX_MAXBORDERSAFE * wide;
		y += XBOX_MAXBORDERSAFE * tall;
	}
	SetPos( x, y );

	SetSize( IO_PANEL_WIDTH, 140 );

	SetVisible( false );
	SetCursor( null );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );

	m_hFont = 0;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	SetZPos( 1000 );
	Q_memset( m_History, 0, sizeof( m_History ) );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	MakePopup();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBlockingFileIOPanel::~CBlockingFileIOPanel( void )
{
}

void CBlockingFileIOPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "Default" );
	Assert( m_hFont );

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlockingFileIOPanel::OnTick( void )
{
	SetVisible( ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBlockingFileIOPanel::ShouldDraw( void )
{
	if ( !cl_showblocking.GetInt() )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CBlockingFileIOPanel::Paint() 
{
	int x = 2;
	
	int maxRecent = clamp( 0, cl_blocking_recentsize.GetInt(), 1000 );
	int bval = cl_showblocking.GetInt();
	if ( bval > 0 )
	{
		IBlockingFileItemList *list = filesystem->RetrieveBlockingFileAccessInfo();
		if ( list )
		{
			int i;
			int c = ARRAYSIZE( m_History );
			for ( i = 0; i < c; ++i )
			{
				m_History[ i ].m_flCurrent = 0.0f;
			}

			// Grab mutex (prevents async thread from filling in even more data...)
			list->LockMutex();
		{
			for ( int j = list->First() ; j != list->InvalidIndex(); j = list->Next( j ) )
			{
				const FileBlockingItem& item = list->Get( j );

				m_History[ item.m_ItemType ].m_flCurrent += item.m_flElapsed;

				RecentPeaks_t recent;
				recent.time = gpGlobals->realtime;
				recent.elapsed = item.m_flElapsed;
				recent.fileName = item.GetFileName();
				recent.reason = item.m_ItemType;
				recent.ioType = item.m_nAccessType;
				while ( m_Recent.Count() > maxRecent )
				{
					m_Recent.Remove( m_Recent.Head() );
				}

				m_Recent.AddToTail( recent );

				m_History[ item.m_ItemType ].m_LastFile = item.GetFileName();

				// Only care about time consuming synch or async blocking calls
				if ( item.m_ItemType == FILESYSTEM_BLOCKING_SYNCHRONOUS ||
					 item.m_ItemType == FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK )
				{
					if ( item.m_flElapsed > cl_blocking_threshold.GetFloat() )
					{
						SpewItem( recent );
					}
				}
			}
			list->Reset();
		}
		// Finished
		list->UnlockMutex();

		// Now draw some bars...
		int itemHeight = ( vgui::surface()->GetFontTall( m_hFont ) + 2 );

		int y = 2;
		int w = GetWide();

		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_SYNCHRONOUS, "Synchronous", Color( 255, 0, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK, "Async Block", Color( 255, 100, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_CALLBACKTIMING, "Callback", Color( 255, 255, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_ASYNCHRONOUS, "Asynchronous", Color( 0, 255, 0, 255 ) );

		for ( i = 0; i < c; ++i )
		{
			if ( m_History[ i ].m_flCurrent > m_History[ i ].m_flHistory )
			{
				m_History[ i ].m_flHistory = m_History[ i ].m_flCurrent;
				m_History[ i ].m_flHistorySpike = m_History[ i ].m_flCurrent;
				m_History[ i ].m_flLatchTime = gpGlobals->realtime;
			}
			else
			{
				// After this long, start to decay the previous history value
				if ( gpGlobals->realtime > m_History[ i ].m_flLatchTime + 1.0f )
				{
					m_History[ i ].m_flHistory = m_History[ i ].m_flHistory * IO_DECAY_FRAC + ( 1.0f - IO_DECAY_FRAC ) * m_History[ i ].m_flCurrent;
				}
			}
		}
		}
	}
}

static ConVar cl_blocking_msec( "cl_blocking_msec", "100", 0, "Vertical scale of blocking graph in milliseconds" );

static const char *GetBlockReason( int reason )
{
	switch ( reason )
	{
		case FILESYSTEM_BLOCKING_SYNCHRONOUS:
			return "Synchronous";
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS:
			return "Asynchronous";
		case FILESYSTEM_BLOCKING_CALLBACKTIMING:
			return "Async Callback";
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK:
			return "Async Blocked";
	}
	return "???";
}

static const char *GetIOType( int iotype )
{
	if ( FileBlockingItem::FB_ACCESS_APPEND == iotype )
	{
		return "Append";
	}
	else if ( FileBlockingItem::FB_ACCESS_CLOSE == iotype )
	{
		return "Close";
	}
	else if ( FileBlockingItem::FB_ACCESS_OPEN == iotype)
	{
		return "Open";
	}
	else if ( FileBlockingItem::FB_ACCESS_READ == iotype)
	{
		return "Read";
	}
	else if ( FileBlockingItem::FB_ACCESS_SIZE == iotype)
	{
		return "Size";
	}
	else if ( FileBlockingItem::FB_ACCESS_WRITE == iotype)
	{
		return "Write";
	}
	return "???";
}

void CBlockingFileIOPanel::SpewItem( const RecentPeaks_t& item )
{
	switch ( item.reason )
	{
		default:
			Assert( 0 );
			// break; -- intentionally fall through
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS:
		case FILESYSTEM_BLOCKING_CALLBACKTIMING:
			Msg( "%8.3f %16.16s i/o [%6.6s] took %8.3f msec:  %33.33s\n", 
				 item.time, 
				 GetBlockReason( item.reason ), 
				 GetIOType( item.ioType ),
				 item.elapsed * 1000.0f, 
				 item.fileName.String()
				);
			break;
		case FILESYSTEM_BLOCKING_SYNCHRONOUS:
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK:
			Warning( "%8.3f %16.16s i/o [%6.6s] took %8.3f msec:  %33.33s\n", 
					 item.time, 
					 GetBlockReason( item.reason ), 
					 GetIOType( item.ioType ),
					 item.elapsed * 1000.0f, 
					 item.fileName.String()
				);
			break;
	}
}

void CBlockingFileIOPanel::SpewRecent()
{
	FOR_EACH_LL( m_Recent, i )
	{
		const RecentPeaks_t& item = m_Recent[ i ];
		SpewItem( item );
	}
}

void  CBlockingFileIOPanel::DrawIOTime( int x, int y, int w, int h, int slot, char const *label, const Color& clr )
{
	float t = m_History[ slot ].m_flCurrent;
	float history = m_History[ slot ].m_flHistory;
	float latchedtime = m_History[ slot ].m_flLatchTime;
	float historyspike = m_History[ slot ].m_flHistorySpike;

	// 250 msec is considered a huge spike
	float maxTime = cl_blocking_msec.GetFloat() * 0.001f;
	if ( maxTime < 0.000001f )
		return;
	float frac = clamp( t / maxTime, 0.0f, 1.0f );
	float hfrac = clamp( history / maxTime, 0.0f, 1.0f );
	float spikefrac = clamp( historyspike / maxTime, 0.0f, 1.0f );

	g_pMatSystemSurface->DrawColoredText( m_hFont, x + 2, y + 1, 
										  clr[0], clr[1], clr[2], clr[3], 
										  "%s", 
										  label );

	int textWidth = 95;

	x += textWidth;
	w -= ( textWidth + 5 );

	int prevFileWidth = 140;
	w -= prevFileWidth;

	bool bDrawHistorySpike = false;

	if ( m_History[ slot ].m_LastFile.IsValid() && 
		 ( gpGlobals->realtime < latchedtime + 10.0f ) )
	{
		bDrawHistorySpike = true;
		g_pMatSystemSurface->DrawColoredText( m_hFont, x + w + 5, y + 1, 
											  255, 255, 255, 200, "[%8.3f ms]", m_History[ slot ].m_flHistorySpike * 1000.0f );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, y + h + 1, 
											  255, 255, 255, 200, "%s", m_History[ slot ].m_LastFile.String() );
	}

	y += 2;
	h -= 4;

	int barWide = ( int )( w * frac + 0.5f );
	int historyWide = ( int ) ( w * hfrac + 0.5f );
	int spikeWide = ( int ) ( w * spikefrac + 0.5f );

	int useWide = MAX( barWide, historyWide );

	vgui::surface()->DrawSetColor( Color( 0, 0, 0, 31 ) );
	vgui::surface()->DrawFilledRect( x, y, x + w, y + h );
	vgui::surface()->DrawSetColor( Color( 255, 255, 255, 128 ) );
	vgui::surface()->DrawOutlinedRect( x, y, x + w, y + h );
	vgui::surface()->DrawSetColor( clr );
	vgui::surface()->DrawFilledRect( x+1, y+1, x + useWide, y + h -1 );
	if ( bDrawHistorySpike )
	{
		vgui::surface()->DrawSetColor( Color( 255, 255, 255, 192 ) );
		vgui::surface()->DrawFilledRect( x + spikeWide, y + 1, x + spikeWide + 1, y + h - 1 );
	}
}

class CBlockingFileIO : public IShowBlockingPanel
{
private:
	CBlockingFileIOPanel *ioPanel;
public:
	CBlockingFileIO( void )
	{
		ioPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		ioPanel = new CBlockingFileIOPanel( parent );
	}

	void Destroy( void )
	{
		if ( ioPanel )
		{
			ioPanel->SetParent( (vgui::Panel *)NULL );
			delete ioPanel;
		}
	}

	void Spew()
	{
		if ( ioPanel )
		{
			ioPanel->SpewRecent();
		}
	}
};

static CBlockingFileIO g_IOPanel;
IShowBlockingPanel *iopanel = ( IShowBlockingPanel * )&g_IOPanel;

CON_COMMAND( spewblocking, "Spew current blocking file list." )
{
	g_IOPanel.Spew();
}

#endif // TRACK_BLOCKING_IO
