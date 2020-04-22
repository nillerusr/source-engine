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

#include "toolutils/dmerenderable.h"
#include "datamodel/dmelement.h"
#include "toolframework/itoolentity.h"
#include "materialsystem/MaterialSystemUtil.h"


//-----------------------------------------------------------------------------
// Represents an editable entity; draws its helpers
//-----------------------------------------------------------------------------
class CDmeVMFEntity : public CDmeVisibilityControl< CDmeRenderable< CDmElement > >
{
	DEFINE_ELEMENT( CDmeVMFEntity, CDmeVisibilityControl< CDmeRenderable< CDmElement > > );

public:
	// Inherited from CDmElement
	virtual	void	OnAttributeChanged( CDmAttribute *pAttribute );

public:
	// Inherited from DmeRenderable
	virtual const Vector &GetRenderOrigin( void );
	virtual const QAngle &GetRenderAngles( void );
	virtual int		DrawModel( int flags );
	virtual void	GetRenderBounds( Vector& mins, Vector& maxs );

public:
	int GetEntityId() const;

	const char *GetClassName() const;
	const char *GetTargetName() const;

	bool IsPlaceholder() const;

	// Entity Key iteration
	CDmAttribute *FirstEntityKey();
	CDmAttribute *NextEntityKey( CDmAttribute *pEntityKey );

	// Attach/detach from an engine entity with the same editor index
	void AttachToEngineEntity( bool bAttach );

private:
	bool IsEntityKey( CDmAttribute *pEntityKey );

	CDmaVar<Vector> m_vecLocalOrigin;
	CDmaVar<QAngle> m_vecLocalAngles;

	CDmaString m_ClassName;
	CDmaString m_TargetName;
	CDmaVar<bool> m_bIsPlaceholder;

	// The entity it's connected to in the engine
	HTOOLHANDLE	m_hEngineEntity;

	CMaterialReference m_Wireframe;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const char *CDmeVMFEntity::GetClassName() const
{
	return m_ClassName;
}

inline const char *CDmeVMFEntity::GetTargetName() const
{
	return m_TargetName;
}

inline bool CDmeVMFEntity::IsPlaceholder() const
{
	return m_bIsPlaceholder;
}


#endif // DMEVMFENTITY_H
