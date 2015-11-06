#include "mediactrl.h"

mediaCtrl::mediaCtrl(mediaSource * ms, bool reuse){
	m_playing = false;
	m_parsed = false;
	m_refernce = 0;
	m_mediaSource = ms;
	m_nextTimestamp = 0;
	m_reuse = reuse;
	m_timerId = globTimerId::GetId();
}

mediaCtrl:: ~mediaCtrl(){
	printf("~mediaCtrl\n");
	safe_del(m_mediaSource);
}

int mediaCtrl::StartPlay(IStreamCtrl * mss, ITimerTask* task){
	if (!m_playing){
		m_playing = true;
		m_nextTimestamp = 0;
		IncreaseRefernce();
		task->add_delay_task(m_timerId, 0, this);
	}
	m_streamCtrlMap[mss] = true;
	return 0;
}

int mediaCtrl::StopPlay(IStreamCtrl * mss){
	m_streamCtrlMap.erase(mss);
	return 0;
}

int mediaCtrl::Prepare(){
	if(!m_parsed){
		m_parsed = true;
		return m_mediaSource->parseMedia();
	}
	return 0;
}

int mediaCtrl::Close(){
	if(m_reuse){
		return 0;
	}
	m_parsed = false;
	m_playing = false;
	return m_mediaSource->closeMedia();	
}

bool mediaCtrl::IsReuse(){
	return m_reuse;
}

uint mediaCtrl::Timestamp_Inc(){
	return m_mediaSource->Timestamp_Inc();
}

int mediaCtrl::getSdp(std::string & sdp){
	return m_mediaSource->getSdp(sdp);
}

void mediaCtrl::setPayloadType(uint type){
	m_mediaSource->SetPayloadType(type);
}
	
const std::string & mediaCtrl::streamName(){
	return m_mediaSource->GetstreamName();
}

int mediaCtrl::Playing(ITimerTask * task){
	
	DecreaseRefernce();
	
	uchar* payload = NULL;
	int payload_size = 0;
	bool end_of_frame = false;
	while(!end_of_frame){

		int ret  = m_mediaSource->NextFrame(&payload, payload_size, end_of_frame);
		if (ret < 0){
			printf("debug:Playing fail or end\n");
			m_streamCtrlMap.clear();
			m_mediaSource->closeMedia();
			return -1;
		}
		std::map<IStreamCtrl *, bool>::iterator it =  m_streamCtrlMap.begin();
		for(; it != m_streamCtrlMap.end(); it++){
			it->first->TransferStream(payload, payload_size,end_of_frame);
		}
	}

	IncreaseRefernce();
	task->add_delay_task(m_timerId, TransferInterval(), this);
	return 0;
}

void mediaCtrl::IncreaseRefernce(){
	m_refernce++;
}

void mediaCtrl::DecreaseRefernce(){
	m_refernce--;
}

int mediaCtrl::ReferncCount(){
	return m_refernce;
}

uint mediaCtrl::TransferInterval(){

	int64 nowTimestamp = GetTickCount64U();

	uint duration = m_mediaSource->uDuration();//us
	int interval = 0;

	if (!m_nextTimestamp){
		m_nextTimestamp = nowTimestamp + duration;
		interval = duration;
	}
	else{
		m_nextTimestamp += duration;
		interval = (int)(m_nextTimestamp - nowTimestamp);
	}
	if (interval < 0){
		interval = 0;
	}
	return interval/1000;
}

////////////////////////////////////////////////////////

mediaCtrlHub::mediaCtrlHub(){
	
}

mediaCtrlHub * mediaCtrlHub::instance(){
	static mediaCtrlHub mch;
	return &mch;
}

mediaCtrl*  mediaCtrlHub::getMediaCtrl(const std::string & streamName, bool resue){

	mediaCtrl * ms = NULL;
	if(!resue){
		ms = createMediaCtrl(streamName, resue);
	}
	else{
		std::map<std::string, mediaCtrl*>::iterator it = m_mediaCtrlHub.find(streamName);
		if (it == m_mediaCtrlHub.end()){
			 ms = createMediaCtrl(streamName, resue);
		}
		else{
			ms = it->second;
		}
	}
	
	if(!ms){
		return ms;
	}
	
	ms->IncreaseRefernce();
	if(resue){
		m_mediaCtrlHub[streamName] = ms;
	}
	
	return ms;
}

void  mediaCtrlHub::destoryMediaCtrl(mediaCtrl * mc){
	if(mc->ReferncCount()){
		mc->DecreaseRefernce();
	}
	printf("destoryMediaCtrl %d\n", mc->ReferncCount());	
	if(!mc->ReferncCount()){
		if(mc->IsReuse()){
			m_mediaCtrlHub.erase(mc->streamName());	
		}
		delete mc;
	}
}

mediaCtrl*  mediaCtrlHub::createMediaCtrl(const std::string & streamName, bool resue){
	size_type pos = streamName.rfind('.');
	if (pos == std::string::npos){
		return NULL;
	}
	std::string  extension = streamName.substr(pos , streamName.length() - pos);
	mediaCtrl * mc = NULL;
	if (extension == ".264" || extension == ".h264"){
		mc =new mediaCtrl(new h264FileSource(streamName), resue);
	}
	else if(extension == ".aac"){
		mc = new mediaCtrl(new aacFileSource(streamName), resue);
	}
	
	return mc;
}

