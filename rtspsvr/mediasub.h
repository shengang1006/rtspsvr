#pragma once
#include <vector>
#include <map>

#include "mediactrl.h"

#define RTP_TCP_EXTRA_HEADER 4
#define RTP_MTU  1400
enum{RTP_UDP, RTP_TCP, RAW_UDP};

class MediaSubSession:public IStreamCtrl{
	
public:
	MediaSubSession(mediaCtrl * mc);

	virtual ~MediaSubSession();
	void SetTrackId(uint trackID);
	const std::string & GetTrackId();
	void SetPayloadType(uint8 number);
	virtual int GetSdpLines(std::string & sdpLines);

	//set up
	virtual int GetUdpParam(uint clientRtpPort, 
						    uint clientRtcpPort,
							uint dstIp,
							uint & serverRtpPort, 
							uint & serverRtcpPort);

	virtual int GetTcpParam(
		void * tcpConnection,
		uint8 rtpChannelId, 
		uint8 rtcpChannelId,
		uint8 &dstTTL);

	virtual int StartStream(ITimerTask* task); //play
	virtual int PauseStream();
	virtual int SeekStream();

	//tear down
	virtual int StopStream();
	virtual uint RtpTimestamp();
	virtual uint SeqNo();
	
	int SendPacket(uint8 * data, int len);
	int SendRtpOverUdp(uint8 * data, int len);
    int SendRtpOverTcp(uint8 * data, int len);
	 
protected:
	
	std::string m_trackIdStr;

	uint8 m_payLoadType;
	uint m_timestamp;
	int m_proto;

	mediaCtrl * m_mediaCtrl;

	uint16 m_seqNO;
	uint m_ssrc;
	socket_pair m_socket_pair;
};


class H264MediaSubSession : public MediaSubSession{
public:
	H264MediaSubSession(mediaCtrl * mc);
	virtual ~H264MediaSubSession();
	 int GetSdpLines(std::string & sdpLines);
	 int TransferStream(uint8 * data, int len, bool end_of_frame);		  
};


class Mp4AMediaSubSession : public MediaSubSession{
public:
	Mp4AMediaSubSession(mediaCtrl * mc);
	virtual ~Mp4AMediaSubSession();
	int GetSdpLines(std::string & sdpLines);
	int TransferStream(uint8 * data, int len, bool end_of_frame);
};
