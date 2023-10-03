#ifndef _INCLUDED_C_ASW_SIMPLE_ALIEN_H
#define _INCLUDED_C_ASW_SIMPLE_ALIEN_H

#include "iasw_client_aim_target.h"
#include "asw_shareddefs.h"
#include "glow_outline_effect.h"

class C_ASW_Simple_Alien : public C_BaseAnimating, public IASW_Client_Aim_Target
{
public:
	DECLARE_CLASS( C_ASW_Simple_Alien, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

	C_ASW_Simple_Alien();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_UNKNOWN; }

	void FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	void AlienStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity );
	surfacedata_t* GetGroundSurface();
	void PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force );
	virtual void DoAlienFootstep( Vector &vecOrigin, float fvol );	

	C_BaseAnimating * BecomeRagdollOnClient( void );
	void PostDataUpdate( DataUpdateType_t updateType );

	bool m_bStepSideLeft;

	// aim target interface
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 23; }
	virtual bool IsAimTarget() { return false; }		// disabled this: no autoaiming at grubs
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming);
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return m_vecLastRenderedPos; }

	// custom shadow
	virtual bool GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const;

	int m_nLastSetModel;
	virtual void ClientThink();
	Vector m_vecLastRenderedPos;
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

private:
	C_ASW_Simple_Alien( const C_ASW_Simple_Alien & ); // not defined, not accessible
	static float sm_flLastFootstepTime;

	CGlowObject m_GlowObject;
};

#endif /* _INCLUDED_C_ASW_SIMPLE_ALIEN_H */