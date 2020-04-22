//=== === Copyright 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

using System;
using System.Runtime.InteropServices;

namespace Valve.SteamVRInterop
{

class NativeEntrypoints
{


[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetWindowBounds")]
internal static extern void SteamAPI_IHmd_GetWindowBounds(IntPtr instancePtr, ref int32_t pnX, ref int32_t pnY, ref uint32_t pnWidth, ref uint32_t pnHeight);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetRecommendedRenderTargetSize")]
internal static extern void SteamAPI_IHmd_GetRecommendedRenderTargetSize(IntPtr instancePtr, ref uint32_t pnWidth, ref uint32_t pnHeight);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetEyeOutputViewport")]
internal static extern void SteamAPI_IHmd_GetEyeOutputViewport(IntPtr instancePtr, vr::Hmd_Eye eEye, ref uint32_t pnX, ref uint32_t pnY, ref uint32_t pnWidth, ref uint32_t pnHeight);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetProjectionMatrix")]
internal static extern vr::HmdMatrix44_t SteamAPI_IHmd_GetProjectionMatrix(IntPtr instancePtr, vr::Hmd_Eye eEye, float fNearZ, float fFarZ, vr::GraphicsAPIConvention eProjType);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetProjectionRaw")]
internal static extern void SteamAPI_IHmd_GetProjectionRaw(IntPtr instancePtr, vr::Hmd_Eye eEye, ref float pfLeft, ref float pfRight, ref float pfTop, ref float pfBottom);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_ComputeDistortion")]
internal static extern vr::DistortionCoordinates_t SteamAPI_IHmd_ComputeDistortion(IntPtr instancePtr, vr::Hmd_Eye eEye, float fU, float fV);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetHeadFromEyePose")]
internal static extern vr::HmdMatrix34_t SteamAPI_IHmd_GetHeadFromEyePose(IntPtr instancePtr, vr::Hmd_Eye eEye);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetViewMatrix")]
internal static extern bool SteamAPI_IHmd_GetViewMatrix(IntPtr instancePtr, float fSecondsFromNow, ref vr::HmdMatrix44_t pMatLeftView, ref vr::HmdMatrix44_t pMatRightView, ref vr::HmdTrackingResult peResult);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetD3D9AdapterIndex")]
internal static extern int32_t SteamAPI_IHmd_GetD3D9AdapterIndex(IntPtr instancePtr);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetDXGIOutputInfo")]
internal static extern void SteamAPI_IHmd_GetDXGIOutputInfo(IntPtr instancePtr, ref int32_t pnAdapterIndex, ref int32_t pnAdapterOutputIndex);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_AttachToWindow")]
internal static extern void SteamAPI_IHmd_AttachToWindow(IntPtr instancePtr, IntPtr hWnd);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetTrackerFromHeadPose")]
internal static extern bool SteamAPI_IHmd_GetTrackerFromHeadPose(IntPtr instancePtr, float fPredictedSecondsFromNow, ref vr::HmdMatrix34_t pmPose, ref vr::HmdTrackingResult peResult);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetLastTrackerFromHeadPose")]
internal static extern bool SteamAPI_IHmd_GetLastTrackerFromHeadPose(IntPtr instancePtr, ref vr::HmdMatrix34_t pmPose);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_WillDriftInYaw")]
internal static extern bool SteamAPI_IHmd_WillDriftInYaw(IntPtr instancePtr);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_ZeroTracker")]
internal static extern void SteamAPI_IHmd_ZeroTracker(IntPtr instancePtr);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetTrackerZeroPose")]
internal static extern vr::HmdMatrix34_t SteamAPI_IHmd_GetTrackerZeroPose(IntPtr instancePtr);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetDriverId")]
internal static extern uint32_t SteamAPI_IHmd_GetDriverId(IntPtr instancePtr, string pchBuffer, uint32_t unBufferLen);
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_IHmd_GetDisplayId")]
internal static extern uint32_t SteamAPI_IHmd_GetDisplayId(IntPtr instancePtr, string pchBuffer, uint32_t unBufferLen);

}


public abstract class IHmd
{
public abstract void GetWindowBounds(out int32_t pnX,out int32_t pnY,out uint32_t pnWidth,out uint32_t pnHeight);
public abstract void GetRecommendedRenderTargetSize(out uint32_t pnWidth,out uint32_t pnHeight);
public abstract void GetEyeOutputViewport(vr::Hmd_Eye eEye,out uint32_t pnX,out uint32_t pnY,out uint32_t pnWidth,out uint32_t pnHeight);
public abstract vr::HmdMatrix44_t GetProjectionMatrix(vr::Hmd_Eye eEye,float fNearZ,float fFarZ,vr::GraphicsAPIConvention eProjType);
public abstract void GetProjectionRaw(vr::Hmd_Eye eEye,out float pfLeft,out float pfRight,out float pfTop,out float pfBottom);
public abstract vr::DistortionCoordinates_t ComputeDistortion(vr::Hmd_Eye eEye,float fU,float fV);
public abstract vr::HmdMatrix34_t GetHeadFromEyePose(vr::Hmd_Eye eEye);
public abstract bool GetViewMatrix(float fSecondsFromNow,out vr::HmdMatrix44_t pMatLeftView,out vr::HmdMatrix44_t pMatRightView,out vr::HmdTrackingResult peResult);
public abstract int32_t GetD3D9AdapterIndex();
public abstract void GetDXGIOutputInfo(out int32_t pnAdapterIndex,out int32_t pnAdapterOutputIndex);
public abstract void AttachToWindow(IntPtr hWnd);
public abstract bool GetTrackerFromHeadPose(float fPredictedSecondsFromNow,out vr::HmdMatrix34_t pmPose,out vr::HmdTrackingResult peResult);
public abstract bool GetLastTrackerFromHeadPose(out vr::HmdMatrix34_t pmPose);
public abstract bool WillDriftInYaw();
public abstract void ZeroTracker();
public abstract vr::HmdMatrix34_t GetTrackerZeroPose();
public abstract uint32_t GetDriverId(string pchBuffer,uint32_t unBufferLen);
public abstract uint32_t GetDisplayId(string pchBuffer,uint32_t unBufferLen);
}


public class CHmd : IHmd
{
public CHmd(IntPtr hmd)
{
	m_hmd = hmd;
}
IntPtr m_hmd;

private void CheckIfUsable()
{
	if (m_hmd == IntPtr.Zero)
	{
		throw new Exception("Steam Pointer not configured");
	}
}
public override void GetWindowBounds(out int32_t pnX,out int32_t pnY,out uint32_t pnWidth,out uint32_t pnHeight)
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_GetWindowBounds(m_hmd,ref pnX,ref pnY,ref pnWidth,ref pnHeight);
}
public override void GetRecommendedRenderTargetSize(out uint32_t pnWidth,out uint32_t pnHeight)
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_GetRecommendedRenderTargetSize(m_hmd,ref pnWidth,ref pnHeight);
}
public override void GetEyeOutputViewport(vr::Hmd_Eye eEye,out uint32_t pnX,out uint32_t pnY,out uint32_t pnWidth,out uint32_t pnHeight)
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_GetEyeOutputViewport(m_hmd,eEye,ref pnX,ref pnY,ref pnWidth,ref pnHeight);
}
public override vr::HmdMatrix44_t GetProjectionMatrix(vr::Hmd_Eye eEye,float fNearZ,float fFarZ,vr::GraphicsAPIConvention eProjType)
{
	CheckIfUsable();
	vr::HmdMatrix44_t result = NativeEntrypoints.SteamAPI_IHmd_GetProjectionMatrix(m_hmd,eEye,fNearZ,fFarZ,eProjType);
	return result;
}
public override void GetProjectionRaw(vr::Hmd_Eye eEye,out float pfLeft,out float pfRight,out float pfTop,out float pfBottom)
{
	CheckIfUsable();
	pfLeft = 0;
	pfRight = 0;
	pfTop = 0;
	pfBottom = 0;
	NativeEntrypoints.SteamAPI_IHmd_GetProjectionRaw(m_hmd,eEye,ref pfLeft,ref pfRight,ref pfTop,ref pfBottom);
}
public override vr::DistortionCoordinates_t ComputeDistortion(vr::Hmd_Eye eEye,float fU,float fV)
{
	CheckIfUsable();
	vr::DistortionCoordinates_t result = NativeEntrypoints.SteamAPI_IHmd_ComputeDistortion(m_hmd,eEye,fU,fV);
	return result;
}
public override vr::HmdMatrix34_t GetHeadFromEyePose(vr::Hmd_Eye eEye)
{
	CheckIfUsable();
	vr::HmdMatrix34_t result = NativeEntrypoints.SteamAPI_IHmd_GetHeadFromEyePose(m_hmd,eEye);
	return result;
}
public override bool GetViewMatrix(float fSecondsFromNow,out vr::HmdMatrix44_t pMatLeftView,out vr::HmdMatrix44_t pMatRightView,out vr::HmdTrackingResult peResult)
{
	CheckIfUsable();
	bool result = NativeEntrypoints.SteamAPI_IHmd_GetViewMatrix(m_hmd,fSecondsFromNow,ref pMatLeftView,ref pMatRightView,ref peResult);
	return result;
}
public override int32_t GetD3D9AdapterIndex()
{
	CheckIfUsable();
	int32_t result = NativeEntrypoints.SteamAPI_IHmd_GetD3D9AdapterIndex(m_hmd);
	return result;
}
public override void GetDXGIOutputInfo(out int32_t pnAdapterIndex,out int32_t pnAdapterOutputIndex)
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_GetDXGIOutputInfo(m_hmd,ref pnAdapterIndex,ref pnAdapterOutputIndex);
}
public override void AttachToWindow(IntPtr hWnd)
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_AttachToWindow(m_hmd,hWnd);
}
public override bool GetTrackerFromHeadPose(float fPredictedSecondsFromNow,out vr::HmdMatrix34_t pmPose,out vr::HmdTrackingResult peResult)
{
	CheckIfUsable();
	bool result = NativeEntrypoints.SteamAPI_IHmd_GetTrackerFromHeadPose(m_hmd,fPredictedSecondsFromNow,ref pmPose,ref peResult);
	return result;
}
public override bool GetLastTrackerFromHeadPose(out vr::HmdMatrix34_t pmPose)
{
	CheckIfUsable();
	bool result = NativeEntrypoints.SteamAPI_IHmd_GetLastTrackerFromHeadPose(m_hmd,ref pmPose);
	return result;
}
public override bool WillDriftInYaw()
{
	CheckIfUsable();
	bool result = NativeEntrypoints.SteamAPI_IHmd_WillDriftInYaw(m_hmd);
	return result;
}
public override void ZeroTracker()
{
	CheckIfUsable();
	NativeEntrypoints.SteamAPI_IHmd_ZeroTracker(m_hmd);
}
public override vr::HmdMatrix34_t GetTrackerZeroPose()
{
	CheckIfUsable();
	vr::HmdMatrix34_t result = NativeEntrypoints.SteamAPI_IHmd_GetTrackerZeroPose(m_hmd);
	return result;
}
public override uint32_t GetDriverId(string pchBuffer,uint32_t unBufferLen)
{
	CheckIfUsable();
	uint32_t result = NativeEntrypoints.SteamAPI_IHmd_GetDriverId(m_hmd,pchBuffer,unBufferLen);
	return result;
}
public override uint32_t GetDisplayId(string pchBuffer,uint32_t unBufferLen)
{
	CheckIfUsable();
	uint32_t result = NativeEntrypoints.SteamAPI_IHmd_GetDisplayId(m_hmd,pchBuffer,unBufferLen);
	return result;
}
}


public class SteamVRInterop
{
[DllImportAttribute("Steam_api", EntryPoint = "VR_Init")]
internal static extern IntPtr VR_Init(out HmdError peError);
[DllImportAttribute("Steam_api", EntryPoint = "VR_Shutdown")]
internal static extern void VR_Shutdown();
[DllImportAttribute("Steam_api", EntryPoint = "VR_IsHmdPresent")]
internal static extern bool VR_IsHmdPresent();
[DllImportAttribute("Steam_api", EntryPoint = "SteamAPI_UnregisterCallback")]
internal static extern string VR_GetStringForHmdError(HmdError error);
[DllImportAttribute("Steam_api", EntryPoint = "Hmd")]
internal static extern IntPtr Hmd();
}


public enum Hmd_Eye
{
	Eye_Left = 0,
	Eye_Right = 1,
}
public enum GraphicsAPIConvention
{
	API_DirectX = 0,
	API_OpenGL = 1,
}
public enum HmdTrackingResult
{
	TrackingResult_Uninitialized = 1,
	TrackingResult_Calibrating_InProgress = 100,
	TrackingResult_Calibrating_OutOfRange = 101,
	TrackingResult_Running_OK = 200,
	TrackingResult_Running_OutOfRange = 201,
}
public enum HmdError
{
	HmdError_None = 0,
	HmdError_Init_InstallationNotFound = 100,
	HmdError_Init_InstallationCorrupt = 101,
	HmdError_Init_VRClientDLLNotFound = 102,
	HmdError_Init_FileNotFound = 103,
	HmdError_Init_FactoryNotFound = 104,
	HmdError_Init_InterfaceNotFound = 105,
	HmdError_Init_InvalidInterface = 106,
	HmdError_Init_UserConfigDirectoryInvalid = 107,
	HmdError_Init_HmdNotFound = 108,
	HmdError_Init_NotInitialized = 109,
	HmdError_Driver_Failed = 200,
	HmdError_Driver_Unknown = 201,
	HmdError_Driver_HmdUnknown = 202,
	HmdError_Driver_NotLoaded = 203,
	HmdError_IPC_ServerInitFailed = 300,
	HmdError_IPC_ConnectFailed = 301,
	HmdError_IPC_SharedStateInitFailed = 302,
	HmdError_VendorSpecific_UnableToConnectToOculusRuntime = 1000,
}
[StructLayout(LayoutKind.Sequential)] public struct HmdMatrix34_t
{
	public float m;
}
[StructLayout(LayoutKind.Sequential)] public struct HmdMatrix44_t
{
	public float m;
}
[StructLayout(LayoutKind.Sequential)] public struct DistortionCoordinates_t
{
	public float rfRed;
	public float rfGreen;
	public float rfBlue;
}

public class SteamVR
{
public static IHMD Init(out HmdError peError)
{
	return new CHMD(VR_Init(out peError));
}

}



}

