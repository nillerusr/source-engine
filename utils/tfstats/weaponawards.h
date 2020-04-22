//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface of the CWeaponAward class, and its subclasses
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#ifndef WEAPONAWARDS_H
#define WEAPONAWARDS_H
#ifdef WIN32
#pragma once
#endif
#include "Award.h"

//------------------------------------------------------------------------------------------------------
// Purpose: CWeaponAward is the superclass for any award that is based simply
// on number of kills with a specific weapon. 
//------------------------------------------------------------------------------------------------------
class CWeaponAward: public CAward
{
protected:
	map<PID,int> accum;
	char* killtype;
public:
	CWeaponAward(char* awardname, char* killname):CAward(awardname),killtype(killname){}
	void getWinner();
};

//------------------------------------------------------------------------------------------------------
// Purpose: CFlamethrowerAward is an award given to the player who gets the 
// most kills with "flames"
//------------------------------------------------------------------------------------------------------
class CFlamethrowerAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CFlamethrowerAward():CWeaponAward("Blaze of Glory","flames"){}
};

//------------------------------------------------------------------------------------------------------
// Purpose:  CAssaultCannonAward is an award given to the player who gets the
// most kills with "ac" (the assault cannon)
//------------------------------------------------------------------------------------------------------
class CAssaultCannonAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CAssaultCannonAward():CWeaponAward("Swiss Cheese","ac"){}
};


//------------------------------------------------------------------------------------------------------
// Purpose: CKnifeAward is an award given to the player who gets the most kills
// with the "knife"
//------------------------------------------------------------------------------------------------------
class CKnifeAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CKnifeAward():CWeaponAward("Assassin","knife"){}
};

//------------------------------------------------------------------------------------------------------
// Purpose: CRocketryAward is an award given to the player who gets the most kills
// with "rocket"s.
//------------------------------------------------------------------------------------------------------
class CRocketryAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CRocketryAward():CWeaponAward("Rocketry","rocket"){}
};

//------------------------------------------------------------------------------------------------------
// Purpose: CGrenadierAward is an award given to the player who gets the most 
// kills with "gl_grenade"s
//------------------------------------------------------------------------------------------------------
class CGrenadierAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CGrenadierAward():CWeaponAward("Grenadier","gl_grenade"){}
};

//------------------------------------------------------------------------------------------------------
// Purpose: CDemolitionsAward is an award given to the player who kills the most
// people with "detpack"s.
//------------------------------------------------------------------------------------------------------
class CDemolitionsAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CDemolitionsAward():CWeaponAward("Demolitions","detpack"){}
};

//------------------------------------------------------------------------------------------------------
// Purpose: CBiologicalWarfareAward is given to the player who kills the most
// people with "infection"s
//------------------------------------------------------------------------------------------------------
class CBiologicalWarfareAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	CBiologicalWarfareAward():CWeaponAward("Biological Warfare","infection"){}
};


//------------------------------------------------------------------------------------------------------
// Purpose:  CBestSentryAward is given to the player who kills the most people
// with sentry guns that he/she created ("sentrygun")
//------------------------------------------------------------------------------------------------------
class CBestSentryAward: public CWeaponAward
{
protected:
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	CBestSentryAward():CWeaponAward("Best Sentry Placement","sentrygun"){}
};



#endif // WEAPONAWARDS_H
