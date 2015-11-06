#include "mediasource.h"
#define next_24bit(ptr) (((ptr)[0]<<16)|((ptr)[1]<<8)|(ptr)[2])
	
	
	
mediaSource::mediaSource(const std::string & streamName){
	m_streamName = streamName;
}

mediaSource::~mediaSource(){

}

const std::string & mediaSource::GetstreamName(){
	return m_streamName;
}

void init_framebuffer(framebuffer & fb, int size){

	fb.buf =  new uint8[size];
	fb.len = size;
	fb.rpos = fb.wpos = 0;
}

void del_framebuffer(framebuffer & fb){
	if (fb.buf && fb.len){
		delete fb.buf;
		fb.buf = 0;
		fb.len = 0;
	}
	fb.rpos = fb.wpos = 0;
}

/////////////////////////////////////////////////////////////
h264FileSource::h264FileSource(const std::string & fileName):
mediaSource(fileName){
	m_fFid = NULL;
	init_framebuffer(m_frameBuf, MaxBufSize);
	m_frameRate = 25.0f;
	m_payloadType = 96;
	m_profile_level_id = 0;
}

h264FileSource::~h264FileSource(){
	del_framebuffer(m_frameBuf);
	closeMedia();
}

bool isAccessUnitStart(uint8 nal_unit_type){
	return (nal_unit_type >= 6 && nal_unit_type <= 9) ||
	       (nal_unit_type >= 14 && nal_unit_type <= 18);
}

bool isEof(uint8 nal_unit_type){
	return (nal_unit_type == 10 || nal_unit_type == 11);
}

bool isVCL(uint8 nal_unit_type){
	return (nal_unit_type <= 5 && nal_unit_type > 0);
}

int h264FileSource::get_naul(uchar ** payload, int & payload_size){
	
	int consume = 0;
	int has = m_frameBuf.wpos - m_frameBuf.rpos;
	uint8 * data = m_frameBuf.buf + m_frameBuf.rpos;

	int ret = h264_split_nal(data, has, payload, payload_size, consume);
	if (ret < 0){
		//naul too big
		if(has == m_frameBuf.len){
			return -1;
		}

		if(has){
			memmove(m_frameBuf.buf, data, has);
		}
		
		m_frameBuf.rpos = 0;
		m_frameBuf.wpos = has;
		//read more
		if (!feof(m_fFid)){
			int nread = fread(m_frameBuf.buf + m_frameBuf.wpos, 
				1, m_frameBuf.len - m_frameBuf.wpos, m_fFid);
			if (!nread){
				return -1;
			}
			m_frameBuf.wpos += nread;
		}
		else{
			return -1;
		}
		
		//try again
		ret = h264_split_nal(m_frameBuf.buf, m_frameBuf.wpos, payload, payload_size, consume);
		if (ret < 0){
			return -1;
		}
	}
	
	m_frameBuf.rpos += consume;
	
	return 0;
}

int h264FileSource::get_next_naul_bytes(uint8 * buf, int bytes){
	

	uint8 * data = m_frameBuf.buf + m_frameBuf.rpos;
	int has = m_frameBuf.wpos - m_frameBuf.rpos;
	
	if(has < 4 + bytes){
		
		memmove(m_frameBuf.buf, data, has);
		m_frameBuf.rpos = 0;
		m_frameBuf.wpos = has;
		
		if (!feof(m_fFid)){
			int nread = fread(m_frameBuf.buf + m_frameBuf.wpos, 
				1, m_frameBuf.len - m_frameBuf.wpos, m_fFid);
			if (!nread){
				return -1;
			}
			m_frameBuf.wpos += nread;
		}
		else{
			return -1;
		}
		
		if(has < 4 + bytes){
			return -1;
		}
	
		data = m_frameBuf.buf ;	
	}
	
	if(next_24bit(data)){
		memcpy(buf, data + 3, bytes);
	}
	else if(next_24bit(data + 1)){
		memcpy(buf, data + 4, bytes);
	}
	else{
		error_log("parse buffer fail");
		return -1;
	}
	
	return 0;
}

//RTP  1460 + rtp header
int h264FileSource::NextFrame(uint8 ** payload, int & payload_size, bool & access_unit_end){
	
	if (!m_fFid){
		return -1;
	}
	
	access_unit_end = false;
	*payload = NULL;
	payload_size = 0;
	
	if(get_naul(payload, payload_size) < 0){
		access_unit_end = true;
		return -1;
	}

	uint8 nal_unit_type = (*payload)[0] & 0x1f;
		
	//access_unit_start
	if(isAccessUnitStart(nal_unit_type)){
		access_unit_end = false;
	}
	//access_unit_end
	else if(isEof(nal_unit_type )){
		printf("eof %d\n", nal_unit_type);
		access_unit_end = true;
	}
	else{
		
	  // We need to check the *next* NAL unit to figure out whether
      // the current NAL unit ends an 'access unit':
	    uint8 next_naul_header[2] = {0};
		if(get_next_naul_bytes(next_naul_header, sizeof(next_naul_header)) < 0){
			access_unit_end = true;
			printf("get_next_naul_bytes fail \n");
		}
		else{
			uint8 next_nal_unit_type = next_naul_header[0] & 0x1f;
			//printf("next_nal_unit_type %d \n", next_nal_unit_type);
			
			//VCL type
			if(isVCL(next_nal_unit_type)){
				access_unit_end = (next_naul_header[1] &0x80) != 0;
			//	printf("isVCL %d \n", next_nal_unit_type);
			}
			else if(isAccessUnitStart(next_nal_unit_type)){
		//		printf("next isAccessUnitStart %d \n", next_nal_unit_type);
				access_unit_end = true;
			}
			else{
				access_unit_end = false;
			}
		}
	}
	
	
	return 0;
}

int h264FileSource::parseMedia(){

	if (!m_fFid){
		m_fFid = fopen(m_streamName.c_str(), "rb");
	}
	
	if (!m_fFid){
		return -1;
	}

	uint8 *data = NULL;
	int nsize = 0;
	int findsps = 0, findpps = 0;
	m_profile_level_id = 0;

	memset(&m_sps, 0, sizeof(m_sps));

	int ret = -1;
	while(!get_naul(&data, nsize)){

		uint8* nal_data = data;
		int   nal_size  = nsize;
		uint8 nal_type  = nal_data[0] & 0x1f;

		if (nal_type ==  7 && !findsps){//sps
			findsps = 1;
			if (nsize >= 4){
				m_profile_level_id = (nal_data[1]<<16)|(nal_data[2]<<8)|nal_data[3]; 
			}
			Base64Encode(nal_data, nal_size, m_sps_base64);
		
			uint8 * sps_buf = new uint8[nal_size];
			int sps_size = 0;
			h264_decode_annexb(sps_buf,&sps_size, nal_data, nal_size);
			h264_decode_seq_parameter_set(sps_buf, sps_size, &m_sps);
			if(m_sps.num_units_in_tick && m_sps.time_scale){
				m_frameRate = (float)m_sps.time_scale / (2 * m_sps.num_units_in_tick);
			}
			delete[] sps_buf;
		}
		if (nal_type ==  8 && !findpps){//pps
			findpps = 1;
			Base64Encode(nal_data, nal_size, m_pps_base64);
		}
		if (findpps && findsps){
			ret = 0;
			break;
		}
	}

	m_frameBuf.rpos = m_frameBuf.wpos = 0;
	fseek(m_fFid, 0, SEEK_SET);
	return ret;
}

int h264FileSource::getSdp(std::string & sdpLines){

	append(sdpLines,
			"a=framerate:%.2f\r\n" //for mplayer need this
			"a=rtpmap:%u H264/90000\r\n"
			"a=fmtp:%u packetization-mode=1"
			";profile-level-id=%06X"
			";sprop-parameter-sets=%s,%s\r\n",
			m_frameRate,
			m_payloadType,
			m_payloadType, 
			m_profile_level_id,
			m_sps_base64.c_str(),
			m_pps_base64.c_str());
	return 0;
}

uint h264FileSource::Timestamp_Inc(){
	return (uint)(0.5 + 90000.0 / m_frameRate);
}

uint h264FileSource::uDuration(){
	return (uint)(1000 * 1000/ m_frameRate);
}

void h264FileSource::SetPayloadType(uint type){
	m_payloadType = type;
}

int h264FileSource::closeMedia(){
	if(m_fFid){
		fclose(m_fFid);
		m_fFid = NULL;
	}
	return 0;
}

void h264FileSource::h264_decode_annexb(uint8 *dst, int *dstlen, const uint8 *src, const int srclen){
	uint8 *dst_sav = dst;
	const uint8 *end = &src[srclen];
	while (src < end){
		if (src < end - 3 && src[0] == 0x00 && src[1] == 0x00 &&
			src[2] == 0x03){
			*dst++ = 0x00;
			*dst++ = 0x00;
			src += 3;
			continue;
		}
		*dst++ = *src++;
	}
	*dstlen = dst - dst_sav;
}

int h264FileSource::h264_split_nal(uint8 * p_data, uint buf_size, uint8 **payload, int & payload_size, int & consume){
		
	uint first_pos = 0;
	uint second_pos = 0;
	uint start_code = 0;
	payload_size = 0;
	consume = 0;
	/* Split nal units */
	while(second_pos + 4 < buf_size ){
		if (next_24bit(p_data + second_pos) == 0x000001){
			if (start_code){
				payload_size = second_pos - first_pos;
				if(!p_data[second_pos - 1])payload_size--;
				*payload = p_data + first_pos;
				consume = first_pos + payload_size;
				return 0;
			}
			first_pos = second_pos + 3;//skip startcode
			start_code = 1;
			second_pos += 4;
		}
		else{
			second_pos++;
		}
	}
	return -1;
}
//////////////////////////////////////////////////

aacFileSource::aacFileSource(const std::string & fileName):
mediaSource(fileName){
	m_fFid = NULL;
	m_samplingFrequency = 0;
	m_channels = 0;
	m_payloadType = 96;
	m_cfg0 = 0;
	m_cfg1 = 0;
	init_framebuffer(m_frameBuf, MaxBufSize);

}


aacFileSource:: ~aacFileSource(){
	del_framebuffer(m_frameBuf);
	closeMedia();
}

void aacFileSource::SetPayloadType(uint type){
	m_payloadType = type;
}

int aacFileSource::closeMedia(){
	del_framebuffer(m_frameBuf);
	if(m_fFid){
		fclose(m_fFid);
		m_fFid = NULL;
	}
	return 0;
}

int aacFileSource::getSdp(std::string & sdpLines){
	append(sdpLines,
		"a=rtpmap:%u MPEG4-GENERIC/%u/%u\r\n"
		"a=fmtp:%d streamtype=5;profile-level-id=1;"
		"mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;"
		"config=%02X%02x\r\n",
		m_payloadType,
		m_samplingFrequency,
		m_channels,
		m_payloadType,
		m_cfg0,
		m_cfg1);
	return 0;
}

int aacFileSource::parseMedia(){
	
	if (!m_fFid){
		m_fFid = fopen(m_streamName.c_str(), "rb");
	}
	
	if (!m_fFid){
		return -1;
	}

	static uint const samplingFrequencyTable[16] = {
		96000, 88200, 64000, 48000,
		44100, 32000, 24000, 22050,
		16000, 12000, 11025, 8000,
		7350,  0,     0,     0
	};

	// Now, having opened the input file, read the fixed header of the first frame,
	// to get the audio stream's parameters:
	uint8 fixedHeader[4]; // it's actually 3.5 bytes long
	if (fread(fixedHeader, 1, sizeof(fixedHeader), m_fFid) < sizeof(fixedHeader)){
		return -1;
	}
	fseek(m_fFid, 0, SEEK_SET);

	// Check the 'syncword':
	if (!(fixedHeader[0] == 0xFF && (fixedHeader[1]&0xF0) == 0xF0)) {
		//Bad 'syncword' at start of ADTS file
		return -1;
	}

	// Get and check the 'profile':
	uint8 profile = (fixedHeader[2]&0xC0)>>6; // 2 bits
	if (profile == 3) {
		//Bad (reserved) 'profile': 3 in first frame of ADTS file
		return -1;
	}

	uint8 sampling_frequency_index = (fixedHeader[2]&0x3C)>>2; // 4 bits
	if (samplingFrequencyTable[sampling_frequency_index] == 0) {
		//"Bad 'sampling_frequency_index' in first frame of ADTS file"
		return -1;
	}

	// Get and check the 'channel_configuration':
	uint8 channel_configuration = ((fixedHeader[2]&0x01)<<2)|((fixedHeader[3]&0xC0)>>6); // 3 bits


	// If we get here, the frame header was OK.
	// Reset the fid to the beginning of the file:
	//	SeekFile64(m_fFid, SEEK_SET, 0);

	m_samplingFrequency = samplingFrequencyTable[sampling_frequency_index];

	m_channels = channel_configuration ? channel_configuration : 2;

	// Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
	uint8 const audioObjectType = profile + 1;

	m_cfg0 = (audioObjectType<<3) | (sampling_frequency_index>>1);
	m_cfg1 = (sampling_frequency_index<<7) | (channel_configuration<<3);
	return 0;
}


int aacFileSource::NextFrame(uint8 ** payload, int & payload_size, bool & end_of_frame){

	// Begin by reading the 7-byte fixed_variable headers:		
	*payload = NULL;
	payload_size = 0;
	end_of_frame = true;

	if (!m_fFid){
		return -1;
	}

	int consume = 0;
	int has = m_frameBuf.wpos - m_frameBuf.rpos;
	uint8 * data = m_frameBuf.buf + m_frameBuf.rpos;
	int ret = get_frame(data, has, payload, payload_size, consume);

	if(!ret){
		m_frameBuf.rpos += consume;
		return 0;
	}

	memmove(m_frameBuf.buf, data, has);
	m_frameBuf.rpos = 0;
	m_frameBuf.wpos = has;

	if (!feof(m_fFid)){
		int nread = fread(m_frameBuf.buf,1, m_frameBuf.len - has, m_fFid);
		if (nread < 0){
			return -1;
		}
		m_frameBuf.wpos += nread;
	}
	else{
		return -1;
	}

	has = m_frameBuf.wpos;
	data = m_frameBuf.buf;
	ret = get_frame(data, has, payload, payload_size, consume);
	if (ret < 0){
		return -1;
	}
	m_frameBuf.rpos += consume;
	
	return 0;
}

int aacFileSource::get_frame(uint8 * p_data, uint buf_size, uint8 ** payload, int & payload_size, int &consume){

	if (buf_size < 7){
		return -1;
	}

	uint8 * headers = (uint8*)p_data;

	bool protection_absent = headers[1]&0x01;
	
	uint16 frame_length
		= ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);
	
	uint numBytesToRead = frame_length > 7 ? frame_length - 7 : 0;
	
	// If there's a 'crc_check' field, skip it:
	int crc_check = 0;
	if (!protection_absent) {
		crc_check = 2;
		numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
 	}

	if (buf_size < crc_check + 7 + numBytesToRead){
		return -1;
	}

	*payload = p_data + crc_check + 7;
	payload_size = numBytesToRead;
	consume = crc_check + 7 + numBytesToRead;

	return 0;

}

//for aac 1024 samples
uint aacFileSource::Timestamp_Inc(){
	return 1024;
}

uint aacFileSource::uDuration(){
	return (1000 * 1000 * 1024 / m_samplingFrequency);
}
