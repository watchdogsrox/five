#ifndef _ANIM_SCENE_EVENT_ITERATOR_H_
#define _ANIM_SCENE_EVENT_ITERATOR_H_

#include "parser/structure.h"

template <class EventType>
class CAnimSceneEventIterator
{
public:
	CAnimSceneEventIterator(CAnimScene& animScene, bool overridePlaybackList = false, CAnimScenePlaybackList::Id overrideList = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);
	CAnimSceneEventIterator(const CAnimSceneEventIterator<EventType>& other);
	~CAnimSceneEventIterator();

	void begin();
	CAnimSceneEventIterator& operator++();
	CAnimSceneEventIterator operator++(int);
	const EventType* operator*() const;
	EventType* operator*();

private:
	EventType *FindNextEvent();
	EventType *FindNextEvent(CAnimSceneEventList& eventList, EventType* bestEvent, bool& passedCurrent);

	CAnimScene* m_animScene;
	EventType* m_currentEvent;
	bool m_overridePlaybackList;
	CAnimScenePlaybackList::Id m_overrideList;
};


template <class EventType>
CAnimSceneEventIterator<EventType>::CAnimSceneEventIterator(CAnimScene& animScene, bool overridePlaybackList /*= false*/, CAnimScenePlaybackList::Id overrideList/* = ANIM_SCENE_PLAYBACK_LIST_ID_INVALID*/) 
: m_animScene(&animScene)
, m_currentEvent(NULL) 
, m_overridePlaybackList(overridePlaybackList)
, m_overrideList(overrideList)
{

}

template <class EventType>
CAnimSceneEventIterator<EventType>::CAnimSceneEventIterator(const CAnimSceneEventIterator<EventType>& other) 
: m_animScene(other.m_animScene)
, m_currentEvent(other.m_currentEvent)
, m_overridePlaybackList(other.m_bOverridePlaybackList)
, m_overrideList(other.m_overrideList)
{

}

template <class EventType>
CAnimSceneEventIterator<EventType>::~CAnimSceneEventIterator()
{

}

template <class EventType>
void CAnimSceneEventIterator<EventType>::begin()
{
	m_currentEvent = NULL;
	m_currentEvent = FindNextEvent();
}

template <class EventType>
CAnimSceneEventIterator<EventType>& CAnimSceneEventIterator<EventType>::operator++() 
{ 
	if(m_currentEvent) 
	{ 
		m_currentEvent = FindNextEvent(); 
	} 

	return *this; 
}

template <class EventType>
CAnimSceneEventIterator<EventType> CAnimSceneEventIterator<EventType>::operator++(int) 
{ 
	CAnimSceneEventIterator<EventType> preVal(*this); 

	if(m_currentEvent) 
	{ 
		m_currentEvent = FindNextEvent(); 
	} 

	return preVal; 
}

template <class EventType>
const EventType* CAnimSceneEventIterator<EventType>::operator*() const 
{ 
	return (m_animScene ? m_currentEvent : NULL); 
}

template <class EventType>
EventType* CAnimSceneEventIterator<EventType>::operator*() 
{ 
	return (m_animScene ? m_currentEvent : NULL); 
}

template <class EventType>
EventType *CAnimSceneEventIterator<EventType>::FindNextEvent()
{
	if(!m_animScene)
	{
		return NULL;
	}

	bool passedCurrent = m_currentEvent ? false : true;

	EventType *bestEvent = NULL;
	
	{
		CAnimSceneSectionIterator it(*m_animScene, false, m_overridePlaybackList, m_overrideList);

		while (*it)
		{
			bestEvent = FindNextEvent((*it)->m_events, bestEvent, passedCurrent);
			++it;
		}
	}	

	int count = m_animScene->m_data->m_childScenes.GetCount();
	for(int i=0; i < count; ++i)
	{
		CAnimScene* childScene = m_animScene->m_data->m_childScenes[i];
		if(childScene)
		{
			CAnimSceneSectionIterator it(*childScene, false, m_overridePlaybackList, m_overrideList);

			while (*it)
			{
				bestEvent = FindNextEvent((*it)->m_events, bestEvent, passedCurrent);
				++it;
			}
		}
	}

	return bestEvent;
}

template <class EventType>
EventType *CAnimSceneEventIterator<EventType>::FindNextEvent(CAnimSceneEventList& eventList, EventType* bestEvent, bool& passedCurrent)
{
	float currentTime = m_currentEvent ? m_currentEvent->GetStart() : 0.f;
	
	int count = eventList.GetEventCount();
	for(int i=0; i < count; ++i)
	{
		CAnimSceneEvent *event = eventList.GetEvent(i);
		if(event && event->parser_GetStructure()->IsSubclassOf<EventType>())
		{
			if(event == m_currentEvent)
			{
				passedCurrent = true;
				continue;
			}
			float startTime = event->GetStart();
			if(startTime > currentTime || (passedCurrent && startTime == currentTime))
			{
				if(bestEvent == NULL || startTime < bestEvent->GetStart())
				{
					bestEvent = static_cast<EventType*>(event);
				}
			}
		}

		if(event->HasChildren() && event->GetChildren())
		{
			bestEvent = FindNextEvent(*event->GetChildren(), bestEvent, passedCurrent);
		}
	}

	return bestEvent;
}

#endif // _ANIM_SCENE_EVENT_ITERATOR_H_