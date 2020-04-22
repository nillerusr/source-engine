//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>
#include <string.h>

#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

#include "VGUI_Border.h"
#include "vgui_internal.h"
#include "VPanel.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Border::Border()
{
	_inset[0]=0;
	_inset[1]=0;
	_inset[2]=0;
	_inset[3]=0;
	_name = NULL;
	m_eBackgroundType = IBorder::BACKGROUND_FILLED;

	memset(_sides, 0, sizeof(_sides));
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Border::~Border()
{
	delete [] _name;

	for (int i = 0; i < 4; i++)
	{
		delete [] _sides[i].lines;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::SetInset(int left,int top,int right,int bottom)
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::GetInset(int& left,int& top,int& right,int& bottom)
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::Paint(int x, int y, int wide, int tall)
{
	Paint(x, y, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Draws the border with the specified size
//-----------------------------------------------------------------------------
void Border::Paint(int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd)
{
	// iterate through and draw all lines
	// draw left
	int i;
	for (i = 0; i < _sides[SIDE_LEFT].count; i++)
	{
		line_t *line = &(_sides[SIDE_LEFT].lines[i]);
		g_pSurface->DrawSetColor(line->col[0], line->col[1], line->col[2], line->col[3]);

		if (breakSide == SIDE_LEFT)
		{
			// split into two section
			if (breakStart > 0)
			{
				// draw before the break Start
				g_pSurface->DrawFilledRect(x + i, y + line->startOffset, x + i + 1, y + breakStart);
			}

			if (breakEnd < (tall - line->endOffset))
			{
				// draw after break end
				g_pSurface->DrawFilledRect(x + i, y + breakEnd + 1, x + i + 1, tall - line->endOffset);
			}
		}
		else
		{
			g_pSurface->DrawFilledRect(x + i, y + line->startOffset, x + i + 1, tall - line->endOffset);
		}
	}

	// draw top
	for (i = 0; i < _sides[SIDE_TOP].count; i++)
	{
		line_t *line = &(_sides[SIDE_TOP].lines[i]);
		g_pSurface->DrawSetColor(line->col[0], line->col[1], line->col[2], line->col[3]);
		
		if (breakSide == SIDE_TOP)
		{
			// split into two section
			if (breakStart > 0)
			{
				// draw before the break Start
				g_pSurface->DrawFilledRect(x + line->startOffset, y + i, x + breakStart, y + i + 1);
			}

			if (breakEnd < (wide - line->endOffset))
			{
				// draw after break end
				g_pSurface->DrawFilledRect(x + breakEnd + 1, y + i, wide - line->endOffset, y + i + 1);
			}
		}
		else
		{
			g_pSurface->DrawFilledRect(x + line->startOffset, y + i, wide - line->endOffset, y + i + 1);
		}
	}

	// draw right
	for (i = 0; i < _sides[SIDE_RIGHT].count; i++)
	{
		line_t *line = &(_sides[SIDE_RIGHT].lines[i]);
		g_pSurface->DrawSetColor(line->col[0], line->col[1], line->col[2], line->col[3]);
		g_pSurface->DrawFilledRect(wide - (i+1), y + line->startOffset, (wide - (i+1)) + 1, tall - line->endOffset);
	}

	// draw bottom
	for (i = 0; i < _sides[SIDE_BOTTOM].count; i++)
	{
		line_t *line = &(_sides[SIDE_BOTTOM].lines[i]);
		g_pSurface->DrawSetColor(line->col[0], line->col[1], line->col[2], line->col[3]);
		g_pSurface->DrawFilledRect(x + line->startOffset, tall - (i+1), wide - line->endOffset, (tall - (i+1)) + 1);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::Paint(VPANEL panel)
{
	// get panel size
	int wide, tall;
	((VPanel *)panel)->GetSize(wide, tall);
	Paint(0, 0, wide, tall, -1, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::ApplySchemeSettings(IScheme *pScheme, KeyValues *inResourceData)
{
	// load inset information
	const char *insetString = inResourceData->GetString("inset", "0 0 0 0");

	int left, top, right, bottom;
	GetInset(left, top, right, bottom);
	sscanf(insetString, "%d %d %d %d", &left, &top, &right, &bottom);
	SetInset(left, top, right, bottom);

	// get the border information from the scheme
	ParseSideSettings(SIDE_LEFT, inResourceData->FindKey("Left"),pScheme);
	ParseSideSettings(SIDE_TOP, inResourceData->FindKey("Top"),pScheme);
	ParseSideSettings(SIDE_RIGHT, inResourceData->FindKey("Right"),pScheme);
	ParseSideSettings(SIDE_BOTTOM, inResourceData->FindKey("Bottom"),pScheme);

	m_eBackgroundType = (backgroundtype_e)inResourceData->GetInt("backgroundtype");
}

//-----------------------------------------------------------------------------
// Purpose: parses scheme data
//-----------------------------------------------------------------------------
void Border::ParseSideSettings(int side_index, KeyValues *inResourceData, IScheme *pScheme)
{
	if (!inResourceData)
		return;

	// count the numeber of lines in the side
	int count = 0;
	KeyValues *kv;
	for (kv = inResourceData->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		count++;
	}

	// allocate memory
	_sides[side_index].count = count;
	_sides[side_index].lines = new line_t[count];

	// iterate through the keys
	//!! this loads in order, ignoring key names
	int index = 0;
	for (kv = inResourceData->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		line_t *line = &(_sides[side_index].lines[index]);

		// this is the color name, get that from the color table
		const char *col = kv->GetString("color", NULL);
		line->col = pScheme->GetColor(col, Color(0, 0, 0, 0));

		col = kv->GetString("offset", NULL);
		int Start = 0, end = 0;
		if (col)
		{
			sscanf(col, "%d %d", &Start, &end);
		}
		line->startOffset = Start;
		line->endOffset = end;

		index++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *Border::GetName()
{
	if (_name)
		return _name;
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void Border::SetName(const char *name)
{
	if (_name)
	{
		delete [] _name;
	}

	int len = Q_strlen(name) + 1;
	_name = new char[ len ];
	Q_strncpy( _name, name, len );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IBorder::backgroundtype_e Border::GetBackgroundType()
{
	return m_eBackgroundType;
}

