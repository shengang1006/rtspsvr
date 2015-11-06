#pragma once

#include "tool.h"
#include "h264_sps.h"
#include "h264_slice.h"
#include "base64.h"
#include <string>

#define MaxBufSize  (1024 * 1024 )

struct framebuffer{
	uchar * buf;
	int len;
	int rpos;
	int wpos;
};

struct socket_pair{

	int rtp_socket;
	ushort rtp_port;
	uint dst_rtpIp;
	ushort dst_rtpPort;

	int rtcp_socket;
	ushort rtcp_port;
	uint dst_rtcpIp;
	ushort dst_rtcpPort;

	void* tcp_connection;
	uchar rtp_channel;
	uchar rtcp_channel;
};


class mediaSource{

public:
	mediaSource(const std::string & streamName);
	virtual ~mediaSource();

	virtual int parseMedia()= 0;
	virtual int NextFrame(uchar ** payload, int & payload_size, bool & end_of_frame)= 0;
	virtual uint Timestamp_Inc()= 0;
	virtual uint uDuration()= 0;
	virtual void SetPayloadType(uint type)= 0;
	virtual int  closeMedia() = 0;
	virtual int getSdp(std::string & sdp) = 0;
	const std::string & GetstreamName();
protected:
	
	std::string m_streamName;
};

/*---------------------------------------*/
class h264FileSource:public mediaSource{
	
public:
	h264FileSource(const std::string & fileName);
	~h264FileSource();
	
	int parseMedia();
	int getSdp(std::string & sdp);
	int closeMedia();
	
	int NextFrame(uchar ** payload, int & payload_size, bool & access_unit_end);
	uint Timestamp_Inc();
	uint uDuration();
	void SetPayloadType(uint type);
	
protected:
	void h264_decode_annexb(uchar *dst, int *dstlen, const uchar *src, const int srclen);
	int h264_split_nal(uchar * p_data, uint buf_size, uchar ** payload, 
	int & payload_size, int & consume);
	int get_naul(uchar ** payload, int & payload_size);
	int get_next_naul_bytes(uint8 * data, int bytes);
protected:
	FILE * m_fFid;
	uint m_payloadType;
	framebuffer m_frameBuf;
	float m_frameRate;
	h264_sps_t	m_sps;
	std::string m_sps_base64;
	std::string m_pps_base64;
	uint m_profile_level_id;
};


/*---------------------------------------*/
class aacFileSource:public mediaSource{
public:
	aacFileSource(const std::string & fileName);
	virtual ~aacFileSource();
	int parseMedia();
	int getSdp(std::string & sdp);
	int closeMedia();
	
	int NextFrame(uchar ** payload, int & payload_size, bool & access_unit_end);
	void SetPayloadType(uint type);
	uint Timestamp_Inc();
	uint uDuration();
	
protected:
	int get_frame(uchar * p_data, uint buf_size, uchar ** payload, int & payload_size, int & consume);
private:
	FILE * m_fFid;
	uint m_samplingFrequency;
	uint m_channels;
	uint  m_payloadType;
	framebuffer m_frameBuf;
	uint8 m_cfg0;
	uint8 m_cfg1;
};
