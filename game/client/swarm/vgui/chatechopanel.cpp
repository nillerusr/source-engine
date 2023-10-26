#include "cbase.h"
#include "ChatEchoPanel.h"
#include <vgui/vgui.h>
#include "ienginevgui.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include "hud_element_helper.h"
#include "hud_basechat.h"
#include "asw_hud_chat.h"
#include "vgui_controls/AnimationController.h"
#include <vgui/ILocalize.h>
#include "clientmode_asw.h"

class CHudChat;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CASWChatHistoryEcho::CASWChatHistoryEcho( vgui::Panel *pParent, const char *panelName ) : BaseClass( pParent, "HudChatHistory" )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	InsertFade( -1, -1 );
}

void CASWChatHistoryEcho::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled(false);

	SetFont( pScheme->GetFont( "Default", IsProportional() ) );
	SetAlpha( 255 );

	Msg( "set history echo to %d\n", IsProportional() );
}

ChatEchoPanel::ChatEchoPanel(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{	
	m_pChatInputLine = new vgui::RichText(this, "EchoChatInputLine");
	//m_pChatHistory = new CASWChatHistoryEcho(this, "EchoChatHistory");
	m_pChatHistory = NULL;
	if (m_pChatHistory)
	{
		m_pChatHistory->SetMaximumCharCount( 127 * 100 );
	}
	m_InputBuffer[0] = '\0';

	m_hFont = vgui::INVALID_FONT;

	SetMouseInputEnabled(true);

	// hijack the chat!
	CHudChat *pHUDChat = dynamic_cast<CHudChat*>(GET_HUDELEMENT( CHudChat ));
	if (pHUDChat)
	{
		//pHUDChat->HijackChat(this);
	}
}

ChatEchoPanel::~ChatEchoPanel()
{
	// give it back
	CHudChat *pHUDChat = dynamic_cast<CHudChat*>(GET_HUDELEMENT( CHudChat ));
	if (pHUDChat)
	{
		//pHUDChat->ReturnChat(this);
	}
}

void asw_return_chat_f()
{
	ChatEchoPanel *pEcho = dynamic_cast<ChatEchoPanel*>(GetClientMode()->GetViewport()->FindChildByName("ChatEchoPanel", true));
	if (!pEcho)
	{
		Msg("failed to find echo panel\n");
		return;
	}
	// give it back
	CHudChat *pHUDChat = dynamic_cast<CHudChat*>(GET_HUDELEMENT( CHudChat ));
	if (pHUDChat)
	{
		//pHUDChat->ReturnChat(pEcho);
	}
}
ConCommand asw_return_chat("asw_return_chat", asw_return_chat_f);

void ChatEchoPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "Default", IsProportional() );
	m_pChatInputLine->SetFont(m_hFont);	
	int line_tall = vgui::surface()->GetFontTall(m_hFont);
	m_pChatInputLine->SetTall(line_tall);
	m_pChatInputLine->SetVerticalScrollbar(false);
	m_pChatInputLine->SetVisible(false);
}

bool ChatEchoPanel::AddCursorToBuffer(wchar_t *buffer, int &iCursorPos, bool bFlash)
{
	int integer_time = gpGlobals->curtime;
	if (!bFlash || 
		(fabs(gpGlobals->curtime - integer_time) < 0.25f ||
								(fabs(gpGlobals->curtime - integer_time) > 0.5f
								&& fabs(gpGlobals->curtime - integer_time) < 0.75f)))
	{
		buffer[iCursorPos] = L'|';
		iCursorPos++;
		return true;
	}
	return false;
}

void ChatEchoPanel::OnThink()
{
	CBaseHudChat *pHUDChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
	if (!pHUDChat)
		return;

	// ASWTODO - Unprotect all the hud chat stuff so this code below works - or figure out a better way of showing the chat during the various frames

	// copying the chat input line
	CBaseHudChatInputLine *pChatInput = pHUDChat->GetChatInput();
	if ( pChatInput)
	{
		vgui::TextEntry *pTextEntry = dynamic_cast<vgui::TextEntry*>(pChatInput->GetInputPanel());
		vgui::Label *pPrompt = pChatInput->GetPrompt();
		if (pTextEntry)
		{
			wchar_t buffer[256];
			pTextEntry->GetText(buffer, sizeof(buffer));
			bool bInputVisible = (pChatInput->IsVisible() && pTextEntry->IsVisible()
						&& pChatInput->GetAlpha() > 200  && pTextEntry->GetAlpha() > 200);
			//if (Q_strcmp(buffer, m_InputBuffer))
			{				
				pTextEntry->GetText(m_InputBuffer, sizeof(m_InputBuffer));
				if (bInputVisible)
				{
					// grab our prompt (or use a default)
					wchar_t prompt[64];
					if (pPrompt)
						pPrompt->GetText(prompt, sizeof(prompt));
					else
					{
						char promptbuffer[8];
						Q_snprintf(promptbuffer, sizeof(promptbuffer), "Say: ");
						g_pVGuiLocalize->ConvertANSIToUnicode(promptbuffer, prompt, sizeof( prompt ));
					}

					// copy x chars around the cursor into our echo
					const int max_copy_chars = 54;
					int line_length = wcslen(m_InputBuffer);
					int cursor_pos = 0;
					if (pTextEntry)
						cursor_pos = pTextEntry->GetTextCursorPos();
					int copy_start = MAX((cursor_pos + 20) - max_copy_chars, 0);
					int copy_end = copy_start + max_copy_chars;
					if (copy_end > line_length)
					{
						int diff = copy_end - line_length;
						copy_start = MAX(copy_start - diff, 0);
						copy_end = MIN(copy_start + max_copy_chars, line_length);
					}

					// copy the prompt over
					int prompt_len = wcslen(prompt);
					int iCopyCursor = 0;
					for (int i=0;i<prompt_len;i++)
					{
						buffer[iCopyCursor] = prompt[i];
						iCopyCursor++;
					}

					buffer[iCopyCursor] = L' ';
					iCopyCursor++;
					bool bAddedCursor = false;

					// check for drawing the cursor at the start
					if (cursor_pos == 0)
					{
						bAddedCursor |= AddCursorToBuffer(buffer, iCopyCursor, (cursor_pos == copy_end));  // if cursor is also at the end, we can flash it						
					}

					// now copy the section of our input line, insert the cursor in the right place
					for (int i=copy_start;i<copy_end;i++)
					{
						if (iCopyCursor >= 253)	// make sure we don't go over out limit for safety (never should though)
							iCopyCursor = 253;
						buffer[iCopyCursor] = m_InputBuffer[i];
						iCopyCursor++;
						if (i == (cursor_pos-1))
						{
							bAddedCursor |= AddCursorToBuffer(buffer, iCopyCursor, (cursor_pos == copy_end));  // if cursor is at the end, we can flash it
						}
					}

					// make sure we've added the cursor
					if (!bAddedCursor)
						AddCursorToBuffer(buffer, iCopyCursor, true);
					
					//if (copy_start == copy_end)
					//{
						//if (fabs(gpGlobals->curtime - integer_time) < 0.25f ||
								//(fabs(gpGlobals->curtime - integer_time) > 0.5f
								//&& fabs(gpGlobals->curtime - integer_time) < 0.75f)) 
						//{
							//buffer[iCopyCursor] = L'|';
							//iCopyCursor++;
						//}
					//}
					buffer[iCopyCursor] = L'\0';
					
					//if 
						//Q_snprintf(buffer, sizeof(buffer), "%s %s", prompt, m_InputBuffer);
					//else
						//Q_snprintf(buffer, sizeof(buffer), "%s %s|", prompt, m_InputBuffer);
					
					m_pChatInputLine->SetText(buffer);
					m_pChatInputLine->SetVisible(true);
				}
				else
				{
					m_pChatInputLine->SetText(m_InputBuffer);					
				}
			}			
			if (m_pChatInputLine->IsVisible() != bInputVisible)
				m_pChatInputLine->SetVisible(bInputVisible);
		}
		else
		{
			if (m_pChatInputLine->IsVisible())
				m_pChatInputLine->SetVisible(false);
		}
	}	
	/*
	// copying the chat history
	if ( pHUDChat->GetChatHistory() )
	{
		CHudChatHistory *pHistory = pHUDChat->GetChatHistory();
						
		//pHistory->GetText(0, m_InputBuffer, sizeof(m_InputBuffer) * sizeof(wchar_t));
		pHistory->GetEndOfText(ASW_CHAT_ECHO_HISTORY_WCHARS-1, m_InputBuffer, ASW_CHAT_ECHO_HISTORY_WCHARS);
		if (m_pChatHistory)
		{
			if (pHistory->IsVisible())
			{
				m_pChatHistory->SetText(m_InputBuffer);
				m_pChatHistory->SetVisible(true);

				m_pChatHistory->SetPaintBorderEnabled( false );
				m_pChatHistory->GotoTextEnd();
				m_pChatHistory->SetMouseInputEnabled( false );
			}
			else
			{
				m_pChatHistory->SetVisible(false);			
			}
		}
	}
	*/
}

void ChatEchoPanel::PerformLayout()
{
	float history_w = ScreenWidth() * 0.625f;
	float w = ScreenWidth() * 0.86f;

	int iLines = 4;
	if (ScreenHeight() <= 480)
		iLines = 3;
	//else if (ScreenHeight() <= 600)

	int line_tall = ScreenHeight() * 0.02f;	
	if (m_hFont != vgui::INVALID_FONT)	
	{
		line_tall = vgui::surface()->GetFontTall(m_hFont);
	}
	float h = line_tall * (iLines + 1.75f);
	SetSize(w, h);

	//Msg("ChatEchoPanel::PerformLayout line_tall=%d h=%f ScreenHeight=%d inputliney=%f\n",
		//line_tall, h, ScreenHeight(), float(line_tall) * (iLines + 0.5f));
	int x, y;
	GetPos(x, y);
	SetPos(x, ScreenHeight() - h);

	// centre the input line in whatever gap is left beneath our 4 chat lines
	m_pChatInputLine->SetBounds(0, line_tall * (iLines + 0.5f), w, line_tall+10);

	//Resize the History Panel so it fits more lines depending on the screen resolution.	
	int iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH;

	if (m_pChatHistory)
	{
		m_pChatHistory->GetBounds( iChatHistoryX, iChatHistoryY, iChatHistoryW, iChatHistoryH );

		iChatHistoryH = line_tall * iLines;

		m_pChatHistory->SetBounds( 0,  (h - line_tall) - iChatHistoryH, history_w, iChatHistoryH );
	}
}