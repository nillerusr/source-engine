//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#pragma warning (disable:4786)
#include "PlrPersist.h"
#include "TextFile.h"

#include <map>
#include <string>
using namespace std;

//------------------------------------------------------------------------------------------------------
// Function:	CPlrPersist::generate
// Purpose:	fills in the fields of this with the data in the given CPlayer object
// Input:	cp - the player object to get data from
//------------------------------------------------------------------------------------------------------
void CPlrPersist::generate(CPlayer& cp)
{
	kills=deaths=timeon=0;
	valid=true;
	WONID=cp.WONID;
	matches=1;
	
	lastplayed=cp.logofftime;

	//do perteam stuff
	CTimeIndexedList<int>::iterator teamiter=cp.teams.begin();
	for (teamiter;teamiter!=cp.teams.end();++teamiter)
	{
		int tdt=teamiter->data;
		kills+=cp.perteam[teamiter->data].kills;
		deaths+=cp.perteam[teamiter->data].deaths;
		timeon+=cp.perteam[teamiter->data].timeon;

		map<string,int>::iterator it;
		it=cp.perteam[teamiter->data].weaponKills.begin();
		for (it;it!=cp.perteam[teamiter->data].weaponKills.end();++it)
		{
			string name=it->first;
			int kills=it->second;
			weapmap[name]+=kills;
		}
	}

	CTimeIndexedList<player_class>::iterator clsit=cp.allclassesplayed.begin();
	for (clsit;clsit!=cp.allclassesplayed.end();++clsit)
	{
		string classname=plrClassNames[clsit->data];
		classmap[classname]+=cp.allclassesplayed.howLong(clsit->data);
	}
	
	CTimeIndexedList<string>::iterator nameiter;
	for (nameiter=cp.aliases.begin();nameiter!=cp.aliases.end();++nameiter)
	{
		nickmap[nameiter->data]+=cp.aliases.howLong(nameiter->data);
	}

	pair<time_t,time_t> startstop;
	startstop.first=cp.logontime;
	startstop.second=cp.logofftime;

	playtimes.push_back(startstop);
}

//------------------------------------------------------------------------------------------------------
// Function:	CPlrPersist::merge
// Purpose:	 merges the stats of another CPlrPersist object into this one.
//	This is the key operation of this class.  This is how player stats are kept up 
//	to date over time.  If the two data files have playtimes that overlap, they 
//	are not merged (unless the mergeOverlaps flag is true)
// Input:	other - the CPlrPersist object that we want to merge into this one
//				mergeOverlaps - if true, overlapping playtimes are ignored. 
//------------------------------------------------------------------------------------------------------
void CPlrPersist::merge(CPlrPersist& other,bool mergeOverlaps)
{
	if (!other.valid)
		return;	//don't modify
	if (WONID!=other.WONID)
	{
		g_pApp->warning("merging stats for two different WONIDs (%lu, %lu)",WONID,other.WONID);
		
	}
	else
	{
		//do playtimes first to see if overlaps occur
		list<pair<time_t,time_t> >::iterator itOther=other.playtimes.begin();
		for (itOther;itOther!=other.playtimes.end();++itOther)
		{
			list<pair<time_t,time_t> >::iterator overlap=timesOverlap(itOther->first,itOther->second);
			time_t overlapSecond=overlap->second;
			time_t overlapFirst=overlap->first;
			if (mergeOverlaps || overlap==playtimes.end())
				playtimes.push_back(*itOther);
			else
			{
				g_pApp->warning("not merging stats for WON ID# %lu, playtime ranges overlap\n\t((%lu-%lu) overlaps with (%lu-%lu))",WONID,itOther->first,itOther->second,overlap->first,overlap->second);
				return;
			}
		}
	}


	matches+=other.matches;
	kills+=other.kills;
	deaths+=other.deaths;
	timeon+=other.timeon;
	if (other.lastplayed > lastplayed)
		lastplayed=other.lastplayed;

	//do names
	map<string,int>::iterator it;
	it=other.nickmap.begin();
	for (it;it!=other.nickmap.end();++it)
	{
		string name=it->first;
		int time=it->second;
		nickmap[name]+=time;
	}
	
	//do weapons
	it=other.weapmap.begin();
	for (it;it!=other.weapmap.end();++it)
	{
		string name=it->first;
		int kills=it->second;
		weapmap[name]+=kills;
	}
	
	//do classes
	it=other.classmap.begin();
	for (it;it!=other.classmap.end();++it)
	{
		string name=it->first;
		int time=it->second;
		classmap[name]+=time;
	}

}

//------------------------------------------------------------------------------------------------------
// Function:	CPlrPersist::read
// Purpose:	 fills in the fields of this by reading data out of a file
// Input:	f - the file from which to read the data
//------------------------------------------------------------------------------------------------------
void CPlrPersist::read(CTextFile& f)
{
	if (!f.isValid())
	{
		kills=deaths=timeon=0; WONID=-1;
		valid=false;
		return;
	}

	if(WONID==-1)
	{
		//parse it out of f;
		string s=f.fileName();
		char buf[100];
		int startpos=s.find_last_of(g_pApp->os->pathSeperator());
		int endpos=s.find_last_of(".");
		if (endpos == -1)
			return;
		if (startpos==-1)
			startpos=0;
		
		s.copy(buf,(endpos-startpos),startpos);
		buf[endpos-startpos]=0;
		WONID=strtoul(buf,NULL,10);
		if (!WONID)
		{
			WONID=-1;
			valid=false;
			return;
		}
			

	}
	
	

	valid=false;
	
	if (!f.eof()) kills=f.readInt();	else return;
	if (!f.eof()) deaths=f.readInt(); else return;
	if (!f.eof()) timeon=f.readULong(); else return;
	if (!f.eof()) matches=f.readInt(); else return;
	if (!f.eof()) lastplayed=f.readULong(); else return;

	string next;

	if (!f.eof())
	{
		f.discard("names");
		next= f.peekNextString();
		while ( next!="endnames")
		{
			string name=f.readString();
			int timeon=f.readInt();
			nickmap[name]=timeon;
			next=f.peekNextString();
		}
		f.discard("endnames");
	} else return;
	
	if (!f.eof())
	{
		f.discard("weapons");
		next= f.peekNextString();
		while (next!="endweapons")
		{
			string name=f.readString();
			int kills=f.readInt();
			weapmap[name]=kills;
			next=f.peekNextString();
		}
		f.discard("endweapons");
	} else return;

	if (!f.eof())
	{
		f.discard("classes");
		next= f.peekNextString();
		while (next!="endclasses")
		{
			string name=f.readString();
			int timeused=f.readInt();
			classmap[name]=timeused;
			next=f.peekNextString();
		}
		f.discard("endclasses");
	} else return;

	if (!f.eof())
	{
		f.discard("playtimes");
		next= f.peekNextString();
		while (next!="endplaytimes")
		{
			pair<time_t,time_t> startstop;
			startstop.first=f.readULong();
			startstop.second=f.readULong();
			playtimes.push_back(startstop);
			next=f.peekNextString();
		}
		f.discard("endplaytimes");
	} else return;

	valid=true;
	
}

//------------------------------------------------------------------------------------------------------
// Function:	CPlrPersist::read
// Purpose:	 converts the WONID to a file name (<wonid>.tfs) and passes execution
//	off to the above read function.
// Input:	WONID - the WONID of the player whose datafile we want to read
//------------------------------------------------------------------------------------------------------
void CPlrPersist::read(unsigned long WONID)
{
	
	string file=g_pApp->playerDirectory;
	char buf[100];
	file+=g_pApp->os->ultoa(WONID,buf,10);
	file+=".tfs";

	this->WONID=WONID;

	CTextFile f(file.c_str());
	read(f);
}



void CPlrPersist::write()
{
	string file=g_pApp->playerDirectory;
	char buf[100];
	file+=g_pApp->os->ultoa(WONID,buf,10);
	file+=".tfs";

	FILE* fout=fopen(file.c_str(),"wt");

	fprintf(fout,"%li //kills\n",kills);
	fprintf(fout,"%li //deaths\n",deaths);
	fprintf(fout,"%lu //timeon\n",timeon);
	fprintf(fout,"%li //matches played\n",matches);
	fprintf(fout,"%lu //last played\n",lastplayed);
	
	map<string,int>::iterator it;
	
	fprintf(fout,"names\n");
	it=nickmap.begin();
	for (it;it!=nickmap.end();++it)
	{
		string name=it->first;
		int time=it->second;
		fprintf(fout,"\t\"%s\" %li //has used the name \"%s\" for %02li:%02li:%02li\n",name.c_str(),time,name.c_str(),Util::time_t2hours(time),Util::time_t2mins(time),Util::time_t2secs(time));
	}
	fprintf(fout,"endnames\n");

	fprintf(fout,"weapons\n");
	it=weapmap.begin();
	for (it;it!=weapmap.end();++it)
	{
		string name=it->first;
		int kills=it->second;
		fprintf(fout,"\t\"%s\" %li //has killed %li people with \"%s\"\n",name.c_str(),kills,kills,name.c_str());
	}
	fprintf(fout,"endweapons\n");

	fprintf(fout,"classes\n");
	it=classmap.begin();
	for (it;it!=classmap.end();++it)
	{
		string name=it->first;
		int time=it->second;
		fprintf(fout,"\t\"%s\" %li //has played as a \"%s\" for %02li:%02li:%02li\n",name.c_str(),time,name.c_str(),Util::time_t2hours(time),Util::time_t2mins(time),Util::time_t2secs(time));
	}
	fprintf(fout,"endclasses\n");

	fprintf(fout,"playtimes\n");
	list<pair<time_t,time_t> >::iterator it2=playtimes.begin();
	for (it2;it2!=playtimes.end();++it2)
	{
		char buf[500];
		time_t t1=it2->first;
		time_t t2=it2->second;
		bool doesOverlap;
		
		list<pair<time_t,time_t> >::iterator overlap=timesOverlap(it2->first,it2->second,false);
		doesOverlap= overlap!=playtimes.end();
			

		fprintf(fout,"\t%lu %lu //played from %s.",it2->first,it2->second,Util::makeDurationString(it2->first,it2->second,buf," to "));
		if (doesOverlap)
			fprintf(fout,"Warning! overlaps with time range (%lu-%lu)",overlap->first,overlap->second);
		fprintf(fout,"\n");

	}
	fprintf(fout,"endplaytimes\n");


	fclose(fout);

}


list<pair<time_t,time_t> >::iterator CPlrPersist::timesOverlap(time_t start, time_t end,bool testself)
{
	list<pair<time_t,time_t> >::iterator it;
	it=playtimes.begin();
	for (it;it!=playtimes.end();++it)
	{
		time_t itFirst=it->first;
		time_t itSecond=it->second;
		if (start == it->first && end == it->second)
		{
			if (testself)
				break;
		}
		//if start is in current range
		else if (start >= it->first  && start <= it->second)
			break;

		//if end is in current range
		else if (end >= it->first  && end <= it->second)
			break;

		//if the start is before this range and end is after
		else if (start <= it->first && end >= it->second)
			break;
	}
	return it;
}

string CPlrPersist::faveString(map<string,int>& theMap)
{
	string retstr;
	time_t max=0;
	map<string,int>::iterator it=theMap.begin();
	for (it;it!=theMap.end();++it)
	{
		if (it->second > max)
		{
			max=it->second;
			retstr=it->first;
		}
	}
	return retstr;
}

string CPlrPersist::faveName()
{
	return faveString(nickmap);
}
string CPlrPersist::faveWeap()
{
	string s=faveString(weapmap);
	faveweapkills=weapmap[s];
	return s;
}
string CPlrPersist::faveClass()
{
	return faveString(classmap);
}

double CPlrPersist::rank()
{
	return ((double)((double)kills - (double)deaths) * 1000.0) / (double)timeon;
}
