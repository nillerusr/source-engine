//========= Copyright Valve Corporation, All rights reserved. ============//
#include "HapticControlBox.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include "cvarslider.h"
#include "mathlib/vmatrix.h"
#include <vgui/ISurface.h>
#include "KeyValues.h"
#include <vgui/ISystem.h>
ControlBoxVisual::ControlBoxVisual(vgui::Panel *parent,const char *panelName, CCvarSlider *n, CCvarSlider *r, CCvarSlider *u, CCvarSlider *f, CCvarSlider *l, CCvarSlider *d) :
BaseClass(parent,panelName)
{
	m_iMouseOver = -1;
	m_flTime=0;

	//disable mouse input.
	SetMouseInputEnabled(false);

	SlideValues = new CCvarSliderCube(n, r, u, f, l, d);

	SlideValues->Down->AddActionSignalTarget(GetVPanel());
	SlideValues->Far->AddActionSignalTarget(GetVPanel());
	SlideValues->Up->AddActionSignalTarget(GetVPanel());
	SlideValues->Near->AddActionSignalTarget(GetVPanel());
	SlideValues->Left->AddActionSignalTarget(GetVPanel());
	SlideValues->Right->AddActionSignalTarget(GetVPanel());
	SetWide(64);
	SetTall(64);
}

void ControlBoxVisual::OnSlideEnter(KeyValues*data)
{
	vgui::VPANEL fromPanel = data->GetInt("VPANEL");
	if(fromPanel == SlideValues->Up->GetVPanel())
		m_iMouseOver = HUI_BOX_UP;
	else if(fromPanel == SlideValues->Down->GetVPanel())
		m_iMouseOver = HUI_BOX_DOWN;
	else if(fromPanel == SlideValues->Right->GetVPanel())
		m_iMouseOver = HUI_BOX_RIGHT;
	else if(fromPanel == SlideValues->Left->GetVPanel())
		m_iMouseOver = HUI_BOX_LEFT;
	else if(fromPanel == SlideValues->Far->GetVPanel())
		m_iMouseOver = HUI_BOX_FAR;
	else if(fromPanel == SlideValues->Near->GetVPanel())
		m_iMouseOver = HUI_BOX_NEAR;
}
void ControlBoxVisual::OnSlideExit(KeyValues*data)
{
	//vgui::VPANEL fromPanel = data->GetInt("VPANEL");
	m_iMouseOver=-1;
}
void ControlBoxVisual::Paint()
{
	m_flTime = vgui::system()->GetFrameTime();
	BaseClass::Paint();
	vgui::surface()->DrawSetColor(0,0,0,255);
	DrawCube();
	vgui::surface()->DrawSetColor(255,255,255,255);

	//first draw the cube once.
	DrawCube(	SlideValues->Near->GetSliderValue()*-1,
		SlideValues->Right->GetSliderValue()*-1,
		SlideValues->Up->GetSliderValue()*-1,
		SlideValues->Far->GetSliderValue(),
		SlideValues->Left->GetSliderValue(),
		SlideValues->Down->GetSliderValue());
	//then check if we have something selected
	if(m_iMouseOver!=-1)
	{
		vgui::surface()->DrawSetColor(255,0,0,255);
		// if we do, draw a special cube.
		DrawCube(	SlideValues->Near->GetSliderValue()*-1,
			SlideValues->Right->GetSliderValue()*-1,
			SlideValues->Up->GetSliderValue()*-1,
			SlideValues->Far->GetSliderValue(),
			SlideValues->Left->GetSliderValue(),
			SlideValues->Down->GetSliderValue(),
			m_iMouseOver);


	}
}

void ControlBoxVisual::DrawCube(float n, float r, float u, float f, float l, float d, int specialside)
{


	l*=-1;
	r*=-1;//flip



	Vector right[4];
	//right side
	right[0]= Vector(f,r,d);
	right[1]= Vector(f,r,u);
	right[2]= Vector(n,r,u);
	right[3]= Vector(n,r,d);

	Vector left[4];
	//left side
	left[0]= Vector(f,l,d);
	left[1]= Vector(f,l,u);
	left[2]= Vector(n,l,u);
	left[3]= Vector(n,l,d);
	//yikes, this is kind of alot of typing.
	switch(specialside)
	{
	case HUI_BOX_UP:
		left[0] = left[1];
		left[3] = left[2];
		left[1] =Vector(1,-1,-1);
		left[2] =Vector(-1,-1,-1);
		right[0]= right[1];
		right[3]= right[2];
		right[1]=Vector(1,1,-1);
		right[2]=Vector(-1,1,-1);
		break;
	case HUI_BOX_DOWN:
		left[1] = left[0];
		left[2] = left[3];
		left[0] =Vector(1,-1,1);
		left[3] =Vector(-1,-1,1);
		right[1] = right[0];
		right[2] = right[3];
		right[0] =Vector(1,1,1);
		right[3] =Vector(-1,1,1);
		break;
	case HUI_BOX_LEFT:
		right[0] = left[0];
		right[1] = left[1];
		right[2] = left[2];
		right[3] = left[3];
		left[0] =Vector(1,-1,1);
		left[1] =Vector(1,-1,-1);
		left[2] =Vector(-1,-1,-1);
		left[3] =Vector(-1,-1,1);
		break;
	case HUI_BOX_RIGHT:
		left[0] = right[0];
		left[1] = right[1];
		left[2] = right[2];
		left[3] = right[3];
		right[0] =Vector(1,1,1);
		right[1] =Vector(1,1,-1);
		right[2] =Vector(-1,1,-1);
		right[3] =Vector(-1,1,1);
		break;
	case HUI_BOX_FAR:
		left[3] = left[0];
		left[2] = left[1];
		left[0] =Vector(1,-1,1);
		left[1] =Vector(1,-1,-1);
		right[3] = right[0];
		right[2] = right[1];
		right[0] =Vector(1,1,1);
		right[1] =Vector(1,1,-1);
		break;
	case HUI_BOX_NEAR:
		left[0] = left[3];
		left[1] = left[2];
		left[3] =Vector(-1,-1,1);
		left[2] =Vector(-1,-1,-1);
		right[0] = right[3];
		right[1] = right[2];
		right[3] =Vector(-1,1,1);
		right[2] =Vector(-1,1,-1);
		break;
	default:
		break;
	}

	Vector pos = Vector(0,0.25 + sin(m_flTime),0.5);
	VMatrix Project = SetupMatrixProjection(pos,VPlane(Vector(1,0,0),-3));
	Vector vdrawsize = Vector(1,(float)GetWide()/10.0f,(float)GetTall()/10.0f);
	for(int i=0;i!=4;i++)
	{
		right[i] = Project.VMul3x3(right[i])*vdrawsize;
		left[i] = Project.VMul3x3(left[i])*vdrawsize;
	}
	vdrawsize *=5;
	for(int i = 0;i!=4;i++)
	{
		int next = i+1;
		if(next>3)
			next = 0;
		vgui::surface()->DrawLine(vdrawsize.y+left[i].y,vdrawsize.z+left[i].z,vdrawsize.y+left[next].y,vdrawsize.z+left[next].z);
		vgui::surface()->DrawLine(vdrawsize.y+right[i].y,vdrawsize.z+right[i].z,vdrawsize.y+right[next].y,vdrawsize.z+right[next].z);
		vgui::surface()->DrawLine(vdrawsize.y+left[i].y,vdrawsize.z+left[i].z,vdrawsize.y+right[i].y,vdrawsize.z+right[i].z);
	}
}