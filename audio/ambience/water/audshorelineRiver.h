#ifndef AUD_AUDSHORELINERIVER_H
#define AUD_AUDSHORELINERIVER_H

#include "vector/vector3.h"
#include "audshoreline.h"

#define AUD_MAX_RIVERS_PLAYING 2

class audShoreLine;

class audShoreLineRiver : public audShoreLine
{
public:
	audShoreLineRiver(const ShoreLineRiverAudioSettings* settings = NULL);
	~audShoreLineRiver();

	virtual void						Load(BANK_ONLY(const ShoreLineAudioSettings *settings = NULL));
	virtual void						UpdateWaterHeight(bool updateAllHeights = false);
	virtual void						Update(const f32 timeElapsedS);
	virtual bool						UpdateActivation();
	virtual bool						IsPointOverWater(const Vector3& pos) const;
	virtual WaterType					GetWaterType() const { return AUD_WATER_TYPE_RIVER; }
	virtual void						CalculateLeftPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
															,Vector3 &leftPoint,const f32 distance, const u8 closestRiverIdx = 0);
	virtual void						CalculateRightPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
															,Vector3 &rightPoint,const f32 distance, const u8 closestRiverIdx = 0);
	void 								SetNextRiverReference(audShoreLineRiver* nextShoreline) { m_NextShoreline = nextShoreline;};
	void 								SetPrevRiverReference(audShoreLineRiver* prevShoreline) { m_PrevShoreline = prevShoreline;};

	audShoreLineRiver* 					GetNextRiverReference() { return m_NextShoreline;};
	audShoreLineRiver* 					GetPrevRiverReference() { return m_PrevShoreline;};

	const ShoreLineRiverAudioSettings*	GetRiverSettings() const {return static_cast<const ShoreLineRiverAudioSettings*>(m_Settings);};

	static void 						InitClass();
	static void							ShutDownClass();
	//static f32							GetClosestDistanceToShore() {return sm_ClosestDistToShore;}
	static void 						ResetDistanceToShore();
	static void 						CheckShorelineChange();
	static void							StopSounds(const u8 closestRiverIdx);

#if __BANK
	virtual void 						AddDebugShorePoint(const Vector2 &vec);
	virtual void 						EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos);
	virtual void 						EditShorelinePointWidth(const u32 pointIdx);
	virtual void 						InitDebug();
	virtual void 						DrawShoreLines(bool drawSettingsPoints = true, const u32 pointIndex = -1, bool checkForPlaying = true)const;
	virtual void 						RenderDebug() const ;
	virtual void 						FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName);
	const char *						GetRiverTypeName(RiverType riverType);
	virtual	bool 						Serialise(const char* name,bool editing = false);
	virtual void 						MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 side);
	virtual u32 						GetNumPoints();
	static void 						AddWidgets(bkBank &bank);
	static void 						AddEditorWidgets(bkBank &bank);
	static void 						Cancel();
	static void							DrawPlayingShorelines()  ;
#endif

private:
	struct audRiverSoundInfo
	{
		atRangeArray<atRangeArray<audSound*,NumWaterSoundPos>,2> 	sounds;
		atRangeArray<audShoreLineRiver*,NumWaterSoundPos> 			currentShore;
		atRangeArray<f32,NumWaterSoundPos> 							linkRatio;
		atRangeArray<f32,NumWaterHeights>							waterHeights;
		atRangeArray<u8,NumWaterSoundPos> 							currentSoundIdx;
		atRangeArray<bool,NumWaterSoundPos> 						ending;// Use for crossfading
	};
	struct audRiverInfo
	{
		Vector3	prevNode;
		Vector3 closestNode;
		Vector3	nextNode;
		audRiverSoundInfo riverSounds;
		audShoreLineRiver* river;
		f32 distanceToShore;
		f32 distanceToClosestNode;
		f32 currentWidth;
		f32 nextNodeWidth;
		f32 closestNodeWidth;
		f32 prevNodeWidth;
		u8 closestNodeIdx;
		u8 prevNodeIdx;
		u8 nextNodeIdx;
	};

	void 								CalculatePoints(Vector3 &centrePoint,Vector3 &leftPoint,Vector3 &rightPoint, const u8 closestRiverIdx = 0);
	void								ComputeShorelineNodesInfo(const u8 closestRiverIdx);
	void 								ComputeClosestShores();
	void								CalculateClosestShoreInfo(f32 &sqdDistance, u8 &closestNodeIdx);
	void								GetSoundsRefs(RiverType riverType, audMetadataRef &soundRef,const u8 soundIdx);
	void								UpdateRiverSound(const audMetadataRef soundRef,const u8 soundIdx,const bool ending,const Vector3 &position,const u8 closestRiverIdx);
	void 								UpdateRiverSounds(const u8 closestRiverIdx);

	f32									GetWaterHeightAtPos(Vec3V_In pos);

	audShoreLineRiver*					FindFirstReference();
	audShoreLineRiver*					FindLastReference();
	bool 								IsClosestShore(u8 &closestRiverIdx);
	bool								IsLinkedRiver(const audShoreLineRiver* river);
	bool								IsPrevShorelineLinked(const audShoreLineRiver* river);
	bool								IsNextShorelineLinked(const audShoreLineRiver* river);
	bool								LinkedToClosestRiver(const u8 avoidChecking);

	// Next and prev shoreline refs
	audShoreLineRiver*													m_NextShoreline;
	audShoreLineRiver*													m_PrevShoreline;

	static atRangeArray<audRiverInfo,AUD_MAX_RIVERS_PLAYING>			sm_ClosestShores;
	static atRangeArray<audShoreLineRiver*,AUD_MAX_RIVERS_PLAYING>		sm_LastClosestShores;

	static audCurve 													sm_EqualPowerFallCrossFade;
	static audCurve 													sm_EqualPowerRiseCrossFade;

	static audSoundSet													sm_BrookWeakSoundset;
	static audSoundSet													sm_BrookStrongSoundset;
	static audSoundSet													sm_LAWeakSoundset;
	static audSoundSet													sm_LAStrongSoundset;
	static audSoundSet													sm_WeakSoundset;
	static audSoundSet													sm_MediumSoundset;
	static audSoundSet													sm_StrongSoundset;
	static audSoundSet													sm_RapidsWeakSoundset;
	static audSoundSet													sm_RapidsStrongSoundset;

	static f32															sm_MaxDistanceToShore;
	static f32															sm_InnerDistance;
	static f32															sm_OutterDistance;

	BANK_ONLY(atArray<f32>												m_ShoreLinePointsWidth;);

	BANK_ONLY(static f32												sm_RiverWidthInCurrentPoint;);
	BANK_ONLY(static u32												sm_MaxNumPointsPerShore;);
};

#endif // AUD_AUDSHORELINERIVER_H
