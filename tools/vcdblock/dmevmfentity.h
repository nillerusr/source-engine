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
class CDmeVMFEntity : public CDmeMdlRenderable<CDmElement>
{
	DEFINE_ELEMENT( CDmeVMFEntity, CDmeMdlRenderable<CDmElement> );

public:
	// Inherited from CDmElement
	virtual	void	OnAttributeChanged( CDmAttribute *pAttribute );

public:
	// Inherited from DmeRenderable
	virtual const Vector &GetRenderOrigin( void );
	virtual const QAngle &GetRenderAngles( void );
	virtual int	DrawModel( int flags );
	virtual bool IsTransparent( void );
	virtual void GetRenderBounds( Vector& mins, Vector& maxs );

public:
	int GetEntityId() const;

	// Returns the next available entity id
	static int GetNextEntityId();
	static void SetNextEntityId( int nEntityId );

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
	bool IsDirty( void ) { return m_bIsDirty; };

	void MarkDeleted( bool bDeleted = true );
	bool IsDeleted( void ) { return m_bIsDeleted; };

	bool CopyFromServer( CBaseEntity *pServerEnt );
	bool CopyFromServer( CBaseEntity *pServerEnt, const char *szField );
	bool CopyFromServer( CBaseEntity *pServerEnt, const char *szSrcField, const char *szDstField );
	bool CopyToServer( void );

	bool IsSameOnServer( CBaseEntity *pServerEntity );
	bool CreateOnServer( void );

private:
	bool IsEntityKey( CDmAttribute *pEntityKey );

	// Draws the helper for the entity
	void DrawSprite( IMaterial *pMaterial );
	void DrawDragHelpers( IMaterial *pMaterial );
	void DrawFloorTarget( IMaterial *pMaterial );

	CDmaVar<Vector> m_vecLocalOrigin;
	// CDmAttributeVar<QAngle> m_vecLocalAngles;
	CDmaVar<Vector> m_vecLocalAngles; // something funky with the vmf importer, it asserts when it's a QAngle

	CDmaString m_ClassName;
	CDmaString m_TargetName;
	CDmaVar<bool> m_bIsPlaceholder;

	// The entity it's connected to in the engine
	HTOOLHANDLE	m_hEngineEntity;

	CMaterialReference m_Wireframe;

	CMaterialReference m_SelectedInfoTarget;
	CMaterialReference m_InfoTargetSprite;

	// pretty sure this entity is edited
	bool m_bIsDirty;

	// entity needs to be deleted
	CDmaVar<bool> m_bIsDeleted;

	// FIXME: This is a hack for info targets
	bool m_bInfoTarget;

	// Used to store the next unique entity id;
	static int s_nNextEntityId;
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
