

#ifndef HFX_INTERNAL
struct HFX_vect3Base32
{
	union
	{
		struct
		{
			float x;
			float y;
			float z;
		};
		struct
		{
			float x;
			float y;
			float z;
		} axis;
		struct
		{
			float right;
			float up;
			float backward;
		} direction;
		struct
		{
			float pitch;
			float yaw;
			float roll;
		} rotation;
		struct
		{
			float wide;
			float tall;
			float deep;
		} dimension;
		float m[3];
	};
};
// 192 == 3 * 64 == 3 doubles
struct HFX_vect3Base64
{
	union
	{
		struct
		{
			double x;
			double y;
			double z;
		};
		struct
		{
			double x;
			double y;
			double z;
		} axis;
		struct
		{
			double right;
			double up;
			double backward;
		} direction;
		struct
		{
			double pitch;
			double yaw;
			double roll;
		} rotation;
		struct
		{
			double wide;
			double tall;
			double deep;
		} dimension;
		double m[3];
	};
};
#endif
#ifdef HFX_INTERNAL
struct hfxVec3internal
#endif
#ifndef HFX_INTERNAL
struct hfxVec3
#endif
	: public HFX_vect3Base64
{
#ifndef HFX_INTERNAL
	HFX_INLINE HFX_EXPLICIT hfxVec3(const double &_x, const double &_y, const double &_z);
	HFX_INLINE HFX_EXPLICIT hfxVec3(const double array_d[3] );
	HFX_INLINE hfxVec3(const hfxVec3 &_copy);
#ifdef HFX_ZERO_INIT
	HFX_INLINE HFX_EXPLICIT hfxVec3();
#endif HFX_ZERO_INIT
	HFX_INLINE hfxVec3();
#endif
	
	HFX_INLINE double &operator [](int i);
	HFX_INLINE const double &operator[](int i) const;

	HFX_INLINE operator double *();
	HFX_INLINE operator const double *() const;
	
	HFX_INLINE hfxVec3 operator -() const;
	
	HFX_INLINE hfxVec3 operator -(const hfxVec3 &vect) const;
	HFX_INLINE hfxVec3 &operator -=(const hfxVec3 &vect);

	HFX_INLINE hfxVec3 operator +(const hfxVec3 &vect) const;
	HFX_INLINE hfxVec3 &operator +=(const hfxVec3 &vect);

	HFX_INLINE hfxVec3 operator *(const hfxVec3 &vect) const;
	HFX_INLINE hfxVec3 operator *(const double &df) const;
	//HFX_INLINE hfxVec3 operator *(const float f) const;
	//HFX_INLINE hfxVec3 operator *(const int i){return ((*this)*((double)i));};
	
	HFX_INLINE hfxVec3 operator /(const hfxVec3 &vect) const;
	HFX_INLINE hfxVec3 operator /(const double &df) const;
	//HFX_INLINE hfxVec3 operator /(const float f) const;
	//HFX_INLINE hfxVec3 operator /(const int i){return ((*this)/((double)i));};
	
	HFX_INLINE hfxVec3 &operator /=(const hfxVec3 &vect);
	HFX_INLINE hfxVec3 &operator /=(const double &df);
	//HFX_INLINE hfxVec3 &operator /=(const float f);
	//HFX_INLINE hfxVec3 &operator /=(const int i){return ((*this)/=((double)i));};

	HFX_INLINE hfxVec3 &operator *=(const hfxVec3 &vect);
	HFX_INLINE hfxVec3 &operator *=(const double &df);
	//HFX_INLINE hfxVec3 &operator *=(const float f);
	//HFX_INLINE hfxVec3 &operator *=(const int i){return ((*this)*=((double)i));};
	HFX_INLINE hfxVec3 &operator =(const hfxVec3 &vect);
	HFX_INLINE hfxVec3 &operator =(const double &all);
	//HFX_INLINE hfxVec3 &operator =(const float all);
	//HFX_INLINE hfxVec3 &operator =(const int i){return ((*this)=((double)i));}

	HFX_INLINE const double *operator *()const;
	HFX_INLINE double *operator *();
	
	HFX_INLINE hfxVec3 Cross(const hfxVec3 &vect) const;
	HFX_INLINE double Dot( const hfxVec3 &vect ) const;
	HFX_INLINE double Magnitude() const;
	// get normalize
	HFX_INLINE hfxVec3 Normalized() const;
	// normalize in place
	HFX_INLINE hfxVec3 &Normalize();
	HFX_INLINE bool IsValid() const;
	HFX_INLINE bool IsNaN() const;
#ifdef HFX_INTERNAL
	HFX_INLINE hfxVec3internal *GetVec3Pointer(){ return (this); };
	HFX_INLINE const hfxVec3internal *GetVec3Pointer()const{ return (this); };
	HFX_INLINE operator hfxVec3 &(){ return *(reinterpret_cast<hfxVec3*>(this)); }
	HFX_INLINE operator const hfxVec3 &()const { return *(reinterpret_cast<const hfxVec3*>(this)); }
#endif
#ifndef HFX_PAT_DOESNT_KNOW
	HFX_INLINE double Length() const;
	HFX_INLINE hfxVec3 Normal() const;
#endif
};
#ifndef HFX_INTERNAL
#ifndef HFX_NO_TYPEDEFS
typedef hfxVec3 Vect3;
#endif
struct hfxVec3F : public HFX_vect3Base32
{
	HFX_INLINE hfxVec3F(const float _x, const float _y, const float _z);
	HFX_INLINE hfxVec3F(const float array_f[3] );
	HFX_INLINE hfxVec3F(const hfxVec3F &_copy);
	HFX_INLINE hfxVec3F();
	
	HFX_INLINE float &operator [](int i);
	HFX_INLINE const float &operator[](int i) const;

	HFX_INLINE operator float *();
	HFX_INLINE operator const float *() const;
	
	HFX_INLINE hfxVec3F operator -() const;
	
	HFX_INLINE hfxVec3F operator -(const hfxVec3F &vect) const;
	HFX_INLINE hfxVec3F &operator -=(const hfxVec3F &vect);

	HFX_INLINE hfxVec3F operator +(const hfxVec3F &vect) const;
	HFX_INLINE hfxVec3F &operator +=(const hfxVec3F &vect);

	HFX_INLINE hfxVec3F operator *(const hfxVec3F &vect) const;
	HFX_INLINE hfxVec3F operator *(const float f) const;
	//HFX_INLINE hfxVec3F operator *(const double &df) const;
	//HFX_INLINE hfxVec3F operator *(const int i){return ((*this)*(double)i);};
	
	HFX_INLINE hfxVec3F operator /(const hfxVec3F &vect) const;
	HFX_INLINE hfxVec3F operator /(const float f) const;
	//HFX_INLINE hfxVec3F operator /(const double &df) const;
	//HFX_INLINE hfxVec3F operator /(const int i){return ((*this)/(double)i);};
	
	HFX_INLINE hfxVec3F &operator /=(const hfxVec3F &vect);
	HFX_INLINE hfxVec3F &operator /=(const float f);
	//HFX_INLINE hfxVec3F &operator /=(const double &df);
	//HFX_INLINE hfxVec3F &operator /=(const int i){return ((*this)/=(double)i);};

	HFX_INLINE hfxVec3F &operator *=(const hfxVec3F &vect);
	HFX_INLINE hfxVec3F &operator *=(const float f);
	//HFX_INLINE hfxVec3F &operator *=(const double &df);
	//HFX_INLINE hfxVec3F &operator *=(const int i){return ((*this)*=(double)i);};

	HFX_INLINE hfxVec3F &operator =(const hfxVec3F &vect);
	HFX_INLINE hfxVec3F &operator =(const float all);
	//HFX_INLINE hfxVec3F &operator =(const double &all);
	//HFX_INLINE hfxVec3F &operator =(const int i){return ((*this)=(double)i);}

	HFX_INLINE const float *operator *()const;
	HFX_INLINE float *operator *();
	
	HFX_INLINE hfxVec3F Cross(const hfxVec3F &vect) const;
	HFX_INLINE float Dot( const hfxVec3F &vect ) const;
	HFX_INLINE float Magnitude() const;
	// get normalize
	HFX_INLINE hfxVec3F Normalized() const;
	// normalize in place
	HFX_INLINE hfxVec3F &Normalize();
	HFX_INLINE bool IsValid() const;
	HFX_INLINE bool IsNaN() const;
#ifndef HFX_PAT_DOESNT_KNOW
	HFX_INLINE float Length() const;
	HFX_INLINE hfxVec3F Normal() const;
#endif
};

struct _box3Base
{
	union
	{
		struct{
			HFX_vect3Base64 bound_min;
			HFX_vect3Base64 bound_max;
		}box;
		HFX_vect3Base64 min_max[2];
		struct
		{
			double left;
			double down;
			double forward;
			double right;
			double up;
			double backward;
		}side;
		double m[6];
	};
};
struct BoundingBox3 : public _box3Base
{
	inline BoundingBox3(const double &left=0, const double &down=0, const double &forward=0,
		const double &right=0, const double &up=0, const double &backward=0)
	{
		side.left = left;
		side.down = down;
		side.forward = forward;
		side.right = right;
		side.up = up;
		side.backward = backward;
	}
	inline BoundingBox3(const hfxVec3 &bMin, const hfxVec3 &bMax)
	{
		BoxMin() = bMin;
		BoxMax() = bMax;
	}
	inline hfxVec3 &BoxMin(){return (hfxVec3&)box.bound_min;};
	inline const hfxVec3 &BoxMin() const {return (const hfxVec3&)box.bound_min;};
	inline hfxVec3 &BoxMax(){return (hfxVec3&)box.bound_max;};
	inline const hfxVec3 &BoxMax() const {return (const hfxVec3&)box.bound_max;};
	inline hfxVec3 Center() const { return (BoxMin() + BoxMax())/(double)2.0; };
	inline hfxVec3 Dimensions() const { return BoxMax() - BoxMin();};
	inline hfxVec3 SpaceBetweenCentered(const BoundingBox3 &other) const {return other.Dimensions() - Dimensions();};
	inline BoundingBox3 SpaceBetween(const BoundingBox3 &other) const {return other - *this;}
	inline BoundingBox3 &SetDimensions(const hfxVec3 &dimm) { hfxVec3 cent = Center(); hfxVec3 dim = dimm/2.0; BoxMin() = cent - dim; BoxMax() = cent + dim; return *this; }
	inline BoundingBox3 &SetCenter(const hfxVec3 &cent) { hfxVec3 dim = Dimensions(); BoxMin() = cent - dim; BoxMax() = cent + dim; return *this; }
	inline operator hfxVec3 *(){return (hfxVec3*)min_max;};
	inline operator const hfxVec3 *() const {return (const hfxVec3*)min_max;};
	inline operator double *() {return m;};
	inline operator const double *() const {return m;};
	inline BoundingBox3 operator -(const hfxVec3 &vect) const { return BoundingBox3( BoxMin() - vect, BoxMax() - vect ); }
	inline BoundingBox3 operator +(const hfxVec3 &vect) const { return BoundingBox3( BoxMin() + vect, BoxMax() + vect ); }
	inline BoundingBox3 operator *(const hfxVec3 &vect) const { return BoundingBox3( BoxMin() * vect, BoxMax() * vect ); }
	inline BoundingBox3 operator /(const hfxVec3 &vect) const { return BoundingBox3( BoxMin() / vect, BoxMax() / vect ); }
	inline BoundingBox3 operator *(const double &df) const{ return BoundingBox3( BoxMin() * df, BoxMax() * df ); }
	inline BoundingBox3 operator /(const double &df) const{ return BoundingBox3( BoxMin() / df, BoxMax() / df ); }
	inline BoundingBox3 operator -(const BoundingBox3 &bbox) const { return BoundingBox3( BoxMin() - bbox.BoxMin(), BoxMax() - bbox.BoxMax() ); }
	inline BoundingBox3 operator +(const BoundingBox3 &bbox) const { return BoundingBox3( BoxMin() + bbox.BoxMin(), BoxMax() + bbox.BoxMax() ); }
	inline BoundingBox3 operator *(const BoundingBox3 &bbox) const { return BoundingBox3( BoxMin() * bbox.BoxMin(), BoxMax() * bbox.BoxMax() ); }
	inline BoundingBox3 operator /(const BoundingBox3 &bbox) const { return BoundingBox3( BoxMin() / bbox.BoxMin(), BoxMax() / bbox.BoxMax() ); }
	inline BoundingBox3 &operator =(const BoundingBox3 &bbox) { BoxMin() = bbox.BoxMin(); BoxMax() = bbox.BoxMax(); return *this; }
	inline BoundingBox3 &operator -=(const hfxVec3 &vect){ BoxMin()-=vect; BoxMax()-=vect; return *this; }
	inline BoundingBox3 &operator +=(const hfxVec3 &vect){ BoxMin()+=vect; BoxMax()+=vect; return *this; }
	inline BoundingBox3 &operator *=(const hfxVec3 &vect){ BoxMin()*=vect; BoxMax()*=vect; return *this; }
	inline BoundingBox3 &operator /=(const hfxVec3 &vect){ BoxMin()/=vect; BoxMax()/=vect; return *this; }
	inline BoundingBox3 &operator *=(const double &df){ BoxMin()*=df; BoxMax()*=df; return *this; }
	inline BoundingBox3 &operator /=(const double &df){ BoxMin()/=df; BoxMax()/=df; return *this; }
	inline BoundingBox3 &operator -=(const BoundingBox3 &bbox){ BoxMin()-=bbox.BoxMin(); BoxMax()-=bbox.BoxMax(); return *this; }
	inline BoundingBox3 &operator +=(const BoundingBox3 &bbox){ BoxMin()+=bbox.BoxMin(); BoxMax()+=bbox.BoxMax(); return *this; }
	inline BoundingBox3 &operator *=(const BoundingBox3 &bbox){ BoxMin()*=bbox.BoxMin(); BoxMax()*=bbox.BoxMax(); return *this; }
	inline BoundingBox3 &operator /=(const BoundingBox3 &bbox){ BoxMin()/=bbox.BoxMin(); BoxMax()/=bbox.BoxMax(); return *this; }
};
#endif
#include "inl_hfxMath.h"
#ifdef HFX_INTERNAL
typedef ::hfxVec3 hfxVec3;
//VECTOR CLASS
#endif
