#ifndef _INCLUDED_C_ASW_ENTITY_DISSOLVE_H
#define _INCLUDED_C_ASW_ENTITY_DISSOLVE_H

#include "cbase.h"

//-----------------------------------------------------------------------------
// ASW - Custom version of the entity dissolve effect, used by alien goo when it fades out (doesn't have sparks, etc.)
//-----------------------------------------------------------------------------
class C_ASW_Entity_Dissolve : public C_BaseEntity, public IMotionEvent
{
public:
	DECLARE_CLIENTCLASS();
	DECLARE_CLASS( C_ASW_Entity_Dissolve, C_BaseEntity );

	C_ASW_Entity_Dissolve( void );

	// Inherited from C_BaseEntity
	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual int		DrawModel( int flags );
	virtual bool	ShouldDraw() { return true; }
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	UpdateOnRemove( void );

	virtual void BloodSpurts();

	// Inherited from IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

	void			SetupEmitter( void );

	void			ClientThink( void );

	void			SetServerLinkState( bool state ) { m_bLinkedToServerEnt = state; }

	float	m_flStartTime;
	float	m_flFadeOutStart;
	float	m_flFadeOutLength;
	float	m_flFadeOutModelStart;
	float	m_flFadeOutModelLength;
	float	m_flFadeInStart;
	float	m_flFadeInLength;
	int		m_nDissolveType;
	float   m_flNextSparkTime;

protected:

	float GetFadeInPercentage( void );		// Fade in amount (entity fading to black)
	float GetFadeOutPercentage( void );		// Fade out amount (particles fading away)
	float GetModelFadeOutPercentage( void );// Mode fade out amount

	// Compute the bounding box's center, size, and basis
	void ComputeRenderInfo( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, 
		Vector *pVecAbsOrigin, Vector *pXVec, Vector *pYVec );
	void BuildTeslaEffect( mstudiobbox_t *pHitBox, const matrix3x4_t &hitboxToWorld, bool bRandom, float flYawOffset );

	void DoSparks( mstudiohitboxset_t *set, matrix3x4_t *hitboxbones[MAXSTUDIOBONES] );

private:

	CSmartPtr<CSimpleEmitter>	m_pEmitter;

	bool	m_bLinkedToServerEnt;
	IPhysicsMotionController	*m_pController;
};

#endif // _INCLUDED_C_ASW_ENTITY_DISSOLVE_H

