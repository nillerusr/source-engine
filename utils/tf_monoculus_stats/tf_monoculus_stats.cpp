//========= Copyright Valve Corporation, All rights reserved. ============//
// Parse Halloween 2011 server logs and mine Eyeball Boss (Monoculus) data

#define _CRT_SECURE_NO_WARNINGS // STFU

#include <tchar.h>

#include "stdio.h"
#include <math.h>
#include <vector>

#define MAX_BUFFER_SIZE 256
#define MAX_NAME_SIZE 64

enum EventType
{
	EYEBALL_SPAWN,			// eyeball_spawn (max_health 11200) (player_count 18)
	EYEBALL_ESCAPE,			// eyeball_escaped (max_dps 220.00) (health 3071)
	EYEBALL_DEATH,			// eyeball_death (max_dps 467.63) (max_health 12000) (player_count 23)
	EYEBALL_STUN,			// "Aque0us<174><STEAM_0:0:212532><Red>" eyeball_stunned with "NoWeapon" (attacker_position "-1033 872 143")
	PURGATORY_ENTER,		// "I_DONT_KNOW<181><STEAM_0:1:34210475><Red>" purgatory_teleport "spawn_purgatory"
	PURGATORY_ESCAPE,		// "I_DONT_KNOW<181><STEAM_0:1:34210475><Red>" purgatory_escaped
	LOOT_ISLAND_ENTER,		// "I_DONT_KNOW<181><STEAM_0:1:34210475><Red>" purgatory_teleport "spawn_loot"
	EYEBALL_KILLER,			// "Spaghetti Western<192><STEAM_0:1:41812531><Red>" eyeball_killer with "liberty_launcher" (attacker_position "-1629 -293 213")

	EVENT_COUNT
};

#define TEAM_NONE 0
#define TEAM_RED 1
#define TEAM_BLUE 2

struct EventInfo
{
	EventType m_type;
	char m_date[ MAX_NAME_SIZE ];
	int m_timestamp;		// seconds since log start
	int m_health;
	int m_maxHealth;
	int m_playerCount;
	int m_maxDPS;
	int m_attackerPosX;
	int m_attackerPosY;
	int m_attackerPosZ;
	char m_weaponName[ MAX_NAME_SIZE ];
	char m_playerName[ MAX_NAME_SIZE ];
	int m_playerID;
	int m_team;
};

std::vector< EventInfo > TheData;


//----------------------------------------------------------------------------------
bool ParseEvent( FILE *fp, EventInfo *event )
{
	return false;
}


//----------------------------------------------------------------------------------
char *ParsePlayer( EventInfo *event, char *data )
{
	// "TheSauce<17><STEAM_0:1:19333174><Red>"
	// ARGH! Player names can contain '<' and '>'

	// skip the "
	++data;

	char *restOfData = NULL;

	char *c = &data[ strlen(data)-1 ];

	// backup until we find '>'
	while( c != data )
	{
		--c;
		if ( *c == '>' )
		{
			restOfData = c+2;

			// terminate the team name
			*c = '\000';
			break;
		}
	}

	// backup until we find '<'
	char *teamName = NULL;

	while( c != data )
	{
		--c;
		if ( *c == '<' )
		{
			teamName = c+1;

			// back up and terminate the Steam ID
			--c;
			*c = '\000';
			break;
		}
	}

	if ( !teamName )
	{
		return NULL;
	}

	if ( !strcmp( "Red", teamName ) )
	{
		event->m_team = TEAM_RED;
	}
	else if ( !strcmp( "Blue", teamName ) )
	{
		event->m_team = TEAM_BLUE;
	}
	else
	{
		return NULL;
	}

	// "TheSauce<17><STEAM_0:1:19333174><Red>"

	// backup until we find '<'
	char *steamID = NULL;

	while( c != data )
	{
		--c;
		if ( *c == '<' )
		{
			steamID = c+1;

			// back up and terminate the player ID
			--c;
			*c = '\000';
			break;
		}
	}

	if ( !steamID )
	{
		return NULL;
	}

	// backup until we find '<'
	char *playerID = NULL;

	while( c != data )
	{
		--c;
		if ( *c == '<' )
		{
			playerID = c+1;

			// terminate the player name
			*c = '\000';
			break;
		}
	}

	if ( !playerID )
	{
		return NULL;
	}

	event->m_playerID = atoi( playerID );

	strcpy( event->m_playerName, data );

	return restOfData;

#ifdef OLDWAY
	// skip the "
	++data;

	char *token = strtok( data, "<" );
	if ( !token )
		return NULL;

	strcpy( event->m_playerName, token );

	token = strtok( NULL, ">" );
	if ( !token )
		return NULL;

	event->m_playerID = atoi( token );

	// steam ID
	token = strtok( NULL, "<>" );
	if ( !token )
		return NULL;

	// team
	token = strtok( NULL, "<>" );
	if ( !token )
		return NULL;

	if ( !strcmp( "Red", token ) )
	{
		event->m_team = TEAM_RED;
	}
	else if ( !strcmp( "Blue", token ) )
	{
		event->m_team = TEAM_BLUE;
	}

	// get rest of line after closing quote
	token = strtok( NULL, "" );
	token += 2;

	return token;
#endif
}


//----------------------------------------------------------------------------------
// Parse and return int value from "(name value)"
bool parse_strtok_int( const char *name, int *value )
{
	char *token = strtok( NULL, "( " );
	if ( !token )
		return false;

	if ( _stricmp( name, token ) )
		return false;

	// get value
	token = strtok( NULL, ") " );
	if ( !token )
		return false;

	*value = atoi( token );
	return true;
}


//----------------------------------------------------------------------------------
bool ProcessData( const char *filename )
{
	FILE *fp = fopen( filename, "r" );
	if ( !fp )
	{
		printf( "ERROR: Cannot access file '%s'\n", filename );
		return false;
	}

	FILE *errorFP = fopen( "cooked_stats_error.txt", "w" );
	if ( !errorFP )
	{
		printf( "ERROR: Can't open output file\n" );
		return false;
	}

	char buffer[ MAX_BUFFER_SIZE ];

	bool isFirstLine = true;
	int initialTimestamp = 0;

	int line = 0;

	EventInfo info;

	while( true )
	{
		memset( &info, 0, sizeof( EventInfo ) );

		fgets( buffer, MAX_BUFFER_SIZE, fp );
		++line;

		// eat filename
		char *token = buffer;
		while( *token != 'L' )
		{
			++token;
		}

		if ( !token )
			break;

		// read date
		token = strtok( token, "L " );
		if ( !token )
			break;

		strcpy( info.m_date, token );

		// read time
		token = strtok( NULL, "- " );
		if ( !token )
			break;

		token[2] = '\000';
		int hour = atoi( token );

		token[5] = '\000';
		int minute = atoi( &token[3] );

		token[8] = '\000';
		int second = atoi( &token[6] );

		int timestamp = hour * 3600 + minute * 60 + second;

		if ( isFirstLine )
		{
			isFirstLine = false;
			initialTimestamp = timestamp;
		}

		info.m_timestamp = timestamp - initialTimestamp;

		int fullDay = 24 * 3600;
		if ( info.m_timestamp > fullDay )
		{
			// wrapped past midnight
			info.m_timestamp -= fullDay;
		}

		// eat "HALLOWEEN"
		token = strtok( NULL, ": " );
		if ( !token )
			break;

		// get the rest of the line
		token = strtok( NULL, "" );
		if ( !token )
			break;

		// skip trailing space
		++token;

		if ( !strncmp( "eyeball_spawn", token, 13 ) )
		{
			// eyeball_spawn (max_health 8000) (player_count 10)
			info.m_type = EYEBALL_SPAWN;

			// eat 'eyeball_spawn'
			token = strtok( token, " " );
			if ( !token )
				break;

			if ( !parse_strtok_int( "max_health", &info.m_maxHealth ) )
				break;

			if ( !parse_strtok_int( "player_count", &info.m_playerCount ) )
				break;
		}
		else if ( !strncmp( "eyeball_escaped", token, 15 ) )
		{
			// eyeball_escaped (max_dps 143.64) (health 1525)
			info.m_type = EYEBALL_ESCAPE;

			// eat 'eyeball_escape'
			token = strtok( token, " " );
			if ( !token )
				break;

			if ( !parse_strtok_int( "max_dps", &info.m_maxDPS ) )
				break;

			if ( !parse_strtok_int( "health", &info.m_health ) )
				break;
		}
		else if ( !strncmp( "eyeball_death", token, 13 ) )
		{
			// eyeball_death (max_dps 285.54) (max_health 13200) (player_count 24)
			info.m_type = EYEBALL_DEATH;

			// eat 'eyeball_death'
			token = strtok( token, " " );
			if ( !token )
				break;

			if ( !parse_strtok_int( "max_dps", &info.m_maxDPS ) )
				break;

			if ( !parse_strtok_int( "max_health", &info.m_maxHealth ) )
				break;

			if ( !parse_strtok_int( "player_count", &info.m_playerCount ) )
				break;
		}
		else if ( token[0] == '"' )
		{
			char *data = ParsePlayer( &info, token );

			if ( data )
			{
				token = strtok( data, " " );
				if ( !token )
					continue;

				if ( !strncmp( "purgatory_escaped", token, 17 ) )
				{
					info.m_type = PURGATORY_ESCAPE;
				}
				else if ( !strcmp( "purgatory_teleport", token ) )
				{
					token = strtok( NULL, "\" " );
					if ( !token )
						continue;

					if ( !strcmp( "spawn_purgatory", token ) )
					{
						info.m_type = PURGATORY_ENTER;
					}
					else if ( !strcmp( "spawn_loot", token ) )
					{
						info.m_type = LOOT_ISLAND_ENTER;
					}
					else
					{
						fprintf( errorFP, "ERROR @ Line %d: Unknown purgatory teleport '%s'\n", line, token );
						continue;
					}
				}
				else if ( !strcmp( "eyeball_stunned", token ) )
				{
					// eyeball_stunned with "short_stop" (attacker_position "-1951 -166 256")
					info.m_type = EYEBALL_STUN;

					// eat 'with'
					token = strtok( NULL, " " );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					strcpy( info.m_weaponName, token );

					// eat (attacker_position 
					token = strtok( NULL, " " );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosX = atoi( token );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosY = atoi( token );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosZ = atoi( token );
				}
				else if ( !strcmp( "eyeball_killer", token ) )
				{
					// eyeball_killer with "short_stop" (attacker_position "-1213 271 236")
					info.m_type = EYEBALL_KILLER;

					// eat 'with'
					token = strtok( NULL, " " );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					strcpy( info.m_weaponName, token );

					// eat (attacker_position 
					token = strtok( NULL, " " );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosX = atoi( token );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosY = atoi( token );

					token = strtok( NULL, "\" " );
					if ( !token	)
						continue;

					info.m_attackerPosZ = atoi( token );
				}
				else
				{
					fprintf( errorFP, "ERROR at Line %d: Unknown player event '%s'\n", line, token );
				}
			}
		}
		else
		{
			fprintf( errorFP, "ERROR at Line %d: Unknown data '%s'\n", line, token );
			continue;
		}

		TheData.push_back( info );
	}

	fclose( fp );


	//----------------------------------------
	unsigned int bossSpawnCount = 0;
	unsigned int bossEscapedCount = 0;
	unsigned int bossDeathCount = 0;
	unsigned int bossSpawnTimestamp = 0;
	unsigned int bossDroppedCount = 0;

	std::vector< int > bossLifetime;
	std::vector< int > bossEscapeLifetime;
	std::vector< int > bossDeathLifetime;
	std::vector< int > bossStunCount;

	bool isBossAlive = false;

	for( unsigned int i=0; i<TheData.size(); ++i )
	{
		switch( TheData[i].m_type )
		{
		case EYEBALL_SPAWN:
			if ( isBossAlive )
			{
				// we didn't get an ESCAPE or DEATH - map change?
				// timestamp can go backwards on map change, so just don't count these lifetimes
				bossLifetime.pop_back();
				bossEscapeLifetime.pop_back();
				bossDeathLifetime.pop_back();
				bossStunCount.pop_back();
				--bossSpawnCount;
				++bossDroppedCount;
			}

			isBossAlive = true;
			bossSpawnTimestamp = TheData[i].m_timestamp;
			++bossSpawnCount;

			bossLifetime.push_back(0);
			bossEscapeLifetime.push_back(0);
			bossDeathLifetime.push_back(0);
			bossStunCount.push_back(0);

			break;

		case EYEBALL_ESCAPE:
			if ( !isBossAlive )
			{
				fprintf( errorFP, "%d: Got ESCAPE when not spawned\n", i );
			}
			else
			{
				isBossAlive = false;
				bossLifetime[ bossSpawnCount-1 ] = TheData[i].m_timestamp - bossSpawnTimestamp;
				bossEscapeLifetime[ bossSpawnCount-1 ] = TheData[i].m_timestamp - bossSpawnTimestamp;
				++bossEscapedCount;
			}
			break;

		case EYEBALL_DEATH:
			if ( !isBossAlive )
			{
				int lifetime = TheData[i].m_timestamp - bossSpawnTimestamp;

				// using large time delta to account for stuns while escaping followed by a death
				if ( lifetime - bossEscapeLifetime[ bossSpawnCount-1 ] < 60.0f )
				{
					// killed while escaping - he didn't actually escape!
					bossEscapeLifetime[ bossSpawnCount-1 ] = 0;
					--bossEscapedCount;
				}
				else
				{
					fprintf( errorFP, "%d: Got DEATH when not spawned\n", i );
				}
			}
			else
			{
				isBossAlive = false;
				bossLifetime[ bossSpawnCount-1 ] = TheData[i].m_timestamp - bossSpawnTimestamp;
				bossDeathLifetime[ bossSpawnCount-1 ] = TheData[i].m_timestamp - bossSpawnTimestamp;
				++bossDeathCount;
			}

			break;

		case EYEBALL_STUN:
			// skip alive check to collect stuns that happen just after escape starts
			bossStunCount[ bossSpawnCount-1 ] = bossStunCount[ bossSpawnCount-1 ] + 1;

			break;

		case PURGATORY_ENTER:
			break;
		case PURGATORY_ESCAPE:
			break;
		case LOOT_ISLAND_ENTER:
			break;
		case EYEBALL_KILLER:
			break;
		}
	}

	fclose( errorFP );


	//----------------------------------------
	sprintf( buffer, "%s_cooked.txt", filename );

	fp = fopen( buffer, "w" );
	if ( !fp )
	{
		printf( "ERROR: Can't open output file '%s'\n", buffer );
		return false;
	}

	if ( bossSpawnCount )
	{
		int minTime = 99999, maxTime = 0, avgTime = 0;

#define USE_COLUMNS
#ifdef USE_COLUMNS
		fprintf( fp, "Lifetime, EscapeLifetime, DeathLifetime, StunCount\n" );

		for( unsigned int i=0; i<bossSpawnCount; ++i )
		{
			fprintf( fp, "%d, %d, %d, %d\n", bossLifetime[i], bossEscapeLifetime[i], bossDeathLifetime[i], bossStunCount[i] );

			if ( bossLifetime[i] < minTime )
			{
				minTime = bossLifetime[i];
			}

			if ( bossLifetime[i] > maxTime )
			{
				maxTime = bossLifetime[i];
			}

			avgTime += bossLifetime[i];
		}
		fprintf( fp, "\n" );
#else
		fprintf( fp, "Lifetime, " );
		for( unsigned int i=0; i<bossSpawnCount; ++i )
		{
			fprintf( fp, "%d, ", bossLifetime[i] );

			if ( bossLifetime[i] < minTime )
			{
				minTime = bossLifetime[i];
			}

			if ( bossLifetime[i] > maxTime )
			{
				maxTime = bossLifetime[i];
			}

			avgTime += bossLifetime[i];
		}
		fprintf( fp, "\n" );

		fprintf( fp, "EscapeLifetime, " );
		for( unsigned int i=0; i<bossSpawnCount; ++i )
		{
			if ( bossEscapeLifetime[i] > 0 )
				fprintf( fp, "%d, ", bossEscapeLifetime[i] );
		}
		fprintf( fp, "\n" );

		fprintf( fp, "DeathLifetime, " );
		for( unsigned int i=0; i<bossSpawnCount; ++i )
		{
			if ( bossDeathLifetime[i] > 0 )
				fprintf( fp, "%d, ", bossDeathLifetime[i] );
		}
		fprintf( fp, "\n" );

		fprintf( fp, "StunCount, " );
		for( unsigned int i=0; i<bossSpawnCount; ++i )
		{
			fprintf( fp, "%d, ", bossStunCount[i] );
		}
		fprintf( fp, "\n\n" );
#endif

		fprintf( fp, "Boss spawn count = %d\n", bossSpawnCount );
		fprintf( fp, "Boss escape count = %d\n", bossEscapedCount );
		fprintf( fp, "Boss escape ratio = %d%%\n", 100 * bossEscapedCount / bossSpawnCount );
		fprintf( fp, "Boss killed count = %d\n", bossDeathCount );
		fprintf( fp, "Boss killed ratio = %d%%\n", 100 * bossDeathCount / bossSpawnCount );
		fprintf( fp, "Boss dropped count = %d\n", bossDroppedCount );
		fprintf( fp, "Boss dropped ratio = %d%%\n", 100 * bossDroppedCount / bossSpawnCount );
		fprintf( fp, "Boss MinLifetime %d, AvgLifeTime %d, MaxLifetime %d\n", minTime, avgTime / bossSpawnCount, maxTime );
	}
	else
	{
		fprintf( fp, "No Halloween data found.\n" );
	}

	fclose( fp );

	return true;
}


//----------------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
	if ( argc < 2 )
	{
		printf( "USAGE: %s <server log .txt file>\n", argv[0] );
		return -1;
	}

	for( int i=1; i<argc; ++i )
	{
		ProcessData( argv[i] );
	}

	return 0;
}