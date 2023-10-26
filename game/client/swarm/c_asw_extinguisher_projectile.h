#ifndef C_ASW_EXTINGUISHER_PROJECTILE_H
#define C_ASW_EXTINGUISHER_PROJECTILE_H

#include "c_basecombatcharacter.h"


class C_ASW_Extinguisher_Projectile : public C_BaseCombatCharacter
{
public:
	DECLARE_CLASS( C_ASW_Extinguisher_Projectile, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Extinguisher_Projectile();
	virtual			~C_ASW_Extinguisher_Projectile();

private:
	C_ASW_Extinguisher_Projectile( const C_ASW_Extinguisher_Projectile & ); // not defined, not accessible
};

#endif /* C_ASW_EXTINGUISHER_PROJECTILE_H */