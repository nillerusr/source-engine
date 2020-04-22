#ifndef I_NOVINT_HFX
#define I_NOVINT_HFX

#define HFX_VERSION_MAJOR 0
#define HFX_VERSION_MAJOR_SZ "0"
#define HFX_VERSION_MINOR 5
#define HFX_VERSION_MINOR_SZ "5"
#define HFX_VERSION_FLOAT 0.5
#include "util/HFXInterfaceHelper.h"

//( NovintHFX )
#include "HFXConfig.h"
//( HFXClasses )
namespace std{
	template<class _Ty>
	class allocator;
	template <class _Ty,
	class _Ax > class vector;
};

#if _MSC_VER < 1400
#define VECTOR_TYPE(type) std::vector<type>
#endif

#if _MSC_VER >= 1400
#define VECTOR_TYPE(type) ::std::vector<type, ::std::allocator<type> >
#endif

#pragma warning( disable : 4251 )
class EffectTag;

//ensure these are not int he NovintHFX namespace.
#include "IHFXParam.h"
#include "IHFXEffect.h"

typedef int _declspec(dllimport) (*LinkHFX_Fn)( const char * password, const char *cmdline, void **effects, void **system );
//temporarily disable warning regarding to data classes
// needing external classes
#pragma warning(disable:4661)
//forward decl.
class IDevice;
class IStack;
struct IBaseEffectParams;
class IBaseEffect;
//( HapticsMath )
#include "Types/hfxVec3.h"
//( HapticsSystem )
struct IHapticEffectParamGroup;
typedef IHapticEffectParamGroup IHFXParamGroup;
typedef int DeviceIndex;
typedef unsigned long hfxPreciseTime;
typedef bool (*EnableMouseFn)( void );

enum eMouseMove
{
	eHFXMM_Delta=0,
	eHFXMM_Absolute=0,
};

enum eMouseButton
{
	eHFXMM_1232=0,//1=left 2=right 3=middle 4=right
	eHFXMM_1111,//1=left 2=left 3=left 4=left
	eHFXMM_1342,//1=left 2=middle 3=4th 4=right
	eHFXMM_None,
};
enum eMouseMethod
{
	eHFXMouseMeth_OSInput=0,
	eHFXMouseMeth_fMouse,
	eHFXMouseMeth_useCallback,
	eHFXMouseMeth_smartDelta,
	eHFXMouseMeth_delta,
	eHFXMouseMeth_COUNT,
};
enum eMouseClick
{
	eHFXMC_Left=0,
	eHFXMC_Right,
	eHFXMC_Middle,
	eHFXMC_Fourth,
	eHFXMC_COUNT,
};
typedef bool (*MouseEmulationFn)( const double &devicex_pos, 
								 const double &devicey_pos, 
								 const double &devicez_pos, 
								 const double &devicex_delta, 
								 const double &devicey_delta, 
								 const double &devicez_delta, 
								 const int &buttons_down, 
								 const int &buttons_pressed, 
								 const int &buttons_released,
								 bool moved,
								 /* adjust if you would liek to send a mouse button */
								 bool buttonsDown[eHFXMC_COUNT]
								 );

struct ApplicationData
{
};
typedef void (*RollCallIndexSetCallBack)(IDevice *pDevice, const int devices_left);
class HFX_PURE_INTERFACE IHapticsSystem 
{
public:

	// call this method prior to shutting down the game.
	virtual void ShutDown(bool hard=false) =0;
	// THIS FUNCTION MUST BE CALLED BY YOUR APPLICATION AND RETURN TRUE PRIOR TO CREATING A DEVICE!
	// window = the HWND of the program.
	// returns weather or not the applicationdata was sufficent.
	virtual bool AttachApplication(ApplicationData &app) =0;
	virtual bool AttachApplicationByProcessName(ApplicationData &app, const char *szProcessName) =0;

	// If you want to use more than 1 device set this before. NOTE: for multiple devices you must use device lock
	//HFX_VIRTUAL bool SetOptionalDeviceCount( unsigned int nDevices );

	// Use this function to create a device. (note: when making multiple devices they must be connected in numerical order.)
	virtual bool RunDevice(IDevice **ppDevice, const char *deviceName=0, const char *configDir=0) =0;

	// When using multiple devices you _MUST_ lock devices after device connection attempts are made.
	virtual void LockDevices() =0;

	// When using mutliple devices you _MUST_ unlock devices once they are locked to connect more devices and then lock again afterwards.
	virtual void UnlockDevices() =0;

	// Note: this will return the max number of devices specified in the constructor of HapticsSystem.
	virtual unsigned int GetTargetDeviceCount() const =0;

	// connects multiple devices at once. returns number of devices connected.
	// if targetNumber==-1 then all connected devices will be ran.
	virtual int RunMultipleDevices(const int targetNumber=-1) =0;

	// returns true if device was stopped.
	virtual bool StopRunningDevice(IDevice **ppDevice) =0;

	// returns true if all instances were stopped.
	virtual bool StopAllRunningDevices() =0;

	//this will ask the user to press a button on each device. the order in which they
	// press a button on each connected device will order their device id from zero to the number
	// of falcons the user has minus one.
	// note: without roll call devices are ordered by their serial number.
	// returns false if only one falcon is connected.
	virtual bool RollCallDevices(RollCallIndexSetCallBack CallBackFn=0,bool bCreateThread=false) =0;

	// will return true untill roll call is over.
	virtual bool IsInRollCall() const =0;

	// updates roll call data. only call this if your in a roll call and the roll call was not started
	// with bCreateThread set to true.
	// returns true if still in roll call.
	virtual bool RollCallUpdate() =0; 

	// cancles roll call in progress. if ApplyMadeChanges is true the devices who responded
	// to the roll call will be set in and the unset will be moved to the end of the index list.
	virtual bool StopRollCall(bool ApplyMadeChanges=false) =0;

	//only call this ONCE per game input tick.
	// returns number of devices updated.
	virtual int InputUpdate();

	//void SetMouseMode(HWND window,eHFXMouseMove MoveMode=eHFXMM_Delta,eHFXMouseButton ButtonMode=eHFXMM_1232);
	virtual void SetMouseMode(eMouseMove MoveMode=eHFXMM_Delta,eMouseButton ButtonMode=eHFXMM_1232) =0;
	virtual void StartMouse(IDevice *pDev) =0;
	virtual void StartMouse(const DeviceIndex device) =0;
	virtual void StartMouse() =0;
	virtual void MouseUpdate() =0;
	virtual void StopMouse() =0;

	// returns if we are in cursor emulation. If pDevice is null it will return weather any device is in mouse emulation.
	virtual bool IsMouseMode(const IDevice *pDevice=0) =0;

	//returns true if forces are allowed.
	virtual bool ForcesAllowed() =0;

	//returns true if input is allowed.
	virtual bool InputAllowed() =0;

	//returns true if forces are enabled.
	// Note: this could be true and forces may not be allowed.
	// MouseMode RollCall window focus and possibly other things will not allow forces. 
	virtual bool ForcesEnabled() =0;

	//HFX_VIRTUAL void DisableForces() HFX_PURE;
	//HFX_VIRTUAL void EnableForces() HFX_PURE;
	//returns device of index nDevice
	virtual IDevice *GetRunningDeviceByIndex(const DeviceIndex nDevice) const =0;

	//returns device on hardware index.
	virtual IDevice *GetRunningDeviceByHardwareIndex(const int nSerialOrder) const =0;

	//returns number of running devices.
	virtual int RunningDeviceCount() const =0;

	//returns number of devices connected to the users computer.
	virtual int ConnectedDeviceCount() const =0;

	// this will re count available devices.
	// use ConnectedDeviceCount() to get the count.
	virtual void RecountConnectedDevices() =0;

	virtual bool AllocateEffectParameter(IHFXParamGroup *&pIEffectParam, HFXEffectID associatedClass, const char *copyCachedSettings=0, const char *storageName=0) =0;
	HFX_INLINE bool AllocateEffectParameter(IHFXParamGroup *&pIEffectParam, const char *copyCachedSettings=(const char*)0, const char *storageName=0 ){return AllocateEffectParameter(pIEffectParam, 0, copyCachedSettings, storageName);}
	virtual bool DeallocateEffectParameter(IHFXParamGroup *&ppIEffectParam) =0;

	virtual unsigned int GetCachedParameterCount() const =0;
	virtual IHFXParamGroup *GetCachedParameter(const char *name) =0;
	virtual IHFXParamGroup *GetCachedParameter(const unsigned int name) =0;
	virtual const char *GetCachedParameterName(const unsigned int id) const =0;

	// Note: any format ( besides Bitfile ) where it cannot find the file specified will try to 
	// load a encoded bitfile if it does not exist. A bit file will be generated when you load any
	// format. When release time comes around just delete the iniFile and include the bitfile output

	// each format has a way to turn this functionality off. if you 
	virtual bool CacheEffectParametersINI( const char *iniFile ) =0;

	virtual bool CacheEffectParametersBitfile( const char *bitFile ) =0;
	// will add .nvnt extension
	virtual bool SaveCache( const char *file ) =0;

	//HFX_VIRTUAL bool LoadHFXModule( const char *moduleName, const char *modulePassword=0 ) HFX_PURE;
	virtual hfxPreciseTime GetBaseTime() const =0;
	virtual hfxPreciseTime GetRunTime() const =0;
	virtual double GetRunTimeSeconds() const =0;
	virtual double GetBaseTimeSeconds() const =0;

	virtual bool CreateNewEffectStack(const char *uniqueName, IStack**ppStack, unsigned int inputDevices=0) =0;
	virtual bool DeleteEffectStack(IStack**ppStack) =0;
	virtual IStack* FindEffectStack(const char *name) =0;

	//command support
	//HFX_VIRTUAL int RunCommand( const char *cmd ) HFX_PURE;

	//HFX_VIRTUAL bool InitFromXML(ApplicationData &appdata,unsigned int &devices, const char *xmlLoc) HFX_PURE;

	virtual int LogMessage(const int type, const char *fmt, ...);
	virtual int LogMessage(const char *fmt, ...);

	virtual bool SetMouseEmulation( eMouseMethod method ) =0;
	virtual bool SetMouseEmulationFunction( MouseEmulationFn method_function ) =0;
	#ifndef HFX_INTERNAL
	inline bool SetMouseEmulation( MouseEmulationFn method_function ){if(!method_function) return false; return ( SetMouseEmulation(eHFXMouseMeth_useCallback) && SetMouseEmulationFunction(method_function)); }
	#endif
	virtual bool ScaleDeviceCoordsForCursor( double &x, double &y, double width=-1, double height=-1, double offsetx=-1, double offsety=-1 ) =0;
	// try and put device to sleep. if null all devices will try to sleep. devices will sleep untill touched.
	virtual void AttemptSlumber(IDevice *device=0) =0;

	virtual void ActivateDynamicWindowHandler(const char *windowClassName, const char *windowText, unsigned int msHeartInterval=70) =0;
	virtual void DeactivateDynamicWindowHandler() =0;
	virtual bool IsDynamicWindowHandlerRunning() const =0;

	virtual HFXEffectID RegisterEffectClass(const char *tag, HFXCreate_t alloc, HFXDestroy_t dealloc, HFXNEED needs, IHFXParamGroup *defaults=0) =0;
#define HFX_NON_ELEMENT 0xFFFFF0
	virtual bool SetParameterGroupVar(IHFXParamGroup &params, HFXParamID var, const char *string, unsigned int element=HFX_NON_ELEMENT) =0;
	virtual bool SetParameterGroupVar(IHFXParamGroup &params, const char *varString, const char *string, unsigned int element=HFX_NON_ELEMENT) =0;
	virtual HFXEffectID LookupEffectIDByName(const char *name) const =0;
	virtual const char *LookupEffectNameByID(HFXEffectID id) const =0;

	// only call if explicit stack syncs are enabled
	virtual void SyncEffectStacks() =0;

	virtual HFXParamID EnumerateEffectParameters( HFXEffectID id, HFXParamID LastParamID ) =0;
	virtual bool CacheEffectParameterGroupCopy(const char *cachename, IHFXParamGroup *params) =0;
	virtual bool SaveEffectParameterCache(const char *filename, const char *type = "ini") =0;
	virtual void DoReport() =0;
};

//( HapticsDevice )
//HFX BUTTON DEFINES : Based on HDL Button Defines.
#define HFX_BUTTON_1 0x00000001   /**< Mask for button 1 */
#define HFX_BUTTON_2 0x00000002   /**< Mask for button 2 */
#define HFX_BUTTON_3 0x00000004   /**< Mask for button 3 */
#define HFX_BUTTON_4 0x00000008   /**< Mask for button 4 */
#define HFX_BUTTON_ANY 0xffffffff /**< Mask for any button */

//Internal class.
struct HFX_PURE_INTERFACE IDeviceData 
{
};

namespace NovintHFX{
	class Device;
	class Stack;
	namespace Effects{
		struct command_info;
		int ProcessCommand( IBaseEffect *pEffect, const char *argv[], const unsigned int argc );
	}
};
// This function callback type will be used with SetServoLoopCallbackFunction.
//   The main difference between hdlServoOp and this is that you get the device.
typedef int (*OldServoLoopFn) (void *pParam, class IDevice *pDevice, double outforces[3]);
class HFX_PURE_INTERFACE IDevice 
{
public:
	//Is this device able to output forces?
	virtual bool AcceptingForces() const =0;

	//The device is connected.
	virtual bool IsConnected() const =0;

	//Sets pos to the tool Position.
	virtual void GetToolPosition(double pos[3]) const =0;
	virtual void GetToolPositionWorkspace(double pos[3]) const =0;
	virtual void GetButtonData(int *down=0, int *pressed=0, int *released=0) const =0;
	virtual void GetCurrentForce(double force[3]) const =0;
	virtual bool IsButtonDown( int nButton ) const =0;
	virtual IStack *GetEffectStack() =0;
	virtual const DeviceIndex GetIndex() const =0;
	virtual int GetHardwareIndex() const =0;
	virtual void InputUpdate() =0;

	virtual bool ConnectStack( IStack *stack ) =0;
	virtual bool DisconnectStack( IStack *stack ) =0;

	// This function is here for quick integration into games which already have a few custom
	// effects of their own. Taking the old ServoOp function and adding in pDevice
	virtual void SetServoLoopCallbackFunction( OldServoLoopFn old_fn, void *pParam ) =0;

	// you should never call this unless your in a callback running on servo.
	virtual void _GetServoData(double toolpos[3], int &buttons) =0;

	virtual __int64 GetSerialNumber() const =0;

	virtual BoundingBox3 GetWorkspace() const =0;

	virtual void SetEngineData(void *data) =0;
	virtual void *GetEngineData() const =0;

	virtual bool HasRested( unsigned int msDur=1 ) const =0;
	virtual bool IsSleeping(bool justForces=false) const =0;
	virtual bool TriggerFade(unsigned int msMuteTime, unsigned int msDuration, bool forceOverride=false) =0;
	virtual void ForceSlumber() =0;
	virtual void SetForceScale(const double &scale) =0;
	virtual double GetForceScale() const =0;
};

//( HapticsStack )
class Processor;
//EFFECT STACK
//note one stack per device handle only.
class HFX_PURE_INTERFACE IStack 
{
public:

	// Returns true if effect was created.
	// --
	// NOTE: NOT ALL EFFECTS ( SUCH AS SELF DELETING )
	// WILL ALLOW YOU TO HAVE A HANDLE TO IT. 
	// CHECKING POINTER VALIDITY OF THE HANDLE IN THE
	// CREATE NEW EFFECT WILL ONLY LET YOU KNOW IF
	// ANYTHING WAS SET TO IT, NOT NECCISARILLY IF
	// THE EFFECT WAS CREATED.
	// --
	virtual bool CreateCachedEffect(IHFXEffect *&pHandle, const char *entry, const char *instancename=0 );

	template<typename T>
	HFX_INLINE bool CreateCachedEffect(T *&pTypedHandle, const char *entry, const char *instanceName=0)
	{
		return CreateCachedEffect((IHFXEffect*&)pTypedHandle, entry, instanceName);
	}
	HFX_INLINE bool CreateCachedEffect(const char *entry){ return CreateCachedEffect(hfxNoHandle, entry); }

	virtual bool CreateNewEffect(IHFXEffect *&ppHandle, HFXEffectID effectName, IHFXParamGroup *params, const char *instanceName=0);

	template<typename T>
	HFX_INLINE bool CreateNewEffect(T *&pTypedHandle, HFXEffectID effectName, IHFXParamGroup *params, const char *instanceName=0)
	{
		return CreateNewEffect((IHFXEffect*&)pTypedHandle, effectName, params, instanceName);
	}

	HFX_INLINE bool CreateNewEffect(HFXEffectID effectName, IHFXParamGroup *params){ return CreateNewEffect(hfxNoHandle, effectName, params, 0); };
	// Returns NULL if there was no effect using that name.
	// --
	// NOTE: YOU SHOULD ONLY HAVE ONE HANDLE OF A EFFECT
	// IN AS A MEMBER VARIABLE ( NON LOCAL ). OTHERWISE
	// YOU RISK INVALID POINTERS IF YOU DELETE THE EFFECT.
	// WITH A SECOND REFERENCE.
	virtual IHFXEffect *FindRunningEffect(const char *instanceName) const =0;
	// Returns true if effect was deleted
	// --
	// NOTE: THIS IS ONLY FOR EFFECTS WHICH NEED TO BE
	// DELETED MANUALLY. MANY EFFECTS WILL DELETE
	// THEMSELFS AND THOSE EFFECTS YOU SHOULD NOT
	// EVER RECEIVE HANDLES FOR.
	// --
	virtual bool DeleteRunningEffect(IHFXEffect *&effect) =0;

	template<typename T>
	HFX_INLINE bool DeleteRunningEffect(T *&pTypedEffect)
	{
		return DeleteRunningEffect((IHFXEffect*&)pTypedEffect);
	}

	virtual void GetLastForce(double force[3])const =0;

	virtual void SetDeviceLock(bool state) =0;
	virtual bool IsDeviceListLocked() const =0;

	// returns true if device was added or is already a member. if slot is -1 it will just enter the device
	// as the next available slot.
	virtual bool AddDevice(IDevice*pDevice, int slot=-1) =0;

	// returns the device in slot
	virtual IDevice *GetDeviceInSlot(unsigned int id=0) const =0;

	virtual void GetCalclatedForces(double force[3]) =0;
	virtual const char *Name() const =0;

	//command support
	//HFX_VIRTUAL int RunCommand( const char *cmd ) HFX_PURE;

	virtual void *GetUserData() const =0;
	virtual void SetUserData(void*data) =0;

	virtual void SetVolume( const double &scale ) =0;
	virtual double GetVolume() const =0;

	virtual unsigned int GetTargetDeviceCount() const =0;

	virtual void SetEngineData(void *data) =0;
	virtual void *GetEngineData() const =0;

	virtual void SetEffectUserData(IHFXEffect *pEffect, void *pData) =0;
	virtual void *GetEffectUserData(IHFXEffect *pEffect) =0;
};

//( HapticsEffect )
struct HFX_PURE_INTERFACE IRegister
{
public:
	virtual void AllocateEffect(IHFXEffect *&pEffectPtr) =0;
	virtual void DeallocateEffect(IHFXEffect *&pEffectPtr) =0;
	virtual void AllocateParameterGroup(IHFXParamGroup *&pEffectPtr) =0;
	virtual void DeallocateParameterGroup(IHFXParamGroup *&pEffectPtr) =0;
	virtual const char *GetTagName() const =0;
};

#define HFX_PROCESSOR_PARAM_FORCESCALE 4294967293
#define HFX_PROCESSOR_PARAM_TIMESCALE 4294967294

class HFX_PURE_INTERFACE IProcessor
{
public:

	// call this to mute and unmute this effect. (note: effect will still be updated, just will not output force.)
	// note : this setting will not be applied untill the next sync op ( InputUpdate ) called by the application.
	virtual void SetMuted(bool mute) =0;
	virtual void SetPaused(bool pause) =0;

	// see if the effect is muted.
	HFX_INLINE bool IsMuted() const { return ((GetEffectState() & HFXSTATE_MUTE)!=0); }

	// see if the effect is explicitly paused.
	HFX_INLINE bool IsPaused() const { return ((GetEffectState() & HFXSTATE_PAUSE)!=0); }

	// scale this force (NOTE: THIS IS SEPERATE FROM PARAMETER SCALE!)
	virtual void SetForceScale( double scale ) =0;
	virtual double GetForceScale( ) const =0;

	virtual const char *GetEffectTypeName() const =0;

	// override this function to return the number of falcons this
	// effect requires.
	HFX_INLINE unsigned int RequiredDeviceCount() const 
	{
		HFXNEED needs= GetEffectNeeds();
		return HFXNEED_UTIL_COUNT_DEVICES(needs);
	}

	// returns true if the effect is running. 
	HFX_INLINE bool IsRunning() const {return ( (GetEffectState() & (HFXSTATE_RUNNING)) != 0 );}

	// if your effect is not self deleting and requires runtime data you should override this to true.
	HFX_INLINE  bool CanBeHandled() const { return (GetEffectNeeds() & HFXNEED_ENCAPSULATED)==0; }

	// this lets the haptic effect stack know you are done with this effect and it is waiting to be deleted.
	// CANNOT BE OVERRIDDEN ( see OnFlaggedForRemoval() )
	virtual void FlagForRemoval() =0;

	//Stack functions!
	friend class NovintHFX::Stack;
	friend int NovintHFX::Effects::ProcessCommand( IBaseEffect *pEffect, const char *argv[], const unsigned int argc );

	// do not use.
	//HFX_VIRTUAL bool Stop() HFX_PURE;

	// DO NOT OVERRIDE!
	HFX_INLINE bool NeedsRemoval() const 
	{	return ( ( GetEffectNeeds() & HFXNEED_REMOVE ) ? ( ( ( GetEffectState() & HFXSTATE_RUNNING ) ) ? false : true ) : false ); }

	// DO NOT OVERRIDE!
	HFX_INLINE bool WantsSyncOp() const 
	{
		return (((GetEffectNeeds() & HFXNEED_SYNC)!=0) && ((GetEffectState() & HFXSTATE_WANT_SYNC)!=0)); 
	}
	// DO NOT OVERRIDE!
	virtual bool WantsUpdate() const {return (((GetEffectNeeds() & HFXNEED_PROCESS)!=0) && ((GetEffectState() & HFXSTATE_WANT_UPDATE)!=0) && ((GetEffectState() & HFXSTATE_RUNNING)!=0));} 

	virtual HFXSTATE GetEffectState() const =0;
	virtual const HFXNEED &GetEffectNeeds() const =0;

	virtual void *_output() const =0;

	virtual const double &Runtime() const =0;
	virtual const double &Frametime() const =0;

	virtual IStack *GetStack() =0;
	virtual IHFXSystem *GetSystem() =0;
	virtual void *GetUserData() =0;
	virtual void SetUserData(void *userData) =0;
	HFX_INLINE hfxVec3 &Output() { return *(reinterpret_cast<hfxVec3*>(_output())); }
	HFX_INLINE const hfxVec3 &Output() const{return *(reinterpret_cast<const hfxVec3*>(_output()));}
	HFX_INLINE double &operator[](int i){ return (Output().m[i]); };
	HFX_INLINE const double &operator[](int i)const{ return (Output()[i]); };
	HFX_INLINE bool IsOutputValid() const { return !IsOutputNaN();}
	HFX_INLINE bool IsOutputNaN() const { const double *o=Output(); return(o[0]!=o[0]||o[1]!=o[1]||o[2]!=o[2]);}
	HFX_INLINE hfxVec3 &operator =(const hfxVec3 &vect){hfxVec3 &out = Output(); out = vect; return out;}
};
typedef IProcessor IHFXProcessor;
typedef char HFX_VarType;

#define HFX_Double 'd'
#define HFX_Float 'f'
#define HFX_Int 'i'
#define HFX_Bool 'b'
#define HFX_Other 'o'
#define HFX_Pointer 'p'
#define HFX_Null 0


#pragma warning( default : 4251 )

#ifdef HFX_MODULE_LAYER
#include HFX_MODULE_LAYER
#endif

#define ConnectNovintHFX YOU_MUST_INCLUDE_WINDOWS_PRIOR_TO_INCLUDING_INovintHFX_h
#endif

// to connect windows.h must be included
#if ( !defined(I_NOVINT_HFX_WINDOWS) && ( defined(_INC_WINDOWS) || defined(LoadLibrary) && defined(HMODULE) ) ) && !defined(STATIC_IHFX)
#define I_NOVINT_HFX_WINDOWS

#ifdef ConnectNovintHFX
#undef ConnectNovintHFX
#endif

extern HMODULE dllNovintHFX_;
extern IHapticsSystem *_hfx;

//quick helper class to ensure novint hfx unloads properly.
struct NovintHFXCloseHelper
{
	NovintHFXCloseHelper() : forceSkipQuit(false)
	{
		static bool ONE_INSTANCE_ONLY=true;
		if(!ONE_INSTANCE_ONLY)
		{
#ifdef _DEBUG
			// look at the call stack and please remove whatever is calling this constructor.
			// there should be only one HFX_INIT_VARS.
			DebugBreak();
#endif
			ExitProcess(100);
		}
		ONE_INSTANCE_ONLY = false;
	};
	bool forceSkipQuit;
	~NovintHFXCloseHelper()
	{
		if(!forceSkipQuit&&_hfx)
		{
			_hfx->ShutDown(true);
		}
	};
};
extern NovintHFXCloseHelper _hfx_close;

#define HFX_INIT_VARS() \
	HMODULE dllNovintHFX_=0; \
	IHapticsSystem *_hfx =0; \
	NovintHFXCloseHelper _hfx_close;

// if windows is included heres a inline function to get the interface!
inline bool ConnectNovintHFX( IHapticsSystem **ppSystem, void *window, const char *cmd, void*pCursorEnableFn=0, unsigned int TargetDevices=1)
{
	if(ppSystem==0)
		return false;

	char szNovintDir[ MAX_PATH ];
	char szNovintDll[ MAX_PATH ];
	if ( GetEnvironmentVariableA( "NOVINT_DEVICE_SUPPORT", szNovintDir, sizeof( szNovintDir ) ) == 0 || !V_IsAbsolutePath( szNovintDir ) )
	{
		return false;
	}

	unsigned int tries = 0;
	while(dllNovintHFX_==0)
	{
		const char *dllName = HFX_DYNAMIC_LIBRARY_NAME(tries);
		if(!dllName)
			break;
		V_sprintf_safe( szNovintDll, "%s\\bin\\%s", szNovintDir, dllName );
		dllNovintHFX_ = LoadLibraryA( szNovintDll );
		if(!dllNovintHFX_)
			tries++;
	}

	NovintHFX_ExposeInterfaceFn connectFn = 0;
	if(dllNovintHFX_)
	{
		connectFn = (NovintHFX_ExposeInterfaceFn)GetProcAddress(dllNovintHFX_, HFX_CONNECT_FUNCTION_NAME());
	}
	if(!connectFn){
		// if direct load failed..
		if(dllNovintHFX_)
		{
			FreeLibrary(dllNovintHFX_);
		}
		V_sprintf_safe( szNovintDll, "%s\\bin\\%s", szNovintDir, "hfx.dll" );
		dllNovintHFX_ = LoadLibraryA( szNovintDll );
		if(dllNovintHFX_)
		{
			connectFn = (NovintHFX_ExposeInterfaceFn)GetProcAddress(dllNovintHFX_, "CreateHFX");
		}
	}
	if(connectFn&&connectFn((void **)ppSystem, window, cmd, HFX_VERSION_MAJOR, HFX_VERSION_MINOR, pCursorEnableFn, TargetDevices )==0)
	{
		_hfx = (*ppSystem);
		return true;
	}

	return false;
}
#ifndef HFX_STRIPPED
inline bool ConnectNovintHFX_XML( IHapticsSystem **ppSystem, void *window, const char *xml, void*pCursorEnableFn=0, unsigned int TargetDevices=1)
{
	if(ppSystem==0)
		return false;

	char szNovintDir[ MAX_PATH ];
	char szNovintDll[ MAX_PATH ];
	if ( GetEnvironmentVariable( "NOVINT_DEVICE_SUPPORT", szNovintDir, sizeof( szNovintDir ) ) == 0 || !V_IsAbsolutePath( szNovintDir ) )
	{
		return false;
	}

	unsigned int tries = 0;
	while(dllNovintHFX_==0)
	{
		const char *dllName = HFX_DYNAMIC_LIBRARY_NAME(tries);
		if(!dllName)
			break;
		V_sprintf_safe( szNovintDll, "%s\\bin\\%s", szNovintDir, dllName );
		dllNovintHFX_ = LoadLibraryA( szNovintDll );
		if(!dllNovintHFX_)
			tries++;
	}
	NovintHFX_ExposeInterfaceFn connectFn = 0;
	if(dllNovintHFX_)
	{
		connectFn = (NovintHFX_ExposeInterfaceFn)GetProcAddress(dllNovintHFX_, HFX_CONNECT_FUNCTION_NAME_XML());
	}
	if(!connectFn){
		if(dllNovintHFX_)
		{
			FreeLibrary(dllNovintHFX_);
			dllNovintHFX_=0;
		}
		V_sprintf_safe( szNovintDll, "%s\\bin\\%s", szNovintDir, "hfx.dll" );
		dllNovintHFX_ = LoadLibraryA( szNovintDll );
		if(dllNovintHFX_)
		{
			connectFn = (NovintHFX_ExposeInterfaceFn)GetProcAddress(dllNovintHFX_, "CreateHFX_XML");
		}

	}
	if(connectFn&&connectFn((void **)ppSystem, window, xml, HFX_VERSION_MAJOR, HFX_VERSION_MINOR, pCursorEnableFn, TargetDevices )==0)
	{
		_hfx = (*ppSystem);
		return true;
	}
	return false;
}
#endif
inline bool ConnectNovintHFX( IHapticsSystem **ppSystem, HWND hwnd, const char *cmd, void*pCursorEnableFn=0, unsigned int TargetDevices=1 ){ return ConnectNovintHFX( ppSystem, (void*)hwnd, cmd, pCursorEnableFn, TargetDevices); }
inline void DisconnectNovintHFX(IHapticsSystem **ppSystem=0)
{
	if(dllNovintHFX_&&_hfx)
	{
		IHapticsSystem *pTemp = _hfx;
		_hfx = 0;
		_hfx_close.forceSkipQuit=true;
		if(ppSystem)
		{
			*ppSystem = 0;
		}
		pTemp->ShutDown();
		if(FreeLibrary(dllNovintHFX_))
		{
			dllNovintHFX_ = 0;
		}
	}
}
#endif

