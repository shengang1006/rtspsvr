#pragma  once

#include <map>
#include "worker.h"
#include "media.h"

class rtsp;
class RtspClientSession;

class RtspClientConnection
{
public:
	RtspClientConnection(connection * n, rtsp * r);
	virtual ~RtspClientConnection();
	
	int handle_options();
	
	int handle_describle(
		std::string & urlPreSuffix,
		std::string & urlSuffix,
		std::string & fullRequestStr);

	int handle_getparameter(std::string & fullRequestStr);
	
	int handle_setparameter(std::string & fullRequestStr);

	int handleCmd_bad(){return setRtspResponse("400 Bad Request");}

	int handleCmd_notSupported(){return setRtspResponse("405 Method Not Allowed");}

	int handleCmd_notFound(){return setRtspResponse("404 stream not find");}

	int handleCmd_sessionNotFound(){return setRtspResponse("454 Session Not Found");}

	int handleCmd_unsupportedTransport(){return setRtspResponse("461 Unsupported Transport");}

	int setRtspResponse(const char * responseStr);
	int setRtspResponse(const char * responseStr, const char * contentStr);
	int setRtspResponseSession(const char * responseStr, const char * seseionId);
	int setRtspResponse(const char * responseStr, const char * seseionId, const char * contentStr);

	const ipaddr & get_localaddr();
	const ipaddr & get_peeraddr();
	
	std::string &getSeq();
	void SetSeq(std::string & seqNo);
	int post_send(const char * data, int len);
	rtsp* get_rtsp();
	
	RtspClientSession * createNewRtspClientSession();
	RtspClientSession * lookupRtspClientSession(const std::string & sessionId);
protected:
	ipaddr m_serveAddr;
	connection * m_client;
	rtsp * m_rtsp;
	std::string m_seqNo;
	std::map<std::string, RtspClientSession*> m_clientSessionMap;
};

class RtspClientSession{
public:
	RtspClientSession(std::string & sessionId);
	~RtspClientSession();

	int handle_setup(
		RtspClientConnection * rcc,
		std::string & urlPreSuffix,
		std::string & urlSuffix,  
		std::string & fullRequestStr);
	
	int handle_insession(
		RtspClientConnection * rcc,
		std::string & cmd, 
		std::string & urlPreSuffix, 
		std::string & urlSuffix, 
		std::string & fullRequestStr);
	
	int handle_play(
		RtspClientConnection * rcc, 
		MediaSubSession * subSession, 
		std::string & fullRequestStr);
	
	int handle_pause(
		RtspClientConnection * rcc,
		MediaSubSession * subSession);
	
	int handle_teardown(
		RtspClientConnection * rcc, 
		MediaSubSession * subSession,
		std::string & fullRequestStr);
	
	int handle_getparameter(
		RtspClientConnection * rcc, 
		MediaSubSession * subSession,
		std::string & fullRequestStr);
	
	int handle_setparameter(
		RtspClientConnection * rcc, 
		MediaSubSession * subSession,
		std::string & fullRequestStr);

protected:
	MediaSession * m_mediaSession;
	uint8 m_tcpStreamCount;
	std::string  m_sessionId;
};


class rtsp : public worker, ITimerTask{
public:
	virtual ~rtsp();
	rtsp();

	int on_initialize();
	int on_accept(connection * n) ;
    int on_recv(connection * n, char * data, int len) ;
	int on_close(connection * n, int reason) ;
	int on_connect(connection * n) ;
	int on_timer(int event, int interval, void * ptr) ;
	int on_unpack(char * data, int len, int& packlen, char *&packet);
	
	int rtsp_msg(connection * n, char * data, int len);
	int rtp_msg(connection * n, char * data, int len);
	
	int add_delay_task(int id, int delay, void * context = NULL);
	
	virtual MediaSession* createAttachMediaSession(std::string & streamName);
	virtual MediaSession* getAndDetachMediaSession(std::string & streamName);
	
	friend class RtspClientSession;
protected:
	virtual MediaSession* createMediaSession(std::string & streamName);

private:
	 std::map<std::string, MediaSession*> m_mediaSessionMap;
};