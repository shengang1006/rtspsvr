
#pragma once

#define char64(c)  ((c > 127) ? (char) 0xff : index_64[(c)])

static uint load_block(const char * in, uint size, uint pos, char * out){
	uint i, len; 
	uint	c;
	len = i = 0;
	while ((len<4) && ((pos+i)<size)) {
		c = in[pos+i];
		if ( ((c>='A') && (c<='Z'))
			|| ((c>='a') && (c<='z'))
			|| ((c>='0') && (c<='9'))
			|| (c=='=') || (c=='+') || (c=='/')
			) {
			out[len] = c;
			len++;
		}
		i++;
	}
	while (len<4) { out[len] = (char) 0xFF; len++; }
	return pos+i;
};


/*!
*\brief base64 encoder
*
*Encodes a data buffer to Base64
*\param pData input data buffer
*\param dataSize input data buffer size
*\param base64String output Base64 buffer
*\return size of the encoded Base64 buffer
*\note the encoded data buffer is not NULL-terminated.
*/
inline static uint Base64Encode(uint8 * pData, uint dataSize, std::string & base64){
	const char base_64[128] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	int	padding;
	uint	i = 0, j = 0;
	
	uint8 *	in;
	uint	outSize;
	uint8 *	out;

	uint iOut = ((dataSize + 2) / 3) * 4;
	outSize = iOut += 2 * ((iOut / 60) + 1);
	out = new uint8 [outSize];

	in = pData;

	if (outSize < (dataSize * 4 / 3)) return 0;

	while (i < dataSize) 	{
		padding = 3 - (dataSize - i);
		if (padding == 2) {
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[(in[i] & 0x03) << 4];
			out[j+2] = '=';
			out[j+3] = '=';
		} else if (padding == 1) {
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[((in[i] & 0x03) << 4) | ((in[i+1] & 0xf0) >> 4)];
			out[j+2] = base_64[(in[i+1] & 0x0f) << 2];
			out[j+3] = '=';
		} else{
			out[j] = base_64[in[i]>>2];
			out[j+1] = base_64[((in[i] & 0x03) << 4) | ((in[i+1] & 0xf0) >> 4)];
			out[j+2] = base_64[((in[i+1] & 0x0f) << 2) | ((in[i+2] & 0xc0) >> 6)];
			out[j+3] = base_64[in[i+2] & 0x3f];
		}
		i += 3;
		j += 4;
	}
	out[j] = '\0';
	base64 = (char*)out;
	delete[] out;
	return j;
}
