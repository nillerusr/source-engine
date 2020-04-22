//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHOREOCHANNELWIDGET_H
#define CHOREOCHANNELWIDGET_H
#ifdef _WIN32
#pragma once
#endif

#include "choreowidget.h"
#include "utlvector.h"

class CChoreoEventWidget;
class CChoreoActorWidget;
class CChoreoChannel;
class CChoreoChannelWidget;

//-----------------------------------------------------------------------------
// Purpose: The channel container
//-----------------------------------------------------------------------------
class CChoreoChannelWidget : public CChoreoWidget
{
public:
	typedef CChoreoWidget		BaseClass;

	enum
	{
		FULLMENU = 0,
		NEWEVENTMENU
	};

	enum
	{
		CLOSECAPTION_NONE = 0,
		CLOSECAPTION_EXPANDCOLLAPSE,
		CLOSECAPTION_PREVLANGUAGE,
		CLOSECAPTION_NEXTLANGUAGE,
		CLOSECAPTION_SELECTOR,
		CLOSECAPTION_CAPTION,
	};

	// Construction
								CChoreoChannelWidget( CChoreoActorWidget *parent );
	virtual						~CChoreoChannelWidget( void );

	virtual void				Create( void );
	virtual void				Layout( RECT& rc );

	virtual	int					GetItemHeight( void );

	virtual void				redraw(CChoreoWidgetDrawHelper& drawHelper);
	virtual void				redrawStatus( CChoreoWidgetDrawHelper& drawHelper, RECT& rcClient, int areaUnderMouse );

	// Accessors
	CChoreoChannel				*GetChannel( void );
	void						SetChannel( CChoreoChannel *channel );

	// Manipulate child events
	void						AddEvent( CChoreoEventWidget *event );
	void						RemoveEvent( CChoreoEventWidget *event );

	void						MoveEventToTail( CChoreoEventWidget *event );

	CChoreoEventWidget			*GetEvent( int num );
	int							GetNumEvents( void );

	// Determine time for click position
	float						GetTimeForMousePosition( int mx );

	int							GetChannelItemUnderMouse( int mx, int my );

	CChoreoEvent				*GetCaptionClickedEvent();
	void						GetMasterAndSlaves( CChoreoEvent *master, CUtlVector< CChoreoEvent * >& fulllist );

	void						HandleSelectorClicked();

private:

	struct CloseCaptionInfo
	{
		bool		isSelector;
		RECT		rcSelector;
		RECT		rcCaption;
		int			eventindex;
	};

	void						GetCloseCaptionExpandCollapseRect( RECT& rc );
	void						GetCloseCaptionLanguageRect( RECT& rc, bool previous );
	void						GetCloseCaptions( CUtlVector< CloseCaptionInfo >& selectors );

	int							GetVerticalStackingCount( bool dolayout, RECT* rc );
	void						LayoutEventInRow( CChoreoEventWidget *event, int row, RECT& rc );

	void						RenderCloseCaptionInfo( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea );
	void						RenderCloseCaptions( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea );
	void						RenderCloseCaptionExpandCollapseRect( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea );
	void						RenderCloseCaptionSelectors( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea );

	void						SetUsingCombinedFieldByTokenName( char const *token, bool usingcombinedfile );

	bool						CheckHasAudio();

	// The actor to whom we belong
	CChoreoActorWidget			*m_pParent;

	// The underlying scene object
	CChoreoChannel				*m_pChannel;

	// Children
	CUtlVector < CChoreoEventWidget * >	m_Events;
	bool						m_bHasAudio;
	int							m_nBaseHeight;

	int							m_nSelectorEventIndex;
};

#endif // CHOREOCHANNELWIDGET_H
