//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef RICHEDITCTRLEX_H
#define RICHEDITCTRLEX_H
#ifdef _WIN32
#pragma once
#endif

#pragma warning(push, 1)
#include <list>
#include <stack>
#pragma warning(pop)

using namespace std;
#pragma  warning(disable : 4786)


#ifdef RICHED_IMPL
	#ifdef _AFXEXT
		#define RICHED_DECL __declspec(dllexport)
	#else 
		#define RICHED_DECL 
	#endif
#else
	#ifdef _AFXEXT
		#define RICHED_DECL __declspec(dllimport)
	#else
		#define RICHED_DECL 
	#endif//_AFXEXT
#endif //RICHED_IMPL


class CRTFBuilder;
class CStringManip;
class CIntManip;


typedef CRTFBuilder &(*RTFSM_PFUNC)	(CRTFBuilder &);
typedef CRTFBuilder &(*RTFSM_STRINGPFUNC) (CRTFBuilder &, CString &); 
typedef CRTFBuilder &(*RTFSM_INTPFUNC) (CRTFBuilder &, int);
typedef CRTFBuilder &(*RTFSM_BOOLPFUNC)	(CRTFBuilder &, bool);
typedef CRTFBuilder &(*RTFSM_CONTROLPFUNC) (CRTFBuilder &, CRichEditCtrl &);


class CBoolString
{
	private:

		bool m_b;
		CString m_strOn;
		CString m_strOff;

	public:

		CBoolString(CString strOn, CString strOff = "")
		{
			m_strOn = strOn;
			m_strOff = strOff;
			m_b = false;
		}

		void operator=(bool b)
		{
			m_b = b;
		}

		operator CString() const
		{
			return m_b ? m_strOn : m_strOff;
		}
};


class CTextAttributes
{
	protected:

		int m_nFontSize;

		CBoolString m_bsBold;
		CBoolString m_bsUnderline;
		CBoolString m_bsItalic;
		CBoolString m_bsStrike;

		int m_nFontNumber;
		int m_nColorFground;
		int m_nColorBground;

	public:

		CTextAttributes()
			: m_bsBold("\\b") ,
			  m_bsUnderline("\\ul"),
			  m_bsItalic("\\i"),
			  m_bsStrike("\\strike") 
		{
			m_nColorBground = m_nColorFground = m_nFontNumber = m_nFontSize = 0;
			m_bsBold = false;
		}

		operator CString() const
		{
			CString s;
			s.Format("\\plain%s%s%s%s\\f%d\\fs%d\\cb%d\\cf%d ",
					((CString)m_bsBold).GetBuffer(),
					((CString)m_bsUnderline).GetBuffer(), ((CString)m_bsItalic).GetBuffer(), ((CString)m_bsStrike).GetBuffer(),
					m_nFontNumber ,
					m_nFontSize ,
					m_nColorBground,
					m_nColorFground);
			return s;
		}

	friend class CRTFBuilder;
};



class CFontList : public list<CString>
{
	public:

		operator CString() const
		{ 
			CString s;
			s = "{\\fonttbl";

			int nCount = 0;
			for (const_iterator i = begin(); i!=end(); i++)
			{
				CString s2;
				s2.Format("{\\f%d %s;}", nCount++, (const char*)(*i));
				s+=s2;
			}

			s+='}';
			return s;
		}

		void add(const CString &s)
		{
			push_back(s);
		}
};


class CColorList : public list<COLORREF>
{
	public:

		int add(COLORREF c)
		{ 
			push_back(c); 
			return size() - 1;
		}

		int find(COLORREF c)
		{
			int n = 0;
			for (iterator i = begin(); i != end(); i++, n++)
			{
				COLORREF cComp(*i);
				if (cComp == c)
				{
					return n;
				}
			}

			return -1;
		}

		
		operator CString() const
		{
			CString s("{\\colortbl");
			for (const_iterator i = begin(); i != end(); i++)
			{
				COLORREF c(*i);
				int r((c & 0x000000ff));
				int g((c >> 8) & 0x000000ff);
				int b((c >> 16) & 0x000000ff);

				CString s2;
				s2.Format("\\red%d\\green%d\\blue%d;", r, g, b);
				s += s2;
			}

			s += '}';
			return s;
		}
};


class RICHED_DECL CManip
{
	protected:

		CString	m_strVal;
		int m_nVal;
		LPVOID m_pFunc;
		bool m_bVal;

	public:

		virtual CRTFBuilder &go(CRTFBuilder &) = 0;

		CManip()
		{ 
			m_pFunc =  NULL; 
			m_nVal = 0; 
			m_strVal = ""; 
		}

		CManip(LPVOID p, CString s)
		{
			m_pFunc = p;
			m_strVal = s;
		}

		CManip(LPVOID p, int n)
		{
			m_pFunc = p;
			m_nVal = n;
		}

		CManip(LPVOID p, bool b)
		{
			m_pFunc = p;
			m_bVal = b;
		}
};


class RICHED_DECL CStringManip : public CManip
{
public:

	CStringManip(RTFSM_STRINGPFUNC p, CString s = "") : CManip ((LPVOID)p, s) {};

	CRTFBuilder &go(CRTFBuilder &b)
	{
		return((RTFSM_STRINGPFUNC)m_pFunc) (b, m_strVal);
	}
};


class RICHED_DECL CControlManip : public CManip
{
	protected:

		CRichEditCtrl &m_control;

	public:

		CControlManip(RTFSM_CONTROLPFUNC p, CRichEditCtrl& c) : m_control(c), CManip((LPVOID)p, (CString)"") {};

		CRTFBuilder &go(CRTFBuilder &b)
		{
			return((RTFSM_CONTROLPFUNC)m_pFunc)(b, m_control);
		}
};


class RICHED_DECL CIntManip : public CManip
{
	public:

		CIntManip(RTFSM_INTPFUNC p,	int n = 0) : CManip ((LPVOID)p, n) {};

		CRTFBuilder &go(CRTFBuilder &b)
		{
			return((RTFSM_INTPFUNC)m_pFunc)(b, m_nVal);
		}
};


class RICHED_DECL CBoolManip : public CManip
{
	public:

		CBoolManip(RTFSM_BOOLPFUNC p, bool b) : CManip((LPVOID)p, b) {};

		CRTFBuilder &go(CRTFBuilder &b)
		{
			return ((RTFSM_BOOLPFUNC	) m_pFunc)(	b, 	m_bVal);
		}
};


class RICHED_DECL CRTFBuilder
{
	protected:

		CString m_string;
		CTextAttributes m_attr;
		CFontList m_fontList;
		CColorList m_colorList;
		stack<CTextAttributes> m_attrStack;

	public:

		void bold (bool b = true);
		void strike(bool b = true);
		void italic(bool b = true);
		void underline(bool b = true);
		void normal();
		void size(int n);
		void font(const CString &i);
		void black();
		void blue();
		void green();
		void red();
		void color(COLORREF);
		void backColor(COLORREF);

		void push();
		void pop();
		
		CRTFBuilder &operator+=(CString &s);
		CRTFBuilder();
		virtual ~CRTFBuilder();

		void addFont(const CString &s)
		{
			m_fontList.add(s);
		}
		
		void addColor(COLORREF c)
		{
			m_colorList.add(c);
		}


		CRTFBuilder & operator+=(LPCTSTR p);

		operator CString() const
		{
			return m_string;
		}

		void write(CRichEditCtrl &);

		int colorCount() const
		{
			return m_colorList.size();
		}

	public:

		CRTFBuilder & operator<<(LPCTSTR);
		CRTFBuilder & operator<<(int);
		CRTFBuilder & operator>>(CRichEditCtrl &);

	friend RICHED_DECL CRTFBuilder &normal(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &push(CRTFBuilder&);
	friend RICHED_DECL CRTFBuilder &pop(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &black(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &red(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &green(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &blue(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &bold(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &strike(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &italic(CRTFBuilder &);
	friend RICHED_DECL CRTFBuilder &underline(CRTFBuilder &);
};


RICHED_DECL CControlManip	write		(CRichEditCtrl &);
RICHED_DECL CIntManip		normal		(int = 0);
RICHED_DECL CIntManip		push		(int = 0);
RICHED_DECL CIntManip		pop			(int = 0);
RICHED_DECL CIntManip		size		(int);
RICHED_DECL CIntManip		color		(int);
RICHED_DECL CIntManip		backColor	(int);
RICHED_DECL CIntManip		addColor	(int);
RICHED_DECL CIntManip		font		(int);
RICHED_DECL CStringManip	font		(LPCTSTR);
RICHED_DECL CStringManip	addFont		(LPCTSTR);
RICHED_DECL CBoolManip		bold		(bool);
RICHED_DECL CBoolManip		strike		(bool);
RICHED_DECL CBoolManip		italic		(bool);
RICHED_DECL CBoolManip		underline	(bool);

RICHED_DECL CRTFBuilder & operator<<(CRTFBuilder &, RTFSM_PFUNC);
RICHED_DECL CRTFBuilder & operator<<(CRTFBuilder &, CManip & m);


class RICHED_DECL CRichEditCtrlEx : public CRichEditCtrl
{
	public:

		// Construction
		CRichEditCtrlEx();

	public:

		void enable(bool b = true)
		{
			ModifyStyle(b ? WS_DISABLED : 0, b ?  0 : WS_DISABLED, 0);
		}
		
		void disable(bool b = true)
		{
			enable(!b);
		}

		void readOnly(bool b = true)
		{
			SetReadOnly(b);
		}

		void writable(bool b = true)
		{
			readOnly(!b);
		}

		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CRichEditCtrlEx)
		protected:
		virtual void PreSubclassWindow();
		//}}AFX_VIRTUAL

	public:
		virtual ~CRichEditCtrlEx();

	protected:
		//{{AFX_MSG(CRichEditCtrlEx)
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()
};

#endif // RICHEDITCTRLEX_H
