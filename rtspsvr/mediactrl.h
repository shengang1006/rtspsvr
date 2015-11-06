#pragma  once
#include "mediasource.h"
#include "tool.h"
#include <map>

class mediaCtrl{
	
public:
	mediaCtrl(mediaSource * ms, bool reuse);
	virtual ~mediaCtrl();

	int Prepare();
	int Close();
	
	int StartPlay(IStreamCtrl * mss, ITimerTask * task);
	int StopPlay(IStreamCtrl * mss);
	int Playing(ITimerTask* task);
	
	uint Timestamp_Inc();
	void setPayloadType(uint type);
	int getSdp(std::string & sdp);
	
	void IncreaseRefernce();
	void DecreaseRefernce();
	int ReferncCount();
	uint TransferInterval();
	
	bool IsReuse();
	
	const std::string & streamName();

protected:
	mediaSource * m_mediaSource;
	bool m_playing;
	bool m_reuse;
	bool m_parsed;
	std::map<IStreamCtrl *, bool> m_streamCtrlMap;
	int m_refernce;
	uint m_timerId;
	int64 m_nextTimestamp;
};

class mediaCtrlHub{
public:
	static mediaCtrlHub * instance();
private:
	mediaCtrlHub();		
	mediaCtrlHub(const mediaCtrlHub &);  
	mediaCtrlHub & operator = (const mediaCtrlHub &); 
	mediaCtrl* createMediaCtrl(const std::string & streamName, bool resue);
public:
		
	mediaCtrl* getMediaCtrl(const std::string & streamName, bool resue);
	
	void destoryMediaCtrl(mediaCtrl * mc);

public:
	std::map<std::string, mediaCtrl*> m_mediaCtrlHub;
};