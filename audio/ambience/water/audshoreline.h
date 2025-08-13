#ifndef AUD_AUDSHORELINE_H
#define AUD_AUDSHORELINE_H

// Rage Headers
#include "vector/vector3.h"
#include "vector/color32.h"
#include "spatialdata/aabb.h"
#include "audioengine/curve.h"
#include "audioengine/soundset.h"
#include "fwutil/QuadTree.h"

// Game headers
#include "audio/ambience/ambientaudioentity.h"

namespace rage
{
	class audSound;
	struct ShoreLineAudioSettings;
}

enum audWaterSoundPositioning
{
	Left = 0,
	Right,
	Centre,
	NumWaterSoundPos
};

enum audWaterHeights
{
	PrevIndex = 0,
	ClosestIndex,
	NextIndex,
	NumWaterHeights
};

class audShoreLine
{
public:
	audShoreLine();
	virtual ~audShoreLine() {};

	virtual void Load(BANK_ONLY(const ShoreLineAudioSettings *settings = NULL))= 0;
	virtual void Update(const f32 timeElapsedS) = 0;
	virtual void UpdateWaterHeight(bool updateAllHeights = false) = 0; 
	virtual bool UpdateActivation();
	virtual bool IsPointOverWater(const Vector3& pos) const = 0;
	virtual WaterType GetWaterType() const = 0;
	virtual void	CalculateLeftPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										,Vector3 &leftPoint,const f32 distance, const u8 closestRiverIdx = 0);
	virtual void	CalculateRightPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										,Vector3 &rightPoint,const f32 distance, const u8 closestRiverIdx = 0);

	void PlayRainOnWaterSound(const audMetadataRef &soundRef);
	f32	 GetDistanceToShore() {return m_DistanceToShore;};
	static void StopRainOnWaterSound() ;
	const fwRect& GetActivationBoxAABB() const { return m_ActivationBox; };
	fwRect& GetActivationBoxAABB()  { return m_ActivationBox; };
	bool IsActive() const						 { return m_IsActive; }

	const ShoreLineAudioSettings *GetSettings(){ return m_Settings;};

	// Computes the closest point to the player in the shore
	void CalculateCentrePoint(const Vector3& closestPoint,const Vector3& nextPoint,const Vector3& prevPoint
											, const Vector3& lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
											,const Vector3& playerPos,const Vector3& dirToPlayer, Vector3& centrePoint);
	static void InitClass();
	//Shoreline editor helpers
#if __BANK
	void SetActivationBoxSize(f32 width,f32 height);
	void MoveActivationBox(const Vector2 &vec);
	void RotateActivationBox(f32 rotAngle);

	Vector2 GetPoint(const u32 pointIdx);
	bool GetPointZ(const u32 pointIdx,Vector3 &point);
	const f32  GetWaterHeightOnPoint(u32 idx) const;
	const u32  GetWaterHeightsCount() const  { return m_WaterHeights.GetCount();};

	virtual void DrawActivationBox(bool useSizeFromData = false)const;
	virtual void RenderDebug() const;
	virtual void EditShorelinePointWidth(const u32 pointIdx);

	virtual void AddDebugShorePoint(const Vector2 &vec)= 0;
	virtual void EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos) = 0;
	virtual void InitDebug() = 0;
	virtual void DrawShoreLines(bool  drawSettingsPoints = true, const u32  pointIndex = -1, bool checkForPlaying = false )const  = 0;
	virtual void FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName)  = 0;
	virtual void MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 side)  = 0;
	virtual u32 GetNumPoints()  = 0;
	virtual	bool Serialise(const char* name,bool editing = false)  = 0;

	static void AddWidgets(bkBank &bank);
	static void AddEditorWidgets(bkBank &bank);
	static void Cancel();
#endif

protected:
#if __BANK
	void SerialiseActivationBox(char* xmlMsg, char* tmpBuf);
#endif
	naEnvironmentGroup* CreateShoreLineEnvironmentGroup(const Vector3 &position);


	Vector3							m_CentrePoint;
	const ShoreLineAudioSettings*	m_Settings;
	fwRect							m_ActivationBox;

	f32								m_DistanceToShore;
	bool 							m_IsActive;
	bool 							m_JustActivated;

	static audSound*				sm_RainOnWaterSound;
	static audCurve					sm_DistToNodeSeparation;
	static f32						sm_DistanceThresholdToStopSounds;

#if __BANK
	atArray<Vector2>				m_ShoreLinePoints;
	atArray<f32>					m_WaterHeights;

	f32								m_RotAngle;
	f32 							m_Width;
	f32 							m_Height;
	static u32						sm_MaxNumPointsPerShore;
	static u8						sm_ShoreLineType;

	static bool						sm_DrawWaterBehaviour;
#endif 

};

class ShoreLinesQuadTreeUpdateFunction : public fwQuadTreeNode::fwQuadTreeFn
{
public:

	ShoreLinesQuadTreeUpdateFunction(atArray<audShoreLine*>* activeShoreLines) :
	  m_ActiveShoreLines(activeShoreLines)
	  {

	  }

	  void operator()(const Vector3& UNUSED_PARAM(posn), void* data)
	  {
		  audShoreLine* shoreLine = static_cast<audShoreLine*>(data);

		  if(shoreLine)
		  {
#if __BANK
			  if(audAmbientAudioEntity::sm_DrawShoreLines)
			  {
				  shoreLine->DrawShoreLines();
			  }
			  if(audAmbientAudioEntity::sm_DrawActivationBox)
			  {
				  shoreLine->DrawActivationBox(true);
			  }
#endif
			  if(shoreLine->UpdateActivation())
			  {
				  m_ActiveShoreLines->PushAndGrow(shoreLine);
			  }
			  else 
			  {
				  //TODO: Make sure we stop all sounds and reset everything.
				  // shoreLine->Reset(); 
			  }
		  }
	  }
private:
	atArray<audShoreLine*>* m_ActiveShoreLines;
};

class ShoreLineQuadTreeIntersectFunction : public fwQuadTreeNode::fwQuadTreeFn
{
public:
	ShoreLineQuadTreeIntersectFunction(atArray<audShoreLine*>* shoreLines, const fwRect& boundingBox):
	  m_ShoreLines(shoreLines)
    , m_AABB(boundingBox)
	  {
	  }

	  void operator()(const fwRect& UNUSED_PARAM(bb), void* data)
	  {
		  audShoreLine* shoreLine = static_cast<audShoreLine*>(data);

		  if(shoreLine)
		  {
			  // Pools are generally too small to care about. May need to have a flag if there are any extra big ones
			  // that we don't want to play ambient rules above
			  if(shoreLine->GetWaterType() != AUD_WATER_TYPE_POOL)
			  {
				  if(shoreLine->GetActivationBoxAABB().IsInside(m_AABB) ||
					  shoreLine->GetActivationBoxAABB().DoesIntersect(m_AABB))
				  {
					  if(m_ShoreLines->Find(shoreLine) == -1)
					  {
						  m_ShoreLines->PushAndGrow(shoreLine);
					  }
				  }
			  }
		  }
	  }

private:
	atArray<audShoreLine*>* m_ShoreLines;
	fwRect m_AABB;
};


#if __BANK
class ShoreLinesQuadTreeRenderFunction : public fwQuadTreeNode::fwQuadTreeFn
{
public:

	ShoreLinesQuadTreeRenderFunction()  { }

	void operator()(const Vector3& UNUSED_PARAM(posn), void* data)
	{
		audShoreLine* shoreLine = static_cast<audShoreLine*>(data);

		if(shoreLine)
		{
			shoreLine->RenderDebug();
		}
	}
};
#endif
#endif // AUD_AUDSHORELINE_H
