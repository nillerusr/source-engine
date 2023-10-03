#ifndef GAMEOBJ_H
#define GAMEOBJ_H

#include "gmThread.h"

//
// GameObj.h
//
// Example game object that uses the script interface component
//

// Fwd decls
class ScriptObj;



class GameObj
{
public:

  GameObj();
  virtual ~GameObj();

  ScriptObj* GetScriptObj()               { return m_scriptObj; }

  float GetPosX() const;
  float GetPosY() const;
  void SetPos(const float a_x, const float a_y);
  void MoveTo(const float a_x, const float a_y);
  void SetSpeed(const float a_speed);
  float GetSpeed();
  void SetColor(int a_colorIndex);

  void Update(float a_deltaTime);
  void Render();

  static void RegisterScriptBindings();

private:

  static int GM_CDECL GameObj_SetPos(gmThread* a_thread);
  static int GM_CDECL GameObj_GetPosX(gmThread* a_thread);
  static int GM_CDECL GameObj_GetPosY(gmThread* a_thread);
  static int GM_CDECL GameObj_IsValid(gmThread* a_thread);
  static int GM_CDECL GameObj_MoveTo(gmThread* a_thread);
  static int GM_CDECL GameObj_SetSpeed(gmThread* a_thread);
  static int GM_CDECL GameObj_GetSpeed(gmThread* a_thread);
  static int GM_CDECL GameObj_SetColor(gmThread* a_thread);

  ScriptObj* m_scriptObj;
  float m_posX;
  float m_posY;
  float m_destX;
  float m_destY;
  float m_speed;
  int m_colorIndex;

};


#endif //GAMEOBJ_H