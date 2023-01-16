#ifndef INCEPTIONV3_UTILS_H
#define INCEPTIONV3_UTILS_H

#include "AWNN_image.h"

#ifdef __cplusplus
extern "C" {
#endif

	/*
	* Resize rgb image. 
	*/
	void resize_c3(AWNNImg* src_img, AWNNImg* dst_img);

	/*
	* Normalize the image and pad the sides of the image to the target size.
	*/
	float* pad_image(AWNNImg* img, float *meanVals, float *normVals, int targetSize,
		int* targetWidth, int *targetHeight);
	
	/*
	* Return the indexes of the topNum and the sorted values by sorting the values in descending order.
	*/
	int* get_tops(int *values, int num, int topNum);

#ifdef __cplusplus
}
#endif

#endif	//!INCEPTIONV3_UTILS_H
