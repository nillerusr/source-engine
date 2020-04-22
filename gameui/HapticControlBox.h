//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef HAPTICCONTROLBOX_H
#define HAPTICCONTROLBOX_H

#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include "cvarslider.h"
class ControlBoxVisual : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(ControlBoxVisual,vgui::Panel);
public:
	ControlBoxVisual(vgui::Panel *parent, const char *panelName, CCvarSlider *near, CCvarSlider *right, CCvarSlider *up, CCvarSlider *far, CCvarSlider *left, CCvarSlider *down);
	virtual void Paint();
	MESSAGE_FUNC_PARAMS(OnSlideEnter, "CursorEnteredSlider", data);
	MESSAGE_FUNC_PARAMS(OnSlideExit, "CursorExitedSlider", data);
protected:
	void DrawCube(float Near=-1, float Right=-1, float Up=-1, float Far=1, float Left=1, float Down=1, int specialside=-1);
	enum eBoxID
	{
		HUI_BOX_UP =0,
		HUI_BOX_RIGHT,
		HUI_BOX_NEAR,
		HUI_BOX_DOWN,
		HUI_BOX_LEFT,
		HUI_BOX_FAR,
		HUI_BOX_SLIDERCOUNT,
	};

	struct CCvarSliderCube
	{
		CCvarSliderCube(CCvarSlider *n,CCvarSlider *r,CCvarSlider *u,CCvarSlider *f,CCvarSlider *l,CCvarSlider *d)
		{
			Near = n;
			Right = r;
			Up = u;
			Far = f;
			Left = l;
			Down = d;
		};
		CCvarSlider *Near;
		CCvarSlider *Right;
		CCvarSlider *Up;
		CCvarSlider *Far;
		CCvarSlider *Left;
		CCvarSlider *Down;
	};

	CCvarSliderCube *SlideValues;// up right near down left far and spingk
	int m_iMouseOver;
	float m_flTime;
};

#endif