#ifndef _INCLUDED_C_ASW_CAMERA_VOLUME_H
#define _INCLUDED_C_ASW_CAMERA_VOLUME_H
#ifdef _WIN32
#pragma once
#endif

// A clientside only entity used to control the camera
//   in_camera.cpp will check if the marine is within any of these camera volumes and if so,
//   tilt the camera to the pitch specified in the volume

class C_ASW_Camera_Volume : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Camera_Volume, C_BaseEntity );
	
	C_ASW_Camera_Volume();
	virtual ~C_ASW_Camera_Volume();

	bool Initialize();
	void Spawn();
	bool ShouldDraw();
	
	bool KeyValue( const char *szKeyName, const char *szValue );
	static void RecreateAll(); // recreate all clientside camera volumes in map
	static void DestroyAll();  // clear all clientside created camera volumes	
	static C_ASW_Camera_Volume *CreateNew(bool bForce = false);
	static float IsPointInCameraVolume(const Vector &src);	// is this point located within any of the camera volumes

	int m_fCameraPitch;

protected:
	static const char * ParseEntity( const char *pEntData );
	static void ParseAllEntities(const char *pMapData);
};

extern CUtlVector<C_ASW_Camera_Volume*>	g_ASWCameraVolumes;

#endif // _INCLUDED_C_ASW_CAMERA_VOLUME_H