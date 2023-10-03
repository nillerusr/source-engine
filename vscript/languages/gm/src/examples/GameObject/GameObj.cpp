//
// GameObj.cpp
//

#include <math.h>
#include "GameObj.h"
#include "App.h"
#include "ScriptObj.h"
#include "ScriptSys.h"

GameObj::GameObj()
{
  m_scriptObj = NULL;
  m_posX = -1;
  m_posY = -1;
  m_destX = m_posX;
  m_destY = m_posY;
  m_speed = 1.0f;
  m_colorIndex = App::COLOR_WHITE;

  m_scriptObj = new ScriptObj(this);
}


GameObj::~GameObj()
{
  if(m_scriptObj)
  {
    delete m_scriptObj;
  }
}


float GameObj::GetPosX() const
{
  return m_posX;
}


float GameObj::GetPosY() const
{
  return m_posY;
}


void GameObj::SetPos(const float a_x, const float a_y)
{
  m_posX = a_x;
  m_posY = a_y;

  App::Get()->ClipScreenCoordsf(m_posX, m_posY);

  m_destX = m_posX;
  m_destY = m_posY;
}


void GameObj::MoveTo(const float a_x, const float a_y)
{
  m_destX = a_x;
  m_destY = a_y;

  App::Get()->ClipScreenCoordsf(m_destX, m_destY);
}


void GameObj::SetSpeed(const float a_speed)
{
  m_speed = a_speed;
}


float GameObj::GetSpeed()
{
  return m_speed;
}


void GameObj::SetColor(int a_colorIndex)
{
  if(a_colorIndex >= App::COLOR_MIN && a_colorIndex < App::COLOR_MAX)
  {
    m_colorIndex = a_colorIndex;
  }
}


void GameObj::Update(float a_deltaTime)
{
  // NOTE: A real game would probably not update each object each frame
  //       but instead only update an object in a particular way when required.
  //       This example will do all updating in one place, and do so each frame.

  // Update movement
  {
    float dx = m_destX - m_posX;
    float dy = m_destY - m_posY;

    float len2 = dx*dx + dy*dy;

    if(len2 > 0.0f)
    {
      float moveThisFrame = a_deltaTime * m_speed;
      float distToGo = sqrtf(len2);

      // We can reach dest this frame
      if(moveThisFrame > distToGo)
      {
        m_posX = m_destX;
        m_posY = m_destY;
      }
      else
      {
        m_posX += dx * moveThisFrame;
        m_posY += dy * moveThisFrame;
      }
    }
  }
}


void GameObj::Render()
{
  
}


//////////////////////////////////////////////////
// Script 
//////////////////////////////////////////////////


int GM_CDECL GameObj::GameObj_MoveTo(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_FLOAT_PARAM(destX, 0);
  GM_CHECK_FLOAT_PARAM(destY, 1);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  thisPtr->MoveTo(destX, destY);

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_SetPos(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(2);
  GM_CHECK_FLOAT_PARAM(posX, 0);
  GM_CHECK_FLOAT_PARAM(posY, 1);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  thisPtr->SetPos(posX, posY);

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_GetPosX(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  a_thread->PushFloat(thisPtr->GetPosX());

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_GetPosY(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  a_thread->PushFloat(thisPtr->GetPosY());

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_IsValid(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    a_thread->PushInt(false);
  }
  else
  {
    a_thread->PushInt(true);
  }

  return GM_OK;
}



int GM_CDECL GameObj::GameObj_SetSpeed(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_FLOAT_PARAM(speed, 0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  thisPtr->SetSpeed(speed);

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_GetSpeed(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  a_thread->PushFloat(thisPtr->GetSpeed());

  return GM_OK;
}


int GM_CDECL GameObj::GameObj_SetColor(gmThread* a_thread)
{
  GM_CHECK_NUM_PARAMS(1);
  GM_CHECK_INT_PARAM(colorIndex, 0);
  GameObj* thisPtr = GetThisGameObj(a_thread);
  if(!thisPtr)
  {
    return GM_EXCEPTION;
  }

  thisPtr->SetColor(colorIndex);

  return GM_OK;
}


void GameObj::RegisterScriptBindings()
{
  static gmFunctionEntry gameObjTypeLib[] = 
  { 
    /*gm
      \lib GameObj
      \brief Game Object class
    */
    /*gm
      \function SetPos
      \brief Set position
      \param float a_posX New position X component
      \param float a_posY New position Y component
    */
    {"SetPos", GameObj_SetPos},
    /*gm
      \function GetPosX
      \brief Get position X
      \return float Get position X component.
    */
    {"GetPosX", GameObj_GetPosX},
    /*gm
      \function GetPosY
      \brief Get position Y
      \return float Get position Y component.
    */
    {"GetPosY", GameObj_GetPosY},
    /*gm
      \function IsValid
      \brief Is this a valid object, or has it been deleted or such
      \return int true if valid
    */
    {"IsValid", GameObj_IsValid},
    /*gm
      \function SetSpeed
      \brief Set speed
      \param a_speed New speed
    */
    {"SetSpeed", GameObj_SetSpeed},
    /*gm
      \function GetSpeed
      \brief Get speed
      \return float Current speed
    */
    {"GetSpeed", GameObj_GetSpeed},
    /*gm
      \function SetColor
      \brief Set color
      \param a_color New color
    */
    {"SetColor", GameObj_SetColor},

  };

  ScriptSys::Get()->GetMachine()->RegisterTypeLibrary(ScriptObj::GMTYPE_GAMEOBJ, gameObjTypeLib, sizeof(gameObjTypeLib) / sizeof(gameObjTypeLib[0]));
}
