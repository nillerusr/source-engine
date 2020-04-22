//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "Award.h"
#include <map>

using namespace std;
class CCureAward: public CAward
{
protected:
	map <PID,int> numcures;
	void noWinner(CHTMLFile& html);
	void extendedinfo(CHTMLFile& html);
public:
	explicit CCureAward():CAward("Life-Saver"){}
	void getWinner();
};