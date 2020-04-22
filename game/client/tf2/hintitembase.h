//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HINTITEMBASE_H
#define HINTITEMBASE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "itfhintitem.h"

class C_TFBaseHint;
class CHintItemBase;

namespace vgui
{
	class Label;
}

#define DECLARE_HINTITEMFACTORY( className ) \
	CHintItemBase *Create_##className##( vgui::Panel *parent, const char *panelName ) \
	{ return new className( parent, panelName ); }

#define GET_HINTITEMFACTORY_NAME( className ) Create_##className

#define DECLARE_HINTFACTORY( className ) \
	C_TFBaseHint *Create_##className##( int id, int entity ) \
	{ return new className( id, 0, entity, NULL ); }

#define GET_HINTFACTORY_NAME( className ) Create_##className

//-----------------------------------------------------------------------------
// Purpose: A hint that shows up as a single line o text
//-----------------------------------------------------------------------------
class CHintItemBase : public vgui::Panel, public ITFHintItem
{
	DECLARE_CLASS_GAMEROOT( CHintItemBase, vgui::Panel );

public:
	CHintItemBase( vgui::Panel *parent, const char *panelName );

	// Draw some extra stuff in the Bg
	virtual void	PaintBackground();
	virtual void	OnSizeChanged( int newWide, int newTall );
	virtual void	SetText( const char *text );

	virtual void	SetFormatString( const char *fmt );
	virtual const char *GetFormatString( void );
	// If using format string
	virtual bool	CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring );
	virtual void	ComputeTitle( void );

	virtual void	SetAutoComplete( float elapsed_time );

	// Scheme settings
	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);

	// Helper
	virtual const char *GetKeyNameForBinding( const char *binding );

	// Implement ITFHintItem
	virtual void	ParseItem( KeyValues *pKeyValues );
	virtual bool	GetCompleted( void );
	virtual void	SetActive( bool active );
	virtual bool	GetActive( void );
	virtual void	Think( void );
	virtual int		GetHeight( void );
	virtual void	SetPosition( int x, int y );
	virtual void	DeleteThis( void );
	virtual void	SetItemNumber( int index );
	virtual void	SetVisible( bool visible );
	virtual void	SetHintTarget( vgui::Panel *panel );
	virtual bool	ShouldRenderPanelEffects( void );
	virtual void	SetKeyValue( const char *key, const char *value );
protected:
	enum
	{
		MAX_TEXT_LENGTH = 256,
	};

	// Has the hint item been completed
	bool			m_bCompleted;
	// Is the hint item active
	bool			m_bActive;
	// Text of hint
	vgui::Label		*m_pLabel;
	// Depends on type of hint
	vgui::Panel		*m_pObject;
	// Time the hint was activated
	float			m_flActivateTime;

	//	vgui::Label		*m_pIndex;
	// Index of hint
	//int				m_nIndex;

	bool			m_bUseFormatString;
	char			m_szFormatString[ MAX_TEXT_LENGTH ];

	bool			m_bAutoComplete;
	float			m_flAutoCompleteTime;

	vgui::HFont		m_hSmallFont;
	vgui::HFont		m_hMarlettFont;
};

CHintItemBase *CreateHintItem( vgui::Panel *parent, const char *name );

#endif // HINTITEMBASE_H
