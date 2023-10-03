#ifndef VSLIDERCONTROL_H
#define VSLIDERCONTORL_H

#include "basemodui.h"
#include "vgui/mousecode.h"

class CGameUIConVarRef;

namespace BaseModUI
{
	class BaseModHybridButton;

	class SliderControl : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( SliderControl , vgui::EditablePanel );
	public:
		SliderControl( vgui::Panel* parent, const char* panelName );
		virtual ~SliderControl( );

		virtual void SetEnabled(bool state);

		float GetCurrentValue();
		void SetCurrentValue( float value, bool bReset = false );
		float Increment( float stepSize = 1.0f );
		float Decrement( float stepSize = 1.0f );

		const char* GetConCommand();
		float GetConCommandDefault();
		float GetStepSize();
		float GetMin();
		float GetMax();
		void SetConCommand( const char* conCommand );
		void SetConCommandDefault( const char* conCommand );
		void SetStepSize( float stepSize );
		void SetMin( float min );
		void SetMax( float max );

		int GetTextureId();

		void SetInverse( bool inverse );
		bool GetInversed();

		bool IsDirty( void ) { return m_bDirty; }

		void Reset(); //get the current value stored in the convar and set it to that.
		void ResetSliderPosAndDefaultMarkers();

		virtual void NavigateTo();
		virtual void NavigateFrom();

	protected:
		virtual void ApplySettings( KeyValues* inResourceData );
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PerformLayout();
		virtual void OnKeyCodePressed( vgui::KeyCode code );
		virtual void OnMousePressed( vgui::MouseCode code );
		virtual void OnStartDragging();
		virtual void OnContinueDragging();
		virtual void OnFinishDragging( bool mousereleased, vgui::MouseCode code, bool aborted = false );
		virtual void OnCursorEntered();
		virtual void OnCursorExited();
		virtual void NavigateToChild( Panel *pNavigateTo );

		void HandleMouseInput( bool bDrag );

		CGameUIConVarRef* GetConVarRef();
		float UpdateProgressBar();
		void UpdateConVar();

	private:

		bool	m_bDragging;

		BaseModHybridButton *m_button;
		vgui::Label* m_lblSliderText;
		vgui::ProgressBar* m_prgValue;
		vgui::Panel* m_defaultMark;

		Color m_focusColor;
		Color m_unfocusColor;
		Color m_backgroundFocusColor;

		bool m_inverse;

		float m_min;
		float m_max;
		float m_curValue;
		float m_stepSize;
		CGameUIConVarRef* m_conVarRef;
		CGameUIConVarRef* m_conVarDefaultRef;

		bool m_bDirty;
	};
};

#endif