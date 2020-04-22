//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FLEXPANEL_H
#define FLEXPANEL_H
#ifdef _WIN32
#pragma once
#endif

#ifndef INCLUDED_MXWINDOW
#include <mxtk/mxWindow.h>
#endif

#define IDC_FLEX					7001
#define IDC_FLEXSCROLL				7101
#define IDC_EXPRESSIONRESET			7102

// NOTE THIS THIS TAKES UP 4 * 96 entries (384)
// #define NEXT_AVAIL				7457 ...etc.
#define IDC_FLEXSCALE				7200

#define IDC_FLEXSCALE_LAST			 7584

#define IDC_FP_UNCHECK_ALL			7800
#define IDC_FP_CHECK_ALL			7801
#define IDC_FP_INVERT				7802
#define IDC_FP_MENU					7803

#include "studio.h"

class mxTab;
class mxChoice;
class mxCheckBox;
class mxSlider;
class mxScrollbar;
class mxLineEdit;
class mxLabel;
class mxButton;
class MatSysWindow;
class TextureWindow;
class mxExpressionSlider;


#include "expressions.h"
#include "faceposertoolwindow.h"

/*
	int nameindex;
	int numkeys;
	int keyindex;
	{ char key, char weight }
*/

class ControlPanel;

class FlexPanel : public mxWindow, public IFacePoserToolWindow
{
	typedef mxWindow BaseClass;

	mxExpressionSlider *slFlexScale[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];

	mxScrollbar *slScrollbar;

	mxButton	*btnResetSliders;
	mxButton	*btnCopyToSliders;
	mxButton	*btnCopyFromSliders;
	mxButton	*btnMenu;


public:
	// CREATORS
	FlexPanel (mxWindow *parent);
	virtual ~FlexPanel ();

	virtual void redraw();
	virtual bool PaintBackground( void );

	void				SetEvent( CChoreoEvent *event );
	virtual void		OnModelChanged();

	// MANIPULATORS
	int handleEvent (mxEvent *event);
	
	void initFlexes ();

	bool	IsValidSlider( int iFlexController ) const;

	float	GetSlider( int iFlexController );
	float	GetSliderRawValue( int iFlexController );
	void	GetSliderRange( int iFlexController, float& minvalue, float& maxvalue );

	void	SetSlider( int iFlexController, float value );
	float	GetInfluence( int iFlexController );
	void	SetInfluence( int iFlexController, float value );
	void	SetEdited( int iFlexController, bool isEdited );
	bool	IsEdited( int iFlexController );
	int		LookupFlex( int iSlider, int barnum );
	int		LookupPairedFlex( int iFlexController );

	// maps global flex_controller index to UI slider
	int		nFlexSliderIndex[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	int		nFlexSliderBarnum[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];

	void PositionSliders( int sboffset );
	void PositionControls( int width, int height );

	void EditExpression( void );
	void NewExpression( void );

	void setExpression( int index );
	void DeleteExpression( int index );
	void SaveExpression( int index );
	void RevertExpression( int index );

	void CopyControllerSettings( void );
	void PasteControllerSettings( void );

	void ResetSliders( bool preserveundo, bool bDirtyClass );

	void CopyControllerSettingsToStructure( CExpression *exp );

private:
	enum
	{
		FP_STATE_UNCHECK = 0,
		FP_STATE_CHECK,
		FP_STATE_INVERT
	};

	void			OnSetAll( int state );
	void			OnMenu();

	bool			m_bNewExpressionMode;

	// Since we combine left/right into one, this will be less than hdr->numflexcontrollers
	int				m_nViewableFlexControllerCount;
};

extern FlexPanel		*g_pFlexPanel;

#endif // FLEXPANEL_H
