//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PANELEFFECT_H
#define PANELEFFECT_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
}

class ITFHintItem;

#include <vgui_controls/PHandle.h>

// Serial under of effect, for safe lookup
typedef unsigned int EFFECT_HANDLE;
#define EFFECT_INVALID_HANDLE (EFFECT_HANDLE)(~0)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPanelEffect
{
public:
	DECLARE_CLASS_NOBASE( CPanelEffect );

	enum
	{
		UNKNOWN = 0,
		FLASHBORDER,
		ARROW,
	};

	enum
	{
		ENDPOINT_UNKNOWN = 0,
		ENDPOINT_PANEL,
		ENDPOINT_POINT,
		ENDPOINT_RECTANGLE,
		ENDPOINT_ENTITY,
	};


							CPanelEffect( ITFHintItem *owner );
	virtual					~CPanelEffect();

	virtual void			doPaint( vgui::Panel *panel );

	virtual void			Think( void );

	virtual bool			ShouldRemove( void );
	virtual void			SetShouldRemove( bool remove );

	virtual EFFECT_HANDLE	GetHandle( void );

	virtual void			SetType( int type );
	virtual int				GetType( void );

	virtual void			SetPanel( vgui::Panel *panel );
	virtual vgui::Panel		*GetPanel( void );

	virtual void			SetPanelOther( vgui::Panel *panel );
	virtual vgui::Panel		*GetPanelOther( void );

	virtual void			SetTargetPoint( int x, int y );
	virtual void			SetTargetRect( int x, int y, int w, int h );
	
	virtual void			SetColor( int r, int g, int b, int a );
	virtual void			GetColor( int& r, int& g, int& b, int& a );

	virtual void			SetEndTime( float time );
	virtual float			GetEndTime( void );

	virtual void			SetOwner( ITFHintItem *owner );
	virtual ITFHintItem		*GetOwner( void );

	virtual void			SetUsingOffset( bool active, int x, int y );
	virtual bool			GetUsingOffset( void );
	virtual void			GetOffset( int& x, int& y );

	virtual int				GetTargetType( void );
	virtual void			SetTargetType( int type );
	virtual bool			GetTargetRectangle( vgui::Panel *outpanel, int&x, int&y, int&w, int&h );

	virtual void			SetVisible( bool visible );
	virtual bool			GetVisible( void );

private:

	static EFFECT_HANDLE	m_nHandleCount;

protected:

	virtual bool			IsVisibleIncludingParent( vgui::Panel *panel );

	EFFECT_HANDLE			m_Handle;

	ITFHintItem				*m_pOwner;

	// Data
	
	// type of effect
	int						m_nType;

	// effect targets
	vgui::PHandle			m_hPanel;
	vgui::PHandle			m_hOtherPanel;

	// effect color
	int						m_r, m_g, m_b, m_a;

	float					m_flEndTime;// 0.0f for no end time

	// true if we should offset endpoint of arrow/lines into m_hOtherPanel by m_nOffset amount
	bool					m_bEndpointIsCoordinate;
	// x, y offset into destination panel
	int						m_nOffset[ 2 ];

	bool					m_bShouldRemove;

	int						m_TargetType;
	int						m_ptX;
	int						m_ptY;
	int						m_rectX;
	int						m_rectY;
	int						m_rectW;
	int						m_rectH;

	bool					m_bVisible;
};

#define EFFECT_FLASH_TIME 0.7f

#define EFFECT_R 100
#define EFFECT_G 150
#define EFFECT_B 220
#define EFFECT_A 255

#define ARROW_R 130
#define ARROW_G 190
#define ARROW_B 240
#define ARROW_A 255

#define AXIALLINE_R 220
#define AXIALLINE_G 220
#define AXIALLINE_B 255
#define AXIALLINE_A 255

// Panel effect APIs
void DestroyPanelEffects( ITFHintItem *owner );
EFFECT_HANDLE CreateFlashEffect( ITFHintItem *owner, vgui::Panel *target );
EFFECT_HANDLE CreateArrowEffect( ITFHintItem *owner, vgui::Panel *from, vgui::Panel *to );
EFFECT_HANDLE CreateAxialLineEffect( ITFHintItem *owner, vgui::Panel *from, vgui::Panel *to );
EFFECT_HANDLE CreateArrowEffectToPoint( ITFHintItem *owner, vgui::Panel *from, int x, int y );
EFFECT_HANDLE CreateAxialLineEffectToPoint( ITFHintItem *owner, vgui::Panel *from, int x, int y );
EFFECT_HANDLE CreateArrowEffectToRect( ITFHintItem *owner, vgui::Panel *from, int x, int y, int w, int h );
EFFECT_HANDLE CreateAxialLineEffectToRect( ITFHintItem *owner, vgui::Panel *from, int x, int y, int w, int h );

#endif // PANELEFFECT_H
