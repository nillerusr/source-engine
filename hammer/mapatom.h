//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MAPATOM_H
#define MAPATOM_H
#pragma once

#include "hammer_mathlib.h"
#include "tier1/utlvector.h"

class Box3D;
class CRender2D;
class CRender3D;

enum SelectionState_t
{
	SELECT_NONE = 0,			// unselected
	SELECT_NORMAL,				// selected
	SELECT_MORPH,				// selected for vertex manipulation
	SELECT_MULTI_PARTIAL,		// partial selection in a multiselect
	SELECT_MODIFY,				// being modified by a tool
};

//
// Notification codes for NotifyDependents.
//
enum Notify_Dependent_t
{
	Notify_Changed = 0,		// The notifying object has changed.
	Notify_Removed,			// The notifying object is being removed from the world.
	Notify_Undo,
	Notify_Transform,
	Notify_Rebuild,
	Notify_Rebuild_Full,
	Notify_Clipped_Intermediate,
	Notify_Clipped
};

class CMapAtom
{
public:


	int m_nObjectID;
	//-----------------------------------------------------------------------------
	// Purpose: Debugging hook.
	//-----------------------------------------------------------------------------
	virtual void Debug(void) {}

	//-----------------------------------------------------------------------------
	// Purpose: Returns whether this object is selected.
	//-----------------------------------------------------------------------------
	virtual bool IsSelected(void) const
	{
		return(m_eSelectionState != SELECT_NONE);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns the current selection state of this object.
	//-----------------------------------------------------------------------------
	virtual SelectionState_t GetSelectionState(void) const
	{
		return(m_eSelectionState);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Sets the current selection state of this object.
	//-----------------------------------------------------------------------------
	virtual SelectionState_t SetSelectionState(SelectionState_t eSelectionState)
	{
		SelectionState_t ePrevState = m_eSelectionState;
		m_eSelectionState = eSelectionState;
		return ePrevState;
	}
		
	//-----------------------------------------------------------------------------
	// Purpose: Sets the render color of this object.
	//-----------------------------------------------------------------------------
	virtual void SetRenderColor(unsigned char red, unsigned char green, unsigned char blue)
	{
		r = red;
		g = green;
		b = blue;
	}
		
	//-----------------------------------------------------------------------------
	// Purpose: Sets the render color of this object.
	//-----------------------------------------------------------------------------
	virtual void SetRenderColor(color32 rgbColor)
	{
		r = rgbColor.r;
		g = rgbColor.g;
		b = rgbColor.b;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns the render color of this object.
	//-----------------------------------------------------------------------------
	virtual void GetRenderColor(unsigned char &red, unsigned char &green, unsigned char &blue)
	{
		red = r;
		green = g;
		blue = b;
	}
		
	//-----------------------------------------------------------------------------
	// Purpose: Returns the render color of this object.
	//-----------------------------------------------------------------------------
	virtual color32 GetRenderColor(void)
	{
		color32 rgbColor;
		rgbColor.r = r;
		rgbColor.g = g;
		rgbColor.b = b;
		rgbColor.a = 0;

		return rgbColor;
	}
		
	//-----------------------------------------------------------------------------
	// Purpose: Sets this object's parent.
	// Input  : pParent - 
	//-----------------------------------------------------------------------------
	virtual void SetParent(CMapAtom *pParent)
	{
		m_pParent = pParent;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns the parent of this CMapAtom.
	// Output : Parent pointer, NULL if this object has no parent.
	//-----------------------------------------------------------------------------
	virtual CMapAtom *GetParent(void) const
	{
		return(m_pParent);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Preloads any rendering info (textures, etc.). Called once per object
	//			from the renderer's Initialize function.
	// Input  : pRender - Pointer to the 3D rendering interface.
	//			bNewContext - True if the renderer pointed to by pRender is a new
	//				rendering context, false if not.
	//-----------------------------------------------------------------------------
	virtual bool RenderPreload(CRender3D *pRender, bool bNewContext)
	{
		return(true);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Renders this object into the 3D view.
	// Input  : pRender - Pointer to the 3D rendering interface.
	//-----------------------------------------------------------------------------
	virtual void Render3D(CRender3D *pRender)
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: Renders this object into the 3D view.
	// Input  : pRender - Pointer to the 3D rendering interface.
	//-----------------------------------------------------------------------------
	virtual void Render2D(CRender2D *pRender)
	{
	}

	
	virtual void AddShadowingTriangles( CUtlVector<Vector> &tri_list )
	{
		// should add triangles representing the shadows this object would cast
		// in lighting preview mode by adding 3 vector positions per triangle
	}


	//-----------------------------------------------------------------------------
	// Purpose: Returns a coordinate frame to render in
	// Input  : matrix - 
	// Output : returns true if a new matrix is returned, false if the new matrix is bad
	//-----------------------------------------------------------------------------
	virtual bool GetTransformMatrix( VMatrix& matrix )
	{
		// try and get our parents transform matrix
		CMapAtom *p = GetParent();
		if ( p )
		{
			return p->GetTransformMatrix( matrix );
		}

		return false;
	}

	//-----------------------------------------------------------------------------
	// Transformation functions.
	//-----------------------------------------------------------------------------
	void Transform(const VMatrix &matrix)
	{ 
		DoTransform(matrix);
		PostUpdate(Notify_Transform);
	}

	void TransMove(const Vector &Delta)
	{
		VMatrix matrix;
		matrix.Identity();
		matrix.SetTranslation(Delta);

		DoTransform(matrix);
		PostUpdate(Notify_Transform);
	}

	void TransRotate(const Vector &RefPoint, const QAngle &Angles)
	{
		VMatrix matrix;
		QAngle hammerAngle( -Angles.y, Angles.z, Angles.x );
		matrix.SetupMatrixOrgAngles( vec3_origin, hammerAngle );
		Vector vOffset;
		matrix.V3Mul( RefPoint, vOffset );
		vOffset = RefPoint - vOffset;
		matrix.SetTranslation( vOffset );

		DoTransform(matrix);
		PostUpdate(Notify_Transform);
	}

	void TransScale(const Vector &RefPoint, const Vector &Scale)
	{
		VMatrix matrix;
		matrix.Identity();
		matrix = matrix.Scale( Scale );
		Vector vOffset;
		matrix.V3Mul( RefPoint, vOffset );
		vOffset = RefPoint - vOffset;
		matrix.SetTranslation( vOffset );

		DoTransform(matrix);
		PostUpdate(Notify_Transform);
	}

	//-----------------------------------------------------------------------------
	// Must be called after modifying the object for bounds recalculation and
	// dependency updates.
	//-----------------------------------------------------------------------------
	virtual void PostUpdate(Notify_Dependent_t eNotifyType) {}

	//-----------------------------------------------------------------------------
	// Override this for helpers. This indicates whether a helper provides a visual
	// representation for the entity that it belongs to. Entities with no
	// visual elements are given a default box so they can be seen.
	//-----------------------------------------------------------------------------
	virtual bool IsVisualElement(void) { return(false);	}

	//-----------------------------------------------------------------------------
	// Override this to tell the renderer to render you last. This is useful for
	// alpha blended elements such as sprites.
	//-----------------------------------------------------------------------------
	virtual bool ShouldRenderLast(void)
	{
		return(false);
	}
	virtual void SignalChanged(void )								// object has changed
	{
	}

protected:

	static int s_nObjectIDCtr;

	CMapAtom(void)
	{
		m_eSelectionState = SELECT_NONE;
		m_pParent = NULL;
		m_nObjectID = s_nObjectIDCtr++;
	}

	//-----------------------------------------------------------------------------
	// DoTransform functions. Virtual, called by base Transfom functions.
	//-----------------------------------------------------------------------------
	virtual void DoTransform(const VMatrix &matrix) {}
		
	CMapAtom *m_pParent;				// This object's parent.
	SelectionState_t m_eSelectionState;	// The current selection state of this object.

	unsigned char r;					// Red color component.
	unsigned char g;					// Green color component.
	unsigned char b;					// Blue color component.
}; 


#endif // MAPATOM_H
