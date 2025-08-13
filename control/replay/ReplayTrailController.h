#ifndef REPLAYTRAILCONTROLLER_H_
#define REPLAYTRAILCONTROLLER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "PacketBasics.h"
#include "templates/CircularArray.h"
#include "streaming/streamingdefs.h"

#include "control/replay/Misc/ReplayPacket.h"

class CReplayRecorder;
class CPacketTrailPolyFrame;

class CReplayTrailController
{
public:
	class CTrailPoly
	{
	public:

		CPacketVector<3>	m_vec0;
		CPacketVector<3>	m_vec1;
		CPacketVector<3>	m_vec2;

		u16					m_colourIndex;
		u16					m_uvIndex;
	};

	class CTrailColour
	{
	public:
		u32					m_colour1;
		u32					m_colour2;
		u32					m_colour3;

		bool operator==(const CTrailColour& other) const
		{
			return m_colour1 == other.m_colour1 && m_colour2 == other.m_colour2 && m_colour3 == other.m_colour3;
		}
	};

	class CTrailUV
	{
	public:
		CPacketVector<2>	m_uv1;
		CPacketVector<2>	m_uv2;
		CPacketVector<2>	m_uv3;

		bool operator==(const CTrailUV& other) const
		{
			return m_uv1 == other.m_uv1 && m_uv2 == other.m_uv2 && m_uv3 == other.m_uv3;
		}
	};

	static CReplayTrailController& GetInstance()
	{
		static CReplayTrailController instance;
		return instance;
	}

	CReplayTrailController();

	void		Init();
	void		Shutdown();

	void		Update();
	void		OnExitClip();

	void		AddPolyFromGame(u32 texDict, u32 tex, const Vec3V& vec0, const Vec3V& vec1, const Vec3V& vec2, Color32 colour1, Color32 colour2, Color32 colour3, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3);
	
	void		AddPolyListFromClip(CTrailPoly* pPolyPtr, u32 polyCount);
	void		AddPolyFromClip(const CTrailPoly& poly);

	void		SetCurrentPlaybackPacket(const CPacketTrailPolyFrame* pPacket)	{ m_pCurrentPlaybackPacket = pPacket; }

	void		SetTextureDictionaryToRelease(strLocalIndex texDict)			{ m_texDictionaryToRelease = texDict; }
private:

	int			GetColourIndex(const CTrailColour& colour);
	int			GetUVIndex(const CTrailUV& uv);

	atArray<CTrailPoly>		m_polys;
	atArray<CTrailColour>	m_colours;
	atArray<CTrailUV>		m_uvs;

	u32						m_texDictionary;
	u32						m_texture;

	strLocalIndex			m_texDictionaryToRelease;

	u8*						m_packetBuffer;

	const CPacketTrailPolyFrame*	m_pCurrentPlaybackPacket;
};



class CPacketTrailPolyFrame : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	void Store(u32 texDict, u32 tex, const atArray<CReplayTrailController::CTrailPoly>& polys, const atArray<CReplayTrailController::CTrailColour>& colours, const atArray<CReplayTrailController::CTrailUV>& uvs);
	void Extract(const CEventInfo<void>&) const;

	ePreloadResult Preload(const CEventInfo<void>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult Preplay(const CEventInfo<void>&) const					{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle /*handle*/) const {}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAILPOLYSFRAME, "Validation of CPacketTrailPolyFrame Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TRAILPOLYSFRAME;
	}

	CReplayTrailController::CTrailPoly*			GetPolys()			{ return (CReplayTrailController::CTrailPoly*)((u8*)this + m_offsetToPolys); }
	CReplayTrailController::CTrailColour*			GetColours()		{ return (CReplayTrailController::CTrailColour*)((u8*)GetPolys() + m_polyCount * sizeof(CReplayTrailController::CTrailPoly)); }
	CReplayTrailController::CTrailUV*				GetUVs()			{ return (CReplayTrailController::CTrailUV*)((u8*)GetColours() + m_colourCount * sizeof(CReplayTrailController::CTrailColour)); }

	const CReplayTrailController::CTrailPoly*		GetPolys() const	{ return (CReplayTrailController::CTrailPoly*)((u8*)this + m_offsetToPolys); }
	const CReplayTrailController::CTrailColour*	GetColours() const	{ return (CReplayTrailController::CTrailColour*)((u8*)GetPolys() + m_polyCount * sizeof(CReplayTrailController::CTrailPoly)); }
	const CReplayTrailController::CTrailUV*		GetUVs() const		{ return (CReplayTrailController::CTrailUV*)((u8*)GetColours() + m_colourCount * sizeof(CReplayTrailController::CTrailColour)); }

	void	Render() const;
private:
	CPacketTrailPolyFrame()
		: CPacketEvent(PACKETID_TRAILPOLYSFRAME, sizeof(CPacketTrailPolyFrame))
		, CPacketEntityInfo()
	{}

	u32		m_texDictionary;
	u32		m_texture;

	u32		m_offsetToPolys;
	u32		m_polyCount;
	u32		m_colourCount;
	u32		m_uvCount;

};

#endif // GTA_REPLAY

#endif // REPLAYTRAILCONTROLLER_H_