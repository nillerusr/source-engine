//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core implementation of vgui
//
// $NoKeywords: $
//=============================================================================//

#ifndef SCALABLE_IMAGE_BORDER_H
#define SCALABLE_IMAGE_BORDER_H

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
class ScalableImageBorder : public vgui::IBorder
{
public:
	ScalableImageBorder();
	~ScalableImageBorder();

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
	ScalableImageBorder(ScalableImageBorder&);

	char *_name;

	backgroundtype_e m_eBackgroundType;

	friend class VPanel;

	int m_iSrcCornerHeight;	// in pixels, how tall is the corner inside the image
	int m_iSrcCornerWidth; // same for width
	int m_iCornerHeight;	// output size of the corner height in pixels
	int m_iCornerWidth;		// same for width

	int m_iTextureID;

	float m_flCornerWidthPercent;	// corner width as percentage of image width
	float m_flCornerHeightPercent;	// same for height

	char *m_pszImageName;
	bool m_bPaintFirst;

	Color m_Color;
};

#endif // SCALABLE_IMAGE_BORDER_H
