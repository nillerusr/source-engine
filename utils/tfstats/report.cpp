//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implematation of CReport
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "report.h"
#include "util.h"

//------------------------------------------------------------------------------------------------------
// Function:	CReport::makeHTMLPage
// Purpose:	makes a whole page out of the element that it is called on
// Input:	pageName - the name of the html file
//				pageTitle - the title of the document
//------------------------------------------------------------------------------------------------------
void CReport::makeHTMLPage(char* pageName,char* pageTitle)
{
	CHTMLFile Page(pageName,pageTitle);
	report(Page);
}
//------------------------------------------------------------------------------------------------------
// Function:	CReport::report
// Purpose:	generates the report's output and adds it to anHTML file
// Input:	html - the HTML file to add this report element to
//------------------------------------------------------------------------------------------------------
void CReport::report(CHTMLFile& html)
{
	generate();
	writeHTML(html);
}
