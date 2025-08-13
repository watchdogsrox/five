#ifndef _ANIM_SCENE_ENTITY_ITERATOR_H_
#define _ANIM_SCENE_ENTITY_ITERATOR_H_

typedef atFixedBitSet<eAnimSceneEntityType_NUM_ENUMS> AnimSceneEntityTypeFilter;

class CAnimSceneEntityIterator
{
public:
	CAnimSceneEntityIterator(CAnimScene* animScene, const AnimSceneEntityTypeFilter& typeFilter);
	CAnimSceneEntityIterator(const CAnimSceneEntityIterator& other);
	~CAnimSceneEntityIterator();

	void begin();
	CAnimSceneEntityIterator& operator++();
	CAnimSceneEntityIterator operator++(int);
	const CAnimSceneEntity* operator*() const;
	CAnimSceneEntity* operator*();

private:
	typedef atBinaryMap<CAnimSceneEntity*, CAnimSceneEntity::Id> animSceneEntityMap;


	CAnimSceneEntity* GetCurrent() const;
	void FindFirstEntity(int& childIdx, int& mapIdx) const;
	bool FindFirstEntity(animSceneEntityMap& entityMap, int& mapIdx) const;

	CAnimScene* m_animScene;
	AnimSceneEntityTypeFilter m_typeFilter;
	int m_CurrentChildIdx;
	int m_CurrentMapIdx;
};


inline CAnimSceneEntityIterator::CAnimSceneEntityIterator(CAnimScene* animScene, const AnimSceneEntityTypeFilter& typeFilter) 
: m_animScene(animScene)
, m_typeFilter(typeFilter)
, m_CurrentChildIdx(-1)
, m_CurrentMapIdx(-1)
{
}

inline CAnimSceneEntityIterator::CAnimSceneEntityIterator(const CAnimSceneEntityIterator& other) 
: m_animScene(other.m_animScene)
, m_typeFilter(other.m_typeFilter)
, m_CurrentChildIdx(other.m_CurrentChildIdx)
, m_CurrentMapIdx(other.m_CurrentMapIdx)
{
}

inline CAnimSceneEntityIterator::~CAnimSceneEntityIterator()
{
}

inline void CAnimSceneEntityIterator::begin()
{
	m_CurrentChildIdx = -1;
	m_CurrentMapIdx = 0;
	FindFirstEntity(m_CurrentChildIdx, m_CurrentMapIdx); 
}

inline CAnimSceneEntityIterator& CAnimSceneEntityIterator::operator++() 
{ 
	if(m_animScene && m_CurrentChildIdx < m_animScene->m_data->m_childScenes.GetCount() && m_CurrentMapIdx >= 0)
	{
		++m_CurrentMapIdx;
		FindFirstEntity(m_CurrentChildIdx, m_CurrentMapIdx); 
	}
	return *this; 
}

inline CAnimSceneEntityIterator CAnimSceneEntityIterator::operator++(int) 
{ 
	CAnimSceneEntityIterator preVal(*this);
	++(*this);
	return preVal; 
}

inline const CAnimSceneEntity* CAnimSceneEntityIterator::operator*() const 
{ 
	return GetCurrent(); 
}

inline CAnimSceneEntity* CAnimSceneEntityIterator::operator*() 
{ 
	return GetCurrent(); 
}

inline CAnimSceneEntity* CAnimSceneEntityIterator::GetCurrent() const
{
	CAnimScene* scene = m_CurrentChildIdx < 0 ? m_animScene : (m_CurrentChildIdx < m_animScene->m_data->m_childScenes.GetCount() ? m_animScene->m_data->m_childScenes[m_CurrentChildIdx] : NULL);
	if(scene)
	{
		if(m_CurrentMapIdx < scene->m_entities.GetCount())
		{
			return *scene->m_entities.GetItem(m_CurrentMapIdx);
		}
	}

	return NULL;
}

inline void CAnimSceneEntityIterator::FindFirstEntity(int& childIdx, int& mapIdx) const
{
	if(!m_animScene)
	{
		return;
	}

	if(childIdx < 0)
	{
		if(FindFirstEntity(m_animScene->m_entities, mapIdx))
		{
			// Valid entity found
			return;
		}

		// No entity found, check the child scenes
		childIdx = 0;
		mapIdx = 0;
	}

	for(int count = m_animScene->m_data->m_childScenes.GetCount(); childIdx < count; ++childIdx)
	{
		CAnimScene* childScene = m_animScene->m_data->m_childScenes[childIdx];
		if(childScene)
		{
			if(FindFirstEntity(childScene->m_entities, mapIdx))
			{
				// Valid entity found
				return;
			}
		}

		// Reset index for next child
		mapIdx = 0;
	}
}

inline bool CAnimSceneEntityIterator::FindFirstEntity(animSceneEntityMap& entityMap, int& mapIdx) const
{
	for(int count = entityMap.GetCount(); mapIdx < count; ++mapIdx)
	{
		CAnimSceneEntity *entity = *entityMap.GetItem(mapIdx);
		if(m_typeFilter.IsSet(entity->GetType()))
		{
			return true;
		}
	}

	return false;
}

#endif // _ANIM_SCENE_ENTITY_ITERATOR_H_