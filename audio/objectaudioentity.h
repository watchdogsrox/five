#ifndef AUD_OBJECTAUDIOENTITY_H
#define AUD_OBJECTAUDIOENTITY_H

#include "gameobjects.h"
#include "scene/RegdRefTypes.h"

#include "audioentity.h"
#include "audio_channel.h"
#include "audioengine/soundset.h"

class CObject;
namespace rage
{
class audSound;
}

class audObjectAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audObjectAudioEntity);
	audObjectAudioEntity();
	~audObjectAudioEntity();

	void Init(CObject* object);
	void Init()
	{
		naAssertf(0, "Need to init audObjectAudioEntity with a CObject");
	}

	virtual void PreUpdateService(u32 timeInMs);

	void UpdateCreakSound(const Vector3 &pos, const f32 theta, const f32 amplitude);
#if __BANK
	static void  AddWidgets(bkBank &bank);
#endif // __BANK
private:

	naEnvironmentGroup* CreateClothEnvironmentGroup();

	audSoundSet m_WindClothSounds;
	audSound *m_ClothSound;
	RegdObj	 m_Object;

	u32		m_LastTimeTriggered;
	static f32 sm_SqdDistanceThreshold;
#if __BANK
	static f32 sm_OverriddenWindStrength;
	static bool sm_UseInterpolatedWind;
	static bool sm_OverrideWind;
	static bool sm_FindCloth;
	static bool sm_ShowClothPlaying;
#endif
};

#endif
