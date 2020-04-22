//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODMENUBACKGROUND_H
#define DODMENUBACKGROUND_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <vgui/ISurface.h>
#include "vgui_controls/BitmapImagePanel.h"

using namespace vgui;

static int iTopDims[8] = 
{
	41, 30,
	562, 30,
	599, 67,
	41, 67
};

static int iMainDims[8] =
{
	41, 67, 
	599, 67,
	599, 465,
	41, 465
};

static int iBoxDims[8] =
{
	69, 83, 
	86, 83,
	86, 89,
	69, 89
};

static int iLineDims[6] = 
{
	69, 89,
	558, 89,
	568, 99
};

class CDODMenuBackground : public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( CDODMenuBackground, vgui::EditablePanel );

public:
	CDODMenuBackground( Panel *parent);

	void Init();
	void ApplySchemeSettings( IScheme *pScheme );

	virtual void Paint( void );

private:
	vgui::Vertex_t m_BackgroundTopVerts[4];
	vgui::Vertex_t m_BackgroundMainVerts[4];
	vgui::Vertex_t m_BoxVerts[4];

	int m_LineDims[6];

	int m_iBackgroundTexture;
};

#endif //DODMENUBACKGROUND_H