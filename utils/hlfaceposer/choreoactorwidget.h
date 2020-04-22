//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOACTORWIDGET_H
#define CHOREOACTORWIDGET_H
#ifdef _WIN32
#pragma once
#endif

#include "studio.h"
#include "choreowidget.h"
#include "utlvector.h"
#include "mxBitmapButton.h"
#include "expressions.h"

class CChoreoActor;
class CChoreoChannelWidget;
class mxCheckBox;
class CChoreoActorWidget;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CActorBitmapButton : public mxBitmapButton
{
public:
	CActorBitmapButton( CChoreoActorWidget *actor, mxWindow *parent, int x, int y, int w, int h, int id = 0, const char *bitmap = 0 );

	CChoreoActorWidget	*GetActor( void );
private:

	CChoreoActorWidget		*m_pActor;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CActorActiveCheckBox : public mxCheckBox
{
public:
	CActorActiveCheckBox( CChoreoActorWidget *actor, mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int id = 0);

	CChoreoActorWidget	*GetActor( void );
private:

	CChoreoActorWidget		*m_pActor;
};

//-----------------------------------------------------------------------------
// Purpose: The base actor ui widget.  Owns the channels
//-----------------------------------------------------------------------------
class CChoreoActorWidget : public CChoreoWidget
{
public:
	typedef CChoreoWidget		BaseClass;

	// Construction / destruction
								CChoreoActorWidget( CChoreoWidget *parent );
	virtual						~CChoreoActorWidget( void );

	virtual void				Create( void );
	virtual void				Layout( RECT& rc );

	virtual void				redraw(CChoreoWidgetDrawHelper& drawHelper);

	// Accessors
	CChoreoActor				*GetActor( void );
	void						SetActor( CChoreoActor *actor );

	// Manipulate channels
	void						AddChannel( CChoreoChannelWidget *channel );
	void						RemoveChannel( CChoreoChannelWidget *channel );
	CChoreoChannelWidget		*GetChannel( int num );
	int							GetNumChannels( void );

	// Override height because we can be open/collapsed and we contain the channels
	virtual int					GetItemHeight( void );

	// UI interactions
	void						DeleteChannel( void );
	void						NewChannel( void );
	void						MoveChannelUp( void );
	void						MoveChannelDown( void );

	// Expanded view or contracted view
	void						ShowChannels( bool show );
	bool						GetShowChannels( void );

	float						*GetSettings( void );

	void						ResetSettings( void );

private:
	// Context menu handler
	void						ShowRightClickMenu( int mx, int my );

	// The underlying actor
	CChoreoActor				*m_pActor;

	// Children
	CUtlVector < CChoreoChannelWidget * > m_Channels;

	// Expanded mode?
	bool						m_bShowChannels;

	// Expand/collapse buttons
	CActorBitmapButton			*m_btnOpen;
	CActorBitmapButton			*m_btnClose;

	CActorActiveCheckBox		*m_cbActive;

	float						m_rgCurrentSetting[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
};

#endif // CHOREOACTORWIDGET_H
