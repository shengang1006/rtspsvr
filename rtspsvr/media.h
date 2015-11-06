#pragma once

#include "mediasub.h"

class MediaSession
{
public:
	MediaSession(const std::string & streamName);

	virtual ~MediaSession();

	void AddSubSession(MediaSubSession * pMediastream);
	
	void DelSubSession(MediaSubSession * pMediastream);

	MediaSubSession* Lookup(std::string & trackId);

	int SubSessionCount();

	MediaSubSession * GetSubSession(int index);

	const std::string & StreamName();

	std::string  GenerateSDPDescription(const ipaddr & local);

	uint increaseRef(){return 0;}

protected:
	std::string m_streamName;
	uint   m_createTime;
	std::vector <MediaSubSession *> m_vecSubSession;
};

