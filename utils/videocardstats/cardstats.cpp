//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//



#include <stdio.h>
#include <stdlib.h>

#include "CardStats.h"


//-----------------------------------------------------------------------------
// Main stats loop
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	if ( argc != 3 ) // a unix style usage string
	{
		printf ( "Usage: <SearchString> <Filename>\n" );
		printf ( "Output file will be <filename>.<searchstring>.txt" );
		return 0;
	}

	char* pszCardtype = argv[1]; // video card type we are looking for
	char* pszFilename = argv[2]; // file containing the data

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	int nFileSize = LoadFileIntoBuffer(buf, pszFilename);

	ParseHeader ( buf );

	CUtlBuffer output ( 0, 255, CUtlBuffer::TEXT_BUFFER );

	int binwidth=100; 
	// video card stats over time
	//TimeSeriesVCard ( buf, output, pszCardtype, nFileSize, binwidth );
	// cpu type stats over time
	//TimeSeriesCPU ( buf, output, pszCardtype, nFileSize, binwidth );
	// CPU vs Videocard
	//ParseFile ( buf, output, pszCardtype, nFileSize, binwidth, 0);
	// CPU vs Memory
	//ParseFile2 ( buf, output, pszCardtype, nFileSize, binwidth );
	//CPU vs network speed
	//ParseFile3 ( buf, output, pszCardtype, nFileSize, binwidth );
	// histogram of video card distribution
	HistogramVidCards ( buf, output, pszCardtype, nFileSize, binwidth );
	//HistogramNetSpeed ( buf, output, pszCardtype, nFileSize, binwidth );
	//HistogramRam ( buf, output, pszCardtype, nFileSize, binwidth );
	//HistogramCPU ( buf, output, pszCardtype, nFileSize, binwidth );
	WriteOutputToFile ( output, pszFilename, pszCardtype );
	
	return 0;
}

void HistogramCPU( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
   ParseFile ( inbuf, outbuf, "", size, binwidth, 1 );
}

//-----------------------------------------------------------------------------
// Reads entire file contents in to utlbuffer
// input-buffer to load fileinto, filename
// output-filesize
//-----------------------------------------------------------------------------

int LoadFileIntoBuffer(CUtlBuffer &buf, char *pszFilename)
{
	// Open the file
	FILE *fh = fopen ( pszFilename, "rb" );
	if (fh == NULL)
	{
		printf ("Unable to open datafile. Check path/name.");
		exit( 0 );
	}

	fseek( fh, 0, SEEK_END );
	int nFileSize = ftell( fh );
	fseek ( fh, 0, SEEK_SET );

	// Read the file in one gulp
	buf.EnsureCapacity( nFileSize );
	int result=fread( buf.Base(), sizeof( char ), nFileSize, fh );
	fclose( fh );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, result );

	return nFileSize;

}

//-----------------------------------------------------------------------------
// Writes the contents of the utlbuffer to file
// datafile name becomes name.card.txt for results
//-----------------------------------------------------------------------------

void WriteOutputToFile( CUtlBuffer &buf, char *pszFilename, char *pszCardtype )
{
	char pszOutFilename[500]; 
	strncpy( pszOutFilename, pszFilename, strlen( pszFilename )-3 );
	pszOutFilename[strlen(pszFilename)-3]='\0';
	strcat ( pszOutFilename, pszCardtype );
	strcat ( pszOutFilename, ".txt\0" );

	FILE *fh = fopen ( pszOutFilename, "w" );
	if ( fh == NULL )
	{
		printf ( "Unable to open outputfile." );
		exit(0);
	}

	fwrite ( buf.Base(), sizeof( char ), buf.TellPut(), fh ); 
	fclose( fh );
}

//-----------------------------------------------------------------------------
// Strips off header fields from datafile
//-----------------------------------------------------------------------------

void ParseHeader( CUtlBuffer &buf )
{
	// remove first line of data (field labels)
	char pszTrash[256];
	buf.Scanf( "%s ", pszTrash);
	buf.Scanf( "%s ", pszTrash);
	buf.Scanf( "%s ", pszTrash);
	buf.Scanf( "%s ", pszTrash);
	buf.Scanf( "%s ", pszTrash);
	buf.Scanf( "%s \n", pszTrash);	
}

//-----------------------------------------------------------------------------
// Counts number of users that have a given cpu bin speed for videocard string
//
// Parses buffer into fields and puts results into outbuf
// size is size of the file read in
//-----------------------------------------------------------------------------

void ParseFile( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth, int nofilter)
{

	// Each bin is how many computers with the given card had cpus > cpubin 
	// and less than the next bin so cpu 200 covers cpu's from 200 to 299
	CUtlVector<int> nCpuList;
	CUtlVector<int> nQuantity;
	for ( int i=0; i <= 2400 ; i+=binwidth)	 // a reasonable cpu range 
	{
		nCpuList.AddToTail(i);
		nQuantity.AddToTail(0);
	}

	int totalUsers=0;
	while ( inbuf.TellGet() < size )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[300];
		int nNetSpeed, nRam, nCpu;
		int nRead=inbuf.Scanf( "%s %d %d %d ", pszVersion, &nNetSpeed, &nRam, &nCpu );
		
		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
		}
		pszCard[i] = '\0';
		
		if (nRead != 4)
			continue;

		// if card is our type
		if ( nofilter || Q_stristr ( pszCard, pszSearchString ) != NULL )
		{	  
			InsertResult(nCpu, nCpuList, nQuantity);
		}
		totalUsers++;
	}

	if (nCpuList.Size()>65000)
	{
		printf ("Too many points, increase bin width\n");
	}
	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	int total=0;
	outbuf.Printf ( "Cpu\tQuantity\n" ); // headers
	for ( int i=0; i < nCpuList.Size(); ++i )
	{
		outbuf.Printf ( "%d\t%d\n", nCpuList[i], nQuantity[i] );
		total+=nQuantity[i];
	}

	printf ("Users in this subset %d\n", total);
	printf ("Percent of dataset %.2f", ((float)total/(float)totalUsers)*100);
}


//-----------------------------------------------------------------------------
// given cpu bin, plots number of users in a given memory bin 
//
// Parses buffer into fields and puts results into outbuf
// size is size of the file read in
//-----------------------------------------------------------------------------

#define NMEMBINS 6
//#define NCPUBINS 48	  // binwidth 50
#define NCPUBINS 24 // binwidth 100

void ParseFile2( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
	binwidth=100;

	// Each bin is how many computers with the given card had cpus > cpubin 
	// and less than the next bin so cpu 200 covers cpu's from 200 to 299
	CUtlVector<int> nCpuList;
	CUtlVector<int> nMemList;
	int nQuantity[NMEMBINS][NCPUBINS];
	for (int i=0; i < NMEMBINS; ++i)
	{
		for (int j=0; j< NCPUBINS; ++j)
			nQuantity[i][j]=0;
	}

	for ( int i=0; i <= 2400 ; i+=binwidth)	 // a reasonable cpu range 
	{
		nCpuList.AddToTail(i);
	}

	nMemList.AddToTail(0);
	int basemem=64;
	while ( basemem < 2050 )
	{
		nMemList.AddToTail(basemem);
		basemem<<=1;
	}

	int totalUsers=0;
	int maxram=0;
	while ( inbuf.TellGet() < size )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		// scan through the rest of the junk
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			++i;
		}
		// determine cpu bin
		if (nCpu < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nCpu > nCpuList[nCpuList.Size()-1] )
			continue;
		i=0;
		while ( nCpu > nCpuList[i] )
		{
			++i;
		}
		int cpuIndex = i-1;
		assert(i<nCpuList.Size());
		// determine memory bin
		if (nRam < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nRam > nMemList[nMemList.Size()-1] )
			continue;
		i=0;
		while ( nRam > nMemList[i] )
		{
			++i;
		}
		int memIndex = i-1;
		assert(i<nMemList.Size());
		// insert data 
		assert(memIndex<NMEMBINS);
		assert(cpuIndex<NCPUBINS);
		(nQuantity[memIndex][cpuIndex])++;

		totalUsers++;
		//printf ("%d\n", totalUsers);
		if (nRam> maxram)
			maxram=nRam;
	}

	if (nCpuList.Size()>65000)
	{
		printf ("Too many points, increase bin width\n");
	}
	printf("TotalUsers %d\n", totalUsers);
	printf("Max ram %d\n", maxram);
	
	// write results to an output buffer
	outbuf.Printf ( "Cpu" ); // headers
	for (int j=1; j < nMemList.Size(); ++j)
		outbuf.Printf ("\tMemoryBin%d", nMemList[j]);
	outbuf.Printf ("\n");

	for ( int i=0; i < NCPUBINS; ++i )
	{
		outbuf.Printf ( "%d", nCpuList[i]);
		for (int j=0; j < NMEMBINS; ++j)
			outbuf.Printf ("\t%d", nQuantity[j][i]);
		outbuf.Printf ("\n");
	}
}



//-----------------------------------------------------------------------------
// given cpu bin, plots number of users in a given network speed bin 
//
// Parses buffer into fields and puts results into outbuf
// size is size of the file read in
//-----------------------------------------------------------------------------

#define NNETBINS 9

void ParseFile3( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
	binwidth=100;

	// Each bin is how many computers with the given card had cpus > cpubin 
	// and less than the next bin so cpu 200 covers cpu's from 200 to 299
	CUtlVector<int> nCpuList;
	CUtlVector<int> nNetList;
	int nQuantity[NNETBINS][NCPUBINS];
	for (int i=0; i < NNETBINS; ++i)
	{
		for (int j=0; j< NCPUBINS; ++j)
			nQuantity[i][j]=0;
	}

	for (int i=0; i <= 2400 ; i+=binwidth)	 // a reasonable cpu range 
	{
		nCpuList.AddToTail(i);
	}

	nNetList.AddToTail(0);
	nNetList.AddToTail(28);
	nNetList.AddToTail(33);
	nNetList.AddToTail(56);
	nNetList.AddToTail(112);
	nNetList.AddToTail(256);
	nNetList.AddToTail(512);
	nNetList.AddToTail(1000);
	nNetList.AddToTail(2000);

	int totalUsers=0;
	int maxspeed=0;
	while ( inbuf.TellGet() < size )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		// scan through the rest of the junk
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			++i;
		}
		// determine cpu bin
		if (nCpu < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nCpu > nCpuList[nCpuList.Size()-1] )
			continue;
		i=0;
		while ( nCpu > nCpuList[i] )
		{
			++i;
		}
		int cpuIndex = i-1;
		assert(i<nCpuList.Size());
		// determine netspeed bin
		if (nNetSpeed < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nNetSpeed > nNetList[nNetList.Size()-1] )
			continue;
		i=0;
		while ( nNetSpeed > nNetList[i] )
		{
			++i;
		}
		int netIndex = i;
		assert(i<nNetList.Size());
		// insert data 
		assert(netIndex<NNETBINS);
		assert(cpuIndex<NCPUBINS);
		(nQuantity[netIndex][cpuIndex])++;

		totalUsers++;
		//printf ("%d\n", totalUsers);
		if (nNetSpeed> maxspeed)
			maxspeed=nNetSpeed;
	}

	if (nCpuList.Size()>65000)
	{
		printf ("Too many points, increase bin width\n");
	}
	printf("TotalUsers %d\n", totalUsers);
	printf("Max ram %d\n", maxspeed);
	
	// write results to an output buffer
	outbuf.Printf ( "Cpu" ); // headers
	for (int j=0; j < nNetList.Size()-1; ++j)
		outbuf.Printf ("\t%d-%d", nNetList[j], nNetList[j+1]);
	outbuf.Printf ("\n");

	for (int i=0; i < NCPUBINS; ++i )
	{
		outbuf.Printf ( "%d", nCpuList[i]);
		for (int j=0; j < NNETBINS; ++j)
			outbuf.Printf ("\t%d", nQuantity[j][i]);
		outbuf.Printf ("\n");
	}
}



//-----------------------------------------------------------------------------
// given time bin, plots number of users in a given cpu TYPE over time,
// as the numbers came into the server
//
// Parses buffer into fields and puts results into outbuf
// size is size of the file read in
//-----------------------------------------------------------------------------

#define CPUBINS 3	  // 0=AMD, 1=Intel, 2=Unknown
#define MAXUSERS 750000

void TimeSeriesCPU( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
	binwidth=600;

	// unequally sized bins

	// Each bin is how many computers with the given card had cpus > cpubin 
	// and less than the next bin so cpu 200 covers cpu's from 200 to 299
	CUtlVector<int> nTimeList;
	int nQuantity[30][CPUBINS];
	for (int i=0; i < 30; ++i)
	{
		for (int j=0; j< CPUBINS; ++j)
			nQuantity[i][j]=0;
	}
					
	nTimeList.AddToTail(0);
	int bin=10000;
	int i=bin;
	for (i; i<=MAXUSERS; i=bin)
	{
		nTimeList.AddToTail(i);
		bin<<=1;
	}
	nTimeList.AddToTail(i);

	// for unequal bins
	int currentTimeBin=1;

	int totalUsers=0;
	while ( (inbuf.TellGet() < size) )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
			if (i >= 300)
				break;
		}
		// check for those blasted hackers
		if (i>= 300)
		{
			// read rest
			while (( chSentinel != '\n' ))
			{
				chSentinel = inbuf.GetChar();
			}
			continue;
		}

		pszCard[i] = '\0';

		int cpuIndex=2; // default is unknown
		if ( Q_stristr ( pszCard, "SSE" ) != NULL )
		{
		   cpuIndex=1; // intel
		}
		else if ( Q_stristr ( pszCard, "KNI" ) != NULL )
		{
			cpuIndex=1; // intel
		}
		else if ( Q_stristr ( pszCard, "3DNOW" ) != NULL )
		{
			cpuIndex=0;
		}

		// update nTimeList bin	if necessary
		if (nTimeList[currentTimeBin] <= totalUsers)
		{
			currentTimeBin++;
		}

		nQuantity[currentTimeBin][cpuIndex]++;
		assert(nQuantity[currentTimeBin][cpuIndex]>0);

		totalUsers++;
	}

	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	outbuf.Printf ( "CPU" ); // headers
	for (int j=1; j < nTimeList.Size(); ++j)
		outbuf.Printf ("\tUsersSoFar%d", nTimeList[j]);
	outbuf.Printf ("\n");

	for (int i=0; i < CPUBINS; ++i )
	{
		outbuf.Printf ( "-");
		for (int j=1; j < nTimeList.Size(); ++j)  // use NTIMEBINS for equal sized bins
			outbuf.Printf ("\t%d", nQuantity[j][i]);
		outbuf.Printf ("\n");
	}
}



//-----------------------------------------------------------------------------
// given time bin, plots number of users in a given video card TYPE over time,
// as the numbers came into the server
//
// Parses buffer into fields and puts results into outbuf
// size is size of the file read in
//-----------------------------------------------------------------------------

// 0=RIVA TNT
// 1=GeForce2 MX
// 2=Microsoft Corporation GDI Generic
// 3=GeForce2 GTS
// 4=Voodoo 3
// 5=Intel 810
// 6=GeForce3

#define CARDBINS 8	  

void TimeSeriesVCard( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
	// unequally sized bins
	CUtlVector<int> nTimeList;
	int nQuantity[30][CARDBINS];
	for (int i=0; i < 30; ++i)
	{
		for (int j=0; j< CARDBINS; ++j)
			nQuantity[i][j]=0;
	}
					
	nTimeList.AddToTail(0);
	int bin=10000;
	int i=bin;
	for (i; i<=MAXUSERS; i=bin)
	{
		nTimeList.AddToTail(i);
		bin<<=1;
	}
	nTimeList.AddToTail(i);

	//int currentTimeBin=1;

	// for unequal bins
	int currentTimeBin=1;
	int numTimeBins=nTimeList.Size();

	int totalUsers=0;
	while ( (inbuf.TellGet() < size) )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
			if (i >= 300)
				break;
		}
		// check for those blasted hackers
		if (i>= 300)
		{
			// read rest
			while (( chSentinel != '\n' ))
			{
				chSentinel = inbuf.GetChar();
			}
			continue;
		}

		pszCard[i] = '\0';
		
		// 0=RIVA TNT
		// 1=GeForce2 MX
		// 2=Microsoft Corporation GDI Generic
		// 3=GeForce2 GTS
		// 4=Voodoo 3
		// 5=Intel 810
		// 6=GeForce3
		// 7=All others
		int cardIndex=7; // default is other
		if ( Q_stristr ( pszCard, "RIVA TNT2" ) != NULL )
		{
			//printf ("%s", pszCard);
			cardIndex=0; 
		}
		else if ( Q_stristr ( pszCard, "GeForce2 MX" ) != NULL )
		{
			cardIndex=1; 
		}
		else if ( Q_stristr ( pszCard, "Microsoft Corporation GDI Generic" ) != NULL )
		{
			cardIndex=2;
		}
		else if ( Q_stristr ( pszCard, "GeForce2 GTS" ) != NULL )
		{
			cardIndex=3;
		}

		else if ( Q_stristr ( pszCard, "Voodoo3" ) != NULL )
		{
			cardIndex=4;
		}

		else if ( Q_stristr ( pszCard, "Intel 810" ) != NULL )
		{
			cardIndex=5;
		}

		else if ( Q_stristr ( pszCard, "GeForce3" ) != NULL )
		{
			cardIndex=6;
		}

		// update nTimeList bin	if necessary
		if (nTimeList[currentTimeBin] <= totalUsers)
		{
			currentTimeBin++;
		}

		nQuantity[currentTimeBin][cardIndex]++;

		totalUsers++;
	}

	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	outbuf.Printf ( "Video Card" ); // headers
	for (int j=1; j < nTimeList.Size(); ++j)
		outbuf.Printf ("\tUsersSoFar%d", nTimeList[j]);
	outbuf.Printf ("\n");

	for (int i=0; i < CARDBINS; ++i )
	{
		outbuf.Printf ( "-");
		for (int j=1; j < numTimeBins; ++j)  // use NTIMEBINS for equal sized bins
			outbuf.Printf ("\t%d", nQuantity[j][i]);
		outbuf.Printf ("\n");
	}
}


//-----------------------------------------------------------------------------
// Increment the proper bin's counter
//-----------------------------------------------------------------------------

void InsertResult( int nCpu, CUtlVector<int> &nCpuList, CUtlVector<int> &nQuantity )
{
	if (nCpu < 0) // handle corrupt data
		return;

	// handle outliers--bin is off the scale
	if ( nCpu > nCpuList[nCpuList.Size()-1] )
	{
		//nCpuList.AddToTail( nCpu );
		//nQuantity.AddToTail( 1 );
		return;
	}
	
	int i=0;
	while ( nCpu > nCpuList[i] )
	{
		++i;
	}
	nQuantity[i-1]++;		
}



void HistogramVidCards( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{

#define CARDHISTBINS 15
	// unequally sized bins
	int nQuantity[CARDHISTBINS];
	for (int j=0; j< CARDHISTBINS; ++j)
			nQuantity[j]=0;

	int totalUsers=0;
	while ( (inbuf.TellGet() < size) )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
			if (i >= 300)
				break;
		}
		// check for those blasted hackers
		if (i>= 300)
		{
			// read rest
			while (( chSentinel != '\n' ))
			{
				chSentinel = inbuf.GetChar();
			}
			continue;
		}

		pszCard[i] = '\0';
		
		// 0=RIVA TNT2
		// 1=GeForce2 MX
		// 2=Microsoft Corporation GDI Generic
		// 3=GeForce2 GTS
		// 4=Voodoo 3
		// 5=Intel 810
		// 6=GeForce3
		// 7=Riva TNT
		// 8=GeForce 256
		// 9=Rage 128
		// 10=S3 Savage4
		// 11=SiS 630
		// 12=Radeon DDR
		// 13=Rage 128 Pro
		// 14=All others
		
		int cardIndex=14; // default is other
		if ( Q_stristr ( pszCard, "RIVA TNT2" ) != NULL )
		{
			//printf ("%s", pszCard);
			cardIndex=0; 
		}
		else if ( Q_stristr ( pszCard, "GeForce2 MX" ) != NULL )
		{
			cardIndex=1; 
		}
		else if ( Q_stristr ( pszCard, "Microsoft Corporation GDI Generic" ) != NULL )
		{
			cardIndex=2;
		}
		else if ( Q_stristr ( pszCard, "GeForce2 GTS" ) != NULL )
		{
			cardIndex=3;
		}

		else if ( Q_stristr ( pszCard, "Voodoo3" ) != NULL )
		{
			cardIndex=4;
		}

		else if ( Q_stristr ( pszCard, "Intel 810" ) != NULL )
		{
			cardIndex=5;
		}
		else if ( Q_stristr ( pszCard, "GeForce3" ) != NULL )
		{
			cardIndex=6;
		}
		else if ( Q_stristr ( pszCard, "Riva TNT" ) != NULL )
		{
				cardIndex=7;
		}
		else if ( Q_stristr ( pszCard, "GeForce 256" ) != NULL )
		{
			cardIndex=8;
		}
		else if ( Q_stristr ( pszCard, "Rage 128 Pro" ) != NULL )
		{
			cardIndex=13;
		}
		else if ( Q_stristr ( pszCard, "Rage 128" ) != NULL )
		{
			cardIndex=9;
		}
		else if ( Q_stristr ( pszCard, "S3 Savage4" ) != NULL )
		{
			cardIndex=10;
		}
		else if ( Q_stristr ( pszCard, "SiS 630" ) != NULL )
		{
			cardIndex=11;
		}
		else if ( Q_stristr ( pszCard, "Radeon DDR" ) != NULL )
		{
			cardIndex=12;
		}

		nQuantity[cardIndex]++;

		totalUsers++;
	}

	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	outbuf.Printf ( "Video Card" ); // headers
	outbuf.Printf ("\tNumber of Users\n");
	outbuf.Printf ("\n");

	for ( int i=0; i < CARDHISTBINS; ++i )
	{
		outbuf.Printf ( "-");
		outbuf.Printf ("\t%d", nQuantity[i]);
		outbuf.Printf ("\n");
	}
}


void HistogramNetSpeed( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
#define NETHISTBINS 9
	// unequally sized bins
	int nQuantity[NETHISTBINS];
	for (int j=0; j< NETHISTBINS; ++j)
			nQuantity[j]=0;

	CUtlVector<int> nNetList;
	nNetList.AddToTail(0);
	nNetList.AddToTail(28);
	nNetList.AddToTail(33);
	nNetList.AddToTail(56);
	nNetList.AddToTail(112);
	nNetList.AddToTail(256);
	nNetList.AddToTail(512);
	nNetList.AddToTail(1100);
	nNetList.AddToTail(2000);

	int totalUsers=0;
	while ( (inbuf.TellGet() < size) )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
			if (i >= 300)
				break;
		}
		// check for those blasted hackers
		if (i>= 300)
		{
			// read rest
			while (( chSentinel != '\n' ))
			{
				chSentinel = inbuf.GetChar();
			}
			continue;
		}

		pszCard[i] = '\0';
		
		if (nNetSpeed < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nNetSpeed > nNetList[nNetList.Size()-1] )
			continue;
		i=0;
		while ( nNetSpeed > nNetList[i] )
		{
			++i;
		}
		int netIndex = i;
		nQuantity[netIndex]++;

		totalUsers++;
	}

	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	outbuf.Printf ( "Network Speed" ); // headers
	for (int j=0; j < nNetList.Size()-1; ++j)
			outbuf.Printf ("\t%d-%d", nNetList[j], nNetList[j+1]);
	outbuf.Printf ("\n");

	for ( int i=0; i < NETHISTBINS; ++i )
	{
		outbuf.Printf ( "-");
		outbuf.Printf ("\t%d", nQuantity[i]);
		outbuf.Printf ("\n");
	}
}




void HistogramRam( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth)
{
#define RAMHISTBINS 20
	// unequally sized bins
	int nQuantity[RAMHISTBINS];
	for (int j=0; j< RAMHISTBINS; ++j)
			nQuantity[j]=0;

	CUtlVector<int> nRamList;
	nRamList.AddToTail(0);
	int basemem=16;
	while ( basemem < 2050 )
	{
		nRamList.AddToTail(basemem);
		basemem<<=1;
	}

	int totalUsers=0;
	while ( (inbuf.TellGet() < size) )
	{	
		// Now parse the utlbuffer
		// file has fields of version, netspeed, ram, cpu and card type
		char pszVersion[256];
		int nNetSpeed, nRam, nCpu;
		inbuf.Scanf( "%s %d %d %d", pszVersion, &nNetSpeed, &nRam, &nCpu );

		char pszCard[300]; 
		char chSentinel = ' ';
		int i=0;
		while (( chSentinel != '\n' ))
		{
			chSentinel = inbuf.GetChar();
			pszCard[i] = chSentinel;
			++i;
			if (i >= 300)
				break;
		}
		// check for those blasted hackers
		if (i>= 300)
		{
			// read rest
			while (( chSentinel != '\n' ))
			{
				chSentinel = inbuf.GetChar();
			}
			continue;
		}

		pszCard[i] = '\0';
		
		if (nRam < 0) // handle corrupt data
			continue;
		// handle outliers--bin is off the scale
		if ( nRam > nRamList[nRamList.Size()-1] )
			continue;
		i=0;
		while ( nRam > nRamList[i] )
		{
			++i;
		}
		int ramIndex = i;
		nQuantity[ramIndex]++;

		totalUsers++;
	}

	printf("TotalUsers %d\n", totalUsers);
	
	// write results to an output buffer
	outbuf.Printf ( "RAM" ); // headers
	for (int j=0; j < nRamList.Size()-1; ++j)
		outbuf.Printf ("\t%d-%d", nRamList[j], nRamList[j+1]);
	outbuf.Printf ("\n");

	for ( int i=0; i < nRamList.Size(); ++i )
	{
		outbuf.Printf ( "-");
		outbuf.Printf ("\t%d", nQuantity[i]);
		outbuf.Printf ("\n");
	}
}



