#pragma once

#include "vlc_bits.h"

/**
* Rational number num/den.
*/
typedef struct AVRational_t{
	int num; ///< numerator
	int den; ///< denominator
} AVRational_t;

/**
* Sequence parameter set
*/
typedef struct h264_sps_t{

	int profile_idc;
	int level_idc;
	int transform_bypass;              ///< qpprime_y_zero_transform_bypass_flag
	int log2_max_frame_num;            ///< log2_max_frame_num_minus4 + 4
	int poc_type;                      ///< pic_order_cnt_type
	int log2_max_poc_lsb;              ///< log2_max_pic_order_cnt_lsb_minus4
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int poc_cycle_length;              ///< num_ref_frames_in_pic_order_cnt_cycle
	int ref_frame_count;               ///< num_ref_frames
	int gaps_in_frame_num_allowed_flag;
	int mb_width;                      ///< frame_width_in_mbs_minus1 + 1
	int mb_height;                     ///< frame_height_in_mbs_minus1 + 1
	int frame_mbs_only_flag;
	int mb_aff;                        ///<mb_adaptive_frame_field_flag
	int direct_8x8_inference_flag;
	int crop;                   ///< frame_cropping_flag
	int crop_left;              ///< frame_cropping_rect_left_offset
	int crop_right;             ///< frame_cropping_rect_right_offset
	int crop_top;               ///< frame_cropping_rect_top_offset
	int crop_bottom;            ///< frame_cropping_rect_bottom_offset
	int vui_parameters_present_flag;
	AVRational_t sar;
	int timing_info_present_flag;
	uint num_units_in_tick;
	uint time_scale;
	int fixed_frame_rate_flag;
	short offset_for_ref_frame[256]; //FIXME dyn aloc?
	int bitstream_restriction_flag;
	int num_reorder_frames;
	int scaling_matrix_present;
	int residual_color_transform_flag;
	uint8 scaling_matrix4[6][16];
	uint8 scaling_matrix8[2][64];
}h264_sps_t;

static void h264_decode_hrd_parameters(bs_t& s, h264_sps_t* p_sps)
{
	int cpb_count, i;
	cpb_count = bs_read_ue(&s) + 1;
	bs_read(&s, 4); /* bit_rate_scale */
	bs_read(&s, 4); /* cpb_size_scale */
	for(i=0; i<cpb_count; i++){
		bs_read_ue(&s); /* bit_rate_value_minus1 */
		bs_read_ue(&s); /* cpb_size_value_minus1 */
		bs_read(&s, 1);     /* cbr_flag */
	}
	bs_read(&s, 5); /* initial_cpb_removal_delay_length_minus1 */
	bs_read(&s, 5); /* cpb_removal_delay_length_minus1 */
	bs_read(&s, 5); /* dpb_output_delay_length_minus1 */
	bs_read(&s, 5); /* time_offset_length */
}

static bool h264_decode_seq_parameter_set(uint8* p_nal,  int n_nal_size, h264_sps_t* p_sps)
{
	uint8 *pb_dec = NULL;
	int     i_dec = 0;
	bs_t s;
	int i_sps_id;

	int nal_hrd_parameters_present_flag, vcl_hrd_parameters_present_flag;

	pb_dec = p_nal;
	i_dec = n_nal_size; 

	bs_init( &s, pb_dec, i_dec );

	bs_skip(&s, 8);

	// profile(8)
	p_sps->profile_idc = bs_read( &s, 8);

	/* constraint_set012, reserver(5), level(8) */
	bs_skip( &s, 8 + 8 );

	/* sps id */
	i_sps_id = bs_read_ue( &s );
	if( i_sps_id >= 32/*SPS_MAX*/ )
	{
		printf("invalid SPS (sps_id=%d)", i_sps_id );
		return false;
	}

	p_sps->scaling_matrix_present = 0;
	if(p_sps->profile_idc >= 100)		//high profile
	{ 
		if(bs_read_ue(&s) == 3)			//chroma_format_idc
			p_sps->residual_color_transform_flag = bs_read(&s, 1);//residual_color_transform_flag		
		bs_read_ue(&s);					//bit_depth_luma_minus8
		bs_read_ue(&s);					//bit_depth_chroma_minus8
		p_sps->transform_bypass = bs_read(&s, 1);
		bs_skip(&s, 1); //decode_scaling_matrices(h, sps, NULL, 1, sps->scaling_matrix4, sps->scaling_matrix8);
	}

	/* Skip i_log2_max_frame_num */
	p_sps->log2_max_frame_num = bs_read_ue( &s ) + 4;
	if( p_sps->log2_max_frame_num > 12)
		p_sps->log2_max_frame_num = 12;
	/* Read poc_type */
	p_sps->poc_type/*->i_pic_order_cnt_type*/ = bs_read_ue( &s );
	if( p_sps->poc_type == 0 )
	{
		/* skip i_log2_max_poc_lsb */
		p_sps->log2_max_poc_lsb/*->i_log2_max_pic_order_cnt_lsb*/ = bs_read_ue( &s );
		if( p_sps->log2_max_poc_lsb > 12 )
			p_sps->log2_max_poc_lsb = 12;
	}
	else if( p_sps->poc_type/*p_sys->i_pic_order_cnt_type*/ == 1 )
	{
		int i_cycle;
		/* skip b_delta_pic_order_always_zero */
		p_sps->delta_pic_order_always_zero_flag/*->i_delta_pic_order_always_zero_flag*/ = bs_read( &s, 1 );
		/* skip i_offset_for_non_ref_pic */
		bs_read_se( &s );
		/* skip i_offset_for_top_to_bottom_field */
		bs_read_se( &s );
		/* read i_num_ref_frames_in_poc_cycle */
		i_cycle = bs_read_ue( &s );
		if( i_cycle > 256 ) i_cycle = 256;
		while( i_cycle > 0 )
		{
			/* skip i_offset_for_ref_frame */
			bs_read_se(&s );
			i_cycle--;
		}
	}
	/* i_num_ref_frames */
	bs_read_ue( &s );
	/* b_gaps_in_frame_num_value_allowed */
	bs_skip( &s, 1 );

	/* Read size */
	p_sps->mb_width/*->fmt_out.video.i_width*/  = 16 * ( bs_read_ue( &s ) + 1 );
	p_sps->mb_height/*fmt_out.video.i_height*/ = 16 * ( bs_read_ue( &s ) + 1 );

	/* b_frame_mbs_only */
	p_sps->frame_mbs_only_flag/*->b_frame_mbs_only*/ = bs_read( &s, 1 );
	if( p_sps->frame_mbs_only_flag == 0 )
	{
		bs_skip( &s, 1 );
	}
	/* b_direct8x8_inference */
	bs_skip( &s, 1 );

	/* crop */
	p_sps->crop = bs_read( &s, 1 );
	if( p_sps->crop )
	{
		/* left */
		bs_read_ue( &s );
		/* right */
		bs_read_ue( &s );
		/* top */
		bs_read_ue( &s );
		/* bottom */
		bs_read_ue( &s );
	}

	/* vui */
	p_sps->vui_parameters_present_flag = bs_read( &s, 1 );
	if( p_sps->vui_parameters_present_flag )
	{
		int aspect_ratio_info_present_flag = bs_read( &s, 1 );
		if( aspect_ratio_info_present_flag )
		{
			static const struct { int num, den; } sar[17] =
			{
				{ 0,   0 }, { 1,   1 }, { 12, 11 }, { 10, 11 },
				{ 16, 11 }, { 40, 33 }, { 24, 11 }, { 20, 11 },
				{ 32, 11 }, { 80, 33 }, { 18, 11 }, { 15, 11 },
				{ 64, 33 }, { 160,99 }, {  4,  3 }, {  3,  2 },
				{  2,  1 },
			};

			int i_sar = bs_read( &s, 8 );

			if( i_sar < 17 )
			{
				p_sps->sar.num = sar[i_sar].num;
				p_sps->sar.den = sar[i_sar].den;
			}
			else if( i_sar == 255 )
			{
				p_sps->sar.num = bs_read( &s, 16 );
				p_sps->sar.den = bs_read( &s, 16 );
			}
			else
			{
				p_sps->sar.num = 0;
				p_sps->sar.den = 0;
			}

			//if( den != 0 )
			//	p_dec->fmt_out.video.i_aspect = (int64_t)VOUT_ASPECT_FACTOR *
			//	( num * p_dec->fmt_out.video.i_width ) /
			//	( den * p_dec->fmt_out.video.i_height);
			//else
			//	p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR;
		}
		else
		{
			p_sps->sar.num = 0;
			p_sps->sar.den = 0;
		}

		if(bs_read(&s, 1))		/* overscan_info_present_flag */
		{
			bs_read(&s, 1);     /* overscan_appropriate_flag */
		}

		if(bs_read(&s, 1))		/* video_signal_type_present_flag */
		{      
			bs_read(&s, 3);		/* video_format */
			bs_read(&s, 1);     /* video_full_range_flag */

			if(bs_read(&s, 1))  /* colour_description_present_flag */
			{
				bs_read(&s, 8);	/* colour_primaries */
				bs_read(&s, 8); /* transfer_characteristics */
				bs_read(&s, 8); /* matrix_coefficients */
			}
		}

		if(bs_read(&s, 1))		/* chroma_location_info_present_flag */
		{
			bs_read_ue(&s);		/* chroma_sample_location_type_top_field */
			bs_read_ue(&s);		/* chroma_sample_location_type_bottom_field */
		}

		p_sps->timing_info_present_flag = bs_read(&s, 1);
		if(p_sps->timing_info_present_flag)
		{
			p_sps->num_units_in_tick = bs_read(&s, 32);
			p_sps->time_scale = bs_read(&s, 32);
			p_sps->fixed_frame_rate_flag = bs_read(&s, 1);
		}

		nal_hrd_parameters_present_flag = bs_read(&s, 1);
		if(nal_hrd_parameters_present_flag)
			h264_decode_hrd_parameters(s, p_sps);
		vcl_hrd_parameters_present_flag = bs_read(&s, 1);
		if(vcl_hrd_parameters_present_flag)
			h264_decode_hrd_parameters(s, p_sps);
		if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
			bs_read(&s, 1);     /* low_delay_hrd_flag */
		bs_read(&s, 1);         /* pic_struct_present_flag */

		p_sps->bitstream_restriction_flag = bs_read(&s, 1);
		if(p_sps->bitstream_restriction_flag)
		{
			unsigned int num_reorder_frames;
			bs_read(&s, 1);     /* motion_vectors_over_pic_boundaries_flag */
			bs_read_ue(&s); /* max_bytes_per_pic_denom */
			bs_read_ue(&s); /* max_bits_per_mb_denom */
			bs_read_ue(&s); /* log2_max_mv_length_horizontal */
			bs_read_ue(&s); /* log2_max_mv_length_vertical */
			num_reorder_frames= bs_read_ue(&s);
			bs_read_ue(&s); /*max_dec_frame_buffering*/

			if(num_reorder_frames > 16 /*max_dec_frame_buffering || max_dec_frame_buffering > 16*/){
				printf("illegal num_reorder_frames %d\n", num_reorder_frames);
				return false;
			}

			p_sps->num_reorder_frames= num_reorder_frames;
		}
	}

	return true;
}

