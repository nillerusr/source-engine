//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include <convar.h>
#include "cdll_int.h"
#include <ienginevgui.h>
#include "filesystem.h"
#include <vgui/ISystem.h>
#include "career_button.h"

#include <vgui_controls/Label.h>
#include <vgui_controls/URLLabel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/Button.h>

#include "KeyValues.h"

using namespace vgui;

#ifndef _DEBUG
#define PROPORTIONAL_CAREER_FRAMES 1
#endif

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
CCareerButton::CCareerButton(vgui::Panel *parent, const char *buttonName, const char *buttonText, const char *image, bool textFirst) : BaseClass( parent, buttonName, "" )
{
	m_armedBorder = NULL;

	m_textImage = new TextImage(buttonText);
	m_image = scheme()->GetImage(image, true);
	m_textFirst = textFirst;
}

//--------------------------------------------------------------------------------------------------------------
void CCareerButton::SetImage( const char *image )
{
	m_image = scheme()->GetImage(image, true);
}

//--------------------------------------------------------------------------------------------------------------
void CCareerButton::Paint()
{
	int buttonWide, buttonTall;
	GetSize(buttonWide, buttonTall);

	int imageWide, imageTall;
	if ( m_image )
	{
		m_image->GetSize(imageWide, imageTall);
	}
	else
	{
		imageWide = imageTall = 0;
	}

	int textOffset = m_textPad;

	if (m_textFirst)
	{
		if ( m_image )
		{
			m_image->SetPos(buttonWide - imageWide - m_imagePad, (buttonTall - imageTall)/2);
			m_image->Paint();
		}
	}
	else
	{
		if ( m_image )
		{
			m_image->SetPos(m_imagePad, (buttonTall - imageTall)/2);
			m_image->Paint();
		}
		textOffset += imageWide + m_imagePad;
	}

	int textTall, textWide;
	m_textImage->GetSize(textWide, textTall);

	int textSpace = buttonWide - imageWide - m_imagePad - 2*m_textPad;

	if ( IsEnabled() )
	{
		m_textImage->SetColor( m_textNormalColor );
	}
	else
	{
		m_textImage->SetColor( m_textDisabledColor );
	}
	m_textImage->SetPos(textOffset + (textSpace - textWide)/2, (buttonTall - textTall)/2);
	m_textImage->Paint();

	if (HasFocus()  && IsEnabled() )
	{
		int x0, y0, x1, y1;
		x0 = 3, y0 = 3, x1 = buttonWide - 4 , y1 = buttonTall - 2;
		DrawFocusBorder(x0, y0, x1, y1);
	}
}

//--------------------------------------------------------------------------------------------------------------
void CCareerButton::SetArmedBorder(IBorder *border)
{
	m_armedBorder = border;
	InvalidateLayout(false);
}

//--------------------------------------------------------------------------------------------------------------
IBorder* CCareerButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	if ( /*_buttonBorderEnabled &&*/ armed && !depressed && IsEnabled() )
	{
		return m_armedBorder;
	}

	return BaseClass::GetBorder( depressed, armed, selected, keyfocus );
}

//--------------------------------------------------------------------------------------------------------------
void CCareerButton::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetDefaultBorder( pScheme->GetBorder("CareerButtonBorder") );
	SetDepressedBorder( pScheme->GetBorder("CareerButtonDepressedBorder") );
	m_armedBorder = pScheme->GetBorder("CareerButtonArmedBorder");

	m_textNormalColor = pScheme->GetColor( "Label.TextBrightColor", Color(255, 255, 255, 255) );
	m_textDisabledColor = pScheme->GetColor( "Label.DisabledFgColor2", Color(128, 128, 128, 255) );
	m_textImage->SetColor( m_textNormalColor );
	if ( m_image )
	{
		m_image->SetColor( GetFgColor() );
	}

	m_textImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );

	m_textPad = atoi(pScheme->GetResourceString( "CareerButtonTextPad" ));
	m_imagePad = atoi(pScheme->GetResourceString( "CareerButtonImagePad" ));
	if (IsProportional())
	{
		m_textPad = scheme()->GetProportionalScaledValueEx( GetScheme(),m_textPad);
		m_imagePad = scheme()->GetProportionalScaledValueEx( GetScheme(),m_imagePad);
	}

	const int BufLen = 128;
	char buf[BufLen];
	GetText(buf, BufLen);
	m_textImage->SetText(buf);
	m_textImage->ResizeImageToContent();

	int buttonWide, buttonTall;
	GetSize(buttonWide, buttonTall);

	int imageWide, imageTall;
	if ( m_image )
	{
		m_image->GetSize(imageWide, imageTall);
	}
	else
	{
		imageWide = imageTall = 0;
	}

	int textSpace = buttonWide - imageWide - m_imagePad - 2*m_textPad;

	int textWide, textTall;
	m_textImage->GetContentSize(textWide, textTall);
	if (textSpace < textWide)
		m_textImage->SetSize(textSpace, textTall);

	Color bgColor = pScheme->GetColor( "CareerButtonBG", Color(0, 0, 0, 0) );
	SetDefaultColor( bgColor, bgColor );
	SetArmedColor( bgColor, bgColor );
	SetDepressedColor( bgColor, bgColor );
}

//--------------------------------------------------------------------------------------------------------------
