//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "report.h"

class CMatchResults: public CReport
{
public:
	enum Consts
	{
		WINNER=1,
		LOSER=0,

		STRLEN=200,
	};
private:
	struct team
	{
		bool valid;
		int score;
		int frags;
		int unacc_frags;
		int numplayers;
		bool fWinner;
		bool allies[MAX_TEAMS];
	};

	team teams[MAX_TEAMS];
	int numTeams;
	char winnerString[STRLEN];
	char loserString[STRLEN];

	bool valid;
	bool draw;

	void init();

	void calcRealWinners();

	char* getWinnerTeamsString();
	int getWinnerTeamScore();
	bool Outnumbered(int WinnerOrLoser);
	char* getLoserTeamsString();
	int getLoserTeamScore();
	int numWinningTeams();

public:

	explicit CMatchResults(){init();}

	void generate();
	void writeHTML(CHTMLFile& html);
};