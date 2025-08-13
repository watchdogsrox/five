#ifndef AUD_AUDSHORELINEPOOL_H
#define AUD_AUDSHORELINEPOOL_H

#include "vector/vector3.h"
#include "audshoreline.h"

const u32 g_PoolEdgePoints = 8;

class audShoreLine;

class audShoreLinePool : public audShoreLine
{
public:
	audShoreLinePool(const ShoreLinePoolAudioSettings* settings = NULL);
	~audShoreLinePool();

	virtual void Load(BANK_ONLY(const ShoreLineAudioSettings *settings = NULL));
	virtual void UpdateWaterHeight(bool updateAllHeights = false);
	virtual void Update(const f32 timeElapsedS);
	virtual bool UpdateActivation();
	virtual bool IsPointOverWater(const Vector3& pos) const;
	virtual WaterType GetWaterType() const { return AUD_WATER_TYPE_POOL; }

	static void InitClass();

	const ShoreLinePoolAudioSettings* GetPoolSettings()const {return static_cast<const ShoreLinePoolAudioSettings*>(m_Settings);};
#if __BANK
	virtual void AddDebugShorePoint(const Vector2 &vec);
	virtual void EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos);
	virtual void InitDebug();
	virtual void DrawActivationBox(bool useSizeFromData = false)const;
	virtual void DrawShoreLines(bool drawSettingsPoints = true, const u32 pointIndex = -1, bool checkForPlaying = false)const;
	virtual void RenderDebug() const ;
	virtual void FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName);
	virtual	bool Serialise(const char* name,bool editing = false);
	virtual void MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 side);
	virtual u32 GetNumPoints();
	static void AddWidgets(bkBank &bank);
	static void AddEditorWidgets(bkBank &bank);
	static void Cancel();
#endif

private:
	void GetSearchIndices(const Vector3 &position2D,s32 &startIdx,s32 &endIdx,s32 &startSIdx,s32 &endSIdx) const;
#if __BANK
	f32 CalculateSmallestDistanceToPoint();
#endif

	Vector3										m_PoolSoundDir;

	atRangeArray<audSound*,g_PoolEdgePoints> 	m_WaterLappingSounds;
	atRangeArray<u32,g_PoolEdgePoints>	     	m_LastSplashTime;
	atRangeArray<u16,g_PoolEdgePoints>	 	 	m_RandomSplashDelay;
	atRangeArray<u8,g_PoolEdgePoints>	  	 	m_WaterEdgeIndices;

	audSound*									m_PoolWaterSound;
	f32 										m_PoolSize;
	f32 										m_PoolWaterHeight;
	u32 										m_TimeSinceLastPoolSoundPlayed;

	static audSoundSet							sm_PoolWaterSounds;
	static audCurve								sm_WindStrengthToTimeFrequency; 
	static audCurve								sm_WindStrengthToDistance; 
	static audCurve								sm_WindStrengthToSpeed; 
	static u8									sm_NumExtraPoolPointsToUpdate;

#if __BANK
	static s32 									sm_PoolWaterLappingDelayMin; 
	static s32 									sm_PoolWaterLappingDelayMax; 
	static s32 									sm_PoolWaterSplashDelayMin;
	static s32 									sm_PoolWaterSplashDelayMax;
	static bool									sm_ShowIndices;
#endif
};
#endif // AUD_AUDSHORELINEPOOL_H
