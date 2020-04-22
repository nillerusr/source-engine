#ifndef _SCRIPLIB_H_
#define _SCRIPLIB_H_

#define	MAXTOKEN	128

extern void 	TL_LoadScriptFile(const char* filename);
extern void		TL_SetScriptData(char* data, int length);
extern void 	TL_FreeScriptFile(void);
extern char*	TL_GetToken(bool crossline);
extern char*	TL_GetQuotedToken(bool crossline);
extern void 	TL_UnGetToken(void);
extern bool 	TL_TokenAvailable(void);
extern void 	TL_SaveParser(void);
extern void 	TL_RestoreParser(void);
extern void		TL_ResetParser(void);
extern void		TL_SkipRestOfLine(void);
extern bool		TL_EndOfScript(void);
extern char*	TL_GetRawToken(void);

extern char		g_tl_token[MAXTOKEN];
extern char*	g_tl_scriptbuffer;
extern char*	g_tl_scriptptr;
extern char*	g_tl_scriptendptr;
extern int		g_tl_scriptsize;	// ydnar
extern int		g_tl_scriptline;
extern bool		g_tl_endofscript;

#endif
