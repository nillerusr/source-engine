//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FACEPOSERTOOLWINDOW_H
#define FACEPOSERTOOLWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include <mxtk/mx.h>
#include "mxtk/mxWindow.h"

class CChoreoWidgetDrawHelper;

class IFacePoserToolWindow
{
public:
	IFacePoserToolWindow( char const *toolname, char const *displaynameroot );
	virtual ~IFacePoserToolWindow( void );

	virtual mxWindow *GetMxWindow( void );
	virtual void	Shutdown() { }

	virtual void	Think( float dt );
	virtual bool	IsScrubbing( void ) const { return false; }
	virtual bool	IsProcessing( void ) { return false; }

	bool			IsActiveTool( void );

	virtual bool	IsLocked( void );
	virtual bool	HandleToolEvent( mxEvent *event );
	virtual void	HandleToolRedraw( CChoreoWidgetDrawHelper& helper );
	virtual int		GetCaptionHeight( void );
	void			ToggleLockedState( void );

	void LoadPosition( void );
	void SavePosition( void );

	char const *GetToolName( void ) const;
	char const *GetWindowTitle( void ) const;
	char const *GetDisplayNameRoot( void  ) const;

	void SetDisplayNameRoot( char const *name );
	void SetSuffix( char const *suffix );
	void SetPrefix( char const *prefix );

	void SetUseForMainWindowTitle( bool use );

	void SetAutoProcess( bool autoprocess );
	bool GetAutoProcess( void ) const;

	virtual void	OnModelChanged();

	static int GetToolCount( void );
	static IFacePoserToolWindow *GetTool( int index );

	static IFacePoserToolWindow *GetActiveTool( void );
	static void SetActiveTool( IFacePoserToolWindow *tool );
	static IFacePoserToolWindow *s_pActiveTool;
	static void ToolThink( float dt );
	static void ModelChanged();
	static bool IsAnyToolScrubbing( void );
	static bool IsAnyToolProcessing( void );

	static bool ShouldAutoProcess( void );

	static void InitTools( void );
	static void ShutdownTools( void );

	static void	EnableToolRedraw( bool enabled );
	static bool	s_bToolsCanDraw;

	bool		ToolCanDraw( void );

private:
	void	GetLockRect( RECT& rc );
	void	GetCloseRect( RECT& rc );

	void	ComputeNewTitle( void );

	void	SetToolName( char const *name );

	enum
	{
		MAX_TOOL_NAME = 128,
		PREFIX_LENGTH = 32,
		SUFFIX_LENGTH = 128,
	};

	char		m_szToolName[ MAX_TOOL_NAME ];
	char		m_szDisplayRoot[ MAX_TOOL_NAME ];
	char		m_szPrefix[ PREFIX_LENGTH ];
	char		m_szSuffix[ SUFFIX_LENGTH ];

	char		m_szWindowTitle[ MAX_TOOL_NAME + PREFIX_LENGTH + PREFIX_LENGTH ];

	bool		m_bUseForMainWindowTitle;

	bool		m_bAutoProcess;

	int			m_nToolFrameCount;
};

#endif // FACEPOSERTOOLWINDOW_H
