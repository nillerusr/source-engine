//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DMEDAGRENDERPANEL_H
#define DMEDAGRENDERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "matsys_controls/PotteryWheelPanel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;
class CDmeModel;
class CDmeAnimationList;
class CDmeChannelsClip;
class CDmeSourceSkin;
class CDmeSourceAnimation;
class CDmeDCCMakefile;
class CDmeDrawSettings;
class vgui::MenuBar;

namespace vgui
{
	class IScheme;
}


//-----------------------------------------------------------------------------
// Material Viewer Panel
//-----------------------------------------------------------------------------
class CDmeDagRenderPanel : public CPotteryWheelPanel
{
	DECLARE_CLASS_SIMPLE( CDmeDagRenderPanel, CPotteryWheelPanel );

public:
	// constructor, destructor
	CDmeDagRenderPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeDagRenderPanel();

	// Overriden methods of vgui::Panel
	virtual void PerformLayout();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// Sets the current scene + animation list
	void SetDmeElement( CDmeDag *pScene );
	void SetAnimationList( CDmeAnimationList *pAnimationList );
	void SetVertexAnimationList( CDmeAnimationList *pAnimationList );
	void DrawJoints( bool bDrawJoint );
	void DrawJointNames( bool bDrawJointNames );
	void DrawGrid( bool bDrawGrid );

	CDmeDag *GetDmeElement();

	// Other methods which hook into DmePanel
	void SetDmeElement( CDmeSourceSkin *pSkin );
	void SetDmeElement( CDmeSourceAnimation *pAnimation );
	void SetDmeElement( CDmeDCCMakefile *pDCCMakefile );

	// Select animation by name
	void SelectAnimation( const char *pAnimName );
	void SelectVertexAnimation( const char *pAnimName );

private:
	// Select animation by index
	void SelectAnimation( int nIndex );
	void SelectVertexAnimation( int nIndex );

	// paint it!
	void OnPaint3D();
	void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	MESSAGE_FUNC( OnSmoothShade, "SmoothShade" );
	MESSAGE_FUNC( OnFlatShade, "FlatShade" );
	MESSAGE_FUNC( OnWireframe, "Wireframe" );
	MESSAGE_FUNC( OnBoundingBox, "BoundingBox" );
	MESSAGE_FUNC( OnNormals, "Normals" );
	MESSAGE_FUNC( OnWireframeOnShaded, "WireframeOnShaded" );
	MESSAGE_FUNC( OnBackfaceCulling, "BackfaceCulling" );
	MESSAGE_FUNC( OnXRay, "XRay" );
	MESSAGE_FUNC( OnGrayShade, "GrayShade" );
	MESSAGE_FUNC( OnFrame, "Frame" );

	// Draw joint names
	void DrawJointNames( CDmeDag *pRoot, CDmeDag *pDag, const matrix3x4_t& parentToWorld );

	// Rebuilds the list of operators
	void RebuildOperatorList();

	// Update Menu Status
	void UpdateMenu();
	CTextureReference m_DefaultEnvCubemap;
	CTextureReference m_DefaultHDREnvCubemap;
	vgui::HFont m_hFont;

	bool m_bDrawJointNames : 1;
	bool m_bDrawJoints : 1;
	bool m_bDrawGrid : 1;

	CDmeHandle< CDmeAnimationList > m_hAnimationList;
	CDmeHandle< CDmeAnimationList > m_hVertexAnimationList;
	CDmeHandle< CDmeChannelsClip > m_hCurrentAnimation;
	CDmeHandle< CDmeChannelsClip > m_hCurrentVertexAnimation;
	CUtlVector< IDmeOperator* > m_operators;
	float m_flStartTime;
	CDmeHandle< CDmeDag > m_hDag;

	CDmeDrawSettings *m_pDrawSettings;
	CDmeHandle< CDmeDrawSettings, true > m_hDrawSettings;

	vgui::MenuBar *m_pMenuBar;

	// Menu item numbers
	vgui::Menu *m_pShadingMenu;
	int m_nMenuSmoothShade;
	int m_nMenuFlatShade;
	int m_nMenuWireframe;
	int m_nMenuBoundingBox;
	int m_nMenuNormals;
	int m_nMenuWireframeOnShaded;
	int m_nMenuBackfaceCulling;
	int m_nMenuXRay;
	int m_nMenuGrayShade;
};



#endif // DMEDAGRENDERPANEL_H