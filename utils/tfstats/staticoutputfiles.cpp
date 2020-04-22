//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Contains text files that are embedded into the source code so that
//	only the exe needs to be distributed. 
//
// $Workfile:     $
// $Date:         $
//
//------------------------------------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "TFStatsReport.h"

#ifdef WIN32
#define LBR \

#else

#define LBR
#endif

//the support.js file  contains the implementation of a SuperImage object used
//for the buttons on the nav bar that light up
char* CTFStatsReport::javaScriptSource=
"function BrowserType()\n"LBR
"{\n"LBR
"	this.isIE5=false;\n"LBR
"	this.isIEx=false;\n"LBR
"}\n"LBR
"\n"LBR
"new BrowserType();\n"LBR
"function BrowserType_VerifyIE5() \n"LBR
"{\n"LBR
"	if (navigator.appName == \"Microsoft Internet Explorer\")\n"LBR
"	{\n"LBR
"		this.isIEx=true;\n"LBR
"		if (navigator.appVersion.indexOf(\"MSIE 5.0\") != -1)\n"LBR
"		{\n"LBR
"			this.isIE5=true\n"LBR
"		}\n"LBR
"	}\n"LBR
"	else\n"LBR
"	{\n"LBR
"		this.isIEx=false;\n"LBR
"		this.isIE5=false;\n"LBR
"	}\n"LBR
"//	alert(\"IE5 = \" + this.isIE5.toString() + \"\\n\" + \"IE=\" + this.isIEx.toString() );\n"LBR
"}\n"LBR
"BrowserType.prototype.VerifyIE5=BrowserType_VerifyIE5;\n"LBR
"\n"LBR
"function BrowserType_getDetailBorder()\n"LBR
"{\n"LBR
"	if (!this.isIEx)\n"LBR
"		return \"border=1\";\n"LBR
"	else if (this.isIEx && !this.isIE5)\n"LBR
"		return \"border=1\";\n"LBR
"}\n"LBR
"BrowserType.prototype.getDetailBorder=BrowserType_getDetailBorder;\n"LBR
"function BrowserType_writeDetailTableTag(tableWid, numrows, numcols)\n"LBR
"{\n"LBR
"	if (!this.isIE5)\n"LBR
"		document.write(\"<table width= \"+tableWid.toString() + \" bordercolor=#999999 border=1 cellspacing=0 cellpadding=5 rows=\"+ numrows.toString() + \" cols=\"+ numcols.toString() + \">\\n\");\n"LBR
"	else \n"LBR
"		document.write(\"<table width= \"+tableWid.toString() + \" bordercolor=#999999 border=0 cellspacing=0 cellpadding=5 rows=\"+ numrows.toString() + \" cols=\"+ numcols.toString() + \">\\n\");\n"LBR
"}\n"LBR
"BrowserType.prototype.writeDetailTableTag=BrowserType_writeDetailTableTag;\n"LBR
"\n"LBR
"\n"LBR
"\n"LBR
"\n"LBR
"function SuperImage(imgName,onsrc,offsrc)\n"LBR
"{\n"LBR
"	this.HTMLimgName=imgName;\n"LBR
"	this.onSrc=onsrc;\n"LBR
"	this.offSrc=offsrc;\n"LBR
"	this.selected=false;\n"LBR
"	this.hover=false;\n"LBR
"//	alert(this.HTMLimgName.toString());\n"LBR
"//	alert(this.offSrc.toString());\n"LBR
"//	alert(this.onSrc.toString());\n"LBR
"}\n"LBR
"\n"LBR
" new SuperImage(\"crap\",\"crapon.gif\",\"crapoff.gif\");\n"LBR
"\n"LBR
"function SuperImage_over()\n"LBR
"{\n"LBR
"	this.hover=true;\n"LBR
"//	alert(\"blap\");\n"LBR
"	this.update();\n"LBR
"}\n"LBR
"SuperImage.prototype.over=SuperImage_over;\n"LBR
"	\n"LBR
"function SuperImage_on()\n"LBR
"{\n"LBR
"	this.selected=true;\n"LBR
"	this.update();\n"LBR
"}\n"LBR
"SuperImage.prototype.on=SuperImage_on;\n"LBR
"\n"LBR
"function SuperImage_off()\n"LBR
"{\n"LBR
"	this.selected=false;\n"LBR
"	this.update();\n"LBR
"}\n"LBR
"SuperImage.prototype.off=SuperImage_off;\n"LBR
"\n"LBR
"function SuperImage_out()\n"LBR
"{\n"LBR
"	this.hover=false;\n"LBR
"	this.update();\n"LBR
"}\n"LBR
"SuperImage.prototype.out=SuperImage_out;\n"LBR
"\n"LBR
"function SuperImage_update()\n"LBR
"{\n"LBR
"	if (document.images)\n"LBR
"	{ \n"LBR
"		if (this.hover == true || this.selected == true)\n"LBR
"		    document[this.HTMLimgName].src = this.onSrc;\n"LBR
"		else\n"LBR
"			document[this.HTMLimgName].src = this.offSrc; \n"LBR
"	}\n"LBR
"}\n"LBR
"SuperImage.prototype.update=SuperImage_update;\n"LBR
"\n";

//the style sheet for the TFStats report. All the html files link to this
//and glean informationa bout how they should format data based on
//data in this file
char* CTFStatsReport::styleSheetSource=
"A:link\n"LBR
"{\n"LBR
"    TEXT-DECORATION: none\n"LBR
"}\n"LBR
"A:hover\n"LBR
"{\n"LBR
"    TEXT-DECORATION: underline\n"LBR
"}\n"LBR
".nav\n"LBR
"{\n"LBR
"    MARGIN-LEFT: 20px;\n"LBR
"    MARGIN-RIGHT: 20px;\n"LBR
"    MARGIN-TOP: 24px\n"LBR
"}\n"LBR
".headline\n"LBR
"{\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT: bold 18pt arial,helvetica;\n"LBR
"    MARGIN-RIGHT: 20px;\n"LBR
"    MARGIN-TOP: 20px\n"LBR
"}\n"LBR
".headline2\n"LBR
"{\n"LBR
"    COLOR: #b1a976;\n"LBR
"    FONT: bold 18pt arial,helvetica;\n"LBR
"    MARGIN-RIGHT: 20px;\n"LBR
"    MARGIN-TOP: 20px\n"LBR
"}\n"LBR
".awards\n"LBR
"{\n"LBR
"    COLOR: #818358;\n"LBR
"    FONT: bold 9pt arial,helvetica;\n"LBR
"    MARGIN-LEFT: 10px;\n"LBR
"    MARGIN-RIGHT: 20px\n"LBR
"}\n"LBR
".brightawards\n"LBR
"{\n"LBR
"    COLOR: #b1a976;\n"LBR
"    FONT: bold 9pt arial,helvetica\n"LBR
"}\n"LBR
".awards2\n"LBR
"{\n"LBR
"    COLOR: #818358;\n"LBR
"    FONT: bold 9pt arial,helvetica\n"LBR
"}\n"LBR
".scores\n"LBR
"{\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT: bold 8pt arial,helvetica;\n"LBR
"    MARGIN-LEFT: 10px\n"LBR
"}\n"LBR
".header\n"LBR
"{\n"LBR
"    BACKGROUND: #646464;\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT: bold 10pt arial,helvetica\n"LBR
"}\n"LBR
".playerblue\n"LBR
"{\n"LBR
"    COLOR: #7c9fd9;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playerred\n"LBR
"{\n"LBR
"    COLOR: #a95d26;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".scores HR\n"LBR
"{\n"LBR
"    COLOR: #646464;\n"LBR
"    HEIGHT: 1px\n"LBR
"}\n"LBR
".server\n"LBR
"{\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT: 22pt impact,arial,helvetica;\n"LBR
"    MARGIN-LEFT: 211px;\n"LBR
"    MARGIN-RIGHT: 20px;\n"LBR
"    MARGIN-TOP: 10px\n"LBR
"}\n"LBR
".match\n"LBR
"{\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT: 13pt impact,arial,helvetica;\n"LBR
"    MARGIN-LEFT: 211px;\n"LBR
"    MARGIN-RIGHT: 20px\n"LBR
"}\n"LBR
".dialog\n"LBR
"{\n"LBR
"    FONT-FAMILY: Arial;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold;\n"LBR
"    MARGIN-LEFT: 10px\n"LBR
"}\n"LBR
".cvar\n"LBR
"{\n"LBR
"    COLOR: #857e59;\n"LBR
"    FONT-FAMILY: Arial;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".boardcell_br\n"LBR
"{\n"LBR
"    BORDER-BOTTOM: gray 2px solid;\n"LBR
"    BORDER-LEFT: gray 2px;\n"LBR
"    BORDER-RIGHT: gray 2px solid;\n"LBR
"    BORDER-TOP: gray 2px\n"LBR
"}\n"LBR
".boardcell_r\n"LBR
"{\n"LBR
"    BORDER-RIGHT: gray 2px solid\n"LBR
"}\n"LBR
".boardcell_b\n"LBR
"{\n"LBR
"    BORDER-BOTTOM: gray 2px solid\n"LBR
"}\n"LBR
".boardtext\n"LBR
"{\n"LBR
"    COLOR: white;\n"LBR
"    FONT-FAMILY: Arial;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
"BODY\n"LBR
"{\n"LBR
"    BACKGROUND-COLOR: black\n"LBR
"}\n"LBR
".whitetext\n"LBR
"{\n"LBR
"    COLOR: white;\n"LBR
"    FONT-FAMILY: Arial;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playerblue2\n"LBR
"{\n"LBR
"    COLOR: #7c9fd9;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playerred2\n"LBR
"{\n"LBR
"    COLOR: #a95d26;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".header2\n"LBR
"{\n"LBR
"    BACKGROUND-COLOR: #646464;\n"LBR
"    COLOR: #ffffff;\n"LBR
"    FONT-FAMILY: Arial;\n"LBR
"    FONT-STYLE: normal;\n"LBR
"    FONT-VARIANT: normal;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playeryellow2\n"LBR
"{\n"LBR
"    COLOR: #dfc500;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playergreen2\n"LBR
"{\n"LBR
"    COLOR: #4d8a58;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playeryellow\n"LBR
"{\n"LBR
"    COLOR: #dfc500;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n"LBR
".playergreen\n"LBR
"{\n"LBR
"    COLOR: #4d8a58;\n"LBR
"    FONT-SIZE: 9pt;\n"LBR
"    FONT-WEIGHT: bold\n"LBR
"}\n";
