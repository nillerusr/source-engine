//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_NUMERIC_H
#define HUD_NUMERIC_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

class CHudNumeric : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudNumeric, vgui::Panel );

public:

	CHudNumeric( const char *pElementName, const char *panelName );

	void		SetDrawLabel( bool draw ) { m_bDrawLabel = draw; }
	void		SetDoPulses( bool dopulses ) { m_bSendPulses = dopulses; }
	void		ForcePulse( void );

	void		SetRotaryEffect( int rotary ) { m_nRotaryEffect = rotary; }
	void		SetRotaryTimeMax( float maxTime ) { m_flRotaryTimeMax = maxTime; }
	void		SetRotaryCharsPerSecond( float cps ) { m_flDesiredCharactersPerSecond = cps; }

	// vgui::Panel overrides.
	virtual void Paint( void );
	virtual void PaintBackground( void );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	virtual const char *GetLabelText() = 0;
	virtual const char *GetPulseEvent( bool increment ) = 0;
	virtual bool		GetValue( char *val, int maxlen ) = 0;

	virtual Color		GetColor();
	virtual Color		GetBoxColor();

	static void			BuildPrintablesList( void );
	static int			FindPrintableIndex( int ch );

protected:
	enum
	{
		ROTARY_EFFECT_NONE = 0,
		ROTARY_EFFECT_VERTICAL_ALARM,
		ROTARY_EFFECT_HORIZONTAL_ALARM,
		ROTARY_EFFECT_SPEEDOMETER
	};

	enum
	{
		MAX_VALUE_LENGTH = 128,
	};

	void		PaintValue( const char *value, int textlen, int wide, int tall, Color& clr );
	void		PaintString( const char *text, int textlen, vgui::HFont& font, int x, int y );
	void		PaintStringNormal( const char *text, int textlen, vgui::HFont& font, int x, int y );

	void		PaintStringRotary( float t, const char *text, int textlen, vgui::HFont& font, int x, int y );

	void		GetRotatedChar( float frac, char startchar, char endchar, 
					char& prevchar, char& nextchar, float& subfrac );
	void		PaintRotatedCharacter( int x, int y, vgui::HFont& font, int prevchar, int nextchar, float frac );
	void		PaintRotatedCharacterHoriz( int x, int y, vgui::HFont& font, int prevchar, int nextchar, float frac );
	void		PaintRotatedCharacterSpeedomter( int x, int y, vgui::HFont& font, int prevchar, int nextchar, float frac );

	bool		IsRotating( void ) const;

	float		MaxCharacterDiff( const char *prev, const char *next );
	void		DrawCharacterBackground( const char *text, int textlen, vgui::HFont& font, int x, int y );
	void		DrawCharacterForeground( const char *text, int textlen, vgui::HFont& font, int x, int y );

	int			ComputePixelsRequired( vgui::HFont& font, const char *text, int textlen );

	int			m_nTextLen;
	char		m_szPreviousValue[ MAX_VALUE_LENGTH ];
	char		m_szLatchedValue[ MAX_VALUE_LENGTH ];

	bool		m_bDrawLabel;
	bool		m_bSendPulses;
	bool		m_bPulseForced;

	float		m_flRotaryTime;
	float		m_flRotaryStartTime;
	float		m_flActualCharactersPerSecond;

	static CUtlRBTree< int, int > m_Printables;
	static bool			s_bPrintablesBuilt;

	CPanelAnimationVar( int, m_nRotaryEffect, "Rotary", "0" );
	CPanelAnimationVar( int, m_nRotaryMaxDelta, "RotaryMaxDelta", "0" );
	CPanelAnimationVar( float, m_flRotaryTimeMax, "RotaryMaxTime", "2.0" );
	CPanelAnimationVar( float, m_flDesiredCharactersPerSecond, "RotarySpeed", "7.0" );

	CPanelAnimationVar( bool, m_bDrawCharacterBackground, "DrawCharacterBackground", "false" );
	CPanelAnimationVar( bool, m_bDrawCharacterForeground, "DrawCharacterForeground", "false" );
	CPanelAnimationVar( bool, m_bDrawCharacterBackgroundBorder, "DrawCharacterBackgroundBorder", "false" );

	CPanelAnimationVar( float, m_flBlur, "Blur", "0" );
	CPanelAnimationVar( float, m_flAlphaOverride, "Alpha", "255" );

	CPanelAnimationVar( Color, m_TextColor, "TextColor", "NumericText" );
	CPanelAnimationVar( Color, m_TextColorWarning, "TextColorWarning", "NumericTextWarning" );
	CPanelAnimationVar( Color, m_TextColorCritical, "TextColorCritical", "NumericTextCritical" );

	CPanelAnimationVar( Color, m_BoxColor, "BoxColor", "NumericBox" );
	CPanelAnimationVar( Color, m_BoxColorWarning, "BoxColorWarning", "NumericBoxWarning" );
	CPanelAnimationVar( Color, m_BoxColorCritical, "BoxColorCritical", "NumericBoxCritical" );

	CPanelAnimationVar( Color, m_CharBg, "CharBackground", "NumericCharBg" );
	CPanelAnimationVar( Color, m_CharBgBorder, "CharBackgroundBorder", "NumericCharBgBorder" );
	CPanelAnimationVar( Color, m_CharFg, "CharForeground", "NumericCharFg" );
	
	CPanelAnimationVar( vgui::HFont, m_hLabelFont, "LabelFont", "HudNumbersLabelFont" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudNumbersSmall" );
	CPanelAnimationVar( vgui::HFont, m_hTextFontPulsing, "TextFontPulsing", "HudNumbersSmallGlow" );

	CPanelAnimationVarAliasType( float, label_ypos, "label_ypos", "2", "proportional_float" );
	CPanelAnimationVarAliasType( float, label_xpos_right, "label_xpos_right", "5", "proportional_float" );
	CPanelAnimationVarAliasType( float, value_ypos, "value_ypos", "12", "proportional_float" );
	CPanelAnimationVarAliasType( float, value_xpos_right, "value_xpos_right", "5", "proportional_float" );

	CPanelAnimationVar( bool, m_bUseIcon, "UseIcon", "false" );
	CPanelAnimationVarAliasType( float, m_flIconWidth, "IconWidth", "60", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconHeight, "IconHeight", "30", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconXPos, "IconXPos", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconYPos, "IconYPos", "4", "proportional_float" );
	CPanelAnimationVar( CHudTextureHandle, m_hIcon, "IconTexture", "" );
	CPanelAnimationVar( Color, m_IconColor, "IconColor", "NumericText" );
};

#endif // HUD_NUMERIC_H
