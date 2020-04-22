//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Interface of CHTMLFile.
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef HTML_H
#define HTML_H
#ifdef WIN32
#pragma once
#endif
#include <stdio.h>


//------------------------------------------------------------------------------------------------------
// Purpose:  CHTMLFile represents an HTML text file that is being created. It has
// some misc helper stuff, like writing the body tag for you, and linking to the style
// sheet.  Also some little helper functions to do <br>s and <p>s
//------------------------------------------------------------------------------------------------------
class CHTMLFile
{
public:
	static const bool printBody;
	static const bool dontPrintBody;
	static const bool linkStyle;
	static const bool dontLinkStyle;

private:
	FILE* out;
	char filename[100];
	bool fBody;
public:
	CHTMLFile():out(NULL),fBody(false){}

	CHTMLFile(const char*filenm ,const char* title,bool fPrintBody=true,const char* bgimage=NULL,int leftmarg=0,int topmarg=20);
	void open(const char*);

	void write(PRINTF_FORMAT_STRING const char*,...);

	void hr(int len=0,bool alignleft=false);
	void br(){write("<br>\n");}
	void p(){write("<p>\n");};
	void img(const char* i){write("<img src=%s>\n",i);}

	void div(const char* cls){write("<div class=%s>\n",cls);}
	void div(){write("<div>\n");}
	void enddiv(){write("</div>");}
	
	void close();
	~CHTMLFile();
};
#endif // HTML_H
