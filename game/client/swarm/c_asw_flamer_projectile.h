#ifndef C_ASW_FLAMER_PROJECTILE_H
#define C_ASW_FLAMER_PROJECTILE_H

#include "c_basecombatcharacter.h"
#include "asw_shareddefs.h"

struct dlight_t;

class C_ASW_Flamer_Projectile : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Flamer_Projectile, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Flamer_Projectile();
	virtual			~C_ASW_Flamer_Projectile();
	void ClientThink(void);
	void OnDataChanged(DataUpdateType_t updateType);
	void CreateLight();
	dlight_t* m_pDynamicLight;

	// Classification
	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_FLAMER_PROJECTILE; }

private:
	C_ASW_Flamer_Projectile( const C_ASW_Flamer_Projectile & ); // not defined, not accessible
};

#endif /* C_ASW_FLAMER_PROJECTILE_H */