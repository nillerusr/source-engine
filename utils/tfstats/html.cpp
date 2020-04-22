//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Implementation of CHTMLFile. see HTML.h for details
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include "TFStatsApplication.h"
#include "util.h"
#include "html.h"

//readability aids used when calling constructor
const bool CHTMLFile::printBody=true;
const bool CHTMLFile::dontPrintBody=false;
const bool CHTMLFile::linkStyle=true;
const bool CHTMLFile::dontLinkStyle=false;

using namespace std;
//------------------------------------------------------------------------------------------------------
// Function:	CHTMLFile::CHTMLFile
// Purpose:	
// Input:	filename - name of the html file that will be written
//				title -	title of the html document
//				fPrintBody - true if the <body> tag is to be written. 
//				bgimage - name of a background image, if desired
//				leftmarg - pixels on the left margin (if desired)
//				topmarg - pixels on the top margin (if desired)
//------------------------------------------------------------------------------------------------------
CHTMLFile::CHTMLFile(const char* filenm,const char* title,bool fPrintBody,const char* bgimage,int leftmarg, int topmarg)
{
	strcpy(filename,filenm);
	open(filename);
	
	
	write("<HEAD>\n");
	write("<TITLE> %s </TITLE>\n",title);
	string csshttppath(g_pApp->supportHTTPPath);
	csshttppath+="/style.css";

	write("<link rel=\"stylesheet\" href=\"%s\" type=\"text/css\">\n",csshttppath.c_str());
	write("</HEAD>\n");
	
	fBody=fPrintBody;
	if (fBody)
	{
		write("<BODY leftmargin=%li topmargin=%li ",leftmarg,topmarg);
		if (bgimage)
			write("background=%s",bgimage);
		else
			write("bgcolor = black");
		write(">\n");
	}
	
}



//------------------------------------------------------------------------------------------------------
// Function:	CHTMLFile::open
// Purpose:	 opens the html file, and writes <html>
// Input:	filename - the name of the file to open
//------------------------------------------------------------------------------------------------------
void CHTMLFile::open(const char* filename)
{
	out=fopen(filename,"wt");
	if (!out)
		g_pApp->fatalError("Can't open output file \"%s\"!\nPlease make sure that the file does not exist OR\nif the file does exit, make sure it is not read-only",filename);

	write("<HTML>\n");
}

//------------------------------------------------------------------------------------------------------
// Function:	CHTMLFile::write
// Purpose:	writes a string to the html file
// Input:	fmt - format string, like printf suite of functions
//				... - list of arguments
//------------------------------------------------------------------------------------------------------
void CHTMLFile::write(const char* fmt,...)
{
	va_list va;
	va_start(va,fmt);
	vfprintf(out,fmt,va);
}



//------------------------------------------------------------------------------------------------------
// Function:	CHTMLFile::close
// Purpose:	closes the html file, closing <body> and <html> tags if needed
//------------------------------------------------------------------------------------------------------
void CHTMLFile::close()
{
	if (!out)
		return;
	if (fBody)
		write("</BODY>\n");

	write("</HTML>\n\n");
#ifndef WIN32
		chmod(filename,PERMIT);
#endif
	fclose(out);
	out=NULL;
}

//------------------------------------------------------------------------------------------------------
// Function:	CHTMLFile::~CHTMLFile
// Purpose:	Destructor.  closes the file
//------------------------------------------------------------------------------------------------------
CHTMLFile::~CHTMLFile()
{
	close();
}




void CHTMLFile::hr(int len,bool alignleft)
{
	write("<hr %s width=%li>\n",alignleft?"align=left":"",len);
}