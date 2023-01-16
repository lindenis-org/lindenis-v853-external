#ifndef YOLOV3_UTILS_H
#define YOLOV3_UTILS_H

#include "AWNN_image.h"
#define	AW_DETECTION_RESULT_NUM 100

#ifdef __cplusplus
extern "C" {
#endif
	/*
	* A struct that stores yolov3 detection head params.
	*
	* numClass: the number of class.
	* numBox: the number of predicted box.
	* confidenceThreshold: the classification score threshold.
	* nmsThreshold: the iou threshold.
	* numAnchor: the number of anchor.
	* anchors: predetermined anchor for computing object box.
	* numMask: the number of mask.
	* masks: the index of anchor.
	* numAnchorScale: the number of anchor scale.
	* anchorScales: the ratio of network input size to anchor
	*/
	typedef struct _yolov3_head_param YoloV3HeadParams;
	struct _yolov3_head_param
	{
		int numClass;
		int numBox;
		float confidenceThreshold;
		float nmsThreshold;
		int numAnchor;
		float *anchors;
		int numMask;
		float *masks;
		int numAnchorScale;
		float *anchorScales;
	};

	/*
	* A struct that stores detection results for yolov3.
	*
	* x1: the predicted relative value for left top x of the coordinates.
	* y1: the predicted relative value for left top y of the coordinates.
	* x2: the predicted relative value for right bottom x of the coordinates.
	* y2: the predicted relative value for right bottom y of the coordinates.
	* label: the classification label from 1 scratch.
	* prob:	 the confidence for bounding box.
	*/
	typedef	struct _yolov3_det_object YoloV3DetObject;
	struct	_yolov3_det_object {
		float x1;
		float y1;
		float x2;
		float y2;
		int label;
		float prob;
	};

	/*
	* A struct that stores detection object num and objects for an image.
	*
	* num:     the number of object detection.
	* objects: all objects for detecting an image.
	*/
	typedef struct _yolov3_detection_outputs YoloV3DetOutputs;
	struct _yolov3_detection_outputs {
		int num;
		YoloV3DetObject *objects;
	};

	/*
	* Resize rgb image. 
	*/
	void awnn_resize_c3(AWNNImg* src_img, AWNNImg* dst_img);

	/*
	* Pad width and height of the image by 32 times. 
	*/
	float* pad_image(AWNNImg* img, float *meanVals, float *normVals, int targetSize,
		int* targetWidth, int *targetHeight);

	/*
	* Init the detOutputs.
	*/
	void init_det_outputs(YoloV3DetOutputs *detOutputs);

	/*
	* Compute the results of the yolov3 detection head.
	*/
	void infer_yolov3_detection_outputs(std::vector<float*> outputFloatBuffer, std::vector<AWNNTensorDims> outputFloatDims,
		YoloV3HeadParams& headParams, YoloV3DetOutputs *detOutputs);

	/*
	* Deinit the detOutputs.
	*/
	void deinit_det_outputs(YoloV3DetOutputs *detOutputs);

#ifdef __cplusplus
}
#endif

#endif	//!YOLOV3_UTILS_H
