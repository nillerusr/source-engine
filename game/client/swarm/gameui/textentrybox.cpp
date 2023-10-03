//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#include <vgui/KeyCode.h>

#include "CvarTextEntry.h"
#include "TextEntryBox.h"
#include <vgui_controls/TextEntry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

using namespace vgui;

//--------------------------------------------------------------------------------------------------------------
CTextEntryBox::CTextEntryBox(const char *title, const char *queryText, const char *entryText, bool isCvar, vgui::Panel *parent) : QueryBox(title, queryText,parent)
{
	if (isCvar)
	{
		m_pEntry = m_pCvarEntry = new CCvarTextEntry( this, "TextEntry", entryText );
	}
	else
	{
		m_pEntry = new TextEntry( this, "TextEntry" );
		m_pCvarEntry = NULL;
	}
	m_pEntry->SetTabPosition(3);
	m_pEntry->RequestFocus();
	m_pEntry->GotoTextEnd();
}

//--------------------------------------------------------------------------------------------------------------
CTextEntryBox::~CTextEntryBox()
{
	delete m_pEntry;
}

//--------------------------------------------------------------------------------------------------------------
void CTextEntryBox::ShowWindow(Frame *pFrameOver)
{
	BaseClass::ShowWindow( pFrameOver );

	m_pEntry->RequestFocus();

	InvalidateLayout();
}

//--------------------------------------------------------------------------------------------------------------
void CTextEntryBox::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	wide += x;
	tall += y;

	const int borderW = 10;

	int labelW, labelH;
	int entryW, entryH;
	m_pMessageLabel->GetSize( labelW, labelH );

	entryW = max(120, wide - borderW - borderW - borderW - labelW);
	entryH = max(24, labelH);
	m_pEntry->SetSize( entryW, entryH );

	int boxWidth, boxTall;
	GetSize(boxWidth, boxTall);
	if (boxWidth < labelW + entryW + borderW*3)
		SetSize( labelW + entryW + borderW*3, boxTall );

	m_pMessageLabel->GetPos( x, y );
	m_pMessageLabel->SetPos( borderW, y - (entryH - labelH)/2 );

	m_pEntry->SetPos( borderW + m_pMessageLabel->GetWide() + borderW, y - (entryH - labelH) );
}

//--------------------------------------------------------------------------------------------------------------
void CTextEntryBox::OnCommand(const char *command)
{
	if (!stricmp(command, "Ok"))
	{
		if (m_pCvarEntry)
		{
			m_pCvarEntry->ApplyChanges( true );
		}
	}

	BaseClass::OnCommand(command);
	
}

//--------------------------------------------------------------------------------------------------------------
void CTextEntryBox::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		OnCommand("Cancel");
	}
	if (code == KEY_ENTER)
	{
		OnCommand("Ok");
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

