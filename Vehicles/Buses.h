// Title	:	Buses.cpp
// Author	:	Obbe Vermeij
// Started	:	25/01/2005
//
//
//
#ifndef _BUSES_H_
#define _BUSES_H_

class CBuses
{
public:

	static void		Init(unsigned initMode);
	static void		Shutdown(unsigned shutdownMode);
	static void		Process();
};

#endif//_BUSES_H_

