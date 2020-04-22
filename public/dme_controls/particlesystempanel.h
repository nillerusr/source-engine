//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef DMEPARTICLEPANEL_H
#define DMEPARTICLEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "matsys_controls/PotteryWheelPanel.h"
#include "datamodel/dmattributetypes.h"
#include "particles/particles.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterial;
class CMeshBuilder;
class Vector;
class CParticleCollection;
class CColorPickerButton;
class CDmeParticleSystemDefinition;
class CDmeParticleFunction;
class CControlPointPage;

namespace vgui
{
	class ScrollBar;
	class IScheme;
	class PropertyPage;
	class PropertySheet;
	class Splitter;
	class Label;
	class TextEntry;
}


//-----------------------------------------------------------------------------
// Particle System Viewer Panel
//-----------------------------------------------------------------------------
class CParticleSystemPanel : public CPotteryWheelPanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemPanel, CPotteryWheelPanel );

public:
	// constructor, destructor
	CParticleSystemPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CParticleSystemPanel();

	// Set the particle system to draw
	void SetParticleSystem( CDmeParticleSystemDefinition *pDef );
	void SetDmeElement( CDmeParticleSystemDefinition *pDef );

	CParticleCollection *GetParticleSystem();

	// Indicates that bounds should be drawn
	void RenderBounds( bool bEnable );

	// Indicates that cull sphere should be drawn
	void RenderCullBounds( bool bEnable );

	// Indicates that helpers should be drawn
	void RenderHelpers( bool bEnable );

	// Indicates which helper to draw
	void SetRenderedHelper( CDmeParticleFunction *pOp );

	virtual void OnTick();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// Accessor for control point values
	const Vector& GetControlPointValue( int nControlPoint ) const;
	void SetControlPointValue( int nControlPoint, const Vector &value );

private:
	// Shutdown, startup particle collection
	void StartupParticleCollection();
	void ShutdownParticleCollection();

	// Draw bounds
	void DrawBounds();
	void DrawCullBounds();
 
	// paint it!
	virtual void OnPaint3D();

private:
	bool m_bRenderBounds : 1;
	bool m_bRenderCullBounds : 1;
	bool m_bRenderHelpers : 1;
	bool m_bPerformNameBasedLookup : 1;

	Vector m_pControlPointValue[MAX_PARTICLE_CONTROL_POINTS];

	DmObjectId_t m_RenderHelperId;
	float m_flLastTime;

	// Stores the id or name of the particle system being viewed
	DmObjectId_t m_ParticleSystemId;
	CUtlString m_ParticleSystemName;

	// The particle system to draw
	CParticleCollection *m_pParticleSystem;

	// A texture to use for a lightmap
	CTextureReference m_pLightmapTexture;

	// The default env_cubemap
	CTextureReference m_DefaultEnvCubemap;
};


//-----------------------------------------------------------------------------
// Accessor for control point values
//-----------------------------------------------------------------------------
inline const Vector& CParticleSystemPanel::GetControlPointValue( int nControlPoint ) const
{
	return m_pControlPointValue[nControlPoint];
}

inline void CParticleSystemPanel::SetControlPointValue( int nControlPoint, const Vector &value )
{
	m_pControlPointValue[nControlPoint] = value;
}


//-----------------------------------------------------------------------------
// This panel has a particle system viewer as well as controls
//-----------------------------------------------------------------------------
class CParticleSystemPreviewPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemPreviewPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CParticleSystemPreviewPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CParticleSystemPreviewPanel();

	// Set the material to draw
	void SetParticleSystem( CDmeParticleSystemDefinition *pDef );
	void SetParticleFunction( CDmeParticleFunction *pFunction );
	void SetDmeElement( CDmeParticleSystemDefinition *pDef );
	virtual void OnThink();

private:
	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", params );
	MESSAGE_FUNC_PARAMS( OnBackgroundColorChanged, "ColorPickerPicked", params );
	MESSAGE_FUNC_PARAMS( OnBackgroundColorPreview, "ColorPickerPreview", params );
	MESSAGE_FUNC_PARAMS( OnBackgroundColorCancel, "ColorPickerCancel", params );
	MESSAGE_FUNC( OnParticleSystemReconstructed, "ParticleSystemReconstructed" );

	vgui::Splitter *m_Splitter;
	CParticleSystemPanel *m_pParticleSystemPanel;
	vgui::PropertySheet *m_pControlSheet;
	vgui::PropertyPage *m_pRenderPage;
	CControlPointPage *m_pControlPointPage;

	vgui::CheckButton *m_pRenderCullBounds;
	vgui::CheckButton *m_pRenderBounds;
	vgui::CheckButton *m_pRenderHelpers;
	CColorPickerButton *m_pBackgroundColor;
	vgui::Label *m_pParticleCount;
};



#endif // DMEPARTICLEPANEL_H
	    