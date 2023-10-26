#include "stdafx.h"
#include "sqplusWin32.h"
#include "DXSquirrel_Binds.h"

_MEMBER_FUNCTION_IMPL(BaseMesh,constructor)
{
	StackHandler sa(v);
	return sa.ThrowError(_T("BaseMesh cannot be constructed directly"));
}

_MEMBER_FUNCTION_IMPL(BaseMesh,DrawSubset)
{
	StackHandler sa(v);
	_CHECK_SELF(ID3DXBaseMesh,BaseMesh)
	self->DrawSubset(sa.GetInt(2));
	return 0;
}

_BEGIN_CLASS(BaseMesh)
_MEMBER_FUNCTION(BaseMesh,constructor,NULL,NULL)
_MEMBER_FUNCTION(BaseMesh,DrawSubset,2,_T("xn"))
_END_CLASS(BaseMesh)

_MEMBER_FUNCTION_IMPL(Mesh,constructor)
{
	_CHECK_INST_PARAM(dev,2,IDirect3DDevice9,Device);
	ID3DXMesh *pMesh;
	if( FAILED(D3DXCreateTeapot(dev,&pMesh,NULL))) {
		   MessageBox(NULL, _T("Could not create the mesh"), _T("Meshes.exe"), MB_OK);
            return E_FAIL;
	}
	return construct_RefCounted(pMesh);
}

_BEGIN_CLASS(Mesh)
_MEMBER_FUNCTION(Mesh,constructor,2,_T("xx"))
_END_CLASS_INHERITANCE(Mesh,BaseMesh)