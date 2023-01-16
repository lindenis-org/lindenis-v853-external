#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <algorithm>
#include <math.h>

#include "AWNN_interface.h"
#include "yolov3_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <sys/time.h>

using namespace std;

#define perf_start(tag) \
			struct timeval start##tag, end##tag; \
			gettimeofday(&start##tag, NULL); \

#define perf_end(tag) \
			gettimeofday(&end##tag, NULL); \
			long time_ms##tag = (1000 * (end##tag.tv_sec - start##tag.tv_sec) + (end##tag.tv_usec - start##tag.tv_usec) / 1000); \
			printf("AW_PERF "#tag" cost %ldms\n",time_ms##tag);

const char *coco_names[80] = {"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
	"boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog",
	"horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella",
	"handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite",
	"baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
	"wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich",
	"orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
	"potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote",
	"keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book",
	"clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};


AWNNImg* read_image(const char *imagePath)
{
	if(!imagePath)
	{
		printf("imagePath is nullptr.\n");
	}

	int imgWidth = 0;
	int imgHeight = 0;
	int imgChannel = 0;
	unsigned char* inputData = stbi_load(imagePath, &imgWidth, &imgHeight, &imgChannel, 3);
	if (!inputData)
	{
		printf("Failed to load the image.\n");
	}
	if (imgChannel != 3)
	{
		printf("The image is not rgb data.\n");
	}
	AWNNImg* img = awnn_make_rgb_image(inputData, imgWidth, imgHeight, AWNN_RGB);
	return img;
}

AWNNImg* resize_image(AWNNImg* img, int targetSize, float* ratio)
{
	if (!img || !ratio)
	{
		printf("img or ratio is nullptr.\n");
	}
	if (targetSize <= 0)
	{
		printf("targetSize is illegal input.\n");
	}
	
	int imgWidth = img->w;
	int imgHeight = img->h;
	
	*ratio = min(targetSize * 1.0 / imgWidth, targetSize * 1.0 / imgHeight);
	AWNNImg* imgResized = awnn_make_blank_image(int(round(imgWidth*(*ratio))), int(round(imgHeight*(*ratio))), AWNN_RGB);
	awnn_resize_c3(img, imgResized);
	return imgResized;
}

void testYoloV3()
{
	// set AWNNConfig 
	printf("Load model param and bin...\n");
	AWNNConfig config;
	config.paramInvisible = false;
	config.paramPath = "./model/yolov3_int8.param";
	config.modelPath = "./model/yolov3_int8.bin";

	// create AWNNInstance 
	AWNNInstance caseNet;
	int createFlag = caseNet.create(config);

	// get tensors scale 
	float dataScale = 0.0f;
	std::vector<float> outScale(3);
	caseNet.getTensorScale("data", dataScale);
	caseNet.getTensorScale("81_599", outScale[0]);
	caseNet.getTensorScale("93_685", outScale[1]);
	caseNet.getTensorScale("105_772", outScale[2]);

	printf("data scale: %f\n", dataScale);
	printf("conv 81_599 scale: %f\n", outScale[0]);
	printf("conv 93_685 scale: %f\n", outScale[1]);
	printf("conv 105_772 scale: %f\n", outScale[2]);

	// read image 
	printf("\nRead and transform data...\n");
	char* imagePath = "./data/000000004765.bmp";
	AWNNImg* img = read_image(imagePath);
	printf("image width: %d, image height: %d\n", img->w, img->h);

	// resize image
	int targetSize = 416;
	float ratio = 0.0f;
	AWNNImg* imgResized = resize_image(img, targetSize, &ratio);
	printf("resized image width: %d, resized image height: %d\n", imgResized->w, imgResized->h);

	// pad the shorter side of the image, 32-aligned
	int targetWidth = 0;
	int targetHeight = 0;
	int targetChannel = img->c;
	float meanVals[3] = { 0.0f, 0.0f, 0.0f };
	float normVals[3] = { 1 / 255.0f, 1 / 255.0f, 1 / 255.0f };
	float* dataPadded = pad_image(imgResized, meanVals, normVals, targetSize, &targetWidth, &targetHeight);
	printf("padded image width: %d, padded image height: %d\n", targetWidth, targetHeight);

	// quantize float data to int8
	signed char* inputBuffer = (signed char*)calloc(targetWidth * targetHeight * targetChannel, sizeof(signed char));
	awnn_quantize(dataPadded, targetWidth * targetHeight * targetChannel, dataScale, inputBuffer);
	
	/* set AWNNSessionConfig */
	AWNNSessionConfig sessConfig;
	sessConfig.type = AWNN_FORWARD_IPU;
	// the name of the input tensor of this YOLOV3 model, i.e. the resized, normalized, padded and quantized image data
	sessConfig.inputNames =  { "data" };  
	// the names of the 3 convs' output tenors, which are just above the CPU-mode detection head
	sessConfig.outputNames = { "81_599", "93_685", "105_772" };  

	AWNNTensorDesc input;
	input.dims.w = targetWidth;
	input.dims.h = targetHeight;
	input.dims.c = targetChannel;
	input.size = targetWidth * targetHeight * targetChannel;
	input.data = (void*)inputBuffer;
	sessConfig.inputTensors.push_back(input);

	int outputSize1 = (int)(targetWidth / 32) * (int)(targetHeight / 32) * 255;
	int outputSize2 = (int)(targetWidth / 16) * (int)(targetHeight / 16) * 255;
	int outputSize3 = (int)(targetWidth / 8) * (int)(targetHeight / 8) * 255;
	signed char* outputBuffer = (signed char*)calloc(outputSize1 + outputSize2 + outputSize3, sizeof(signed char));
	AWNNTensorDesc output1, output2, output3;
	output1.data = (void*)outputBuffer;
	output2.data = (void*)(outputBuffer + outputSize1);
	output3.data = (void*)(outputBuffer + outputSize1 + outputSize2);
	sessConfig.outputTensors.push_back(output1);
	sessConfig.outputTensors.push_back(output2);
	sessConfig.outputTensors.push_back(output3);

	printf("\nInfer by IPU...\n");
	int inferenceFlag;
	// perf_start(yolov3)
	// for(int i = 0; i < 100; i++)
	inferenceFlag = caseNet.inference(sessConfig);
	// perf_end(yolov3)
	printf("Infer sucessfully!\n");

	// dequantize the output
	printf("\nDequantize...\n");
	std::vector<float*> outputFloatBuffer(3);
	std::vector<AWNNTensorDims> outputFloatDims(3);
	for (size_t i = 0; i < outputFloatBuffer.size(); i++){
		signed char* data = (signed char*)sessConfig.outputTensors[i].data;
		int size = sessConfig.outputTensors[i].size;
		outputFloatDims[i].w = sessConfig.outputTensors[i].dims.w;
		outputFloatDims[i].h = sessConfig.outputTensors[i].dims.h;
		outputFloatDims[i].c = sessConfig.outputTensors[i].dims.c;

		outputFloatBuffer[i] = (float *)calloc(size, sizeof(float));
		awnn_dequantize(data, size, outScale[i], outputFloatBuffer[i]);
		printf("%d, output width %d, output height %d, output channel %d\n", i, outputFloatDims[i].w, outputFloatDims[i].h, outputFloatDims[i].c);
	}
	
	// compute the output of yolov3 detection head
	YoloV3HeadParams headParams;
	headParams.numClass = 80;
	headParams.numBox = 3;
	headParams.confidenceThreshold = 0.55;
	headParams.nmsThreshold = 0.5;

	headParams.numAnchor = 18;
	float anchors[18] = { 10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326 };
	headParams.anchors = (float *)calloc(18, sizeof(float));
	memcpy(headParams.anchors, anchors, 18 * sizeof(float));
	
	headParams.numMask = 9;
	float masks[9] = {6, 7, 8, 3, 4, 5, 0, 1, 2};
	headParams.masks = (float *)calloc(9, sizeof(float));
	memcpy(headParams.masks, masks, 9 * sizeof(float));

	headParams.numAnchorScale = 3;
	float anchorScales[3] = {32, 16, 8};
	headParams.anchorScales = (float *)calloc(3, sizeof(float));
	memcpy(headParams.anchorScales, anchorScales, 3 * sizeof(float));
	
    YoloV3DetOutputs *detOutputs = (YoloV3DetOutputs*)calloc(1, sizeof(YoloV3DetOutputs));
	init_det_outputs(detOutputs);

	printf("\nInfer by CPU...\n");
	infer_yolov3_detection_outputs(outputFloatBuffer, outputFloatDims, headParams, detOutputs);

	printf("detect object num: %d\n", detOutputs->num);
	for (size_t i = 0; i < detOutputs->num; ++i)
	{
		int label = detOutputs->objects[i].label;
		float prob = detOutputs->objects[i].prob;
		int topLeftX = int(round((detOutputs->objects[i].x1 * targetWidth) / ratio));
		int topLeftY = int(round((detOutputs->objects[i].y1 * targetHeight) / ratio));
		int bottomRightX = int(round((detOutputs->objects[i].x2 * targetWidth) / ratio));
		int bottomRightY = int(round((detOutputs->objects[i].y2 * targetHeight) / ratio));

		if (topLeftX < 0) 
		{
			topLeftX = 0;
		}
		if (topLeftY < 0) 
		{
			topLeftY = 0;
		}
		if (bottomRightX > img->w) 
		{
			bottomRightX = img->w;
		}
		if (bottomRightY > img->h) 
		{
			bottomRightY = img->h;
		}
		printf("%d, label %d, class %s, prob %f, x1 %d, y1 %d, x2 %d, y2 %d\n", i, label,
			coco_names[label - 1], prob, topLeftX, topLeftY, bottomRightX, bottomRightY);
	}	
	caseNet.destroy();
	awnn_free_image(img);
	awnn_free_image(imgResized);
	free(dataPadded);

	free(inputBuffer);
	free(outputBuffer);

	for (int i = 0; i < outputFloatBuffer.size(); i++)
	{
		free(outputFloatBuffer[i]);
	}
	outputFloatBuffer.clear();
	free(headParams.anchors);
	free(headParams.masks);
	free(headParams.anchorScales);

	deinit_det_outputs(detOutputs);
	free(detOutputs);
}


int main()
{
	AWNNInit();

	testYoloV3();

	AWNNDeinit();

	return 0;
}
