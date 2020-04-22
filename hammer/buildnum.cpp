//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include <stdlib.h>
#include <string.h>
#include "tier1/strtools.h"

// char *date = "Oct 24 1996";
char *date = __DATE__ ;

char *mon[12] = 
{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char mond[12] = 
{ 31,    28,    31,    30,    31,    30,    31,    31,    30,    31,    30,    31 };

// returns days since Oct 24 1996
int build_number( void )
{
	int m = 0; 
	int d = 0;
	int y = 0;
	static int b = 0;

	if (b != 0)
		return b;

	for (m = 0; m < 11; m++)
	{
		if ( Q_strnicmp( &date[0], mon[m], 3 ) == 0 )
			break;
		d += mond[m];
	}

	d += atoi( &date[4] ) - 1;

	y = atoi( &date[7] ) - 1900;

	b = d + (int)((y - 1) * 365.25);

	if (((y % 4) == 0) && m > 1)
	{
		b += 1;
	}

	b -= 34995; // Oct 24 1996

	return b;
}
