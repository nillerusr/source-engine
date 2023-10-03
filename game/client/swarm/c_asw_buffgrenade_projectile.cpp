#include "cbase.h"

#include "c_asw_buffgrenade_projectile.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//Precahce the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, ASWPrecacheEffectBuffGrenades )
	PRECACHE( MATERIAL, "swarm/effects/blueflare" )
	PRECACHE( MATERIAL, "effects/yellowflare" )
	PRECACHE( MATERIAL, "effects/yellowflare_noz" )
PRECACHE_REGISTER_END()

IMPLEMENT_CLIENTCLASS_DT( C_ASW_BuffGrenade_Projectile, DT_ASW_BuffGrenade_Projectile, CASW_BuffGrenade_Projectile )
END_RECV_TABLE()


ConVar asw_buffgrenade( "asw_buffgrenade", "98 34 16", 0, "Color of grenades" );


Color C_ASW_BuffGrenade_Projectile::GetGrenadeColor( void )
{
	return asw_buffgrenade.GetColor();
}
