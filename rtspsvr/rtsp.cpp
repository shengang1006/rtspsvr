#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "watcher.h"
#include "rtsp.h"

std::string dateStr(){
	char buf[64] = {0};
	time_t tt = time(NULL);
    strftime(buf, sizeof(buf), "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
	return buf;
}

int ParseRTSPRequestString(std::string  requst, 
						   std::string & cmd,
						   std::string & urlPreSuffix,
						   std::string & urlSuffix,
						   std::string & cseq,
						   std::string & sessionId){
	//CMD rtsp://192.168.20.136:5000/xxx666 RTSP/1.0
	//DESCRIBE rtsp://wm.microsoft.com/ms/video/0001-hi.wmv RTSP/1.0
	
	size_type npos = std::string::npos;

	size_type pos = requst.find(' ');
	if(npos == pos){
		return -1;
	}
	cmd = requst.substr(0, pos);//DESCRIBE


	size_type rtspok = requst.find(" RTSP/", pos + 1);
	if(npos == rtspok){
		return -1;
	}
	
	
	//rtsp://wm.microsoft.com/ms/video/0001-hi.wmv
	std::string url = requst.substr(pos + 1, rtspok - pos - 1);
	
	pos = url.find("rtsp://");
	if(npos == pos){
		return -1;
	}

	//first space or slash after "host" or "host:port"
	size_type i = url.find('/', pos + 7);

	//k: last non-space before "RTSP/"
	size_type k = url.length();

	//k1: last slash in the range [i,k]
	size_type k1 = url.rfind('/');

	//  [k1+1,k] 0001-hi.wmv
	if (i != npos && k > k1 + 1){
		urlSuffix = url.substr(k1 + 1, k - k1 -1);
	}

	//[i+1, k1) ms/video
	if (i != npos && k1 > i + 1){
		urlPreSuffix = url.substr(i +1, k1 - i -1);
	}

	//get cseq
	pos = requst.find("CSeq:", rtspok);
	if(npos == pos){
		return -1;
	}
	pos += 5;
	while(requst[++pos] == ' ');
	size_type spos = requst.find("\r\n", pos);
	if(npos == spos){
		return 0;
	}
	cseq = requst.substr(pos, spos - pos);


	//get Session id
	pos = requst.find("Session:", pos);
	if(npos == pos){
		return 0;
	}
	pos += 7;
	while(requst[++pos] == ' ');
	spos = requst.find("\r\n", pos);
	if(npos == spos){
		return 0;
	}
	sessionId = requst.substr(pos, spos - pos);
	//get Content-Length: need add

	return 0;
}

int parseTransportHeader(std::string & fullRequestStr,
						 int &mode,
						 std::string & destAddrStr,
						 uchar & dstTTL,
						 uint &clientRtpPort, uint &clientRtcpPort, //if udp
						 uchar & rtpChannel, uchar& rtcpChannel //if tcp
						 ){
	mode = RTP_UDP;
	dstTTL = 255;
	clientRtpPort = 0;
	clientRtcpPort = 0;
	rtpChannel = rtcpChannel = 0Xff;

	uint16 p1 = 0, p2 = 0;
	uint   ttl = 0,  rtpId = 0, rtcpId =0;	
	
	size_type pos = 0;
	size_type end = fullRequestStr.length() - (size_type)4; // /r/n/r/n
	
	size_type npos = std::string::npos;
	size_type t = fullRequestStr.find("Transport:");
	if (t == npos){
		error_log("no Transport\n");
		return -1;
	}
	t += 9;
	while(fullRequestStr[++t] == ' ');
	std::string strParse = fullRequestStr.substr(t, fullRequestStr.length() - t);
	
	do
	{
		pos = fullRequestStr.find(';', t);
		if (pos == npos){
			pos = end;
		}
		
		std::string str = fullRequestStr.substr(t, pos - t);
		t = pos + 1;
		
		if (str == "RTP/AVP/TCP")	{
			mode = RTP_TCP;
		}
		else if (str == "RAW/RAW/UDP" || str == "MP2T/H2221/UDP"){
			return -1;
		}
		else if (str.find("destination=") != npos){
			destAddrStr = str.substr(12);
		}
		else if (sscanf(str.c_str(), "ttl%d", &ttl) == 1){
			dstTTL = (int)ttl;
		}
		else if (sscanf(str.c_str(), "client_port=%hu-%hu", &p1, &p2) == 2)
		{
			clientRtpPort = p1;
			clientRtcpPort = mode == RAW_UDP ? 0 : p2;
		}
		else if (sscanf(str.c_str(), "client_port=%hu", &p1) == 1)
		{
			clientRtpPort = p1;
			clientRtcpPort = mode == RAW_UDP ? 0 : (p1 + 1);
		}
		else if (sscanf(str.c_str(), "interleaved=%u-%u", &rtpId, &rtcpId) == 2)
		{
			rtpChannel  = rtpId;
			rtcpChannel = rtcpId; //�����ŵ���Ϊ�����ŵ�
		}
		
	}while(pos != end);

	return 0;
}

int parseRange(std::string & fullRequestStr, 
			   std::string & absStart, 
			   std::string & absEnd, 
			   double & startTime, 
			   double & endTime){
	//parse range
	size_type k = fullRequestStr.find("Range: ");
	
	if(k == std::string::npos){
		return -1;
	}
	k += 6;
	
	//remove space
	while(fullRequestStr[++k] == ' ');
	
	const char * paramStr = fullRequestStr.c_str() + k;
	
	int numCharsMatched = 0;
	char as[32] = {0}, ae[32] = {0};
	
	
	if(sscanf(paramStr, "npt = %lf - %lf",  &startTime, &endTime) == 2){
	}
	else if (sscanf(paramStr, "npt = %lf -",  &startTime) == 1){
		if (startTime < 0){
			endTime = - startTime;
			startTime = 0.0;
		}
		else{
			endTime  = 0.0;
		}
	}
	else if (fullRequestStr.find("npt=now-") != std::string::npos){
		startTime = endTime = 0.0;
	}
	else if (sscanf(paramStr, "clock = %n",  &numCharsMatched) == 0 && numCharsMatched > 0){
		startTime = endTime = 0.0;
		sscanf(&paramStr[numCharsMatched], "%[^-]-%s", as, ae);
		absStart = as;
		absEnd = ae;
	}
	else if (sscanf(paramStr, "smtpe = %n", &numCharsMatched) == 0 && numCharsMatched > 0){
		// not interpret
	}
	else{
		return -1;
	}
	
	return 0;
}


rtsp::rtsp(){
	
}

rtsp::~rtsp(){
	 std::map<std::string, MediaSession*>::iterator it = m_mediaSessionMap.begin();
	 for(; it != m_mediaSessionMap.end(); it++){
		 delete it->second;
	 }
	 m_mediaSessionMap.clear();
}

int rtsp::on_initialize(){
	return 0;
}

int rtsp::on_connect(connection * n){
	return 0;
}

int rtsp::on_accept(connection * n){

	RtspClientConnection * session = new RtspClientConnection(n, this);
	n->set_context((void*)session);
	return 0;
}

int rtsp::on_unpack(char * data, int len, int& packetlen, char *&packet){
	
	if (data[0] != '$'){
		char * flag = strstr(data, "\r\n\r\n");
		if(flag){
			packet = data;
			packetlen = flag - data + 4;
			return packetlen;
		}
		else{
			return 0;
		}
	}
	else{
		
		if(len < 4){
			return 0;
		}
		
		//uint rtpChannel  = data[1];
		int rtpTotalLen = (data[2]<<8) | data[3];
		if (len < rtpTotalLen + 4){
			return 0;
		}
		else{
			packet = data;
			packetlen = rtpTotalLen;
			return rtpTotalLen;
		}
	}
}

//add 2014-7-11
int rtsp::on_recv(connection * n, char * data, int len){

	if (data[0] != '$'){
		rtsp_msg(n, data, len);
	}
	else{
		rtp_msg(n, data, len);
	}
	return 0;
}

int rtsp::rtp_msg(connection * n, char * data, int len){
	return 0;
}

int rtsp::rtsp_msg(connection * n, char * data, int len){

	RtspClientConnection * clientConnection = (RtspClientConnection*)n->get_context();
	std::string  cmd;
	std::string  urlPreSuffix;
	std::string  urlSuffix;
	std::string  strSessionId;
	std::string  cseq;
	std::string  requst(data, len);

	int ret = ParseRTSPRequestString(requst, cmd, urlPreSuffix, urlSuffix, cseq, strSessionId);
	if(ret < 0){
		return clientConnection->handleCmd_bad();
	}

	debug_log("[%u]parseRTSPRequestString:{%s}\n", pthread_self(), requst.c_str());

	bool requsetInsesseion = !strSessionId.empty();
	RtspClientSession * clientSession = NULL;

	clientConnection->SetSeq(cseq);
	if (requsetInsesseion){
		clientSession = clientConnection->lookupRtspClientSession(strSessionId);
		if(!clientSession){
			return clientConnection->handleCmd_sessionNotFound();
		}
	}
	 
	if (cmd == "OPTIONS"){
		clientConnection->handle_options();
	}
	else if (cmd == "DESCRIBE"){
		clientConnection->handle_describle(urlPreSuffix, urlSuffix, requst);
	}
	else if (cmd == "SETUP")
	{
		if(!requsetInsesseion){
			clientSession = clientConnection->createNewRtspClientSession();
		}
		clientSession->handle_setup(clientConnection, urlPreSuffix, urlSuffix, requst);
	}
	else if (cmd == "PLAY" || 
			 cmd == "PAUSE" || 
			 cmd == "GET_PARAMETER" ||
			 cmd == "SET_PARAMETER" || 
			 cmd == "TEARDOWN"){
		
		clientSession->handle_insession(clientConnection, cmd, urlPreSuffix, urlSuffix, requst);

	}
	else if(cmd == "REGISTER" || cmd == "REGISTER_REMOTE"){
	}
	else{
		clientConnection->handleCmd_notSupported();
	}

	return 0;
}



int rtsp::on_close(connection * n,  int reason){

	RtspClientConnection * clientConnection = (RtspClientConnection*)n->get_context();

	if (clientConnection){
		delete clientConnection;
		error_log("v_close RtspClientConnection close, reason %d\n", reason);
	}

	n->set_context(0);

	return 0;
}

int rtsp::add_delay_task(int id, int delay, void * context){
	return set_timer(id, delay, context);
}

int rtsp::on_timer(int event, int interval, void * ptr){

	mediaCtrl * mc = (mediaCtrl *)ptr;
	//printf("ref %d\n", mc->ReferncCount());
	if(mc->ReferncCount() > 1){
		mc->Playing(this);
	}
	else{
		mediaCtrlHub::instance()->destoryMediaCtrl(mc);
	}
	return 0;
}


MediaSession* rtsp::createAttachMediaSession(std::string & streamName){
	std::map<std::string, MediaSession*>::iterator it = m_mediaSessionMap.find(streamName);
	if (it != m_mediaSessionMap.end()){
		return it->second;
	}
	
	MediaSession * session = createMediaSession(streamName);
	if (session){
		m_mediaSessionMap[streamName]= session;
	}
	return session;
}

MediaSession* rtsp::createMediaSession(std::string & streamName){

	MediaSession * session = NULL;
	size_type pos = streamName.rfind('.');
	if (pos == std::string::npos){
		return NULL;
	}
	
	int resue = true;
	mediaCtrl* mc = mediaCtrlHub::instance()->getMediaCtrl(streamName, resue);
	
	std::string  extension = streamName.substr(pos , streamName.length() - pos);
	if (extension == ".264" || extension == ".h264"){
		session = new MediaSession(streamName);
		MediaSubSession * subSession = new H264MediaSubSession(mc);
		session->AddSubSession(subSession);
		subSession->SetTrackId(session->SubSessionCount());
	}
	else if (extension == ".aac"){
		session = new MediaSession(streamName);
		MediaSubSession * subSession = new Mp4AMediaSubSession(mc);
		session->AddSubSession(subSession);
		subSession->SetTrackId(session->SubSessionCount());
	}
	else{
		return NULL;
	}
	

	return session;
}

MediaSession* rtsp::getAndDetachMediaSession(std::string & streamName){
	std::map<std::string, MediaSession*>::iterator it = m_mediaSessionMap.find(streamName);
	if (it != m_mediaSessionMap.end()){
		MediaSession * ms = it->second;
		m_mediaSessionMap.erase(it);
		return ms;
	}
	else{
		return createMediaSession(streamName);
	}
}
/************************************/
RtspClientSession::RtspClientSession(std::string & sessionId){
	m_sessionId = sessionId;
	m_tcpStreamCount = 0;
	m_mediaSession = NULL;
}

RtspClientSession::~RtspClientSession(){
	safe_del(m_mediaSession);
}

int RtspClientSession::handle_setup(RtspClientConnection * rcc, 
									std::string & urlPreSuffix, 
									std::string & urlSuffix, 
									std::string & fullRequestStr){

	//    "urlPreSuffix" is empty and "urlSuffix" is the session (stream) name, or
	//    "urlPreSuffix" concatenated with "urlSuffix" (with "/" inbetween) is the session (stream) name.

	//rtsp://192.168.20.136:5000/xxx666/trackID=0 RTSP/1.0 

	std::string streamName = urlPreSuffix;
	std::string trackId = urlSuffix;
	if(urlPreSuffix.empty()){
		streamName = urlSuffix;
	}
	
	if(m_mediaSession){
		if (m_mediaSession->StreamName() != streamName){
			return rcc->handleCmd_bad();
		}
	}
	else{
		m_mediaSession = rcc->get_rtsp()->getAndDetachMediaSession(streamName);
	}
	
	if (!m_mediaSession){
		return rcc->handleCmd_notFound();
	}

	if (trackId.empty() && m_mediaSession->SubSessionCount() != 1){
		return rcc->handleCmd_bad();
	}

	MediaSubSession * subsession = NULL;
	if (trackId.empty()){
		subsession = m_mediaSession->GetSubSession(0);
	}
	else{
		 subsession = m_mediaSession->Lookup(trackId);
	}

	if (!subsession){
		return rcc->handleCmd_notFound();
	}

	int transMode = RTP_UDP;
	std::string strDstAdder;
	uchar  dstTTL = 255;
	uint  rtpPort = 0;  //UDP
	uint  rtcpPort = 1; //UDP
	uchar  rtpChannel = 0xff; //tcp
	uchar  rtcpChannel = 0xff; //tcp	
	
	parseTransportHeader(fullRequestStr, transMode, strDstAdder, dstTTL, 
		rtpPort, rtcpPort, rtpChannel, rtcpChannel);
	
	if (transMode == RAW_UDP){
		rcc->handleCmd_notSupported();
		return -1;
	}

	if (transMode == RTP_TCP && rtpChannel == 0xff){
		// An anomolous situation, caused by a buggy client.  Either:
		// 1/ TCP streaming was requested, but with no "interleaving=" fields.  (QuickTime Player sometimes does this.), or
		// 2/ TCP streaming was not requested, but we're doing RTSP-over-HTTP tunneling (which implies TCP streaming).
		rtpChannel = m_tcpStreamCount++;
		rtcpChannel = m_tcpStreamCount++;
	}

	if (fullRequestStr.find("x-playNow:") != std::string::npos){
		rcc->handleCmd_notSupported();
		return -1;
	}
	
	ipaddr dstAddr = {{0},0};
	if(!strDstAdder.empty()){
		strncpy(dstAddr.ip, strDstAdder.c_str(), sizeof(dstAddr.ip));
	}
	else{
		dstAddr = rcc->get_peeraddr();
	}
	
	const ipaddr & localAddr = rcc->get_localaddr();
	uint dstIp = ntohl(inet_addr(dstAddr.ip ));
	
	std::string response;

	if (transMode == RTP_TCP){
		subsession->GetTcpParam((void*)rcc, rtpChannel, rtcpChannel, dstTTL);
		append(response,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %s\r\n"
			"%s"
			"Transport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d\r\n"
			"Session: %s\r\n\r\n",
			rcc->getSeq().c_str(), 
			dateStr().c_str(),
			dstAddr.ip, localAddr.ip, 
			rtpChannel, rtcpChannel,
			m_sessionId.c_str());
	}
	else{
		
		uint serverRtpPort  = 0;
		uint serverRtcpPort = 0;
		subsession->GetUdpParam(rtpPort, rtcpPort, dstIp, serverRtpPort, serverRtcpPort);
		append(response,
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %s\r\n"
			"%s"
			"Transport: RTP/AVP;unicast;source=%s;client_port=%d-%d;server_port=%d-%d\r\n"
			"Session: %s\r\n\r\n",
			rcc->getSeq().c_str(), 
			dateStr().c_str(), 
			localAddr.ip,
			rtpPort, rtcpPort, serverRtpPort, serverRtcpPort,
			m_sessionId.c_str());
	}
	
	debug_log("S->C : %s\n", response.c_str());
	rcc->post_send(response.c_str(), response.length());

	return 0;
}

int RtspClientSession::handle_insession(RtspClientConnection * rcc,
									std::string & cmd, 
									std::string & urlPreSuffix, 
									std::string & urlSuffix, 
									std::string & fullRequestStr){

	if (!m_mediaSession){
		rcc->handleCmd_notSupported();
		return -1;
	}

	//Look up the media subsession whose track id is "urlSuffix":
	MediaSubSession *subsession = NULL;
	if (!urlSuffix.empty() && urlPreSuffix == m_mediaSession->StreamName()){
		//rtsp://192.168.20.136:5000/xxx666/trackID=0 RTSP/1.0 
		m_mediaSession->Lookup(urlSuffix);
	}
	else if(urlSuffix == m_mediaSession->StreamName() || 
		(urlSuffix.empty() && urlPreSuffix == m_mediaSession->StreamName())){
		//rtsp://192.168.20.136:5000/xxx666
		//rtsp://192.168.20.136:5000/xxx666/
		// Aggregated operation
		subsession = m_mediaSession->GetSubSession(0);
	}
	else if(!urlPreSuffix.empty() && !urlSuffix.empty()){
		//rtsp://192.168.20.136:5000/media/xxx666
		// Aggregated operation, if <urlPreSuffix>/<urlSuffix> is the session (stream) name:
		std::string streamName = urlPreSuffix + "/" + urlSuffix;
		if(streamName == m_mediaSession->StreamName()){
			subsession = m_mediaSession->GetSubSession(0);
		}
	}
	
	if (!subsession){
		return rcc->handleCmd_notFound();
	}

	if(cmd == "PLAY")
		handle_play(rcc, subsession, fullRequestStr);
	else if(cmd == "PAUSE")
		handle_pause(rcc, subsession);
	else if(cmd == "GET_PARAMETER")
		handle_getparameter(rcc, subsession, fullRequestStr);
	else if(cmd == "SET_PARAMETER")
		handle_setparameter(rcc, subsession, fullRequestStr);
	else if(cmd == "TEARDOWN")
		handle_teardown(rcc, subsession, fullRequestStr);

	return 0;
}


int RtspClientSession::handle_pause(RtspClientConnection * rcc, 
									MediaSubSession * subSession){

	return 0;
}

int RtspClientSession::handle_play(RtspClientConnection * rcc, 
								   MediaSubSession * subSession, 
								   std::string & fullRequestStr){

	std::string strScale;
	
	//parse scale
	size_type pos = fullRequestStr.find("Scale:");
	if(pos != std::string::npos){
		pos += 5;
		while(fullRequestStr[++pos] == ' ');
		float fscale = (float)atof(fullRequestStr.c_str() + pos);
		append(strScale, "Scale: %f\r\n", fscale);
	}
	
	const ipaddr & localAddr = rcc->get_localaddr();
	std::string rtspUrl;
	append(rtspUrl, "RTP-Info: rtsp://%s:%u/%s/%s;seq=%u//;rtptime=%u", 
		localAddr.ip, watcher::instance()->tcp_port(),
		m_mediaSession->StreamName().c_str(),
		subSession->GetTrackId().c_str(),
		subSession->SeqNo(),
		subSession->RtpTimestamp());
	


	std::string absstart, absend;
	double startTime = 0, endTime = 0;
	
	int result = parseRange(fullRequestStr, absstart, absend, startTime, endTime);
	std::string response;
	append(response,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n" 
		"%s" 
		"%s"
		"Session: %s\r\n"
		"%s\r\n\r\n",
		rcc->getSeq().c_str(), 
		dateStr().c_str(), 
		strScale.c_str(),
		m_sessionId.c_str(),
		rtspUrl.c_str());
	if(result < 0){
		
	}
	
	debug_log("play: %s\n", response.c_str());

	rcc->post_send(response.c_str(), response.length());
	subSession->StartStream(rcc->get_rtsp());
	
	return 0;
}

int RtspClientSession::handle_teardown(RtspClientConnection * rcc, 
									   MediaSubSession * subSession, 
									   std::string & fullRequestStr){
	std::string response;	
	append(response,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %s\r\n\r\n",
		rcc->getSeq().c_str(), 
		dateStr().c_str(),
		m_sessionId.c_str());
		
	rcc->post_send(response.c_str(), response.length());

	subSession->StopStream();
	m_mediaSession->DelSubSession(subSession);
	delete subSession;

	if(!m_mediaSession->SubSessionCount()){
		delete m_mediaSession;
		m_mediaSession = NULL;
	}
	return 0;
}

int RtspClientSession::handle_getparameter(RtspClientConnection * rcc, 
										   MediaSubSession * subSession, 
										   std::string & fullRequestStr){
	return rcc->setRtspResponse("200 OK", m_sessionId.c_str(), "2015.10.25");
}

int RtspClientSession::handle_setparameter(RtspClientConnection * rcc, 
										   MediaSubSession * subSession, 
										   std::string & fullRequestStr){
    return rcc->setRtspResponseSession("200 OK", m_sessionId.c_str());
}
/*---------------------------------------
 *	
 *
 *-------------------------------------*/

RtspClientConnection::RtspClientConnection(connection * n, rtsp * r){
	m_client = n;
	m_rtsp = r;
}

RtspClientConnection::~RtspClientConnection(){
	std::map<std::string, RtspClientSession*>::iterator it = m_clientSessionMap.begin();
	for(; it != m_clientSessionMap.end(); it++){
		delete it->second;
	}
	m_clientSessionMap.clear();
}


const ipaddr & RtspClientConnection::get_peeraddr(){
	return m_client->get_peeraddr();
}

const ipaddr & RtspClientConnection::get_localaddr(){
	return m_client->get_localaddr();
}

std::string & RtspClientConnection::getSeq(){
	return m_seqNo;
}

int RtspClientConnection::post_send(const char * data, int len){
	return m_client->post_send((char*)data, len);	
}

void RtspClientConnection::SetSeq(std::string & seqNo){
	m_seqNo = seqNo; 
}

rtsp* RtspClientConnection::get_rtsp(){
	return m_rtsp;
}

int RtspClientConnection::handle_options(){

	std::string str;
	append(str, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Public: %s\r\n\r\n",
		getSeq().c_str(), 
		dateStr().c_str(), 
		"OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER");

	return post_send(str.c_str(), str.length());

}


int RtspClientConnection::handle_describle(
										   std::string & urlPreSuffix,
										   std::string & urlSuffix,
										   std::string & fullRequestStr)
{

	std::string urlTotalSuffix = urlPreSuffix ;
	if (!urlTotalSuffix.empty()){
		urlTotalSuffix.append("/");
	}
	urlTotalSuffix.append(urlSuffix);

	//authenticationOK, add

	MediaSession * session = m_rtsp->createAttachMediaSession(urlTotalSuffix);
	if (!session){
		handleCmd_notFound();
		return -1;
	}

	const ipaddr & localAddr = m_client->get_localaddr();
	std::string sdp = session->GenerateSDPDescription(localAddr);

	//get the rtsp url
	//rtsp://127.0.0.1/
	std::string rtspUrl;

	append(rtspUrl, "rtsp://%s:%u/%s", 
		localAddr.ip, watcher::instance()->tcp_port() ,
		session->StreamName().c_str());

	std::string response = "RTSP/1.0 200 OK\r\n";

	append(response, "CSeq: %s\r\n"
					 "%s"
					 "Content-Base: %s\r\n"
					 "Content-Type: application/sdp\r\n"
					 "Content-Length: %d\r\n\r\n"
					 "%s",
					 getSeq().c_str(),
					 dateStr().c_str(),
					 rtspUrl.c_str(),
					 sdp.length(),
					 sdp.c_str());

	debug_log("S-C : %s\n", response.c_str());
	
	return post_send(response.c_str(), response.length());
}


int RtspClientConnection::handle_getparameter(std::string & fullRequestStr)
{
	/*
	"GET_PARAMETER rtsp://172.16.128.98:554/TestSession/1.h264 RTSP/1.0
	CSeq: 6
	User-Agent: LibVLC/2.1.3 (LIVE555 Streaming Media v2014.01.21)
	Session: 8000
	*/
	return setRtspResponse("200 OK","2015.10.25");
}

int RtspClientConnection::handle_setparameter(std::string & fullRequestStr){
	return setRtspResponse("200 OK");
}

int RtspClientConnection::setRtspResponse(const char * responseStr){
	std::string response;
	append(response, 
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s\r\n",
		responseStr,
		m_seqNo.c_str(),
		dateStr().c_str());
	return post_send(response.c_str(), response.length());
}

int RtspClientConnection::setRtspResponse(const char * responseStr, const char * contentStr){
	if (!contentStr){
		contentStr = "";
	}
	std::string response;

	append(response,
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		m_seqNo.c_str(),
		dateStr().c_str(),
		strlen(contentStr),
		contentStr);
	return post_send(response.c_str(), response.length());
}

int RtspClientConnection::setRtspResponseSession(const char * responseStr, const char *  seseionId){
	std::string response;
	append(response, 
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %s\r\n\r\n",
		responseStr,
		m_seqNo.c_str(),
		dateStr().c_str(),
		seseionId);
	return post_send(response.c_str(), response.length());
}

int RtspClientConnection::setRtspResponse(const char * responseStr, 
	const char* seseionId, const char * contentStr){
	if (!contentStr){
		contentStr = "";
	}
	std::string response;
	append(response, 
		"RTSP/1.0 %s\r\n"
		"CSeq: %s\r\n"
		"%s"
		"Session: %s\r\n"
		"Content-Length: %d\r\n\r\n"
		"%s",
		responseStr,
		m_seqNo.c_str(),
		dateStr().c_str(),
		seseionId,
		strlen(contentStr),
		contentStr);
	return post_send(response.c_str(), response.length());
}

RtspClientSession * RtspClientConnection::createNewRtspClientSession(){

	std::string sessionId;
	append(sessionId, "S%lldD", GetTickCount64U());
	RtspClientSession * rcs = new RtspClientSession(sessionId);
	m_clientSessionMap.insert(std::make_pair(sessionId, rcs));
	return rcs;
}

RtspClientSession * RtspClientConnection::lookupRtspClientSession(const std::string & sessionId){
	std::map<std::string, RtspClientSession*>::iterator it = m_clientSessionMap.find(sessionId);
	return it == m_clientSessionMap.end()? NULL: it->second;
}

