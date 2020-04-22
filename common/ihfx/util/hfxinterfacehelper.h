
typedef int (*NovintHFX_ExposeInterfaceFn)(void **ppInterface, void *hwnd, const char *cmd, unsigned int versionMaj, unsigned int versionMin, void *pMouseEnableFN, unsigned int TargetDevices);
inline const char *HFX_GetVersionMajorString(){
	return HFX_VERSION_MAJOR_SZ;
}
inline const char *HFX_GetVersionMinorString(){
	return HFX_VERSION_MINOR_SZ;
}
inline const char *HFX_CONNECT_FUNCTION_NAME(){
	return "_export_ExposeInterfaceHFX_" HFX_VERSION_MAJOR_SZ "_" HFX_VERSION_MINOR_SZ "_";
}
inline const char *HFX_CONNECT_FUNCTION_NAME_XML(){
	return "_export_ExposeInterfaceHFX_XML_" HFX_VERSION_MAJOR_SZ "_" HFX_VERSION_MINOR_SZ "_";
}
inline const char *HFX_DYNAMIC_LIBRARY_NAME(const unsigned int tries = 0){
	switch(tries)
	{
	default:
		return 0;
	break;

#ifdef HFX_DLL_CUSTOM_NAME
	case 0:
		return HFX_DLL_CUSTOM_NAME;
	break;
#endif

#ifndef HFX_DLL_CUSTOM_NAME
	case 0:
#else
	case 1:
#endif
	return "NovintHFX_" HFX_VERSION_MAJOR_SZ "." HFX_VERSION_MINOR_SZ ".dll";
	break;

#ifndef HFX_DLL_CUSTOM_NAME
	case 1:
#else
	case 2:
#endif
	return "NovintHFX.dll";
	break;

	};
}
inline unsigned int HFX_GetVersionMajor(){
	return HFX_VERSION_MAJOR;
}
inline unsigned int HFX_GetVersionMinor(){
	return HFX_VERSION_MINOR;
}
inline double HFX_GetVersion(){
	return HFX_VERSION_FLOAT;
}