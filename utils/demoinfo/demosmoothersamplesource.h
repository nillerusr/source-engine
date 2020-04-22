//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sample Source Code for "demo smoothing" tool in the engine.  This could be ported into the client .dll
//  pretty easily -- ywb
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_DEMOSMOOTHERPANEL_H
#define CL_DEMOSMOOTHERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui
{
class Button;
class Label;
class ListPanel;
class IScheme;
};

#include "demofile/demoformat.h"
#include "demofile.h"

struct demodirectory_t;
class CSmoothingTypeButton;
class CFixEdgeButton;

typedef float (*EASEFUNC)( float t );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CDemoSmootherPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoSmootherPanel, vgui::Frame );

public:
	CDemoSmootherPanel( vgui::Panel *parent );
	~CDemoSmootherPanel();

	virtual void OnTick();

	// Command issued
	virtual void OnCommand(const char *command);

	void		OnRefresh();

	virtual bool		OverrideView( democmdinfo_t& info, int tick );

	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	virtual void	DrawDebuggingInfo(  int frame, float elapsed );


protected:

	bool		CanEdit();

	void		Reset( void );

	demosmoothing_t *GetCurrent( void );

	void		DrawSmoothingSample( bool original, bool processed, int samplenumber, demosmoothing_t *sample, demosmoothing_t *next );
	void		DrawTargetSpline( void );
	void		DrawKeySpline( void );
	int			GetTickForFrame( int frame );
	int			GetFrameForTick( int tick );
	bool		GetInterpolatedViewPoint( Vector& origin, QAngle& angles );
	bool		GetInterpolatedOriginAndAngles( bool readonly, Vector& origin, QAngle& angles );

	void		DrawLegend( int startframe, int endframe );

	void		OnRevert();
	void		OnPreview( bool original );
	void		OnSave();
	void		OnReload();
	void		OnSelect();
	void		OnTogglePause();
	void		OnStep( bool forward );
	void		OnGotoFrame();

	void		OnToggleKeyFrame( void );
	void		OnToggleLookTarget( void );

	void		OnNextKey();
	void		OnPrevKey();
	void		OnNextTarget();
	void		OnPrevTarget();

	void		OnRevertPoint( void );

	void		PopulateMenuList();
	int			GetStartFrame();
	int			GetEndFrame();

	void		OnSaveKey();
	void		OnSetView();

	void		OnSmoothEdges( bool left, bool right );

	void		PerformLinearInterpolatedAngleSmoothing( int startframe, int endframe );

	void		OnSmoothSelectionAngles( void );
	void		OnSmoothSelectionOrigin( void );
	void		OnLinearInterpolateAnglesBasedOnEndpoints( void );
	void		OnLinearInterpolateOriginBasedOnEndpoints( void );
	void		OnSplineSampleOrigin( void );
	void		OnSplineSampleAngles( void );
	void		OnLookAtPoints( bool spline );
	void		OnSetKeys(float interval);

	void		OnOriginEaseCurve( EASEFUNC easefunc );

	void		SetLastFrame( bool jumptotarget, int frame );

	void		AddSamplePoints( bool usetarget, bool includeboundaries, CUtlVector< demosmoothing_t * >& points, int start, int end );
	demosmoothing_t *GetBoundedSample(  CUtlVector< demosmoothing_t * >& points, int sample );
	void		FindSpanningPoints( int tick, CUtlVector< demosmoothing_t * >& points, int& prev, int& next );

	// Undo/Redo
	void				Undo( void );
	void				Redo( void );

	// Do push before changes
	void				PushUndo( char *description );
	// Do this push after changes, must match pushundo 1for1
	void				PushRedo( char *description );

	void				WipeUndo( void );
	void				WipeRedo( void );

	const char			*GetUndoDescription( void );
	const char			*GetRedoDescription( void );

	bool				CanUndo( void );
	bool				CanRedo( void );

	void				ParseSmoothingInfo( CDemoFile &demoFile, CUtlVector< demosmoothing_t >& smooth );
	void				LoadSmoothingInfo( const char *filename, CSmoothingContext& smoothing );
	void				ClearSmoothingInfo( CSmoothingContext& smoothing );
	void				SaveSmoothingInfo( char const *filename, CSmoothingContext& smoothing );

	CSmoothingTypeButton	*m_pType;

	vgui::Button	*m_pRevert;
	vgui::Button	*m_pOK;
	vgui::Button	*m_pCancel;

	vgui::Button	*m_pSave;
	vgui::Button	*m_pReloadFromDisk;

	vgui::TextEntry		*m_pStartFrame;
	vgui::TextEntry		*m_pEndFrame;

	vgui::Button		*m_pPreviewOriginal;
	vgui::Button		*m_pPreviewProcessed;

	vgui::CheckButton	*m_pBackOff;

	vgui::Label			*m_pSelectionInfo;
	vgui::CheckButton	*m_pShowAllSamples;
	vgui::Button		*m_pSelectSamples;

	vgui::Button		*m_pPauseResume;
	vgui::Button		*m_pStepForward;
	vgui::Button		*m_pStepBackward;

	vgui::CheckButton	*m_pHideLegend;

	vgui::CheckButton	*m_pHideOriginal;
	vgui::CheckButton	*m_pHideProcessed;

	vgui::Button		*m_pToggleKeyFrame;
	vgui::Button		*m_pToggleLookTarget;
	vgui::Button		*m_pRevertPoint;

	vgui::Button		*m_pMoveCameraToPoint;

	vgui::Button		*m_pUndo;
	vgui::Button		*m_pRedo;

	vgui::Button		*m_pNextKey;
	vgui::Button		*m_pPrevKey;
	vgui::Button		*m_pNextTarget;
	vgui::Button		*m_pPrevTarget;

	CFixEdgeButton		*m_pFixEdges;
	vgui::TextEntry		*m_pFixEdgeFrames;

	vgui::Button		*m_pProcessKey;

	vgui::TextEntry		*m_pGotoFrame;
	vgui::Button		*m_pGoto;

	bool				m_bHasSelection;
	int					m_nSelection[2];
	int					m_iSelectionTicksSpan;

	bool				m_bPreviewing;
	bool				m_bPreviewOriginal;
	int					m_iPreviewStartTick;
	float				m_fPreviewCurrentTime;
	int					m_nPreviewLastFrame;
	bool				m_bPreviewPaused;

	CSmoothingContext	m_Smoothing;

	bool				m_bInputActive;
	int					m_nOldCursor[2];


	struct DemoSmoothUndo
	{
		CSmoothingContext *undo;
		CSmoothingContext *redo;
		char		 *udescription;
		char		 *rdescription;
	};

	CUtlVector< DemoSmoothUndo * >	m_UndoStack;
	int					m_nUndoLevel;
	bool				m_bRedoPending;

	bool				m_bDirty;

	Vector				m_vecEyeOffset;
};

#endif // CL_DEMOSMOOTHERPANEL_H
