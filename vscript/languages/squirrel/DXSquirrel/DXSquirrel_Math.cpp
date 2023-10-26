#include "stdafx.h"
#include "sqplus.h"
#include "DXSquirrel_Binds.h"

_IMPL_NATIVE_CONSTRUCTION(Vector3,D3DXVECTOR3);

_MEMBER_FUNCTION_IMPL(Vector3,constructor)
{
	D3DXVECTOR3 temp;
	D3DXVECTOR3 *newv = NULL;
	StackHandler sa(v);
	int nparams = sa.GetParamCount();
	switch(nparams) {
	case 1:
		temp.x = 0;
		temp.y = 0;
		temp.z = 0;
		break;
	case 2:
		if(sa.GetType(2) == OT_INSTANCE) {
			_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
			if(vec)	temp = *vec;
			else return sa.ThrowError(_T("Vector3() invalid instance type"));
		}
		break;
	case 4:
		temp.x = sa.GetFloat(2);
		temp.y = sa.GetFloat(3);
		temp.z = sa.GetFloat(4);
		break;
	default:
		return sa.ThrowError(_T("Vector3() wrong parameters"));
	}
	newv = new D3DXVECTOR3(temp);
	return construct_Vector3(newv);
}


_MEMBER_FUNCTION_IMPL(Vector3,_set)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	const SQChar *s = sa.GetString(2);
	int index = s?s[0]:sa.GetInt(2);
	switch(index) {
	case 0: case 'x': case 'r':
		return sa.Return(self->x = sa.GetFloat(3));
		break;
	case 1: case 'y': case 'g':
		return sa.Return(self->y = sa.GetFloat(3));
		break;
	case 2: case 'z': case 'b':
		return sa.Return(self->z = sa.GetFloat(3));
		break;
	}

	return SQ_ERROR;
}

_MEMBER_FUNCTION_IMPL(Vector3,_get)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	const SQChar *s = sa.GetString(2);
	if(s && (s[1] != 0))
		return SQ_ERROR;
	int index = s && (s[1] == 0)?s[0]:sa.GetInt(2);
	switch(index) {
		case 0: case 'x': case 'r': return sa.Return(self->x); break;
		case 1: case 'y': case 'g':	return sa.Return(self->y); break;
		case 2: case 'z': case 'b': return sa.Return(self->z); break;
	}
	return SQ_ERROR;
}

_MEMBER_FUNCTION_IMPL(Vector3,_nexti)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	int index = -1;
	if(sa.GetType(2) == OT_NULL) {
		index = -1;
	}else {
		const SQChar *s = sa.GetString(2);
		if(s && (s[1] != 0))
			return SQ_ERROR;
		index = s && (s[1] == 0)?s[0]:sa.GetInt(2);
	}
	switch(index) {
		case 0xFFFFFFFF: return sa.Return(_T("x"));
		case 0: case 'x': case 'r': return sa.Return(_T("y"));
		case 1: case 'y': case 'g': return sa.Return(_T("z"));
		case 2: case 'z': case 'b': return sa.Return();
	}
	return sa.Return(_T("invalid index"));
}

_MEMBER_FUNCTION_IMPL(Vector3,_cmp)
{

	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	if((*self) == (*vec))
		return sa.Return(0);
	if((*self) < (*vec))
		return sa.Return(-1);
	return sa.Return(1);
}

_MEMBER_FUNCTION_IMPL(Vector3,_add)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tv = (*self)+(*vec);
	SquirrelObject so = new_Vector3(tv);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Vector3,_sub)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tv = (*self)-(*vec);
	SquirrelObject so = new_Vector3(tv);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Vector3,_mul)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tv = (*self)*sa.GetFloat(2);
	SquirrelObject so = new_Vector3(tv);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Vector3,_div)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tv = (*self)/sa.GetFloat(2);
	SquirrelObject so = new_Vector3(tv);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Vector3,DotProduct)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	return sa.Return(D3DXVec3Dot(self,vec));
}

_MEMBER_FUNCTION_IMPL(Vector3,CrossProduct)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 ret;
	D3DXVec3Cross(&ret,self,vec);
	SquirrelObject so = new_Vector3(ret);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Vector3,SquareDistance)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tmp = *self - *vec;
	return sa.Return(D3DXVec3LengthSq(&tmp));
}

_MEMBER_FUNCTION_IMPL(Vector3,Distance)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tmp = *self - *vec;
	return sa.Return(D3DXVec3Length(&tmp));
}

_MEMBER_FUNCTION_IMPL(Vector3,Length)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	return sa.Return(D3DXVec3Length(self));
}

_MEMBER_FUNCTION_IMPL(Vector3,SquareLength)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	return sa.Return(D3DXVec3LengthSq(self));
}

_MEMBER_FUNCTION_IMPL(Vector3,Normalize)
{
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	D3DXVec3Normalize(self,self);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Vector3,GetNormalized)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXVECTOR3,Vector3);
	D3DXVECTOR3 tmp;
	D3DXVec3Normalize(&tmp,self);
	SquirrelObject so = new_Vector3(tmp);
	return sa.Return(so);
}


_BEGIN_CLASS(Vector3)
_MEMBER_FUNCTION(Vector3,constructor,-1,_T(".n|xnn"))
_MEMBER_FUNCTION(Vector3,_set,3,_T("xs|n"))
_MEMBER_FUNCTION(Vector3,_get,2,_T("xs|n"))
_MEMBER_FUNCTION(Vector3,_add,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,_sub,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,_mul,2,_T("xn"))
_MEMBER_FUNCTION(Vector3,_div,2,_T("xn"))
_MEMBER_FUNCTION(Vector3,_nexti,2,_T("x"))
_MEMBER_FUNCTION(Vector3,_cmp,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,DotProduct,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,CrossProduct,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,SquareDistance,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,Distance,2,_T("xx"))
_MEMBER_FUNCTION(Vector3,Length,1,_T("x"))
_MEMBER_FUNCTION(Vector3,SquareLength,1,_T("x"))
_MEMBER_FUNCTION(Vector3,Normalize,1,_T("x"))
_MEMBER_FUNCTION(Vector3,GetNormalized,1,_T("x"))
_END_CLASS(Vector3)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
_IMPL_NATIVE_CONSTRUCTION(Matrix,D3DXMATRIX);

_MEMBER_FUNCTION_IMPL(Matrix,constructor)
{
	D3DXMATRIX temp;
	D3DXMATRIX *newm = NULL;
	StackHandler sa(v);
	switch(sa.GetParamCount()) {
		case 1:
			D3DXMatrixIdentity(&temp);
			break;
		case 2:
			if(sa.GetType(2) == OT_INSTANCE) {
				_CHECK_INST_PARAM(mat,2,D3DXMATRIX,Matrix);
				if(mat)	temp = *mat;
				else return sa.ThrowError(_T("Matrix() invalid instance type"));
			}
			else {
				SquirrelObject arr = sa.GetObjectHandle(2);
				if(arr.Len() != 16) {
					return sa.ThrowError(_T("Matrix(array) need a 16 elements array"));
				}
				FLOAT *fp = (FLOAT*)&temp.m;
				SquirrelObject idx,val;
				if(arr.BeginIteration()) {
					while(arr.Next(idx,val)) {
						fp[idx.ToInteger()] = val.ToFloat();
					}
					arr.EndIteration();
				}
			}
			break;
		default:
			return sa.ThrowError(_T("Matrix() wrong number of parameters"));
			break;
	}
	newm = new D3DXMATRIX(temp);
	return construct_Matrix(newm);
}

_MEMBER_FUNCTION_IMPL(Matrix,_set)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	int index = sa.GetInt(2);
	if(index < 0 && index > 4*4)
		return SQ_ERROR;
	((FLOAT *)self->m)[index] = sa.GetFloat(2);
	return SQ_OK;
}

_MEMBER_FUNCTION_IMPL(Matrix,_get)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	int index = sa.GetInt(2);
	if(index < 0 && index > 4*4)
		return SQ_ERROR;
	return sa.Return(((FLOAT *)self->m)[index]);
}

_MEMBER_FUNCTION_IMPL(Matrix,_add)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(mat,2,D3DXMATRIX,Matrix);
	D3DXMATRIX tm = (*self)+(*mat);
	SquirrelObject so = new_Matrix(tm);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,_sub)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(mat,2,D3DXMATRIX,Matrix);
	D3DXMATRIX tm = (*self)-(*mat);
	SquirrelObject so = new_Matrix(tm);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,_mul)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	int t = sa.GetType(2);
	if(t == OT_INSTANCE) {
		_CHECK_INST_PARAM(mat,2,D3DXMATRIX,Matrix);
		D3DXMATRIX tm = (*self)*(*mat);
        SquirrelObject so = new_Matrix(tm);
        return sa.Return(so);
	}
	D3DXMATRIX tm = (*self)*sa.GetFloat(2);
    SquirrelObject so = new_Matrix(tm);
    return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,_div)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMATRIX tm = (*self)/sa.GetFloat(2);
    SquirrelObject so = new_Matrix(tm);
    return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,CreateIdentity)
{
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMatrixIdentity(self);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,CreateRotationAxis)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMatrixRotationAxis(self,vec,sa.GetFloat(3));
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,CreateRotationAngles)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMatrixRotationAxis(self,vec,sa.GetFloat(3));
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,CreateScalingMatrix)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMatrixScaling(self,sa.GetFloat(2),sa.GetFloat(3),sa.GetFloat(4));
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,CreateTranslationMatrix)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMatrixTranslation(self,sa.GetFloat(2),sa.GetFloat(3),sa.GetFloat(4));
	return 0;
}
_MEMBER_FUNCTION_IMPL(Matrix,CreateLookAtMatrix)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(eye,2,D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(at,3,D3DXVECTOR3,Vector3);
	_CHECK_INST_PARAM(up,4,D3DXVECTOR3,Vector3);
	D3DXMatrixLookAtLH(self,eye,at,up);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,CreatePerspectiveFovMatrix)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMatrixPerspectiveFovLH(self,sa.GetFloat(2),sa.GetFloat(3),sa.GetFloat(4),sa.GetFloat(5));
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,RotateAngles)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMatrixRotationYawPitchRoll(self,vec->y,vec->x,vec->z);
	return 0;
}


_MEMBER_FUNCTION_IMPL(Matrix,RotateAxis)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMatrixRotationAxis(self,vec,sa.GetFloat(3));
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,Translate)
{
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMATRIX temp;
	D3DXMatrixTranslation(&temp,vec->x,vec->y,vec->z);
	D3DXMatrixMultiply(self,&temp,self);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,Scale)
{
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXMATRIX temp;
	D3DXMatrixScaling(&temp,vec->x,vec->y,vec->z);
	D3DXMatrixMultiply(self,&temp,self);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,GetInverse)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMATRIX temp;
	D3DXMatrixInverse(&temp,NULL,self);
	SquirrelObject so = new_Matrix(temp);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,GetTransposed)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMATRIX temp;
	D3DXMatrixTranspose(&temp,self);
	SquirrelObject so = new_Matrix(temp);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,GetInverseTransposed)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMATRIX temp;
	D3DXMatrixInverse(&temp,NULL,self);
	D3DXMatrixTranspose(&temp,&temp);
	SquirrelObject so = new_Matrix(temp);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,Invert)
{
	_CHECK_SELF(D3DXMATRIX,Matrix);
	D3DXMatrixInverse(self,NULL,self);
	return 0;
}

_MEMBER_FUNCTION_IMPL(Matrix,TransformCoord)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 ret;
	D3DXVec3TransformCoord(&ret,vec,self);
	SquirrelObject so = new_Vector3(ret);
	return sa.Return(so);
}

_MEMBER_FUNCTION_IMPL(Matrix,TransformNormal)
{
	StackHandler sa(v);
	_CHECK_SELF(D3DXMATRIX,Matrix);
	_CHECK_INST_PARAM(vec,2,D3DXVECTOR3,Vector3);
	D3DXVECTOR3 ret;
	D3DXVec3TransformNormal(&ret,vec,self);
	SquirrelObject so = new_Vector3(ret);
	return sa.Return(so);
}


_BEGIN_CLASS(Matrix)
_MEMBER_FUNCTION(Matrix,constructor,-1,_T(".a|x"))
_MEMBER_FUNCTION(Matrix,_set,2,_T("xn"))
_MEMBER_FUNCTION(Matrix,_get,2,_T("xn"))
_MEMBER_FUNCTION(Matrix,_add,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,_sub,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,_mul,2,_T("xx|n"))
_MEMBER_FUNCTION(Matrix,_div,2,_T("xn"))
_MEMBER_FUNCTION(Matrix,CreateIdentity,1,_T("x"))
_MEMBER_FUNCTION(Matrix,CreateRotationAxis,3,_T("xxn"))
_MEMBER_FUNCTION(Matrix,CreateRotationAngles,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,CreateScalingMatrix,4,_T("xnnn"))
_MEMBER_FUNCTION(Matrix,CreateTranslationMatrix,4,_T("xnnn"))
_MEMBER_FUNCTION(Matrix,CreateLookAtMatrix,4,_T("xxxx"))
_MEMBER_FUNCTION(Matrix,CreatePerspectiveFovMatrix,5,_T("xnnnn"))
_MEMBER_FUNCTION(Matrix,RotateAngles,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,RotateAxis,3,_T("xxn"))
_MEMBER_FUNCTION(Matrix,Translate,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,Scale,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,GetInverse,1,_T("x"))
_MEMBER_FUNCTION(Matrix,GetTransposed,1,_T("x"))
_MEMBER_FUNCTION(Matrix,GetInverseTransposed,1,_T("x"))
_MEMBER_FUNCTION(Matrix,Invert,1,_T("x"))
_MEMBER_FUNCTION(Matrix,TransformCoord,2,_T("xx"))
_MEMBER_FUNCTION(Matrix,TransformNormal,2,_T("xx"))
_END_CLASS(Matrix)

