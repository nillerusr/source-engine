#ifndef _INLCUDE_C_ASW_GRUB_H
#define _INLCUDE_C_ASW_GRUB_H

#include "c_asw_alien.h"

class C_ASW_Grub : public C_ASW_Alien
{
public:
	DECLARE_CLASS( C_ASW_Grub, C_ASW_Alien );
	DECLARE_CLIENTCLASS();	

					C_ASW_Grub();
	virtual			~C_ASW_Grub();

	virtual bool IsAimTarget() { return false; }

private:
	C_ASW_Grub( const C_ASW_Grub & ); // not defined, not accessible	
};

#endif /* _INLCUDE_C_ASW_GRUB_H */