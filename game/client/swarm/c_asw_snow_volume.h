#ifndef _INCLUDED_C_ASW_SNOW_VOLUME_H
#define _INCLUDED_C_ASW_SNOW_VOLUME_H
#ifdef _WIN32
#pragma once
#endif

// A clientside only entity used to make snow in the map
//  first one of these created will also create the snow emitter, which will limit itself to snow volumes

class CASWSnowEmitter;
class C_ASW_Player;

class C_ASW_Snow_Volume : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_ASW_Snow_Volume, C_BaseEntity );
	
	C_ASW_Snow_Volume();
	virtual ~C_ASW_Snow_Volume();

	bool Initialize();
	void Spawn();
	bool ShouldDraw();
	
	bool KeyValue( const char *szKeyName, const char *szValue );
	static void RecreateAll(); // recreate all clientside snow volumes in map
	static void DestroyAll();  // clear all clientside created snow volumes	
	static C_ASW_Snow_Volume *CreateNew(bool bForce = false);
	static bool IsPointInSnowVolume(const Vector &src);	// is this point located within any of the snow volumes

	// updates the snow emitter in the position viewed by the local player
	static void UpdateSnow(C_ASW_Player *pPlayer);

	int m_fCameraPitch;
	int m_iSnowType; // 0 = light, 1 = heavy

protected:
	static const char * ParseEntity( const char *pEntData );
	static void ParseAllEntities(const char *pMapData);
};

extern CUtlVector<C_ASW_Snow_Volume*>	g_ASWSnowVolumes;
extern CSmartPtr<CASWSnowEmitter> g_hSnowEmitter;
extern CSmartPtr<CASWSnowEmitter> g_hSnowCloudEmitter;

#endif // _INCLUDED_C_ASW_SNOW_VOLUME_H