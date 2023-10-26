#ifndef _INCLUDED_CASW_Sentry_Top_Icer_H
#define _INCLUDED_CASW_Sentry_Top_Icer_H


#include "asw_shareddefs.h"
#include "c_asw_sentry_top_flamer.h"

// class C_ASW_Sentry_Base;
class CASWGenericEmitter;

extern ConVar asw_sentry_emitter_test;

class C_ASW_Sentry_Top_Icer : public C_ASW_Sentry_Top_Flamer
{
public:
	DECLARE_CLASS( C_ASW_Sentry_Top_Icer, C_ASW_Sentry_Top_Flamer );
	DECLARE_CLIENTCLASS();
	// DECLARE_PREDICTABLE();


	C_ASW_Sentry_Top_Icer();
	// virtual			~CASW_Sentry_Top_Icer();
	virtual bool HasPilotLight() { return false; }

protected:
	/////////////// init functions
	/// Create any old-school emitters associated with weapon fx
	/// Override in child classes ( do not call to base )
	//virtual void CreateObsoleteEmitters( void );

};

#endif /* _INCLUDED_C_ASW_SENTRY_TOP_H */