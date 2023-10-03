/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmVector3Lib.h"
#include "gmThread.h"
#include "gmMachine.h"
#include "gmHelpers.h"
#include <math.h>

// Must be last header
#include "memdbgon.h"


//
// Vector3
//

/// \brief Helper Vector3 class that resembles one used in a game.
/// Users will not use this class but cast to their own Vector3 implementation.
struct gmVector3
{
  static int DominantAxis(const gmVector3& a_vec)
  {
    float absX, absY, absZ;

    absX = fabsf(a_vec.m_x);
    absY = fabsf(a_vec.m_y);
    absZ = fabsf(a_vec.m_z);
  
    if(absY > absX)
    {
      if(absZ > absY) 
        { return 2;} // Dominant Z
      else 
        { return 1;} // Dominant Y
    }
    else if (absZ > absX)
      {return 2;} // Dominant Z
    else
      {return 0;} // Dominant X
  }
  static float Dot(const gmVector3& a_vec1, const gmVector3& a_vec2)
  {
    return (a_vec1.m_x * a_vec2.m_x + a_vec1.m_y * a_vec2.m_y + a_vec1.m_z * a_vec2.m_z);
  }
  static void Cross(const gmVector3& a_vec1, const gmVector3& a_vec2, gmVector3& a_result)
  {
    GM_ASSERT( (&a_result != &a_vec1) && (&a_result != &a_vec2) );

    a_result.m_x = (a_vec1.m_y * a_vec2.m_z) - (a_vec1.m_z * a_vec2.m_y);
    a_result.m_y = (a_vec1.m_z * a_vec2.m_x) - (a_vec1.m_x * a_vec2.m_z);
    a_result.m_z = (a_vec1.m_x * a_vec2.m_y) - (a_vec1.m_y * a_vec2.m_x);
  }
  static float Length(const gmVector3& a_vec)
  {
    return (float)sqrt(LengthSquared(a_vec));
  }
  static float LengthSquared(const gmVector3& a_vec)
  {
    return Dot(a_vec, a_vec);
  }
  static void Normalize(const gmVector3& a_vec, gmVector3& a_result)
  {
    float len2 = LengthSquared(a_vec);
    if(len2 != 0.0f)
    {
      float ooLen = 1.0f / (float)sqrt(len2);
      MulScalar(a_vec, ooLen, a_result);
    }
    else
    {
      a_result.m_x = 0.0f;
      a_result.m_y = 0.0f;
      a_result.m_z = 0.0f;
    }
  }
  static void Add(const gmVector3& a_vec1, const gmVector3& a_vec2, gmVector3& a_result)
  {
    a_result.m_x = a_vec1.m_x + a_vec2.m_x;
    a_result.m_y = a_vec1.m_y + a_vec2.m_y;
    a_result.m_z = a_vec1.m_z + a_vec2.m_z;
  }
  static void Sub(const gmVector3& a_vec1, const gmVector3& a_vec2, gmVector3& a_result)
  {
    a_result.m_x = a_vec1.m_x - a_vec2.m_x;
    a_result.m_y = a_vec1.m_y - a_vec2.m_y;
    a_result.m_z = a_vec1.m_z - a_vec2.m_z;
  }
  static void MulVector3(const gmVector3& a_vec1, const gmVector3& a_vec2, gmVector3& a_result)
  {
    a_result.m_x = a_vec1.m_x * a_vec2.m_x;
    a_result.m_y = a_vec1.m_y * a_vec2.m_y;
    a_result.m_z = a_vec1.m_z * a_vec2.m_z;
  }
  static void MulScalar(const gmVector3& a_vec, const float& a_scale, gmVector3& a_result)
  {
    a_result.m_x = a_vec.m_x * a_scale;
    a_result.m_y = a_vec.m_y * a_scale;
    a_result.m_z = a_vec.m_z * a_scale;
  }
  static void LerpPoints(const gmVector3& a_vecFrom, const gmVector3& a_vecTo, const float a_frac, gmVector3& a_result)  
  {
    a_result.m_x = a_vecFrom.m_x + a_frac * (a_vecTo.m_x - a_vecFrom.m_x);
    a_result.m_y = a_vecFrom.m_y + a_frac * (a_vecTo.m_y - a_vecFrom.m_y);
    a_result.m_z = a_vecFrom.m_z + a_frac * (a_vecTo.m_z - a_vecFrom.m_z);
  }

  // Set to Vector rotated by AxisAngle rotation 
  // Rotate a vector by a axis (unit vector) and angle (radians)
  // Only useful if you want to do this once off, otherwise, create a matrix and rotate multiple vectors more efficiently
  static void RotateAxisAngle(const gmVector3& a_point, const gmVector3& a_axis, const float a_angle, gmVector3& a_result)
  {
    //cos(t) V + (1 - cos(t)) (A dot V) A + sin(t) (A cross V).

    float sinAng, cosAng;
    gmVector3 temp1, temp2;
    gmSinCos(a_angle, sinAng, cosAng);

    MulScalar(a_point, cosAng, temp1);
    MulScalar(a_axis, (1 - cosAng) * Dot(a_axis, a_point), temp2);
    Cross(a_axis, a_point, a_result);
    MulScalar(a_result, sinAng, a_result);
    Add(temp1, a_result, a_result);
    Add(temp2, a_result, a_result);
  }

  static void RotateAboutX(const gmVector3& a_vec, float a_angle, gmVector3& a_result)
  {
    float sinAng, cosAng;

    gmSinCos(a_angle, sinAng, cosAng);

    a_result.m_y = a_vec.m_y * cosAng - a_vec.m_z * sinAng;
    a_result.m_z = a_vec.m_y * sinAng + a_vec.m_z * cosAng;
    a_result.m_x = a_vec.m_x;
  }

  static void RotateAboutY(const gmVector3& a_vec, float a_angle, gmVector3& a_result)
  {
    float sinAng, cosAng;

    gmSinCos(a_angle, sinAng, cosAng);

    a_result.m_z = a_vec.m_z * cosAng - a_vec.m_x * sinAng;
    a_result.m_x = a_vec.m_z * sinAng + a_vec.m_x * cosAng;
    a_result.m_y = a_vec.m_y;
  }

  static void RotateAboutZ(const gmVector3& a_vec, float a_angle, gmVector3& a_result)
  {
    float sinAng, cosAng;

    gmSinCos(a_angle, sinAng, cosAng);

    a_result.m_x = a_vec.m_x * cosAng - a_vec.m_y * sinAng;
    a_result.m_y = a_vec.m_x * sinAng + a_vec.m_y * cosAng;
    a_result.m_z = a_vec.m_z;
  }

  // Spherical linear interpolation between two vectors
  // Using quaternion style, find vector along smallest great circle between vectors
  // [sin((1-t)*A)/sin(A)]*P + [sin(t*A)/sin(A)]*Q
  static void SlerpVectors(const gmVector3& a_vecFrom, const gmVector3& a_vecTo, const float a_frac, gmVector3& a_result)
  {
    float sinA;
    float ang;
    float ooSinA;
    gmVector3 partSrc, partDst;
    float cosAng;

    cosAng = Dot(a_vecFrom, a_vecTo);
    if(fabsf(cosAng) >= 0.999f) //if From is very similar to To
    {
      a_result = a_vecFrom;
      return;
    }

    ang = acosf(cosAng);
    sinA = sinf(ang);
    ooSinA = 1.0f / sinA;

    MulScalar(a_vecFrom, sinf((1 - a_frac) * ang) * ooSinA, partSrc);
    MulScalar(a_vecTo, sinf(a_frac * ang) * ooSinA, partDst);

    Add(partSrc, partDst, a_result);
  }

  static void Project(const gmVector3& a_dir, const gmVector3& a_point, const float a_time, gmVector3& a_result)
  {
    a_result.m_x = a_point.m_x + a_dir.m_x * a_time;
    a_result.m_y = a_point.m_y + a_dir.m_y * a_time;
    a_result.m_z = a_point.m_z + a_dir.m_z * a_time;
  }

  union
  {
    float m_v[3];
    struct
    {
      float m_x;
      float m_y;
      float m_z;
    };
  };
};


/// \brief The Vector3 bindings
/// Just a set of useful functions, operators, etc. for Vector3
struct gmVector3Obj
{
  static int GM_CDECL DominantAxis(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    a_thread->PushInt( gmVector3::DominantAxis(*thisVec) );
    return GM_OK;
  }

  static int GM_CDECL Dot(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
    a_thread->PushFloat( gmVector3::Dot(*thisVec, *otherVec) );

    return GM_OK;
  }

  static int GM_CDECL Cross(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
    gmVector3* newVec = Alloc(a_thread->GetMachine(), false);

    gmVector3::Cross(*thisVec, *otherVec, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL RotateAxisAngle(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(2);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
        
    float angle = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 1, angle))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    
    gmVector3::RotateAxisAngle(*thisVec, *otherVec, angle, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL RotateX(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
        
    float angle = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 0, angle))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::RotateAboutX(*thisVec, angle, *newVec);
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL RotateY(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
        
    float angle = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 0, angle))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::RotateAboutY(*thisVec, angle, *newVec);
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL RotateZ(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
        
    float angle = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 0, angle))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::RotateAboutZ(*thisVec, angle, *newVec);
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL Length(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
    a_thread->PushFloat( gmVector3::Length(*thisVec) );

    return GM_OK;
  }

  static int GM_CDECL LengthSquared(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
    a_thread->PushFloat( gmVector3::LengthSquared(*thisVec) );

    return GM_OK;
  }

  static int GM_CDECL Normalize(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::Normalize(*thisVec, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL Clone(gmThread * a_thread)
  {
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    *newVec = *thisVec;
    a_thread->PushNewUser(newVec, GM_VECTOR3);
    
    return GM_OK;
  }

  static int GM_CDECL Set(gmThread * a_thread)
  {
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();
        
    if(a_thread->Param(0).m_type == GM_VECTOR3)
    {
      GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
      *thisVec = *otherVec;
    }
    else
    {
      GM_CHECK_NUM_PARAMS(3);
      if(!gmGetFloatOrIntParamAsFloat(a_thread, 0, thisVec->m_x))
      {
        return GM_EXCEPTION;
      }
      if(!gmGetFloatOrIntParamAsFloat(a_thread, 1, thisVec->m_y))
      {
        return GM_EXCEPTION;
      }
      if(!gmGetFloatOrIntParamAsFloat(a_thread, 2, thisVec->m_z))
      {
        return GM_EXCEPTION;
      }
    }

    return GM_OK;
  }

  static int GM_CDECL LerpToPoint(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(2);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    float frac = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 1, frac))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::LerpPoints(*thisVec, *otherVec, frac, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL SlerpToVector(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(2);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    float frac = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 1, frac))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::SlerpVectors(*thisVec, *otherVec, frac, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static int GM_CDECL ProjectFrom(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(2);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, otherVec, 0);
    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    float time = 0;
    if(!gmGetFloatOrIntParamAsFloat(a_thread, 1, time))
    {
      return GM_EXCEPTION;
    }

    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmVector3::Project(*thisVec, *otherVec, time, *newVec);
    
    a_thread->PushNewUser(newVec, GM_VECTOR3);

    return GM_OK;
  }

  static void GM_CDECL OpAdd(gmThread * a_thread, gmVariable * a_operands)
  {
    // Check types
    if(a_operands[0].m_type != GM_VECTOR3 || a_operands[1].m_type != GM_VECTOR3)
    {
      a_operands[0].Nullify();
      return;
    }

    // Get operands
    gmVector3* vecObjA = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    gmVector3* vecObjB = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[1].m_value.m_ref))->m_user;

    // Create new
    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
    // Perform operation
    gmVector3::Add(*vecObjA, *vecObjB, *newVec);

    // Return result
    a_operands[0].SetUser(newUserObj);
  }

  // a.SetAdd(b,c)  Demonstrate relative efficiency compared to operator version
  static int GM_CDECL SetAdd(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(2);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, a_vec1, 0);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, a_vec2, 1);

    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    // Perform operation
    gmVector3::Add(*a_vec1, *a_vec2, *thisVec);
    
    return GM_OK;
  }

  // a.Add(b)  Demonstrate relative efficiency compared to operator version
  static int GM_CDECL Add(gmThread * a_thread)
  {
    GM_CHECK_NUM_PARAMS(1);
    GM_CHECK_USER_PARAM(gmVector3*, GM_VECTOR3, a_vec1, 0);

    gmVector3* thisVec = (gmVector3*)a_thread->ThisUser_NoChecks();

    // Perform operation
    gmVector3::Add(*a_vec1, *thisVec, *thisVec);
    
    return GM_OK;
  }

  static void GM_CDECL OpSub(gmThread * a_thread, gmVariable * a_operands)
  {
    // Check types
    if(a_operands[0].m_type != GM_VECTOR3 || a_operands[1].m_type != GM_VECTOR3)
    {
      a_operands[0].Nullify();
      return;
    }

    // Get operands
    gmVector3* vecObjA = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    gmVector3* vecObjB = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[1].m_value.m_ref))->m_user;

    // Create new
    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
    // Perform operation
    gmVector3::Sub(*vecObjA, *vecObjB, *newVec);

    // Return result
    a_operands[0].SetUser(newUserObj);
  }

  static void GM_CDECL OpMul(gmThread * a_thread, gmVariable * a_operands)
  {
    // Check types
    if(a_operands[0].m_type == GM_VECTOR3 && a_operands[1].m_type == GM_VECTOR3)
    {
      // Get operands
      gmVector3* vecObjA = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
      gmVector3* vecObjB = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[1].m_value.m_ref))->m_user;

      // Create new
      gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
      gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
      // Perform operation
      gmVector3::MulVector3(*vecObjA, *vecObjB, *newVec);

      // Return result
      a_operands[0].SetUser(newUserObj);
    }
    else if(a_operands[0].m_type == GM_VECTOR3 && (a_operands[1].m_type != GM_VECTOR3))
    {
      // Get operands
      gmVector3* vecObjA = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
      float scaleB = 0.0f;
      
      if(a_operands[1].m_type == GM_FLOAT)
      {
        scaleB = a_operands[1].m_value.m_float;
      }
      else if(a_operands[1].m_type == GM_INT)
      {
        scaleB = (float)a_operands[1].m_value.m_int;
      }
       
      // Create new
      gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
      gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
      // Perform operation
      gmVector3::MulScalar(*vecObjA, scaleB, *newVec);

      // Return result
      a_operands[0].SetUser(newUserObj);
    }
    else if((a_operands[0].m_type != GM_VECTOR3) && a_operands[1].m_type == GM_VECTOR3)
    {
      // Get operands
      float scaleA = 0.0f;
      gmVector3* vecObjB = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[1].m_value.m_ref))->m_user;

      if(a_operands[0].m_type == GM_FLOAT)
      {
        scaleA = a_operands[0].m_value.m_float;
      }
      else if(a_operands[0].m_type == GM_INT)
      {
        scaleA = (float)a_operands[0].m_value.m_int;
      }

      // Create new
      gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
      gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
      // Perform operation
      gmVector3::MulScalar(*vecObjB, scaleA, *newVec);

      // Return result
      a_operands[0].SetUser(newUserObj);
    }
    else
    {
      a_operands[0].Nullify();
      return;
    }
  }

  static void GM_CDECL OpNeg(gmThread * a_thread, gmVariable * a_operands)
  {
    // Check types
    if(a_operands[0].m_type != GM_VECTOR3)
    {
      a_operands[0].Nullify();
      return;
    }

    // Get operands
    gmVector3* vecObjA = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;

    // Create new
    gmVector3* newVec = Alloc(a_thread->GetMachine(),false);
    gmUserObject* newUserObj = a_thread->GetMachine()->AllocUserObject(newVec, GM_VECTOR3);
    
    // Perform operation
    newVec->m_x = -vecObjA->m_x;
    newVec->m_y = -vecObjA->m_y;
    newVec->m_z = -vecObjA->m_z;

    // Return result
    a_operands[0].SetUser(newUserObj);
  }

  static void GM_CDECL OpGetDot(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;

    GM_ASSERT(a_operands[1].m_type == GM_STRING);
    gmStringObject* stringObj = (gmStringObject*)GM_OBJECT(a_operands[1].m_value.m_ref);
    const char* cstr = stringObj->GetString();
    if(stringObj->GetLength() != 1)
    {
      a_operands[0].Nullify();
      return;
    }

    if(cstr[0] == 'x')
    {
      a_operands[0].SetFloat(thisVec->m_x);
    }
    else if(cstr[0] == 'y')
    {
      a_operands[0].SetFloat(thisVec->m_y);
    }
    else if(cstr[0] == 'z')
    {
      a_operands[0].SetFloat(thisVec->m_z);
    }
    else
    {
      a_operands[0].Nullify();
    }
  }

  static void GM_CDECL OpSetDot(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;

    GM_ASSERT(a_operands[2].m_type == GM_STRING);
    gmStringObject* stringObj = (gmStringObject*)GM_OBJECT(a_operands[2].m_value.m_ref);
    const char* cstr = stringObj->GetString();
    if(stringObj->GetLength() != 1)
    {
      return;
    }

    float newFloat = 0.0f;
    if(a_operands[1].m_type == GM_FLOAT)
    {
      newFloat = a_operands[1].m_value.m_float;
    }
    else if(a_operands[1].m_type == GM_INT)
    {
      newFloat = (float)a_operands[1].m_value.m_int;
    }

    if(cstr[0] == 'x')
    {
      thisVec->m_x = newFloat;
    }
    else if(cstr[0] == 'y')
    {
      thisVec->m_y = newFloat;
    }
    else if(cstr[0] == 'z')
    {
      thisVec->m_z = newFloat;
    }
  }
  
  static void GM_CDECL OpGetInd(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    if(a_operands[1].m_type == GM_INT)
    {
      int index = a_operands[1].m_value.m_int;
      a_operands[0].SetFloat(thisVec->m_v[index]);
      return;
    }
    a_operands[0].Nullify();
  }

  static void GM_CDECL OpSetInd(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    if(a_operands[1].m_type == GM_INT)
    {
      int index = a_operands[1].m_value.m_int;
      if(index < 0 || index >= 3)
      {
        return;
      }
      
      if(a_operands[2].m_type == GM_FLOAT)
      {
        thisVec->m_v[index] = a_operands[2].m_value.m_float;
      }
      else if(a_operands[2].m_type == GM_INT)
      {
        thisVec->m_v[index] = (float)a_operands[2].m_value.m_int;
      }
      else
      {
        thisVec->m_v[index] = 0.0f;
      }
    }
  }

#if GM_BOOL_OP
  static void GM_CDECL OpBool(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    if (thisVec->m_x != 0 || thisVec->m_y != 0 && thisVec->m_z != 0)
    {
      a_operands[0] = gmVariable(1);
    }
    else
    {
      a_operands[0] = gmVariable(0);
    }
  }


  static void GM_CDECL OpNot(gmThread * a_thread, gmVariable * a_operands)
  {
    GM_ASSERT(a_operands[0].m_type == GM_VECTOR3);
    gmVector3* thisVec = (gmVector3*) ((gmUserObject*)GM_OBJECT(a_operands[0].m_value.m_ref))->m_user;
    if (thisVec->m_x != 0 || thisVec->m_y != 0 && thisVec->m_z != 0)
    {
      a_operands[0] = gmVariable(0);
    }
    else
    {
      a_operands[0] = gmVariable(1);
    }
  }
#endif // GM_BOOL_OP
  
  static int GM_CDECL Vector3(gmThread * a_thread)
  {
    int numParams = a_thread->GetNumParams();
    gmVector3* newVec = Alloc(a_thread->GetMachine(),true);
    if(numParams > 0)
    {
      gmGetFloatOrIntParamAsFloat(a_thread, 0, newVec->m_x);
    }
    if(numParams > 1)
    {
      gmGetFloatOrIntParamAsFloat(a_thread, 1, newVec->m_y);
    }
    if(numParams > 2)
    {
      gmGetFloatOrIntParamAsFloat(a_thread, 2, newVec->m_z);
    }
    a_thread->PushNewUser(newVec, GM_VECTOR3);
    return GM_OK;
  }

#if GM_USE_INCGC
  static void GM_CDECL GCDestruct(gmMachine * a_machine, gmUserObject* a_object)
  {
    GM_ASSERT(a_object->m_userType == GM_VECTOR3);
    gmVector3* object = (gmVector3*)a_object->m_user;
    Free(a_machine, object);
  }

#else //GM_USE_INCGC

  // Garbage collect 'Garbage Collect' function
  static void GM_CDECL Collect(gmMachine * a_machine, gmUserObject * a_object, gmuint32 a_mark)
  {
    GM_ASSERT(a_object->m_userType == GM_VECTOR3);
    gmVector3* object = (gmVector3*)a_object->m_user;
    Free(a_machine, object);
  }
#endif //GM_USE_INCGC
  
  static void GM_CDECL AsString(gmUserObject * a_object, char* a_buffer, int a_bufferLen)
  {
    gmVector3* vec = (gmVector3*)a_object->m_user;
    //Note '#' always display decimal place, 'g' display exponent if > precision or 4
    _gmsnprintf(a_buffer, a_bufferLen, "(%#.8g, %#.8g, %#.8g)", vec->m_x, vec->m_y, vec->m_z);
  }

  // Allocate memory for one object
  static gmVector3* Alloc(gmMachine* a_machine, bool a_clearToZero)
  {
    a_machine->AdjustKnownMemoryUsed(sizeof(gmVector3));
    gmVector3* newObj = (gmVector3*)s_mem.Alloc(); //Allocate our type
    if(a_clearToZero) //Optionally clear our members
    {
      newObj->m_x = 0.0f;
      newObj->m_y = 0.0f;
      newObj->m_z = 0.0f;
    }
    return newObj;
  }

  // Free memory for one object
  static void Free(gmMachine* a_machine, gmVector3* a_obj)
  {
    a_machine->AdjustKnownMemoryUsed(-(int)sizeof(gmVector3));
    s_mem.Free(a_obj);
  }

  // Only call when gmMachine is shutdown
  static void FreeAllMemory()
  {
    s_mem.ResetAndFreeMemory();
  }

  // Static members
  static gmMemFixed s_mem;                        ///< Memory for vector3s
};

// Static and Global instances
gmMemFixed gmVector3Obj::s_mem(sizeof(gmVector3), 64);
gmType GM_VECTOR3 = GM_NULL;

/// \brief Push a Vector3. (Eg. Use to return result).
void gmVector3_Push(gmThread* a_thread, const float* a_vec)
{
  gmVector3* newVec = gmVector3Obj::Alloc(a_thread->GetMachine(), false);
  *newVec = *(gmVector3*)a_vec;
  a_thread->PushNewUser(newVec, GM_VECTOR3);
}

/// \brief Create a Vector3 user object and fill it (Eg. use, to set as table member).
gmUserObject* gmVector3_Create(gmMachine* a_machine, const float* a_vec)
{
  gmVector3* newVec = gmVector3Obj::Alloc(a_machine, false);
  *newVec = *(gmVector3*)a_vec;
  return a_machine->AllocUserObject(newVec, GM_VECTOR3);
}

// libs

static gmFunctionEntry s_vector3Lib[] =
{
  /*gm
    \lib Vector3
  */
  /*gm
    \function Vector3
    \brief Create a Vector3 object
    \param float x or [0] optional (0)
    \param float y or [1] optional (0)
    \param float z or [2] optional (0)
  */
  {"Vector3", gmVector3Obj::Vector3},
};

static gmFunctionEntry s_vector3TypeLib[] = 
{ 
  /*gm
    \lib Vector3
    \brief Vector3 math class
  */
  /*gm
    \function DominantAxis
    \brief Find the index of the largest vector component.
    \this Vector to evaluate.
    \return int Index of largest component.
  */
  {"DominantAxis", gmVector3Obj::DominantAxis},
  /*gm
    \function Dot
    \brief Calculate the Dot (or Inner) Product of two vectors.
    \this Vector3 First vector.
    \param Vector3 Second vector.
    \return float result.
  */
  {"Dot", gmVector3Obj::Dot},
  /*gm
    \function Length
    \brief Length will return the length of the vector.
    \return float Dot product result that is cosine of the angle between the two vectors.
  */
  {"Length", gmVector3Obj::Length},
  /*gm
    \function Cross
    \brief Calculate the Cross (or Outer) Product of two vectors.
    \this Vector3 First vector.
    \param Vector3 Second vector.
    \return Vector3 Cross product resultant vector that is perpendicular to the two input vectors and length sine of the angle between them.
  */
  {"Cross", gmVector3Obj::Cross},
  /*gm
    \function Normalize
    \brief Return a unit length copy of this vector.
    \this Vector to be copied.
    \return Vector3 Unit length copy of this vector.
  */
  {"Normalize", gmVector3Obj::Normalize},
  /*gm
    \function LengthSquared
    \brief Return the squared length of the vector.
    \return float Squared length of the vector.
  */
  {"LengthSquared", gmVector3Obj::LengthSquared},
  /*gm
    \function ProjectFrom
    \brief Project a direction from a point.
    \this Vector3 Direction.
    \param Vector3 Start point;
    \param float Distance or time.
    \return Vector3 Projected result.
  */
  {"ProjectFrom", gmVector3Obj::ProjectFrom},
  /*gm
    \function Clone
    \brief Return a copy of this vector.
    \return A copy of this vector.
  */
  {"Clone", gmVector3Obj::Clone},
  /*gm
    \function Set
    \brief Set vector from other vector or 3 components.
  */
  {"Set", gmVector3Obj::Set},
  /*gm
    \function LerpToPoint
    \brief Linear interpolate between two 'point' vectors.
    \this Vector3 From vector.
    \param Vector3 To vector.
    \param float Fraction or time between 0 and 1.
    \return Vector3 Resulting inbetween vector.
  */
  {"LerpToPoint", gmVector3Obj::LerpToPoint},
  /*gm
    \function SlerpToVector
    \brief Spherical linear interpolate between two vectors.
    \this Vector3 From vector.
    \param Vector3 To vector.
    \param float Fraction or time between 0 and 1.
    \return Vector3 Resulting inbetween vector.
  */
  {"SlerpToVector", gmVector3Obj::SlerpToVector},
  /*gm
    \function RotateAxisAngle
    \brief Rotate around Axis by Angle.
    \this Vector3 Vector to rotate.
    \param Vector3 Unit length axis of rotation.
    \param float Angle amount to rotate.
    \return Vector3 Resulting rotated vector.
  */
  {"RotateAxisAngle", gmVector3Obj::RotateAxisAngle},
  /*gm
    \function RotateX
    \brief Rotate around X Axis by Angle.
    \this Vector3 Vector to rotate.
    \param float Angle amount to rotate.
    \return Vector3 Resulting rotated vector.
  */
  {"RotateX", gmVector3Obj::RotateX},
  /*gm
    \function RotateX
    \brief Rotate around Y Axis by Angle.
    \this Vector3 Vector to rotate.
    \param float Angle amount to rotate.
    \return Vector3 Resulting rotated vector.
  */
  {"RotateY", gmVector3Obj::RotateY},
  /*gm
    \function RotateZ
    \brief Rotate around Z Axis by Angle.
    \this Vector3 Vector to rotate.
    \param float Angle amount to rotate.
    \return Vector3 Resulting rotated vector.
  */
  {"RotateZ", gmVector3Obj::RotateZ},

  /*gm
    \function SetAdd
    \brief Add two vectors, store result in this. Demonstrate relative efficiency compared to operator version.
    \this Vector3 Result vector.
    \param Vector3 First vector.
    \param Vector3 Second vector.
  */
  {"SetAdd", gmVector3Obj::SetAdd},

  /*gm
    \function Add
    \brief Add vector to this. Demonstrate relative efficiency compared to operator version.
    \this Vector3 Result vector.
    \param Vector3 vector to add.
  */
  {"Add", gmVector3Obj::Add},
};


/// \brief Bind Vector3 library
void gmBindVector3Lib(gmMachine * a_machine)
{
  // Lib
  a_machine->RegisterLibrary(s_vector3Lib, sizeof(s_vector3Lib) / sizeof(s_vector3Lib[0]));
    
  // Register new user type
  GM_VECTOR3 = a_machine->CreateUserType("Vector3");

  // Operators
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_ADD, NULL, gmVector3Obj::OpAdd);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_SUB, NULL, gmVector3Obj::OpSub);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_MUL, NULL, gmVector3Obj::OpMul);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_NEG, NULL, gmVector3Obj::OpNeg);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_GETDOT, NULL, gmVector3Obj::OpGetDot);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_SETDOT, NULL, gmVector3Obj::OpSetDot);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_GETIND, NULL, gmVector3Obj::OpGetInd);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_SETIND, NULL, gmVector3Obj::OpSetInd);
#if GM_BOOL_OP
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_BOOL, NULL, gmVector3Obj::OpBool);
  a_machine->RegisterTypeOperator(GM_VECTOR3, O_NOT, NULL, gmVector3Obj::OpNot);
#endif // GM_BOOL_OP

  // Type Lib
  a_machine->RegisterTypeLibrary(GM_VECTOR3, s_vector3TypeLib, sizeof(s_vector3TypeLib) / sizeof(s_vector3TypeLib[0]));

  // Register garbage collection for type
#if GM_USE_INCGC
  a_machine->RegisterUserCallbacks(GM_VECTOR3, NULL, gmVector3Obj::GCDestruct, gmVector3Obj::AsString); 
#else //GM_USE_INCGC
  a_machine->RegisterUserCallbacks(GM_VECTOR3, NULL, gmVector3Obj::Collect, gmVector3Obj::AsString); 
#endif //GM_USE_INCGC
}

