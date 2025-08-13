#ifndef AUD_AUDSHORELINEOCEAN_H
#define AUD_AUDSHORELINEOCEAN_H

#include "vector/vector3.h"
#include "audshoreline.h"

class audShoreLine;

class audShoreLineOcean : public audShoreLine
{
public:
	audShoreLineOcean(const ShoreLineOceanAudioSettings* settings = NULL);
	~audShoreLineOcean();

	virtual void 						Load(BANK_ONLY(const ShoreLineAudioSettings *settings = NULL));
	virtual void 						UpdateWaterHeight(bool updateAllHeights = false);
	virtual void 						Update(const f32 timeElapsedS);
	virtual bool 						UpdateActivation();
	virtual bool 						IsPointOverWater(const Vector3& pos) const;
	virtual WaterType					GetWaterType() const { return AUD_WATER_TYPE_OCEAN; }

	virtual void						CalculateLeftPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
														 ,Vector3 &leftPoint,const f32 distance, const u8 closestRiverIdx = 0);
	virtual void						CalculateRightPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
														  ,Vector3 &rightPoint,const f32 distance, const u8 closestRiverIdx = 0);
	void								CalculateDetectionPoints(const Vector3 &centrePoint);


	void								SetPrevOceanReference(audShoreLineOcean* prevShoreline) { m_PrevShoreline = prevShoreline;};
	void								SetNextOceanReference(audShoreLineOcean* nextShoreline) { m_NextShoreline = nextShoreline;};	
	void								ComputeClosestShore(Vec3V position);

	audShoreLineOcean* 					GetPrevOceanReference() { return m_PrevShoreline;};
	audShoreLineOcean* 					GetNextOceanReference() { return m_NextShoreline;};

	static audShoreLineOcean*			GetClosestShore() {return sm_ClosestShore;};
	const ShoreLineOceanAudioSettings*	GetOceanSettings() const {return static_cast<const ShoreLineOceanAudioSettings*>(m_Settings);};
	void 								CalculatePoints(Vector3 &centrePoint,Vector3 &leftPoint,Vector3 &rightPoint, const u8 closestRiverIdx = 0);

	static void							InitClass();
	static void							ShutDownClass();
	static void							PlayOutInTheOceanSound();
	static void							StopOutInTheOceanSound();
	static f32							GetClosestDistanceToShore() {return sm_ClosestDistToShore;}
	static f32							GetSqdDistanceIntoWater() {return sm_SqdDistanceIntoWater;}
	static void							ResetDistanceToShore(bool resetClosestShoreline = false);
	static void							ComputeListenerOverOcean(); 
	static bool							IsListenerOverOcean() { return sm_ListenerAboveWater;}
	static bool							IsPointOverOceanWater(const Vector3& pos);

#if __BANK
	virtual void 						AddDebugShorePoint(const Vector2 &vec);
	virtual void 						EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos);
	virtual void 						InitDebug();
	virtual void 						DrawShoreLines(bool drawSettingsPoints = true, const u32 pointIndex = -1,bool checkForPlaying = false)const;
	virtual void 						RenderDebug() const ;
	virtual void 						FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName);
	virtual	bool 						Serialise(const char* name,bool editing = false);
	virtual void 						MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 side);
	virtual u32 						GetNumPoints();
	const char *						GetOceanTypeName(OceanWaterType oceanType);
	const char *						GetOceanDirectionName(OceanWaterDirection oceanDirection);
	static void 						AddWidgets(bkBank &bank);
	static void 						AddEditorWidgets(bkBank &bank);
	static void 						Cancel();
#endif

private:	
	void 								ComputeClosestShore();
	void								ComputeShorelineNodesInfo();

	void								GetSoundsRefs(audMetadataRef &soundRef,const u8 soundIdx);
	void 								UpdateOceanBeachSound(const Vector3 &centrePoint,const Vector3 &leftPosition,const Vector3 &rightPosition);
	void								UpdateOceanSound(const audMetadataRef soundRef,const u8 soundIdx,const Vector3 &position);
	void 								UpdateWaveStarts(const Vector3 &leftPosition,const Vector3 &rightPosition);
	void 								UpdateWaveBreaks(const Vector3 &leftPosition,const Vector3 &rightPosition);
	void 								UpdateWaveEndsAndRecede(const Vector3 &leftPosition,const Vector3 &rightPosition);
	void								StopSounds();

	audShoreLineOcean*					FindFirstReference();
	audShoreLineOcean*					FindLastReference();
	bool 								IsClosestShore();
	bool								IsFirstShoreLine();

	static atRangeArray<audSound*,NumWaterSoundPos> 					sm_Sounds;
	static atRangeArray<audSound*,NumWaterSoundPos> 					sm_OceanWaveStartSound;
	static atRangeArray<audSound*,NumWaterSoundPos> 					sm_OceanWaveBreakSound;
	static atRangeArray<audSound*,NumWaterSoundPos> 					sm_OceanWaveEndSound;
	static atRangeArray<audSound*,NumWaterSoundPos> 					sm_OceanWaveRecedeSound;
	static atRangeArray<audShoreLineOcean*,NumWaterSoundPos> 			sm_CurrentShore;
	static atRangeArray<f32,NumWaterHeights>							sm_WaterHeights;

 
	// Next and prev shoreline refs
	audShoreLineOcean*									m_PrevShoreline;
	audShoreLineOcean*									m_NextShoreline;

	static audShoreLineOcean*							sm_ClosestShore;

	static audSound*									sm_OutInTheOceanSound;

	static audSoundSet									sm_OceanSoundSet;

	static Vector3										sm_PrevNode;
	static Vector3										sm_ClosestNode;
	static Vector3										sm_NextNode;
	static Vector3										sm_CurrentOceanDirection;
	static Vector3										sm_WaveStartDetectionPoint;
	static Vector3										sm_WaveBreaksDetectionPoint;
	static Vector3										sm_WaveEndsDetectionPoint;

	static f32											sm_WaveStartDetectionPointHeight;
	static f32											sm_WaveBreaksDetectionPointHeight;
	static f32											sm_WaveEndsDetectionPointHeight;

	static f32											sm_LastWaveStartDetectionPointHeight;
	static f32											sm_LastWaveBreaksDetectionPointHeight;
	static f32											sm_LastRecedeEndDetectionPointHeight;

	static f32											sm_ClosestDistToShore;
	static f32											sm_DistanceToShore;

	static f32											sm_SqdDistanceIntoWater;

	static f32											sm_InnerDistance;
	static f32											sm_OutterDistance;
	static f32											sm_MaxDistanceToShore;

	static audSmoother									sm_ProyectionSmoother;
	static audSmoother									sm_WidthSmoother;

	static u8											sm_ClosestNodeIdx;
	static u8											sm_LastClosestNodeIdx;
	static u8											sm_PrevNodeIdx;
	static u8											sm_NextNodeIdx;

	static bool											sm_ListenerAboveWater;
};

#endif // AUD_AUDSHORELINEOCEAN_H
