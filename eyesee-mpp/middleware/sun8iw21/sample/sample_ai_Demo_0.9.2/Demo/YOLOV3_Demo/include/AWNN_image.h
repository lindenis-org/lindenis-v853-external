#ifndef AWNN_IMAGE_H
#define AWNN_IMAGE_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct _awnn_rect AWNNRect;
	struct _awnn_rect {
		int tl_x;
		int tl_y;
		int br_x;
		int br_y;
		int width;
		int height;
	};

	typedef struct _awnn_size AWNNSize;
	struct _awnn_size {
		int width;
		int height;
	};

	typedef enum {
		AWNN_NONE_YUV,  // Default is none.
		AWNN_YUV420SP_NV21,
	}AWNNYuvType;

	typedef enum {
		AWNN_NONE_SPACE, // Default is none.					 
		AWNN_RGB
	}AWNNColorSpace;

	typedef struct _awnn_yuv AWNNYuv;
	struct _awnn_yuv {
		int w;
		int h;

		unsigned char *data;
		AWNNYuvType yuv_type;
	};

	typedef struct _awnn_image AWNNImg;
	struct _awnn_image {
		int w;
		int h;
		int c;

		unsigned char *data;
		AWNNColorSpace c_space;
	};

	/******************************************/
	// Make functions
	/******************************************/
    AWNNImg *awnn_make_blank_image(int w, int h, AWNNColorSpace c_space);

	AWNNYuv *awnn_make_yuv_image(const unsigned char *yuv_buffer, int w, int h, AWNNYuvType yuv_type);

	AWNNImg *awnn_make_rgb_image(const unsigned char *rgb_buffer, int w, int h, AWNNColorSpace c_space);

	AWNNRect awnn_make_rect(int tl_x, int tl_y, int br_x, int br_y);
	
	/******************************************/
	// Image operation functions.
	/******************************************/
 
    int awnn_yuv2rgb(const AWNNYuv *src_yuv, AWNNImg *dst_img);

	int awnn_resize(const AWNNImg *src_img, AWNNImg *dst_img);

	int awnn_crop(const AWNNImg *src_img, AWNNRect rect, AWNNImg *dst_img);

	int awnn_crop_resize(const AWNNImg *src_img, AWNNRect rect, AWNNImg *dst_img);

	int awnn_yuv2rgb_resize(const AWNNYuv *src_yuv, AWNNImg *dst_img);

	int awnn_yuv2rgb_crop_resize(const AWNNYuv *src_yuv, AWNNRect rect, AWNNImg *dst_img);

	/*****************************/
	// Value conversion.
	/*****************************/
	//unsigned char -> normalize (float32)
	void awnn_normalize(const unsigned char *src_data, int size, std::vector<float> &mean_vals, std::vector<float> &norm_vals, float *dst_data); 
	
	//normalized data (float32) -> quant (signed char)
	void awnn_quantize(const float *src_data, int size, float input_scale, signed char *dst_data);  

	//signed char -> fp32
	void awnn_dequantize(const signed char *src_data, int size, float output_scale, float *dst_data);

	/*****************************/
	// Free memory.
	/*****************************/
	void awnn_free_image(AWNNImg *img);
	

#ifdef __cplusplus
}
#endif

#endif	//!AWNN_IMAGE_H
