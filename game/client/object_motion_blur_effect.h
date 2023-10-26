//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Functionality to render object motion blur
//
//===============================================================================

#ifndef OBJECT_MOTION_BLUR_EFFECT_H
#define OBJECT_MOTION_BLUR_EFFECT_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"

//-----------------------------------------------------------------------------
// Central registry for objects that want motion blur applied to them,
// responsible for tracking and drawing models.
//
// Currently requires C_BaseAnimating objects, could be made to work with
// other object types as long as they can 1) be scaled and 2) be rendered.
//-----------------------------------------------------------------------------
class CObjectMotionBlurManager
{
public:
	CObjectMotionBlurManager() :
	m_nFirstFreeSlot( ObjectMotionBlurDefinition_t::END_OF_FREE_LIST )
	{
	}

	// Registers an entity with the object manager and returns an integer handle which must be unregistered before the object gdies
	int RegisterObject( C_BaseAnimating *pEntity, float flVelocityScale );
	
	// Unregisters a previously registered entity
	void UnregisterObject( int nObjectHandle );

	void SetVelocityScale( int nObjectHandle, float flVelocityScale ) 
	{ 
		Assert( !m_ObjectMotionBlurDefinitions[nObjectHandle].IsUnused() );
		m_ObjectMotionBlurDefinitions[nObjectHandle].m_flVelocityScale = flVelocityScale;
	}
	
	// Iterates through all valid registered objects and calls ObjectMotionBlurDefinition_t::DrawModel() on them
	void DrawObjects();

	int GetDrawableObjectCount();

private:
	struct ObjectMotionBlurDefinition_t
	{
		bool ShouldDraw() const { return m_pEntity && m_pEntity->ShouldDraw() && m_flVelocityScale > 0; }
		bool IsUnused() const { return m_nNextFreeSlot != ObjectMotionBlurDefinition_t::ENTRY_IN_USE; }
		void DrawModel();

		C_BaseAnimating *m_pEntity;
		float m_flVelocityScale;

		// Linked list of free slots
		int m_nNextFreeSlot;

		// Special values for ObjectMotionBlurDefinition_t::m_nNextFreeSlot
		static const int END_OF_FREE_LIST = -1;
		static const int ENTRY_IN_USE = -2;
	};

	CUtlVector< ObjectMotionBlurDefinition_t > m_ObjectMotionBlurDefinitions;
	int m_nFirstFreeSlot;
};

extern CObjectMotionBlurManager g_ObjectMotionBlurManager;

//-----------------------------------------------------------------------------
// A scope-based auto-registration class for object motion blur
//-----------------------------------------------------------------------------
class CMotionBlurObject
{
public:
	CMotionBlurObject( C_BaseAnimating *pEntity, float flVelocityScale = 0.2f )
	{
		m_nObjectHandle = g_ObjectMotionBlurManager.RegisterObject( pEntity, flVelocityScale );
	}

	~CMotionBlurObject()
	{
		g_ObjectMotionBlurManager.UnregisterObject( m_nObjectHandle );
	}

	void SetVelocityScale( float flVelocityScale )
	{
		g_ObjectMotionBlurManager.SetVelocityScale( m_nObjectHandle, flVelocityScale );
	}

private:
	int m_nObjectHandle;

	// Assignment & copy-construction disallowed
	CMotionBlurObject( const CMotionBlurObject &other );
	CMotionBlurObject& operator=( const CMotionBlurObject &other );
};

#endif // OBJECT_MOTION_BLUR_EFFECT_H