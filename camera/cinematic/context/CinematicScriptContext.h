//
// camera/cinematic/CinematicScriptContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_SCRIPT_CONTEXT_H
#define CINEMATIC_SCRIPT_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"
#include "script/thread.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 


class camCinematicScriptContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicScriptContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool CreateScriptCinematicShot(u32 ShotRef, u32 Duration, scrThreadId scriptThreadId, const CEntity* pAttchEntity, const CEntity* pLookAtEntity); 

	bool DeleteCinematicShot(u32 ShotRef, scrThreadId scriptThreadId ); 
	
	bool IsScriptCinematicShotActive(u32 ShotRef) const;  

	u32 GetDuration() const { return m_Duration; }	
	
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

protected:

	virtual bool UpdateShotSelector(); 

	camCinematicScriptContext(const camBaseObjectMetadata& metadata);

	const camCinematicScriptContextMetadata& m_Metadata;
	
	virtual void ClearContext(); 
	
	virtual bool UpdateContext(); 

	void DeleteCinematicShot(); 

private:
	
	scrThreadId m_ScriptThreadId;

	u32 m_Duration; 

	bool m_IsShotActive; 
};


class camCinematicScriptedRaceCheckPointContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicScriptedRaceCheckPointContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper;

public:

	virtual void PreUpdate(); 
	
	bool RegisterPosition(const Vector3& pos); 
	
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

protected:
	virtual void ClearContext(); 

	camCinematicScriptedRaceCheckPointContext(const camBaseObjectMetadata& metadata);

	const camCinematicScriptedRaceCheckPointContextMetadata& m_Metadata;

private:

	u32 m_TimePositionWasRegistered; 

	bool m_IsValid; 
};

class camCinematicScriptedMissionCreatorFailContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicScriptedMissionCreatorFailContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper;

public:

	virtual bool IsValid(bool shouldConsiderControlInput = false, bool shouldConsiderViewMode = false) const; 

protected:
	virtual void ClearContext(); 

	camCinematicScriptedMissionCreatorFailContext(const camBaseObjectMetadata& metadata);

	const camCinematicScriptedMissionCreatorFailContextMetadata& m_Metadata;

private:
};



#endif
