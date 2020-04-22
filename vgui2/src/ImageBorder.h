//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core implementation of vgui
//
// $NoKeywords: $
//=============================================================================//

#ifndef IMAGE_BORDER_H
#define IMAGE_BORDER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/IBorder.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include <Color.h>

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: Custom border that renders itself with images
//-----------------------------------------------------------------------------
class ImageBorder : public vgui::IBorder
{
public:
	ImageBorder();
	~ImageBorder();

	virtual void Paint(vgui::VPANEL panel);
	virtual void Paint(int x0, int y0, int x1, int y1);
	virtual void Paint(int x0, int y0, int x1, int y1, int breakSide, int breakStart, int breakStop);
	virtual void SetInset(int left, int top, int right, int bottom);
	virtual void GetInset(int &left, int &top, int &right, int &bottom);

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme, KeyValues *inResourceData);

	virtual const char *GetName();
	virtual void SetName(const char *name);
	virtual backgroundtype_e GetBackgroundType();

	virtual bool PaintFirst( void ) { return m_bPaintFirst; }

protected:
	void		 SetImage(const char *imageName);

protected:
	int _inset[4];

private:
	// protected copy constructor to prevent use
	ImageBorder(ImageBorder&);

	char *_name;

	backgroundtype_e m_eBackgroundType;

	friend class VPanel;

	int m_iTextureID;
	bool m_bTiled;

	char *m_pszImageName;
	bool m_bPaintFirst;
};

#endif // IMAGE_BORDER_H
