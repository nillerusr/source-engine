//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H
#ifdef _WIN32
#pragma once
#endif

#ifndef INCLUDED_MXWINDOW
#include <mxtk/mxWindow.h>
#endif

#include "faceposertoolwindow.h"

#define IDC_TAB						1901
#define IDC_RENDERMODE				2001
#define IDC_GROUND					2003
#define IDC_MOVEMENT				2004
#define IDC_BACKGROUND				2005
#define IDC_HITBOXES				2006
#define IDC_BONES					2007
#define IDC_ATTACHMENTS				2008
#define IDC_PHYSICSMODEL			2009
#define IDC_PHYSICSHIGHLIGHT		2010
#define IDC_MODELSPACING			2011
#define IDC_TOOLSDRIVEMOUTH			2012

#define IDC_SEQUENCE				3001
#define IDC_SPEEDSCALE				3002
#define IDC_PRIMARYBLEND			3003
#define IDC_SECONDARYBLEND			3004

#define IDC_BODYPART				4001
#define IDC_SUBMODEL				4002
#define IDC_CONTROLLER				4003
#define IDC_CONTROLLERVALUE			4004
#define IDC_SKINS					4005

#define IDC_EXPRESSIONCLASS			5001
#define IDC_EXPRESSIONTRAY			5002
#define IDC_ANIMATIONBROWSER		5003

class mxTab;
class mxChoice;
class mxCheckBox;
class mxSlider;
class mxLineEdit;
class mxLabel;
class mxButton;
class MatSysWindow;
class TextureWindow;
class mxExpressionTray;
class FlexPanel;
class PhonemeEditor;
class mxExpressionTab;
class mxExpressionSlider;
class ExpressionTool;
class CChoreoView;


class ControlPanel : public mxWindow, public IFacePoserToolWindow
{
	typedef mxWindow BaseClass;

	mxTab *tab;
	mxChoice *cRenderMode;
	mxCheckBox *cbGround, *cbMovement, *cbBackground;
	mxChoice *cSequence;
	mxSlider *slSpeedScale;
	mxLabel *lSpeedScale;
	mxChoice *cBodypart, *cController, *cSubmodel;
	mxSlider *slController;
	mxChoice *cSkin;
	mxLabel *lModelInfo1, *lModelInfo2;

	mxLineEdit *leMeshScale, *leBoneScale;
	mxSlider *slModelGap;

	mxCheckBox *cbAllWindowsDriveSpeech;
	
public:
	// CREATORS
	ControlPanel (mxWindow *parent);
	virtual ~ControlPanel ();

	// MANIPULATORS

	virtual int handleEvent (mxEvent *event);
	virtual void redraw();

	virtual void		OnDelete();
	virtual bool		CanClose();

	virtual void		Think( float dt );

	void dumpModelInfo ();
	void ChangeModel( const char *filename );

	void setRenderMode (int mode);
	void setShowGround (bool b);
	void setShowMovement (bool b);
	void setShowBackground (bool b);
	void setHighlightBone( int index );

	void initSequenceChoices( );
	void setSequence (int index);

	void setSpeed( float value );

	void initPoseParameters ();
	void setBlend(int index, float value );

	void initBodypartChoices();
	void setBodypart (int index);
	void setSubmodel (int index);

	void initBoneControllerChoices();
	void setBoneController (int index);
	void setBoneControllerValue (int index, float value);

	void initSkinChoices();

	void setModelInfo ();

	void centerView ();

	void fullscreen ();

	void CenterOnFace( void );

	void PositionControls( int width, int height );

	bool CloseClass( int classindex );

	bool Close();
	bool Closeall();

	void Copy( void );
	void Paste( void );

	void Undo( void );
	void Redo( void );

	void UndoExpression( int index );
	void RedoExpression( int index );

	void DeleteExpression( int index );

	float GetModelGap( void );

	bool	AllToolsDriveSpeech( void );

};

extern ControlPanel		*g_pControlPanel;

#endif // CONTROLPANEL_H
