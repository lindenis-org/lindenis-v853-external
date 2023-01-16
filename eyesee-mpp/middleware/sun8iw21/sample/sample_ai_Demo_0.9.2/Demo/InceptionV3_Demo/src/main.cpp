#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <float.h>
#include <algorithm>
#include <math.h>

#include "AWNN_interface.h"
#include "inceptionv3_utils.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;


AWNNImg* read_image(const char *imagePath)
{	
	const char *func = "read_image";

	if(!imagePath)
	{
		fprintf(stderr, "[%s]: imagePath is nullptr.\n", func);
	}

	int imgWidth = 0;
	int imgHeight = 0;
	int imgChannel = 0;
	unsigned char* inputData = stbi_load(imagePath, &imgWidth, &imgHeight, &imgChannel, 3);
	if (!inputData)
	{
		fprintf(stderr, "[%s]: Failed to load the image.\n", func);
	}
	if (imgChannel != 3)
	{
		fprintf(stderr, "[%s]: The image is not rgb data.\n", func);
	}
	AWNNImg* img = awnn_make_rgb_image(inputData, imgWidth, imgHeight, AWNN_RGB);
	return img;
}


AWNNImg* resize_image(AWNNImg* img, int targetSize, float* ratio)
{
	const char *func = "resize_image";

	if (!img || !ratio)
	{
		fprintf(stderr, "[%s]: img or ratio is nullptr.\n", func);
	}
	if (img->w<=0 || img->h<=0)
	{
		fprintf(stderr, "[%s]: img->w or img->h is illegal input.\n", func);
	}
	if (targetSize <= 0)
	{
		fprintf(stderr, "[%s]: targetSize is illegal input.\n", func);
	}
	
	int imgWidth = img->w;
	int imgHeight = img->h;
	
	*ratio = min(targetSize * 1.0 / imgWidth, targetSize * 1.0 / imgHeight);
	AWNNImg* imgResized = awnn_make_blank_image(int(round(imgWidth*(*ratio))), int(round(imgHeight*(*ratio))), AWNN_RGB);
	resize_c3(img, imgResized);
	return imgResized;
}


void testInceptionV3()
{
	// set AWNNConfig
	printf("Load model param and bin ...\n");
	AWNNConfig config;
	config.paramPath = "./model/inceptionv3_int8.param";
	config.modelPath = "./model/inceptionv3_int8.bin";

	// create AWNNInstance
	AWNNInstance caseNet;
	int createFlag = caseNet.create(config);

	// get tensors scale
	float dataScale = 0.0f;
	float outScale = 0.0f;
	caseNet.getTensorScale("data", dataScale);
	caseNet.getTensorScale("inception30_dense0_fwd", outScale);

	printf("data scale: %f\n", dataScale);
	printf("outScale: %f\n", outScale);

	// read image
	printf("\nRead and transform data ...\n");
	char* imagePath = "./data/ILSVRC2012_val_00000015.JPEG";
	AWNNImg* img = read_image(imagePath);
	printf("image width: %d, image height: %d\n", img->w, img->h);

	// resize the original image by setting the longer edge to size(299) 
	// and setting the shorter edge accordingly
	int targetSize = 299;
	float ratio = 0.0f;
	AWNNImg* imgResized = resize_image(img, targetSize, &ratio);
	printf("resized image width: %d, resized image height: %d\n", imgResized->w, imgResized->h);

	// normalize the image and pad the shorter side of the image to size(299).
	int targetWidth = 0;
	int targetHeight = 0;
	int targetChannel = imgResized->c;
	float meanVals[3] = { 123.675, 116.28, 103.53 };
	float normVals[3] = { 0.01712475383, 0.0175070028, 0.0174291938998 };
	float* dataPadded = pad_image(imgResized, meanVals, normVals, targetSize, &targetWidth, &targetHeight);
	printf("padded image width: %d, padded image height: %d\n", targetWidth, targetHeight);
	
	// quantize float data to int8
	signed char* inputBuffer = (signed char*)calloc(targetWidth * targetHeight * targetChannel, sizeof(signed char));
	awnn_quantize(dataPadded, targetWidth * targetHeight * targetChannel, dataScale, inputBuffer);

	// set AWNNSessionConfig
	AWNNSessionConfig sessConfig;
	sessConfig.type = AWNN_FORWARD_IPU;
	sessConfig.inputNames = { "data" };
	sessConfig.outputNames = { "inception30_dense0_fwd" };

	AWNNTensorDesc input;
	input.dims.w = targetWidth;
	input.dims.h = targetHeight;
	input.dims.c = targetChannel;
	input.size = targetWidth * targetHeight * targetChannel;
	input.data = (void*)inputBuffer;
	sessConfig.inputTensors.push_back(input);

	int outputSize = 1000; // the total number of classification 
	signed char* outputBuffer = (signed char*)calloc(outputSize, sizeof(signed char));
	AWNNTensorDesc output;
	output.data = (void*)outputBuffer;
	sessConfig.outputTensors.push_back(output);

	printf("\nInfer by ipu ...");
	int inferenceFlag = caseNet.inference(sessConfig);
	printf("Infer successfully!\n");

	AWNNTensorDims outputFloatDims;
	outputFloatDims.w = sessConfig.outputTensors[0].dims.w;
	outputFloatDims.h = sessConfig.outputTensors[0].dims.h;
	outputFloatDims.c = sessConfig.outputTensors[0].dims.c;
	signed char* data = (signed char*)sessConfig.outputTensors[0].data;
	int size = sessConfig.outputTensors[0].size;
	printf("output width %d, output height %d, output channel %d\n", outputFloatDims.w, outputFloatDims.h, outputFloatDims.c);
	
	// signed char -> int
	int* values = (int*)calloc(size, sizeof(int));
	for (int i = 0; i < size; i++)
	{
		values[i] = (int)data[i];
	}

	// get top5 results
	int topNum = 5;
	int* topIndex = get_tops(values, size, topNum);
	
	printf("\nThe top number: %d\n", topNum);
	for (int i = 0; i < topNum; i++)
	{
		printf("%d, label %4d, score  %f\n", i, topIndex[i], values[i] * 1.0f / outScale);
	}

	caseNet.destroy();
	awnn_free_image(img);
	awnn_free_image(imgResized);

	free(dataPadded);
	free(inputBuffer);
	free(outputBuffer);
	free(values);
	free(topIndex);
}


int main()
{
	AWNNInit();
	
	testInceptionV3();

	AWNNDeinit();

	return 0;
}
