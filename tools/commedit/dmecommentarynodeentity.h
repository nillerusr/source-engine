//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Represents an entity in a VMF
//
//=============================================================================

#ifndef DMEVMFENTITY_H
#define DMEVMFENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include "toolutils/dmemdlrenderable.h"
#include "datamodel/dmelement.h"
#include "toolframework/itoolentity.h"
#include "materialsystem/MaterialSystemUtil.h"


//-----------------------------------------------------------------------------
// Represents an editable entity; draws its helpers
//-----------------------------------------------------------------------------
class CDmeCommentaryNodeEntity : public CDmeMdlRenderable<CDmElement>
{
	DEFINE_ELEMENT( CDmeCommentaryNodeEntity, CDmeMdlRenderable<CDmElement> );

public:
	// Inherited from CDmElement
	virtual	void	OnAttributeChanged( CDmAttribute *pAttribute );

public:
	// Inherited from DmeRenderable
	virtual const Vector &GetRenderOrigin( void );
	virtual const QAngle &GetRenderAngles( void );
	virtual int		DrawModel( int flags );
	virtual void	GetRenderBounds( Vector& mins, Vector& maxs );
	virtual bool	IsTransparent( void ); 

public:
	int GetEntityId() const;

	const char *GetClassName() const;
	const char *GetTargetName() const;

	bool IsPlaceholder() const;

	// Entity Key iteration
	CDmAttribute *FirstEntityKey();
	CDmAttribute *NextEntityKey( CDmAttribute *pEntityKey );

	// Attach/detach from an engine entity with the same editor index
	void AttachToEngineEntity( HTOOLHANDLE hToolHandle );

	void SetRenderOrigin( const Vector &vecOrigin );
	void SetRenderAngles( const QAngle &angles );

	void MarkDirty( bool bDirty = true );

private:
	bool IsEntityKey( CDmAttribute *pEntityKey );

	// Draws the helper for the entity
	void DrawSprite( IMaterial *pMaterial );

	CDmaVar<Vector> m_vecLocalOrigin;
	CDmaVar<Vector> m_vecLocalAngles;

	CDmaString m_ClassName;
	CDmaString m_TargetName;
	CDmaVar<bool> m_bIsPlaceholder;

	bool m_bInfoTarget;
	bool m_bIsDirty;

	// The entity it's connected to in the engine
	HTOOLHANDLE	m_hEngineEntity;

	CMaterialReference m_SelectedInfoTarget;
	CMaterialReference m_InfoTargetSprite;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const char *CDmeCommentaryNodeEntity::GetClassName() const
{
	return m_ClassName;
}

inline const char *CDmeCommentaryNodeEntity::GetTargetName() const
{
	return m_TargetName;
}

inline bool CDmeCommentaryNodeEntity::IsPlaceholder() const
{
	return m_bIsPlaceholder;
}

#endif // DMEVMFENTITY_H
