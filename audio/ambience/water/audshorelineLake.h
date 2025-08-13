#ifndef AUD_AUDSHORELINELAKE_H
#define AUD_AUDSHORELINELAKE_H

#include "vector/vector3.h"
#include "audshoreline.h"

class audShoreLine;

class audShoreLineLake : public audShoreLine
{
public:
	audShoreLineLake(const ShoreLineLakeAudioSettings* settings = NULL);
	~audShoreLineLake();

	virtual void 						Load(BANK_ONLY(const ShoreLineAudioSettings *settings = NULL));
	virtual void 						UpdateWaterHeight(bool updateAllHeights = false);
	virtual void 						Update(const f32 timeElapsedS);
	virtual bool 						UpdateActivation();
	virtual bool 						IsPointOverWater(const Vector3& pos) const;
	virtual WaterType					GetWaterType() const { return AUD_WATER_TYPE_LAKE; }

	void 								SetNextLakeReference(audShoreLineLake* nextShoreline) { m_NextShoreline = nextShoreline;};
	void 								SetPrevLakeReference(audShoreLineLake* prevShoreline) { m_PrevShoreline = prevShoreline;};

	audShoreLineLake* 					GetNextLakeReference() { return m_NextShoreline;};
	audShoreLineLake* 					GetPrevLakeReference() { return m_PrevShoreline;};


	const ShoreLineLakeAudioSettings* 	GetLakeSettings() const {return static_cast<const ShoreLineLakeAudioSettings*>(m_Settings);};

	static void							InitClass();
	static void							ShutDownClass();
	static void							ResetDistanceToShore() {sm_ClosestDistToShore = LARGE_FLOAT;} ;

#if __BANK
	virtual void 						AddDebugShorePoint(const Vector2 &vec);
	virtual void 						EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos);
	virtual void 						InitDebug();
	virtual void 						DrawShoreLines(bool drawSettingsPoints = true, const u32 pointIndex = -1, bool checkForPlaying = false)const;
	virtual void 						RenderDebug() const ;
	virtual void 						FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName);
	virtual	bool 						Serialise(const char* name,bool editing = false);
	virtual void 						MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 side);
	virtual u32 						GetNumPoints();

	static void 						AddWidgets(bkBank &bank);
	static void 						AddEditorWidgets(bkBank &bank);
	static void 						Cancel();
#endif

private:
	void 								CalculatePoints(Vector3 &closestPoint,Vector3 &leftPoint,Vector3 &rightPoint, const u8 closestRiverIdx = 0);
	void 								ComputeClosestShore();
	void 								UpdateLakeSound();

	audShoreLineLake*					FindFirstReference();
	audShoreLineLake*					FindLastReference();
	bool								IsClosestShore();
	
	static atRangeArray<audSound*,NumWaterSoundPos - 1> sm_LakeSound;
	static atRangeArray<f32,NumWaterHeights>			sm_WaterHeights;

	// Next and prev shoreline refs
	audShoreLineLake*									m_NextShoreline;
	audShoreLineLake*									m_PrevShoreline;

	static audShoreLineLake*							sm_ClosestLakeShore;
	static audSoundSet									sm_LakeSounds;
	static f32											sm_MaxDistanceToShore;
	static f32											sm_InnerDistance;
	static f32											sm_OutterDistance;
	static f32											sm_ClosestDistToShore;
	static u8											sm_LakeClosestNode;
	static u8											sm_LastLakeClosestNode;

	BANK_ONLY(static bool								sm_UseLeftRightPoints;)
};

#endif // AUD_AUDSHORELINELAKE_H
