#include "mediasub.h"
#include "mediactrl.h"
#include "rtsp.h"

void InitRtpHeader(uint8 * header, uint8 payloadType, uint16 seqNO, 
					uint time_stamp, uint ssrc, int market){
	header[0] = 0x80;
	header[1] = payloadType;
	
	header[2] = (seqNO >> 8 )&0xff;;
	header[3] = seqNO & 0xff;
	
	header[4] = (uint8)(time_stamp >> 24 )&0xff;
	header[5] = (uint8)(time_stamp >> 16 )&0xff;
	header[6] = (uint8)(time_stamp >>  8 )&0xff;
	header[7] = (uint8)(time_stamp & 0xff);
	
	header[ 8] = (uint8)(ssrc >> 24 )&0xff;
	header[ 9] = (uint8)(ssrc >> 16 )&0xff;
	header[10] = (uint8)(ssrc >>  8 )&0xff;
	header[11] = (uint8)(ssrc & 0xff);
	
	if (market){
		header[1] = (uint8)(header[1]|0x80); //set market bit
	}
}

void init_socket_pair(socket_pair & sp){
	memset(&sp, 0, sizeof(sp));
	sp.rtcp_socket = -1;
	sp.rtp_socket = -1;
}

void clear_socket_pair(socket_pair & sp){
	if(sp.rtp_socket > 0){
		close(sp.rtp_socket);
		sp.rtp_socket = -1;
	}

	if(sp.rtcp_socket > 0){
		close(sp.rtcp_socket);
		sp.rtcp_socket = -1;
	}

	sp.rtcp_channel = NULL;
}

MediaSubSession::MediaSubSession(mediaCtrl * mc){

	m_payLoadType = 96;
	m_seqNO = 0;
	m_proto = RTP_UDP;
	m_ssrc = random_32();
	m_mediaCtrl = mc;
	m_timestamp = 0;
	init_socket_pair(m_socket_pair);
}

MediaSubSession::~MediaSubSession(){

	printf("~MediaSubSession\n");
	mediaCtrlHub::instance()->destoryMediaCtrl(m_mediaCtrl);	
	clear_socket_pair(m_socket_pair);
}

int MediaSubSession::GetSdpLines(std::string & sdpLines){
	return -1;
}

void MediaSubSession::SetTrackId(uint trackID){
	append(m_trackIdStr, "track%d", trackID);
}

const std::string &  MediaSubSession::GetTrackId(){
	return m_trackIdStr;
}

void MediaSubSession::SetPayloadType(uint8 number){
	m_payLoadType = 96 + number;
}

int MediaSubSession::GetUdpParam(uint clientRtpPort, 
								 uint clientRtcpPort,
								 uint dstIp,	
								 uint & serverRtpPort, 
							 	 uint & serverRtcpPort){

	m_proto = RTP_UDP;
	
	uint16 start_port = 3210 + random_32() % 60000;
	
	for (uint16 serverPort = start_port; serverPort < 65535; serverPort += 2){

		int rtpfd  = createUdpSocket(serverPort);
		if(rtpfd == -1){
			continue;
		}
		
		int rtcpfd = createUdpSocket(serverPort + 1);
  		if(rtcpfd == -1){
			close(rtpfd);
			continue;
		}
		
		serverRtpPort = serverPort;
		serverRtcpPort = serverPort + 1;

		m_socket_pair.rtp_socket = rtpfd;
		m_socket_pair.rtcp_socket = rtcpfd;

		m_socket_pair.dst_rtpIp = dstIp;
		m_socket_pair.dst_rtpPort = clientRtpPort;

		m_socket_pair.dst_rtcpIp = dstIp;
		m_socket_pair.dst_rtcpPort = clientRtcpPort;
		break;
	}
	
	return 0;
}

int MediaSubSession::GetTcpParam(void * tcpConnection,
	 						uint8 rtpChannelId, uint8 rtcpChannelId, 
								 uint8 &dstTTL){

	m_proto = RTP_TCP;
	m_socket_pair.tcp_connection = tcpConnection;
	m_socket_pair.rtp_channel = rtpChannelId;
	m_socket_pair.rtcp_channel = rtcpChannelId;
	
	printf("GetTcpParam rtpChannelId %d\n", rtpChannelId);
	return 0;
}

int MediaSubSession::SendPacket(uint8 * data, int len){
	if(m_proto == RTP_UDP){
		return SendRtpOverUdp(data, len);
	}
	else{
		return SendRtpOverTcp(data, len);
	}
}

int MediaSubSession::SendRtpOverUdp(uint8 * data, int len){

	sockaddr_in sin ;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(m_socket_pair.dst_rtpIp);
	sin.sin_port = htons(m_socket_pair.dst_rtpPort);

	int sent = sendto(m_socket_pair.rtp_socket, (char*)data, 
		len, 0, (struct sockaddr*)&sin, sizeof(sin));
		
	if(sent != len){
		return -1;
	}
	return 0;
}

int MediaSubSession::SendRtpOverTcp(uint8 * data, int len){

	int rtpSize = len - RTP_TCP_EXTRA_HEADER;
    data[0] = '$';
    data[1] = m_socket_pair.rtp_channel;
    data[2] = (uint8) ((rtpSize & 0xFF00) >> 8);
    data[3] = (uint8) (rtpSize & 0xFF);
	
	RtspClientConnection * n = (RtspClientConnection *)m_socket_pair.tcp_connection;
	if(n->post_send((char*)data, len) < 0){
		return -1;
	}
	
	return 0;
}


int MediaSubSession::PauseStream(){
	return 0;
}

int MediaSubSession::StartStream(ITimerTask* task){
	
	m_mediaCtrl->Prepare();
	m_timestamp = 0;
	return m_mediaCtrl->StartPlay(this, task);
}

int MediaSubSession::SeekStream(){
	return 0;
}

int MediaSubSession::StopStream(){
	
	m_mediaCtrl->Close();
	m_timestamp = 0;
	return m_mediaCtrl->StopPlay(this);
}

uint MediaSubSession::RtpTimestamp(){
	return m_timestamp;
}

uint MediaSubSession::SeqNo(){
	return m_seqNO;
}



/*---------------------------------------
 *	
 *
 *-------------------------------------*/
H264MediaSubSession::H264MediaSubSession(mediaCtrl * mc)
					 :MediaSubSession(mc){

}

H264MediaSubSession::~H264MediaSubSession(){
}

int H264MediaSubSession::GetSdpLines(std::string & sdpLines){

	if(m_mediaCtrl->Prepare() < 0){
		return -1;
	}
	
	std::string mediaSdp;
	if(m_mediaCtrl->getSdp(mediaSdp) < 0){
		return -1;
	}
	m_mediaCtrl->setPayloadType(m_payLoadType);
	int estBitrate = 500;
	
	append(sdpLines, 
		"m=video 0 RTP/AVP %u\r\n"
		"b=AS:%u\r\n"   // b=AS:<bandwidth>
		"c=IN IP4 0.0.0.0\r\n"
		"%s"
		"a=control:%s\r\n\r\n",
		m_payLoadType,
		estBitrate,
		mediaSdp.c_str(),
		GetTrackId().c_str());
	return 0;
}

int H264MediaSubSession::TransferStream(uint8 * data, int len, bool end_of_frame){

	uint8 sendbuf[RTP_MTU + 64];
	int offset = m_proto == RTP_UDP ? 0: RTP_TCP_EXTRA_HEADER;
	uint8 * rtp_buf = sendbuf + offset;
	
	int packCount = len / RTP_MTU;
	int lastBytes = len % RTP_MTU;
	
	if(lastBytes){
		packCount++;
	}
	else{
		lastBytes = RTP_MTU;
	}
	
	uint timestamp = m_timestamp;
	if (end_of_frame){
		m_timestamp += m_mediaCtrl->Timestamp_Inc();
	}
		
	sockaddr_in sin ;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(m_socket_pair.dst_rtpIp);
	sin.sin_port = htons(m_socket_pair.dst_rtpPort);
	
	if (packCount == 1){
		InitRtpHeader(rtp_buf, m_payLoadType, m_seqNO++, timestamp, m_ssrc, end_of_frame);
		memcpy(rtp_buf + 12, data, len);
		return SendPacket(sendbuf, len + 12 + offset);
	}
	
	int s = 1, e = 0, m = 0, bytes = 0;
	int nal_type = data[0];

	for (int k = 0; k < packCount; k++){
	
		bytes = RTP_MTU;
		// FU indicator  F|NRI|Type
		if (k + 1 == packCount){
			s = 0;
			e = 1;
			m = 1;
			bytes = lastBytes - 1;  //skip naul_header
		}
	
		InitRtpHeader(rtp_buf, m_payLoadType, m_seqNO++, timestamp, m_ssrc, m);
		rtp_buf[12] = (nal_type & 0xe0 ) | 28 ;
		rtp_buf[13] = (s << 7)|(e << 6)|(nal_type & 0x1f);
	
		s = e = m = 0;
		memcpy(rtp_buf + 14, data + k * RTP_MTU + 1, bytes);	
		if(SendPacket(sendbuf, bytes + 14 + offset) < 0){
			return -1;
		}
	}

	return 0;
}



/*---------------------------------------
 *	
 *
 *-------------------------------------*/
Mp4AMediaSubSession::Mp4AMediaSubSession(mediaCtrl * mc)
				:MediaSubSession(mc){
}

Mp4AMediaSubSession::~Mp4AMediaSubSession(){
}

int Mp4AMediaSubSession::GetSdpLines(std::string & sdpLines){
	if(m_mediaCtrl->Prepare() < 0){
		return -1;
	}
	
	std::string mediaSdp;
	if(m_mediaCtrl->getSdp(mediaSdp) < 0){
		return -1;
	}
	
	int estBitrate = 96;
	m_mediaCtrl->setPayloadType(m_payLoadType);
	append(sdpLines, 
		"m=audio 0 RTP/AVP %u\r\n"
		"b=AS:%u\r\n"   // b=AS:<bandwidth>
		"c=IN IP4 0.0.0.0\r\n"
		"%s"
		"a=control:%s\r\n\r\n",
		m_payLoadType,
		estBitrate,
		mediaSdp.c_str(),
		GetTrackId().c_str());
	return -1;
}


int Mp4AMediaSubSession::TransferStream(uint8 * data, int len, bool end_of_frame){
	
	int offset = m_proto == RTP_UDP ? 0: RTP_TCP_EXTRA_HEADER;
	uint8 sendbuf[RTP_MTU + 64];
	
	uint8 * rtp_buf = sendbuf + offset;
	uint8 * pos = data;
	
	while(len > 0){
		
		int payload_size = len <= RTP_MTU ? len : RTP_MTU;
		int m = len <= RTP_MTU ? 1: 0;
		InitRtpHeader(rtp_buf, m_payLoadType, m_seqNO++, m_timestamp, m_ssrc, m);
		rtp_buf[12] = 0;
		rtp_buf[13] = 16;
		rtp_buf[14] = (uint8)(payload_size >> 5);	// for each AU length 13 bits + idx 3bits
		rtp_buf[15] = (uint8)((payload_size & 0xff) << 3);
		
		len -= payload_size;
		memcpy(rtp_buf + 16, pos, payload_size);
		pos += payload_size;
		SendPacket(sendbuf, payload_size + 16 + offset);
	}
		
	m_timestamp += m_mediaCtrl->Timestamp_Inc();
	return 0;
}

