#include "media.h"


/*---------------------------------------
 *	
 *
 *-------------------------------------*/
MediaSession::MediaSession(const std::string & streamName)
	:m_streamName(streamName){
	m_vecSubSession.clear();
}

MediaSession::~MediaSession(){
	for(std::vector <MediaSubSession *>::iterator it = m_vecSubSession.begin(); 
	it != m_vecSubSession.end(); it++){
		(*it)->StopStream();
		delete *it;
	}
	m_vecSubSession.clear();
}

void MediaSession::AddSubSession(MediaSubSession * pMediastream)
{
	pMediastream->SetPayloadType(m_vecSubSession.size());
	m_vecSubSession.push_back(pMediastream);
}

void MediaSession::DelSubSession(MediaSubSession * pMediastream)
{
	for(std::vector <MediaSubSession *>::iterator it = m_vecSubSession.begin(); 
	it != m_vecSubSession.end(); it++){
		if ((*it) == pMediastream){
			m_vecSubSession.erase(it);
			break;
		}
	}
}

MediaSubSession* MediaSession::Lookup(std::string & trackId)
{
	for(std::vector <MediaSubSession *>::iterator it = m_vecSubSession.begin(); 
	it != m_vecSubSession.end(); it++){
		if ((*it)->GetTrackId() == trackId){
			return *it;
		}
	}
	return NULL;
}

int MediaSession::SubSessionCount(){
	return m_vecSubSession.size();
}

MediaSubSession * MediaSession::GetSubSession(int index){
	return m_vecSubSession[index];
}


const std::string & MediaSession::StreamName(){
	return m_streamName;
}


std::string MediaSession::GenerateSDPDescription(const ipaddr & local){
	//v = SDP version
	std::string sdp("v=0\r\n");
	
	//o=  <username><sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
	append(sdp, "o=- %u %u IN IP4 %s:%d\r\n", m_createTime, m_createTime, local.ip, local.port);
	
	
    // v = session name
	sdp.append("s=RTSP SERVER 2014.6.27\r\n");
	
	//i = sdp information
	sdp.append("i=An Example of RTSP Session Usage\r\n");
	
	//ʱ����Ϣ���ֱ���ʾ��ʼ��ʱ���ͽ�����ʱ�䣬һ������ý����ֱ����ʱ���м��ıȽ϶ࡣ
	sdp.append("t=0 0\r\n");
	
	sdp.append("a=type:broadcast\r\n");
	
	sdp.append("a=control:*\r\n");
	
	//get duration here,������ʾý�����ĳ���
	float duration = 0.0;
	append(sdp, "a=range:npt=0-%.3f\r\n", duration);
	
	//get media strem sdp
	for(std::vector <MediaSubSession *>::iterator it = m_vecSubSession.begin(); 
	it != m_vecSubSession.end(); it++)
	{
		std::string subSdp;
		(*it)->GetSdpLines(subSdp);
		sdp.append(subSdp);
	}
	
	return sdp;
}
