 
#ifndef IHFXEFFECT_H_
#define IHFXEFFECT_H_
#include ".\\HFXConfig.h"
#include ".\\IHFXParam.h"
 
struct IHapticEffectParamGroup; 
typedef IHapticEffectParamGroup IHFXParamGroup; 
class IProcessor; 
typedef IProcessor IHFXProcessor; 
class IStack; 
typedef IStack IHFXStack; 
enum HFXStateBits
{
	// when a state variable is made it uses the below value
	HFXSTATE_INITIAL		=0, 
	// explicite pause
	HFXSTATE_PAUSE			=(1<<0), 
	// explicite    mute
	HFXSTATE_MUTE			=(1<<1), 
	// the effect has gone into  its exit phase.
	HFXSTATE_EXITING		=(1<<2), 
	HFXSTATE_WANT_SYNC		=(1<<3), 
	HFXSTATE_CLIENT_ONLY	=4, 
	//everything below will be uncontrolled by servo
	HFXSTATE_WANT_UPDATE	=(1<<HFXSTATE_CLIENT_ONLY), 
	HFXSTATE_RUNNING		=(1<<(HFXSTATE_CLIENT_ONLY+1)), 
	HFXSTATE_HAULT			=(1<<(HFXSTATE_CLIENT_ONLY+2)), 
	HFXSTATE_HOLDING_EFFECT	=(1<<(HFXSTATE_CLIENT_ONLY+3)), 
	HFXSTATE_COUNT			=8, 
	// number of state bits before non servo controlled bits
}; 
typedef unsigned __int8 HFXSTATE; 
typedef HFXSTATE HFXStateStorage; 
 
#if WIN64
typedef const HFXStateStorage &HFXStateTransfer; 
#else
typedef const HFXStateStorage HFXStateTransfer; 
#endif
 
// utility union, takes up same memory as a flagset but gives you
// inline operators and helper functions for state flags
union HFX_ALIGN(8) HFXStateFlags
{
public: 
	//default constructor
	HFX_INLINE HFXStateFlags(){}; 
 
	//copy constructor
	HFX_INLINE HFXStateFlags(const HFXStateFlags &copy) : Storage(copy.Storage){}; 
 
	HFX_INLINE HFXStateFlags(HFXStateStorage flags) : Storage(flags){}; 
 
	HFX_INLINE HFXStateFlags(	 
		bool bRunning, bool bPaused, bool bMuted, bool bHaulted, bool bExiting
		) : Storage((bRunning ? HFXSTATE_RUNNING : 0)|
					(bPaused ? HFXSTATE_PAUSE : 0)|
					(bMuted ? HFXSTATE_MUTE : 0)|
					(bHaulted ? HFXSTATE_HAULT : 0)|
					(bExiting ? HFXSTATE_EXITING : 0)	) 
	{}; 
	// actual flags. (shared memory)
	HFXStateStorage Storage; 
	// flags in boolean form. (shared memory)
	struct {
		bool Running:1; 
		bool Paused:1; 
		bool Muted:1; 
		bool Haulted:1; 
		bool Exiting:1; 
	}; 
	// flags in bool group ( for -> operator )
	struct HFXStateFlagGroup{
		bool Running:1; 
		bool Paused:1; 
		bool Muted:1; 
		bool Haulted:1; 
		bool Exiting:1; 
	}Flags; 
 
	// type cast operators to flagstorage type
	HFX_INLINE operator HFXStateStorage &(){return Storage;}
	HFX_INLINE operator const HFXStateStorage &()const{return Storage;}
	HFX_INLINE operator HFXStateStorage (){return Storage;}
	HFX_INLINE operator const HFXStateStorage ()const{return Storage;}
 
	// at operator to get reference of storage.
	HFX_INLINE HFXStateStorage &operator *(){return Storage;}; 
	HFX_INLINE const HFXStateStorage &operator *()const{return Storage;}; 
	 
	// bool operator, to see if there are any flags set.
	HFX_INLINE bool operator()(int bit){return (Storage & bit)!=0;}; 
 
	// -> operator to get boolean access flags quickly.
	HFX_INLINE HFXStateFlagGroup *operator ->(){return &Flags;}; 
	HFX_INLINE const HFXStateFlagGroup *operator ->()const{return &Flags;}; 
	 
	HFX_INLINE HFXStateStorage *operator &(){return &Storage;}; 
	HFX_INLINE const HFXStateStorage *operator &()const{return &Storage;}; 
 
	HFX_INLINE HFXStateStorage &operator =(HFXStateTransfer state){Storage = state; return Storage;}
	HFX_INLINE HFXStateStorage &operator |=(HFXStateTransfer state){Storage |= state; return Storage;}
	HFX_INLINE HFXStateStorage &operator &=(HFXStateTransfer state){Storage &= state; return Storage;}
	HFX_INLINE HFXStateStorage &operator ^=(HFXStateTransfer state){Storage |= state; return Storage;}
	HFX_INLINE HFXStateStorage operator ~()const{return ~Storage;}
	HFX_INLINE HFXStateStorage operator |(HFXStateTransfer state){return (Storage|state);}
	HFX_INLINE HFXStateStorage operator &(HFXStateTransfer state){return (Storage&state);}
	HFX_INLINE HFXStateStorage operator ^(HFXStateTransfer state){return (Storage^state);}
	HFX_INLINE bool operator ==(HFXStateTransfer state){return (Storage==state);}
	HFX_INLINE bool operator !=(HFXStateTransfer state){return (Storage!=state);}
	HFX_INLINE bool operator !(){return (Storage==0);}
	HFX_INLINE operator bool(){return (Storage!=0);}
	HFX_INLINE HFXStateFlags *UtilPointer(){return this;}; 
	HFX_INLINE const HFXStateFlags *UtilPointer()const{return this;}; 
	HFX_INLINE HFXStateFlags &Util(){return *this;}; 
	HFX_INLINE const HFXStateFlags &Util()const{return *this;}; 
}; 
 
enum HFXNeeds
{
	//effect has a application thread time sync
	HFXNEED_SYNC				=(1 << 0), 
	//effect has a servo thread time update
	HFXNEED_PROCESS				=(1 << 1), 
	//effect takes parameters
	HFXNEED_SET					=(1 << 2), 
	//effect needs stack access
	HFXNEED_STACK				=(1 << 3), 
	//device slots. if you need a device you need a stack.
	HFXNEED_DEVICE1				=(1 << 4), 
	HFXNEED_DEVICE2				=(1 << 5), 
	HFXNEED_DEVICE3				=(1 << 6), 
	HFXNEED_DEVICE4				=(1 << 7), 
	HFXNEED_DEVICE5				=(1 << 8), 
	HFXNEED_DEVICE6				=(1 << 9), 
	HFXNEED_DEVICE7				=(1 << 10), 
	HFXNEED_DEVICE8				=(1 << 11), 
	//ability to create/destroy sub effects
	HFXNEED_MANAGE				=(1 << 12), 
	//effect needs to know how long its been running.
	HFXNEED_RUNTIME				=(1 << 13), 
	//effect needs to know how long its been since the last time its processed a function.
	HFXNEED_FRAMETIME			=(1 << 14), 
	//effect needs a fixed time sent to its framtime
	HFXNEED_FIXEDTIME			=(1 << 15), 
	//effect has a init function
	HFXNEED_INSTANCE			=(1 << 16), 
	//effect needs owning stack's parent access
	HFXNEED_PARENT				=(1 << 17), 
	//effect is self deleting
	HFXNEED_REMOVE				=(1 << 18), 
	//effect does not give handles. these effects need to be self deleting
	HFXNEED_ENCAPSULATED		=(1 << 19), 
 
	HFXNEED_EXIT				=(1 << 20), 
 
	//state change inform flags
	HFXNEED_STATE_INFORM		=(1 << 21), 
	HFXNEED_RUNNING_INFORM		=(1 << 22), 
	HFXNEED_PAUSE_INFORM		=(1 << 23), 
	HFXNEED_HAULT_INFORM		=(1 << 24), 
	HFXNEED_MUTE_INFORM			=(1 << 25), 
	HFXNEED_EXITING_INFORM		=(1 << 26), 
 
	// USE THE BELOW FLAGS TO DECLARE FEATURES TO A EFFECT CLASS WHEN REGISTERING
	HFXNEED_DECL_SYNC			=(HFXNEED_SYNC), 
	HFXNEED_DECL_PROCESS		=(HFXNEED_PROCESS), 
	HFXNEED_DECL_STACK			=(HFXNEED_STACK), 
	HFXNEED_DECL_DEVICE1		=(HFXNEED_DEVICE1|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE2		=(HFXNEED_DEVICE2|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE3		=(HFXNEED_DEVICE3|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE4		=(HFXNEED_DEVICE4|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE5		=(HFXNEED_DEVICE5|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE6		=(HFXNEED_DEVICE6|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE7		=(HFXNEED_DEVICE7|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_DEVICE8		=(HFXNEED_DEVICE8|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_RUNTIME		=(HFXNEED_RUNTIME), 
	HFXNEED_DECL_FRAMETIME		=(HFXNEED_FRAMETIME), 
	// there is no decl for HFXNEED_FIXEDTIME as it is pointless without
	// a runtime or frametime decl.
	HFXNEED_DECL_FIXEDFRAME		=(HFXNEED_FRAMETIME|HFXNEED_FIXEDTIME), 
	HFXNEED_DECL_FIXEDRUNTIME	=(HFXNEED_DECL_RUNTIME|HFXNEED_FIXEDTIME), 
	HFXNEED_DECL_MANAGE			=(HFXNEED_DECL_FRAMETIME|HFXNEED_DECL_STACK), 
	HFXNEED_DECL_REMOVE			=(HFXNEED_REMOVE), 
	HFXNEED_DECL_ENCAPSULATED	=(HFXNEED_ENCAPSULATED|HFXNEED_DECL_REMOVE), 
	HFXNEED_DECL_PARENT			=(HFXNEED_PARENT), 
	HFXNEED_DECL_INSTANCE		=(HFXNEED_INSTANCE), 
	HFXNEED_DECL_SET			=(HFXNEED_SET), 
	// there is no HFXNEED_STATE_INFORM declare as it would be pointless without a
	// declaration of specific informs
	HFXNEED_DECL_RUNNING_INFORM	=(HFXNEED_RUNNING_INFORM|HFXNEED_STATE_INFORM), 
	HFXNEED_DECL_PAUSE_INFORM	=(HFXNEED_PAUSE_INFORM|HFXNEED_STATE_INFORM), 
	HFXNEED_DECL_MUTE_INFORM	=(HFXNEED_MUTE_INFORM|HFXNEED_STATE_INFORM), 
	HFXNEED_DECL_EXITING_INFORM	=(HFXNEED_EXITING_INFORM|HFXNEED_STATE_INFORM), 
	HFXNEED_DECL_HAULT_INFORM	=(HFXNEED_HAULT_INFORM|HFXNEED_STATE_INFORM), 
	// only use this if you will actually be using all informs!
	HFXNEED_DECL_TOTAL_INFORM	=(	HFXNEED_RUNNING_INFORM|HFXNEED_PAUSE_INFORM|
									HFXNEED_MUTE_INFORM|HFXNEED_EXITING_INFORM|
									HFXNEED_STATE_INFORM), 
	// INFORMATION FLAGS
	HFXNEED_INFO_DEVICE			=(	HFXNEED_DEVICE1|HFXNEED_DEVICE2|
									HFXNEED_DEVICE3|HFXNEED_DEVICE4|
									HFXNEED_DEVICE5|HFXNEED_DEVICE6|
									HFXNEED_DEVICE7|HFXNEED_DEVICE8), 
}; 
 
#define HFXNEED_UTIL_COUNT_DEVICES(flags) \
	( ( ( (flags) & HFXNEED_INFO_DEVICE ) != 0) ? ( \
		( ( ( (flags) & HFXNEED_DEVICE1 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE2 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE3 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE4 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE5 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE6 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE7 ) != 0) ? 1 : 0 ) + \
		( ( ( (flags) & HFXNEED_DEVICE8 ) != 0) ? 1 : 0 ) ) \
		: 0 ) 
 
typedef unsigned __int32 HFXNEED; 
 
#define HFX_XCHANGE 16
#define HFX_YCHANGE 17
#define HFX_ZCHANGE 18
 
enum HFXResult
{
	HFXRESULT_ERROR			=-2,	 
	HFXRESULT_FINISHED		=-1,	 
	HFXRESULT_CONTINUE		=0,	 
	// x axis was set		 
	HFXRESULT_XCHANGED		=(1 << HFX_XCHANGE),	 
	// y axis was set		 
	HFXRESULT_YCHANGED		=(1 << HFX_YCHANGE),	 
	// z axis was set		 
	HFXRESULT_ZCHANGED		=(1 << HFX_ZCHANGE),	 
	// x and y axis was set	 
	HFXRESULT_XYCHANGED		=(HFXRESULT_XCHANGED|HFXRESULT_YCHANGED),	 
	// x and z axis was set	 
	HFXRESULT_XZCHANGED		=(HFXRESULT_XCHANGED|HFXRESULT_ZCHANGED),	 
	// y and z axis was set	 
	HFXRESULT_YZCHANGED		=(HFXRESULT_YCHANGED|HFXRESULT_ZCHANGED),	 
	// x and y axis was set	 
	HFXRESULT_YXCHANGED		=HFXRESULT_XYCHANGED,	 
	// x and z axis was set	 
	HFXRESULT_ZXCHANGED		=HFXRESULT_XZCHANGED,	 
	// y and z axis was set	 
	HFXRESULT_ZYCHANGED		=HFXRESULT_YZCHANGED,	 
	// x, y and z axis  set	 
	HFXRESULT_XYZCHANGED	=(HFXRESULT_XCHANGED|HFXRESULT_YCHANGED|HFXRESULT_ZCHANGED),	 
	HFXRESULT_CHANGED		=HFXRESULT_XYZCHANGED,	 
}; 
 
typedef int HFXRESULT; 
 
 
class HFX_PURE_INTERFACE IHapticEffect
{
public: 
	// return false if you decide the processor given to you is not sufficient
	// or any other reason the effect should not be made. 
	// !called one time only. before any other funciton calls.
	// IF YOU WANT THIS TO BE CALLED YOU MUST REGISTER YOUR EFFECT WITH HFXNEED_DECL_INSTANCE
	virtual bool Initialize(IHFXProcessor &processor)=0; 
	// called by the game thread and blocks the haptics thread so the game and haptics loop are safe inside.
	// IF YOU WANT THIS TO BE CALLED YOU MUST REGISTER YOUR EFFECT WITH HFXNEED_DECL_SYNC
	virtual HFXRESULT SyncOp(IHFXProcessor &processor)=0; 
	//called at haptic rate. should set the output parameter to the target output force.
	// calculation should optimized and precise in here.
	// IF YOU WANT THIS TO BE CALLED YOU MUST REGISTER YOUR EFFECT WITH HFXNEED_DECL_PROCESS
	virtual HFXRESULT Update(IHFXProcessor &processor)=0; 
	//return true if parameters sent to you are sufficient.
	virtual bool Set(IHFXProcessor &processor, IHFXParamGroup *parameter)=0; 
	//notification of state change. 
	// WILL ONLY BE CALLED IF YOU DECLARE YOUR EFFECT REGISTER WITH A _INFORM decl and will ONLY be called with those.
	//   note: state will be only one bit. for every bit changed on sync this function will be called.
	virtual void OnStateChange(IHFXProcessor &processor, HFXSTATE state, bool flagged)=0; 
}; 
typedef IHapticEffect IHFXEffect; 
 
typedef void (*hfxFilterFunction)(double outvect[3]); 
typedef void (*HFXCreate_t)(IHFXEffect*&ptr); 
typedef void (*HFXDestroy_t)(IHFXEffect*&ptr); 
 
template<typename T> 
void HFXDefaultAllocateEffect(IHFXEffect *&ptr){ptr = new T;}; 
 
template<typename T> 
void HFXDefaultDeallocateEffect(IHFXEffect *&ptr){if(!ptr)return; delete ((T*)ptr); ptr=0;}; 
 
class IHapticsSystem; 
typedef IHapticsSystem IHFXSystem; 
class IDevice; 
typedef IDevice IHFXDevice; 
class IStack; 
typedef IStack IHFXStack; 
 
// T == effect class
template<typename T>  
inline HFXEffectID HFXDefaultRegisterEffect(IHFXSystem &hfxSystem, const char *tag, HFXNEED needs, IHFXParamGroup *defaults, HFXCreate_t specialCreate = HFXDefaultAllocateEffect<T>, HFXDestroy_t specialDestroy = HFXDefaultDeallocateEffect<T> )  
{
	hfxSystem.LogMessage((const int)1,"EFFECT REGISTERING! %s Size = %i \n", tag, sizeof(T));  
	return hfxSystem.RegisterEffectClass(  
		tag,   
		specialCreate,   
		specialDestroy,  
		needs,   
		defaults);  
};  
  
// T == effect class
// P == parametergroup class
template<typename T, typename P>  
inline HFXEffectID HFXDefaultRegisterEffect(IHFXSystem &hfxSystem, const char *tag, HFXNEED needs, HFXCreate_t specialCreate = 0, HFXDestroy_t specialDestroy = 0)  
{
	P *pGroup = new P;  
	HFXEffectID retval=0;  
	if(pGroup->CopyDefaults(pGroup))  
	{
		retval = HFXDefaultRegisterEffect<T>(hfxSystem, tag, needs, pGroup, specialCreate, specialDestroy);  
	}
	delete pGroup;  
	return retval;  
};  
 
#endif
 