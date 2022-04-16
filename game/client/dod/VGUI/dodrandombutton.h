//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_RANDOM_BUTTON_H
#define DOD_RANDOM_BUTTON_H

#include "dodmouseoverpanelbutton.h"

// CDODRandomButton - has the lower left corner cut out
// and does mouseover

/*

|''''''''''''''''|
|	 9. Random   |
 \_______________|
 */

template <class T>
class CDODRandomButton : public CDODMouseOverButton<T>
{
private:
	//DECLARE_CLASS_SIMPLE( CDODRandomButton, CDODMouseOverButton );

public:
	CDODRandomButton(vgui::Panel *parent, const char *panelName, T *templatePanel ) :
					CDODMouseOverButton<T>( parent, panelName, templatePanel )
	{
	}

protected:
	virtual void PaintBackground();
	virtual void PaintBorder();
};

//===============================================
// CDODRandomButton - differently shaped button
//===============================================
template <class T>
void CDODRandomButton<T>::PaintBackground()
{
	int wide, tall;
	this->GetSize(wide,tall);

	int inset = tall;

	if ( CDODRandomButton<T>::m_iWhiteTexture < 0 )
	{
		CDODRandomButton<T>::m_iWhiteTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( CDODRandomButton<T>::m_iWhiteTexture, "vgui/white" , true, false);
	}

	surface()->DrawSetColor(this->GetBgColor());
	surface()->DrawSetTexture( CDODRandomButton<T>::m_iWhiteTexture );
	
	Vertex_t verts[4];

	verts[0].Init( Vector2D( 0, 0 ) );
	verts[1].Init( Vector2D( wide-1, 0 ) );
	verts[2].Init( Vector2D( wide-1, tall-1 ) );
	verts[3].Init( Vector2D( inset, tall-1 ) );

	surface()->DrawTexturedPolygon(4, verts);
}

template <class T>
void CDODRandomButton<T>::PaintBorder()
{
	int wide, tall;
	this->GetSize(wide,tall);

	int inset = tall;

	surface()->DrawSetColor(this->GetFgColor());

	// top
	surface()->DrawLine( 0, 1, wide-1, 1 );	

	// left
	surface()->DrawLine( 1, 1, inset-1, tall-1 );

	// bottom
	surface()->DrawLine( inset-1, tall-1, wide-1, tall-1 );
	
	// right
	surface()->DrawLine( wide-1, 0, wide-1, tall-1 );
}

#endif	//DOD_RANDOM_BUTTON_H