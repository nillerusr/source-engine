#ifndef GAMEPADUI_PANEL_H
#define GAMEPADUI_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "gamepadui_interface.h"
#include "vgui_controls/Panel.h"

// There are a lot of really sucky macros in here
// solely because VGUI base class stuff completely
// falls apart when you use it with template classes.
// - Josh

#define GAMEPADUI_CONCAT_( x, y ) x ## y
#define GAMEPADUI_CONCAT( x, y ) GAMEPADUI_CONCAT_( x, y )

#define GAMEPADUI_STRINGIFY_( x ) #x
#define GAMEPADUI_STRINGIFY( x ) GAMEPADUI_STRINGIFY_( x )

#define GAMEPADUI_DEFAULT_PANEL_SCHEME GAMEPADUI_RESOURCE_FOLDER "schemepanel.res"

namespace SchemeValueTypes
{
    enum SchemeValueType
    {
        Float,
        Bool,
        ProportionalFloat,
        Color
    };
}
using SchemeValueType = SchemeValueTypes::SchemeValueType;

class SchemeValueMap
{
public:
    struct SchemeValue
    {
        const char* pszName;
        const char *pszScriptName;
        SchemeValueType Type;
        const char *pszDefaultValue;
        PANELLOOKUPFUNC pfnFunc;
    };

    void AddValueToSchemeMap( const char *pszName, const char *pszScriptName, SchemeValueType Type, char const *pszDefaultValue, PANELLOOKUPFUNC pfnFunc)
    {
        m_Values.AddToTail( SchemeValue{ pszName, pszScriptName, Type, pszDefaultValue, pfnFunc } );
    }

    void UpdateSchemeProperties( vgui::Panel* pPanel, vgui::IScheme* pScheme )
    {
        for ( SchemeValue& value : m_Values )
        {
            const char *pszResourceStr = pScheme->GetResourceString( value.pszScriptName );
            switch ( value.Type )
            {
                case SchemeValueTypes::ProportionalFloat:
                case SchemeValueTypes::Float:
                {
                    float flValue = 0.0f;
                    if ( pszResourceStr && *pszResourceStr)
                        flValue = atof( pszResourceStr );
                    else if ( value.pszDefaultValue && *value.pszDefaultValue )
                        flValue = atof( value.pszDefaultValue );
                    if ( value.Type == SchemeValueTypes::ProportionalFloat )
                        flValue = float( vgui::scheme()->GetProportionalScaledValueEx( pPanel->GetScheme(), int( flValue ) ) );

                    *static_cast< float* >( value.pfnFunc( pPanel ) ) = flValue;
                    break;
                }
                case SchemeValueTypes::Bool:
                {
                    int iVal = 0;
                    if ( pszResourceStr && *pszResourceStr )
                        iVal = atoi( pszResourceStr );
                    else if ( value.pszDefaultValue && *value.pszDefaultValue )
                        iVal = atoi( value.pszDefaultValue );
                    *static_cast<bool*>( value.pfnFunc( pPanel ) ) = !!iVal;
                    break;

                }
                case SchemeValueType::Color:
                {
                    Color cVal = Color( 0, 0, 0, 0 );
                    if ( value.pszDefaultValue && *value.pszDefaultValue )
                    {
                        float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
                        sscanf( value.pszDefaultValue, "%f %f %f %f", &r, &g, &b, &a );
                        cVal[ 0 ] = (unsigned char) r;
                        cVal[ 1 ] = (unsigned char) g;
                        cVal[ 2 ] = (unsigned char) b;
                        cVal[ 3 ] = (unsigned char) a;
                    }

                    cVal = pScheme->GetColor( value.pszScriptName, cVal );
                    *static_cast<Color*>( value.pfnFunc( pPanel ) ) = cVal;
                    break;
                }
            }
        }
    }

private:
    CUtlVector< SchemeValue > m_Values;
};


// Josh: Multi-line macro
#define GAMEPADUI_RUN_ANIMATION_COMMAND( name, interpolator ) \
    if ( GamepadUI::GetInstance().GetAnimationController() )            \
        GamepadUI::GetInstance().GetAnimationController()->RunAnimationCommand( this, #name , name##AnimationValue[state], 0.0f, name##AnimationDuration, interpolator )


// Josh: Multi-line macro
#define GAMEPADUI_PANEL_PROPERTY( type, name, scriptname, defaultvalue, typealias )                            \
    class PanelAnimationVar_##name;                                                                            \
    friend class PanelAnimationVar_##name;                                                                     \
    static void *GetVar_##name( vgui::Panel *panel )                                                           \
    {                                                                                                          \
        return &(( ThisClass *)panel)->name;                                                                   \
    }                                                                                                          \
    class PanelAnimationVar_##name                                                                             \
    {                                                                                                          \
    public:                                                                                                    \
        static void InitVar( SchemeValueMap *pMap )                                                            \
        {                                                                                                      \
            pMap->AddValueToSchemeMap( #name, scriptname, typealias, defaultvalue, ThisClass::GetVar_##name ); \
        }                                                                                                      \
        PanelAnimationVar_##name( SchemeValueMap *pMap )                                                       \
        {                                                                                                      \
            PanelAnimationVar_##name::InitVar( pMap );                                                         \
        }                                                                                                      \
    };                                                                                                         \
    PanelAnimationVar_##name m_##name##_register = { this };                                                   \
    type name;


// Josh: Multi-line macro
#define GAMEPADUI_BUTTON_ANIMATED_PROPERTY( type, name, scriptname, defaultvalue, typealias )                                                                   \
    class PanelAnimationVar_##name;                                                                                                                             \
    friend class PanelAnimationVar_##name;                                                                                                                      \
    static void *GetVar_##name( vgui::Panel *panel )                                                                                                            \
    {                                                                                                                                                           \
        return &(( ThisClass *)panel)->name;                                                                                                                    \
    }                                                                                                                                                           \
    static void *GetVar_##name##_Out( vgui::Panel *panel )                                                                                                      \
    {                                                                                                                                                           \
        return &(( ThisClass *)panel)->name##AnimationValue[ButtonStates::Out];                                                                                 \
    }                                                                                                                                                           \
    static void *GetVar_##name##_Over( vgui::Panel *panel )                                                                                                     \
    {                                                                                                                                                           \
        return &(( ThisClass *)panel)->name##AnimationValue[ButtonStates::Over];                                                                                \
    }                                                                                                                                                           \
    static void *GetVar_##name##_Pressed( vgui::Panel *panel )                                                                                                  \
    {                                                                                                                                                           \
        return &(( ThisClass *)panel)->name##AnimationValue[ButtonStates::Pressed];                                                                             \
    }                                                                                                                                                           \
    static void *GetVar_##name##_AnimationDuration( vgui::Panel *panel )                                                                                        \
    {                                                                                                                                                           \
        return &(( ThisClass *)panel)->name##AnimationDuration;                                                                                                 \
    }                                                                                                                                                           \
    class PanelAnimationVar_##name                                                                                                                              \
    {                                                                                                                                                           \
    public:                                                                                                                                                     \
        static void InitVar( SchemeValueMap *pMap )                                                                                                             \
        {                                                                                                                                                       \
            pMap->AddValueToSchemeMap( #name, scriptname ".Out",                typealias, defaultvalue, ThisClass::GetVar_##name );                            \
            pMap->AddValueToSchemeMap( #name, scriptname ".Out",                typealias, defaultvalue, ThisClass::GetVar_##name##_Out );                      \
            pMap->AddValueToSchemeMap( #name, scriptname ".Over",               typealias, defaultvalue, ThisClass::GetVar_##name##_Over );                     \
            pMap->AddValueToSchemeMap( #name, scriptname ".Pressed",            typealias, defaultvalue, ThisClass::GetVar_##name##_Pressed );                  \
            pMap->AddValueToSchemeMap( #name, scriptname ".Animation.Duration", SchemeValueTypes::Float, "0.2", ThisClass::GetVar_##name##_AnimationDuration ); \
            static bool bAdded = false;                                                                                                                         \
            if ( !bAdded )                                                                                                                                      \
            {                                                                                                                                                   \
                bAdded = true;                                                                                                                                  \
                AddToAnimationMap( #name, #type, #name, defaultvalue, false, ThisClass::GetVar_##name );                                                        \
            }                                                                                                                                                   \
        }                                                                                                                                                       \
        PanelAnimationVar_##name( SchemeValueMap *pMap )                                                                                                        \
        {                                                                                                                                                       \
            PanelAnimationVar_##name::InitVar( pMap );                                                                                                          \
        }                                                                                                                                                       \
    };                                                                                                                                                          \
    PanelAnimationVar_##name m_##name##_register = { this };                                                                                                    \
    type name;                                                                                                                                                  \
    type name##AnimationValue[ButtonStates::Count];                                                                                                             \
    float name##AnimationDuration;

#endif // GAMEPADUI_PANEL_H
