#ifndef _INLCUDE_C_ASW_SHIELDBUG_H
#define _INLCUDE_C_ASW_SHIELDBUG_H

#include "c_asw_alien.h"

class C_ASW_Shieldbug : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Shieldbug, C_ASW_Alien );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Shieldbug();
	virtual			~C_ASW_Shieldbug();

	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SHIELDBUG; }	

	virtual void DoAlienFootstep( Vector &vecOrigin, float fvol );
	virtual bool BlockedDamage( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	virtual bool HasCustomDeathForce(){ return true; };
	virtual Vector GetCustomDeathForce();
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming);
private:
	C_ASW_Shieldbug( const C_ASW_Shieldbug & ); // not defined, not accessible	
};

#endif /* _INLCUDE_C_ASW_SHIELDBUG_H */