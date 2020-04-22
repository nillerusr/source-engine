//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base decorator class to make a DME renderable 
//
//===========================================================================//

#ifndef DMERENDERABLE_H
#define DMERENDERABLE_H
#ifdef _WIN32
#pragma once
#endif

#include "iclientunknown.h"
#include "iclientrenderable.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "mathlib/mathlib.h"
#include "basehandle.h"
#include "toolutils/enginetools_int.h"
#include "engine/iclientleafsystem.h"
#include "datamodel/dmelementfactoryhelper.h"


//-----------------------------------------------------------------------------
// Deals with the base implementation for turning a Dme into a renderable
//-----------------------------------------------------------------------------
template < class T >
class CDmeRenderable : public T, public IClientUnknown, public IClientRenderable
{
	DEFINE_UNINSTANCEABLE_ELEMENT( CDmeRenderable, T );

protected:
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

// IClientUnknown implementation.
public:
	virtual void SetRefEHandle( const CBaseHandle &handle );
	virtual const CBaseHandle& GetRefEHandle() const;
	virtual IClientUnknown*		GetIClientUnknown()		{ return this; }
	virtual ICollideable*		GetCollideable()		{ return 0; }
	virtual IClientRenderable*	GetClientRenderable()	{ return this; }
	virtual IClientNetworkable*	GetClientNetworkable()	{ return 0; }
	virtual IClientEntity*		GetIClientEntity()		{ return 0; }
	virtual C_BaseEntity*		GetBaseEntity()			{ return 0; }
	virtual IClientThinkable*	GetClientThinkable()	{ return 0; }
//	virtual const Vector &		GetRenderOrigin( void )	{ return vec3_origin; }
//	virtual const QAngle &		GetRenderAngles( void ) { return vec3_angle; }
	virtual bool				ShouldDraw( void )		{ return false; }
	virtual bool				IsTransparent( void )	{ return false; } 
	virtual bool				IsTwoPass( void )		{ return false; } 
	virtual void				OnThreadedDrawSetup()	{}
	virtual bool				UsesPowerOfTwoFrameBufferTexture() { return false; }
	virtual bool				UsesFullFrameBufferTexture() { return false; }
	virtual ClientShadowHandle_t	GetShadowHandle() const;
	virtual ClientRenderHandle_t&	RenderHandle();
	virtual int					GetBody()				{ return 0; }
	virtual int					GetSkin()				{ return 0; }
	virtual const model_t*		GetModel( ) const		{ return NULL; }
//	virtual int					DrawModel( int flags );
	virtual void				ComputeFxBlend( )		{ return; }
	virtual int					GetFxBlend( )			{ return 255; }
	virtual bool				LODTest()				{ return true; }
	virtual bool				SetupBones( matrix3x4_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime ) { return true; }
	virtual void				SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )	{}
	virtual bool				UsesFlexDelayedWeights() { return false; }
	virtual void				DoAnimationEvents( void ) {}
	virtual IPVSNotify*			GetPVSNotifyInterface() { return NULL; }
	virtual void				GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs );
	virtual void				GetColorModulation( float* color );
//	virtual void				GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool				ShouldReceiveProjectedTextures( int flags ) { return false; }
	virtual bool				GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const { return false; }
	virtual bool				GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const	{ return false; }
	virtual void				GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType );
	virtual bool				IsShadowDirty( ) { return false; }
	virtual void				MarkShadowDirty( bool bDirty )  {}
	virtual IClientRenderable *GetShadowParent() { return NULL; }
	virtual IClientRenderable *FirstShadowChild(){ return NULL; }
	virtual IClientRenderable *NextShadowPeer()  { return NULL; }
	virtual ShadowType_t ShadowCastType()		 { return SHADOWS_NONE; }
	virtual void CreateModelInstance()			 {}
	virtual ModelInstanceHandle_t GetModelInstance() { return MODEL_INSTANCE_INVALID; }
	virtual const matrix3x4_t &RenderableToWorldTransform();
	virtual int LookupAttachment( const char *pAttachmentName ) { return -1; }
	virtual	bool GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool GetAttachment( int number, matrix3x4_t &matrix );
	virtual float *GetRenderClipPlane() { return NULL; }
	virtual void RecordToolMessage() {}
	virtual bool IgnoresZBuffer( void ) const { return false; }

	// Add/remove to engine from drawing
	void DrawInEngine( bool bDrawInEngine );
	bool IsDrawingInEngine() const;

protected:
	virtual CDmAttribute* GetVisibilityAttribute() { return NULL; }
	virtual CDmAttribute* GetDrawnInEngineAttribute() { return m_bWantsToBeDrawnInEngine.GetAttribute(); }

	Vector m_vecRenderOrigin;
	QAngle m_angRenderAngles;

protected:

	CDmaVar<bool> m_bWantsToBeDrawnInEngine;
	bool m_bIsDrawingInEngine;

	CBaseHandle m_RefEHandle;	// Reference ehandle. Used to generate ehandles off this entity.
	ClientRenderHandle_t m_hRenderHandle;
};


//-----------------------------------------------------------------------------
// Construction, destruction
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::OnConstruction()
{
	m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	m_bWantsToBeDrawnInEngine.InitAndSet( this, "wantsToBeDrawnInEngine", false, FATTRIB_DONTSAVE | FATTRIB_HAS_CALLBACK );
	m_bIsDrawingInEngine = false;
}

template < class T >
void CDmeRenderable<T>::OnDestruction()
{
	if ( m_bIsDrawingInEngine )
	{
		if ( clienttools )
		{
			clienttools->RemoveClientRenderable( this );
		}
		m_bIsDrawingInEngine = false;
	}
}


//-----------------------------------------------------------------------------
// EHandles
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::SetRefEHandle( const CBaseHandle &handle )	
{ 
	m_RefEHandle = handle; 
}

template < class T >
const CBaseHandle& CDmeRenderable<T>::GetRefEHandle() const		
{ 
	return m_RefEHandle; 
}

	
//-----------------------------------------------------------------------------
// Add/remove to engine from drawing
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::DrawInEngine( bool bDrawInEngine )
{
	m_bWantsToBeDrawnInEngine = bDrawInEngine;
}

template < class T >
bool CDmeRenderable<T>::IsDrawingInEngine() const
{
	return m_bIsDrawingInEngine;
}


//-----------------------------------------------------------------------------
// Called when attributes changed
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::OnAttributeChanged( CDmAttribute *pAttribute )
{
	T::OnAttributeChanged( pAttribute );
	CDmAttribute *pVisibilityAttribute = GetVisibilityAttribute();
	if ( pAttribute == pVisibilityAttribute || pAttribute == m_bWantsToBeDrawnInEngine.GetAttribute() )
	{
		bool bIsVisible = pVisibilityAttribute ? pVisibilityAttribute->GetValue<bool>() : true;
		bool bShouldDrawInEngine = m_bWantsToBeDrawnInEngine && bIsVisible;
		if ( m_bIsDrawingInEngine != bShouldDrawInEngine )
		{
			m_bIsDrawingInEngine = bShouldDrawInEngine;
			if ( clienttools )
			{
				if ( m_bIsDrawingInEngine )
				{
					clienttools->AddClientRenderable( this, IsTransparent() ? RENDER_GROUP_TRANSLUCENT_ENTITY : RENDER_GROUP_OPAQUE_ENTITY );
				}
				else
				{
					clienttools->RemoveClientRenderable( this );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Color modulation
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::GetColorModulation( float* color )
{
	Assert(color);
	color[0] = color[1] = color[2] = 1.0f;
}


//-----------------------------------------------------------------------------
// Attachments
//-----------------------------------------------------------------------------
template < class T >
bool CDmeRenderable<T>::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	origin = GetRenderOrigin();
	angles = GetRenderAngles();
	return true;
}

template < class T >
bool CDmeRenderable<T>::GetAttachment( int number, matrix3x4_t &matrix )
{
	MatrixCopy( RenderableToWorldTransform(), matrix );
	return true;
}

	
//-----------------------------------------------------------------------------
// Other methods
//-----------------------------------------------------------------------------
template < class T >
void CDmeRenderable<T>::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	GetRenderBounds( mins, maxs );
}

template < class T >
inline ClientShadowHandle_t CDmeRenderable<T>::GetShadowHandle() const
{
	return CLIENTSHADOW_INVALID_HANDLE;
}

template < class T >
inline ClientRenderHandle_t& CDmeRenderable<T>::RenderHandle()
{
	return m_hRenderHandle;
}

template < class T >
void CDmeRenderable<T>::GetRenderBoundsWorldspace( Vector& absMins, Vector& absMaxs )
{
	Vector mins, maxs;
	GetRenderBounds( mins, maxs );

	// FIXME: Should I just use a sphere here?
	// Another option is to pass the OBB down the tree; makes for a better fit
	// Generate a world-aligned AABB
	const QAngle& angles = GetRenderAngles();
	const Vector& origin = GetRenderOrigin();
	if ( angles == vec3_angle )
	{
		VectorAdd( mins, origin, absMins );
		VectorAdd( maxs, origin, absMaxs );
	}
	else
	{
		matrix3x4_t	boxToWorld;
		AngleMatrix( angles, origin, boxToWorld );
		TransformAABB( boxToWorld, mins, maxs, absMins, absMaxs );
	}
	Assert( absMins.IsValid() && absMaxs.IsValid() );
}

template < class T >
const matrix3x4_t &CDmeRenderable<T>::RenderableToWorldTransform()
{
	static matrix3x4_t mat;
	AngleMatrix( GetRenderAngles(), GetRenderOrigin(), mat );
	return mat;
}


//-----------------------------------------------------------------------------
// Adds a 'visibility' attribute onto renderables that need it
//-----------------------------------------------------------------------------
template < class T >
class CDmeVisibilityControl : public T
{
	DEFINE_UNINSTANCEABLE_ELEMENT( CDmeVisibilityControl, T );

public:
	// Control visibility
	bool IsVisible() const;
	void SetVisible( bool bVisible );

private:
	virtual CDmAttribute* GetVisibilityAttribute() { return m_bIsVisible.GetAttribute(); }

	CDmaVar< bool > m_bIsVisible;
};


//-----------------------------------------------------------------------------
// Construction, destruction
//-----------------------------------------------------------------------------
template < class T >
void CDmeVisibilityControl<T>::OnConstruction()
{
	m_bIsVisible.InitAndSet( this, "visible", true, FATTRIB_HAS_CALLBACK );
}

template < class T >
void CDmeVisibilityControl<T>::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Deal with visibility 
//-----------------------------------------------------------------------------
template < class T >
void CDmeVisibilityControl<T>::SetVisible( bool bVisible )
{
	if ( bVisible != m_bIsVisible )
	{
		m_bIsVisible = bVisible;
	}
}

template < class T >
bool CDmeVisibilityControl<T>::IsVisible() const
{
	return m_bIsVisible;
}



#endif // DMERENDERABLE_H
