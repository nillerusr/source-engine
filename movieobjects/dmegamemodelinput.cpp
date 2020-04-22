//========= Copyright Valve Corporation, All rights reserved. ============//
//
// game model input - gets its values from an MDL within the game
//
//=============================================================================

#include "movieobjects/dmegamemodelinput.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameModelInput, CDmeGameModelInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGameModelInput::OnConstruction()
{
	m_skin.Init( this, "skin" );
	m_body.Init( this, "body" );
	m_sequence.Init( this, "sequence" );
	m_visible.Init( this, "visible" );
	m_flags.Init( this, "flags" );
	m_flexWeights.Init( this, "flexWeights" );
	m_viewTarget.Init( this, "viewTarget" );
	m_bonePositions.Init( this, "bonePositions" );
	m_boneRotations.Init( this, "boneRotations" );
	m_position.Init( this, "position" );
	m_rotation.Init( this, "rotation" );
}

void CDmeGameModelInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Operator methods
//-----------------------------------------------------------------------------
bool CDmeGameModelInput::IsDirty()
{
	return true; // TODO - keep some bit of state that remembers when its changed
}

void CDmeGameModelInput::Operate()
{
}

void CDmeGameModelInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_skin.GetAttribute() );
	attrs.AddToTail( m_body.GetAttribute() );
	attrs.AddToTail( m_sequence.GetAttribute() );
	attrs.AddToTail( m_visible.GetAttribute() );
	attrs.AddToTail( m_flags.GetAttribute() );
	attrs.AddToTail( m_flexWeights.GetAttribute() );
	attrs.AddToTail( m_viewTarget.GetAttribute() );
	attrs.AddToTail( m_bonePositions.GetAttribute() );
	attrs.AddToTail( m_boneRotations.GetAttribute() );
	attrs.AddToTail( m_position.GetAttribute() );
	attrs.AddToTail( m_rotation.GetAttribute() );
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
void CDmeGameModelInput::SetFlags( int nFlags )
{
	m_flags = nFlags;
}

	
//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
void CDmeGameModelInput::AddBone( const Vector& pos, const Quaternion& rot )
{
	m_bonePositions.AddToTail( pos );
	m_boneRotations.AddToTail( rot );
}

void CDmeGameModelInput::SetBone( uint index, const Vector& pos, const Quaternion& rot )
{
	m_bonePositions.Set( index, pos );
	m_boneRotations.Set( index, rot );
}

void CDmeGameModelInput::SetRootBone( const Vector& pos, const Quaternion& rot )
{
	m_position.Set( pos );
	m_rotation.Set( rot );
}

uint CDmeGameModelInput::NumBones() const
{
	Assert( m_bonePositions.Count() == m_boneRotations.Count() );
	return m_bonePositions.Count();
}

void CDmeGameModelInput::SetFlexWeights( uint nFlexWeights, const float* flexWeights )
{
	m_flexWeights.CopyArray( flexWeights, nFlexWeights );
}

uint CDmeGameModelInput::NumFlexWeights() const
{
	return m_flexWeights.Count();
}

const Vector& CDmeGameModelInput::GetViewTarget() const
{
	return m_viewTarget;
}

void CDmeGameModelInput::SetViewTarget( const Vector &viewTarget )
{
	m_viewTarget = viewTarget;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameSpriteInput, CDmeGameSpriteInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGameSpriteInput::OnConstruction()
{
	m_visible    .Init( this, "visible" );
	m_frame      .Init( this, "frame" );
	m_rendermode .Init( this, "rendermode" );
	m_renderfx   .Init( this, "renderfx" );
	m_renderscale.Init( this, "renderscale" );
	m_proxyRadius.Init( this, "proxyRadius" );
	m_position   .Init( this, "position" );
	m_rotation   .Init( this, "rotation" );
	m_color      .Init( this, "color" );
}

void CDmeGameSpriteInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Operator methods
//-----------------------------------------------------------------------------
bool CDmeGameSpriteInput::IsDirty()
{
	return true; // TODO - keep some bit of state that remembers when its changed
}

void CDmeGameSpriteInput::Operate()
{
}

void CDmeGameSpriteInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_visible.GetAttribute() );
	attrs.AddToTail( m_frame.GetAttribute() );
	attrs.AddToTail( m_rendermode.GetAttribute() );
	attrs.AddToTail( m_renderfx.GetAttribute() );
	attrs.AddToTail( m_renderscale.GetAttribute() );
	attrs.AddToTail( m_proxyRadius.GetAttribute() );
	attrs.AddToTail( m_position.GetAttribute() );
	attrs.AddToTail( m_rotation.GetAttribute() );
	attrs.AddToTail( m_color.GetAttribute() );
}

//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------

void CDmeGameSpriteInput::SetState( bool bVisible, float nFrame, int nRenderMode, int nRenderFX, float flRenderScale, float flProxyRadius,
								   const Vector &pos, const Quaternion &rot, const Color &color )
{
	m_visible = bVisible;
	m_frame = nFrame;
	m_rendermode = nRenderMode;
	m_renderfx = nRenderFX;
	m_renderscale = flRenderScale;
	m_proxyRadius = flProxyRadius;
	m_position = pos;
	m_rotation = rot;
	m_color = color;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameCameraInput, CDmeGameCameraInput );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGameCameraInput::OnConstruction()
{
	m_position.Init( this, "position" );
	m_rotation.Init( this, "rotation" );
	m_fov.Init( this, "fov" );
}

void CDmeGameCameraInput::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Operator methods
//-----------------------------------------------------------------------------
bool CDmeGameCameraInput::IsDirty()
{
	return true; // TODO - keep some bit of state that remembers when its changed
}

void CDmeGameCameraInput::Operate()
{
}

void CDmeGameCameraInput::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_position.GetAttribute() );
	attrs.AddToTail( m_rotation.GetAttribute() );
	attrs.AddToTail( m_fov.GetAttribute() );
}

//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
void CDmeGameCameraInput::SetPosition( const Vector& pos )
{
	m_position.Set( pos );
}

void CDmeGameCameraInput::SetOrientation( const Quaternion& rot )
{
	m_rotation.Set( rot );
}

void CDmeGameCameraInput::SetFOV( float fov )
{
	m_fov = fov;
}
