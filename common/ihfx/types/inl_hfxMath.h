#ifndef _INC_MATH
#include HFX_NO_MERGE(math.h)
#endif
#define HFXVEC3(other,oper)return HFXVEC3T(x##oper##other.x,y##oper##other.y,z##oper##other.z)
#define HFXVEC3F(other,oper) return HFXVEC3T(x##oper##other,y##oper##other,z##oper##other)
#ifndef HFX_INTERNAL
#define HFXVEC3SELF(other,oper)x##oper##other.x;y##oper##other.y;z##oper##other.z;return(*this)
#define HFXVEC3FSELF(other,oper) x##oper##other;y##oper##other;z##oper##other;return(*this)
#endif
#ifdef HFX_INTERNAL
#define HFXVEC3SELF(other,oper)x##oper##other.x;y##oper##other.y;z##oper##other.z;return *(reinterpret_cast<hfxVec3*>(this))
#define HFXVEC3FSELF(other,oper) x##oper##other;y##oper##other;z##oper##other;return *(reinterpret_cast<hfxVec3*>(this))
#endif
// 192 bit vect3
#define HFXVEC3T hfxVec3
#ifdef HFX_INTERNAL
#define HFXVEC3_DEFTYPE hfxVec3internal
#endif
#ifndef HFX_INTERNAL
#define HFXVEC3_DEFTYPE hfxVec3
#ifdef HFX_ZERO_INIT
HFX_INLINE HFX_EXPLICIT hfxVec3::hfxVec3()
{
	HFX_MEMSET(m,0,sizeof(double)*3);
}
#endif
HFX_INLINE hfxVec3::hfxVec3()
{
	__assume(0);
}
HFX_INLINE hfxVec3::hfxVec3(const double &_x, const double &_y, const double &_z){x=_x;y=_y;z=_z;};
HFX_INLINE hfxVec3::hfxVec3(const hfxVec3 &_copy){HFX_MEMCPY(m,_copy.m,sizeof(double)*3);};
HFX_INLINE hfxVec3::hfxVec3(const double *array_d){HFX_MEMCPY(m,array_d,sizeof(double)*3);}
#endif

HFX_INLINE bool HFXVEC3_DEFTYPE::IsValid()const{return(!IsNaN());}
HFX_INLINE bool HFXVEC3_DEFTYPE::IsNaN()const{return (x!=x||y!=y||z!=z);}

HFX_INLINE double HFXVEC3_DEFTYPE::Magnitude()const{return(sqrt(x*x+y*y+z*z));}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::Normalized()const{hfxVec3 copy(m);return(copy.Normalize());}
#ifndef HFX_INTERNAL
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::Normalize(){double l=Magnitude();x/=l;y/=l;z/=l;return(*this);}
#endif
#ifdef HFX_INTERNAL
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::Normalize(){double l=Magnitude();x/=l;y/=l;z/=l;return(*(reinterpret_cast<hfxVec3*>(this)));}
#endif
#ifndef HFX_PAT_DOESNT_KNOW
HFX_INLINE double HFXVEC3_DEFTYPE::Length()const{return(Magnitude());};
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::Normal()const{return Normalized();};
#endif
HFX_INLINE double HFXVEC3_DEFTYPE::Dot( const hfxVec3 &v )const{return(x*v.x+y*v.y+z*v.z);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::Cross(const hfxVec3 &v)const{return hfxVec3(x*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x);}

HFX_INLINE const double &HFXVEC3_DEFTYPE::operator [](int i)const{return(m[i]);};
HFX_INLINE double &HFXVEC3_DEFTYPE::operator [](int i){return(m[i]);};
HFX_INLINE HFXVEC3_DEFTYPE::operator const double *() const{return(m);}
HFX_INLINE HFXVEC3_DEFTYPE::operator double *(){return(m);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator -()const{return(hfxVec3(-x,-y,-z));}
HFX_INLINE double *HFXVEC3_DEFTYPE::operator *(){return(m);};
HFX_INLINE const double *HFXVEC3_DEFTYPE::operator *()const{return(m);};

HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator *(const hfxVec3 &v)const{HFXVEC3(v,*);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator +(const hfxVec3 &v)const{HFXVEC3(v,+);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator -(const hfxVec3 &v)const{HFXVEC3(v,-);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator /(const hfxVec3 &v)const{HFXVEC3(v,/);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator =(const hfxVec3 &v){HFXVEC3SELF(v,=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator *=(const hfxVec3 &v){HFXVEC3SELF(v,*=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator +=(const hfxVec3 &v){HFXVEC3SELF(v,+=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator -=(const hfxVec3 &v){HFXVEC3SELF(v,-=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator /=(const hfxVec3 &v){HFXVEC3SELF(v,/=);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator *(const double &d)const{HFXVEC3F(d,*);}
HFX_INLINE hfxVec3 HFXVEC3_DEFTYPE::operator /(const double &d)const{HFXVEC3F(d,/);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator *=(const double &d){HFXVEC3FSELF(d,*=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator /=(const double &d){HFXVEC3FSELF(d,/=);}
//HFX_INLINE hfxVec3 hfxVec3::operator *(const float f)const{HFXVEC3F((double)f,*);}
//HFX_INLINE hfxVec3 hfxVec3::operator /(const float f)const{HFXVEC3F((double)f,/);}
//HFX_INLINE hfxVec3 &hfxVec3::operator *=(const float f){HFXVEC3FSELF((double)f,*=);}
//HFX_INLINE hfxVec3 &hfxVec3::operator /=(const float f){HFXVEC3FSELF((double)f,/=);}
HFX_INLINE hfxVec3 &HFXVEC3_DEFTYPE::operator =(const double &d){x=y=z=d;return(*this);}
//HFX_INLINE hfxVec3 &hfxVec3::operator =(const float f){x=y=z=(double)f;return(*this);}
#undef HFXVEC3T
#undef HFXVEC3_DEFTYPE
#ifndef HFX_INTERNAL
// 96 bit vect3
#define HFXVEC3T hfxVec3F
HFX_INLINE hfxVec3F::hfxVec3F()
{
#ifdef HFX_ZERO_INIT
	HFX_MEMSET(m,0,sizeof(float)*3);
#endif
}
HFX_INLINE hfxVec3F::hfxVec3F(const float _x, const float _y, const float _z){x=_x;y=_y;z=_z;};
HFX_INLINE hfxVec3F::hfxVec3F(const hfxVec3F &_copy){HFX_MEMCPY(m,_copy.m,sizeof(float)*3);};
HFX_INLINE hfxVec3F::hfxVec3F(const float *array_d){HFX_MEMCPY(m,array_d,sizeof(float)*3);}

HFX_INLINE bool hfxVec3F::IsValid()const{return(!IsNaN());}
HFX_INLINE bool hfxVec3F::IsNaN()const{return (x!=x||y!=y||z!=z);}

HFX_INLINE float hfxVec3F::Magnitude()const{return(sqrt(x*x+y*y+z*z));}
HFX_INLINE hfxVec3F hfxVec3F::Normalized()const{float l=Magnitude();return(hfxVec3F(x/l,y/l,z/l));}
HFX_INLINE hfxVec3F &hfxVec3F::Normalize(){float l=Magnitude();x/=l;y/=l;z/=l;return(*this);}
#ifndef HFX_PAT_DOESNT_KNOW
HFX_INLINE float hfxVec3F::Length()const{return(Magnitude());};
HFX_INLINE hfxVec3F hfxVec3F::Normal()const{return Normalized();};
#endif
HFX_INLINE float hfxVec3F::Dot( const hfxVec3F &v )const{return(x*v.x+y*v.y+z*v.z);}
HFX_INLINE hfxVec3F hfxVec3F::Cross(const hfxVec3F &v)const{return hfxVec3F(x*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x);}

HFX_INLINE const float &hfxVec3F::operator [](int i)const{return(m[i]);};
HFX_INLINE float &hfxVec3F::operator [](int i){return(m[i]);};
HFX_INLINE hfxVec3F::operator const float *() const{return(m);}
HFX_INLINE hfxVec3F::operator float *(){return(m);}
HFX_INLINE hfxVec3F hfxVec3F::operator -()const{return(hfxVec3F(-x,-y,-z));}
HFX_INLINE float *hfxVec3F::operator *(){return(m);};
HFX_INLINE const float *hfxVec3F::operator *()const{return(m);};

HFX_INLINE hfxVec3F hfxVec3F::operator *(const hfxVec3F &v)const{HFXVEC3(v,*);}
HFX_INLINE hfxVec3F hfxVec3F::operator +(const hfxVec3F &v)const{HFXVEC3(v,+);}
HFX_INLINE hfxVec3F hfxVec3F::operator -(const hfxVec3F &v)const{HFXVEC3(v,-);}
HFX_INLINE hfxVec3F hfxVec3F::operator /(const hfxVec3F &v)const{HFXVEC3(v,/);}
HFX_INLINE hfxVec3F &hfxVec3F::operator =(const hfxVec3F &v){HFXVEC3SELF(v,=);}
HFX_INLINE hfxVec3F &hfxVec3F::operator *=(const hfxVec3F &v){HFXVEC3SELF(v,*=);}
HFX_INLINE hfxVec3F &hfxVec3F::operator +=(const hfxVec3F &v){HFXVEC3SELF(v,+=);}
HFX_INLINE hfxVec3F &hfxVec3F::operator -=(const hfxVec3F &v){HFXVEC3SELF(v,-=);}
HFX_INLINE hfxVec3F &hfxVec3F::operator /=(const hfxVec3F &v){HFXVEC3SELF(v,/=);}
//HFX_INLINE hfxVec3F hfxVec3F::operator *(const double &d)const{HFXVEC3F((float)d,*);}
//HFX_INLINE hfxVec3F hfxVec3F::operator /(const double &d)const{HFXVEC3F((float)d,/);}
//HFX_INLINE hfxVec3F &hfxVec3F::operator *=(const double &d){HFXVEC3FSELF((float)d,*=);}
//HFX_INLINE hfxVec3F &hfxVec3F::operator /=(const double &d){HFXVEC3FSELF((float)d,/=);}
HFX_INLINE hfxVec3F hfxVec3F::operator *(const float f)const{HFXVEC3F(f,*);}
HFX_INLINE hfxVec3F hfxVec3F::operator /(const float f)const{HFXVEC3F(f,/);}
HFX_INLINE hfxVec3F &hfxVec3F::operator *=(const float f){HFXVEC3FSELF(f,*=);}
HFX_INLINE hfxVec3F &hfxVec3F::operator /=(const float f){HFXVEC3FSELF(f,/=);}
//HFX_INLINE hfxVec3F &hfxVec3F::operator =(const double &d){x=y=z=(float)d;return(*this);}
HFX_INLINE hfxVec3F &hfxVec3F::operator =(const float f){x=y=z=f;return(*this);}
#endif
#undef HFXVEC3T
#undef HFXVEC3
#undef HFXVEC3SELF
#undef HFXVEC3F
#undef HFXVEC3FSELF