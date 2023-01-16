#include <stdio.h>
#include <string.h>
#include <float.h>
#include <sys/time.h>

#include "AWNN_interface.h"
#include "retinaface.id.h"

double getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int loadFromBin(const char* binPath, const int& size, void* buffer)
{
	FILE* fp = fopen(binPath, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen %s failed\n", binPath);
		return -1;
	}
	int nread = fread(buffer, 1, size, fp);
	if (nread != size)
	{
		fprintf(stderr, "fread bin failed %d\n", nread);
	    fclose(fp);
		return -1;
	}
	fclose(fp);

	return 0;
}

int getBinSize(const char* binPath, size_t& binSize)
{
	FILE* fp = fopen(binPath, "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen %s failed\n", binPath);
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	binSize = ftell(fp);
	fclose(fp);

	return 0;
}

int compareResult(const char* trueResultPath, const signed char* inferenceResult, const int& size)
{
	signed char*  trueResult = (signed char*)malloc(size);
	int ret = loadFromBin(trueResultPath, size, trueResult);
	if (ret != 0)
	{
		fprintf(stderr, "load result %s error.\n", trueResultPath);
		return ret;
	}
	
	int countSuccess = 0, countFail = 0;
	for (int i = 0; i < size; i++)
	{
		int tmp = trueResult[i] - inferenceResult[i];
		int diff = tmp >= 0 ? tmp : (-tmp);
		if (diff == 0)
		{
			countSuccess++;
			// fprintf(stderr, "diff = %d, trueResult= %d, inferenceResult= %d\n", diff, trueResult[i], inferenceResult[i]);
		}
		else
		{
			countFail++;
			// fprintf(stderr, "diff = %d, trueResult= %d, inferenceResult= %d\n", diff, trueResult[i], inferenceResult[i]);
		}
	}

	if (countFail == 0)
	{
		fprintf(stderr, "%s: test success ^_^ ^_^ ^_^\n", trueResultPath);
		fprintf(stderr, "count success number: %d\n", countSuccess);
	}
	else
	{
		fprintf(stderr, "%s, test fail T_T T_T T_T\n", trueResultPath);
		fprintf(stderr, "count fail number: %d\n", countFail);
	}
	
	free(trueResult);
	
	return 0;
}

int compareResultFP(const char* trueResultPath, float* inferenceResult, const int& size)
{
	float* trueResult = (float*)malloc(size * sizeof(float));
	int ret = loadFromBin(trueResultPath, size * sizeof(float), trueResult);
	if (ret != 0)
	{
		fprintf(stderr, "load result %s error.\n", trueResultPath);
		return ret;
	}

	int countSuccess = 0, countFail = 0;
	for (int i = 0; i < size; i++)
	{
		float tmp = trueResult[i] - inferenceResult[i];
		float diff = tmp >= 0 ? tmp : (-tmp);
		if (diff < 0.0001)
		{
			countSuccess++;
			// fprintf(stderr, "diff = %.4f, trueResult= %.4f, inferenceResult= %.4f\n", diff, trueResult[i], inferenceResult[i]);
		}
		else
		{
			countFail++;
			// fprintf(stderr, "diff = %.4f, trueResult= %.4f, inferenceResult= %.4f\n", diff, trueResult[i], inferenceResult[i]);
		}
	}

	if (countFail == 0)
	{
		fprintf(stderr, "%s: test success ^_^ ^_^ ^_^\n", trueResultPath);
		fprintf(stderr, "count success number: %d\n", countSuccess);
	}
	else
	{
		fprintf(stderr, "%s, test fail T_T T_T T_T\n", trueResultPath);
		fprintf(stderr, "count fail number: %d\n", countFail);
	}

	free(trueResult);

	return 0;
}

void testRetinaFaceInvisible()
{
	/* memory alloc for inputs and outputs of network */
	signed char* inputBuffer = (signed char*)malloc(480 * 384 * 3 * sizeof(signed char));
	signed char* outputBuffer = (signed char*)malloc((15120 * 4 + 15120 * 2 + 15120 * 10) * sizeof(float));

	/* memory alloc for param and model of network */
	const char* paramPath = "../data/model_test/awnn_retinaface/model_opt_int8.param.bin";
	const char* modelPath = "../data/model_test/awnn_retinaface/model_opt_int8.bin";
	size_t paramBufferSize = 0;
	size_t modelBufferSize = 0;
	getBinSize(paramPath, paramBufferSize);
	getBinSize(modelPath, modelBufferSize);
	unsigned char* paramBuffer = (unsigned char*)malloc(paramBufferSize);
	unsigned char* modelBuffer = (unsigned char*)malloc(modelBufferSize);
	loadFromBin(paramPath, paramBufferSize, paramBuffer);
	loadFromBin(modelPath, modelBufferSize, modelBuffer);
	
	/* set AWNNConfig */
	AWNNConfig config;
	config.paramInvisible = true;
	config.paramBuffer = paramBuffer;
	config.modelBuffer = modelBuffer;

	/* set AWNNSessionConfig */
	AWNNSessionConfig sessConfig;
	sessConfig.type = AWNN_FORWARD_AUTO; // hybrid computing
	sessConfig.inputIDs =  { model_opt_int8_param_id::BLOB_input0 };
	sessConfig.outputIDs = { model_opt_int8_param_id::BLOB_output0, model_opt_int8_param_id::BLOB_463, model_opt_int8_param_id::BLOB_462 };

	/* set input information  */
	AWNNTensorDesc input;
	input.dims.w = 480;
	input.dims.h = 384;
	input.dims.c = 3;
	input.size = 480 * 384 * 3;
	input.data = (void*)inputBuffer;
	input.layout = LAYOUT_HWC;
	input.dataType = DATA_TYPE_INT8;
	sessConfig.inputTensors.push_back(input);

	/* set output information  */
	AWNNTensorDesc output1, output2, output3;
	output1.data = (void*)outputBuffer;
	output1.layout = LAYOUT_CHW;
	output1.dataType = DATA_TYPE_FP32;
	output2.data = (void*)(outputBuffer + 15120 * 4 * sizeof(float));
	output2.layout = LAYOUT_CHW;
	output2.dataType = DATA_TYPE_FP32;	
	output3.data = (void*)(outputBuffer + (15120 * 4 + 15120 * 2) * sizeof(float));
	output3.layout = LAYOUT_CHW;
	output3.dataType = DATA_TYPE_FP32;		
	sessConfig.outputTensors.push_back(output1);
	sessConfig.outputTensors.push_back(output2);
	sessConfig.outputTensors.push_back(output3);

	/* create AWNNInstance */
	AWNNInstance caseNet;
	int createFlag = caseNet.create(config);
	free(paramBuffer);
	free(modelBuffer);

	/* inference pipeline */
	int loopNumber = 500;
	if (createFlag == 0)
	{
		double timeMin = DBL_MAX;
		double timeMax = -DBL_MAX;
		double timeAvg = 0;		
		for (int index = 0; index < loopNumber; index++)
		{
			printf("===========================testRetinaFaceInvisible: No %7d==============================\n", index);
			// load data
			const char* binPath = "../data/model_test/awnn_retinaface/data_fake_inputs_hwc.bin";
			loadFromBin(binPath, 480 * 384 * 3 * sizeof(signed char), inputBuffer);

			double start = getCurrentTime();

			// inference
			int inferenceFlag = caseNet.inference(sessConfig);

			double end = getCurrentTime();
			double time = end - start;
			timeMin = std::min(timeMin, time);
			timeMax = std::max(timeMax, time);
			timeAvg += time;

			// compare
			if (inferenceFlag == 0)
			{
				std::vector<std::string> resultPaths;
				resultPaths.push_back("../data/model_test/awnn_retinaface/output0_chw.bin");
				resultPaths.push_back("../data/model_test/awnn_retinaface/463_chw.bin");
				resultPaths.push_back("../data/model_test/awnn_retinaface/462_chw.bin");
				for (size_t i = 0; i < resultPaths.size(); i++)
				{
					compareResultFP(resultPaths[i].c_str(), (float*)sessConfig.outputTensors[i].data, sessConfig.outputTensors[i].size);
				}
			}
		}
		timeAvg /= loopNumber;
		fprintf(stderr, "testRetinaFaceInvisible:  min = %7.2f  max = %7.2f  avg = %7.2f\n", timeMin, timeMax, timeAvg);
	}

	caseNet.destroy();

	free(inputBuffer);
	free(outputBuffer);
}

void testRetinaFace()
{
	/* memory alloc for inputs and outputs of network */
	signed char* inputBuffer = (signed char*)malloc(480 * 384 * 3 * sizeof(signed char));
	signed char* outputBuffer = (signed char*)malloc((15120 * 4 + 15120 * 2 + 15120 * 10) * sizeof(float));

	/* set AWNNConfig */
	AWNNConfig config;
	config.paramInvisible = false;
	config.paramPath = "../data/model_test/awnn_retinaface/model_opt_int8.param";
	config.modelPath = "../data/model_test/awnn_retinaface/model_opt_int8.bin";

	/* set AWNNSessionConfig */
	AWNNSessionConfig sessConfig;
	sessConfig.type = AWNN_FORWARD_AUTO; // hybrid computing
	sessConfig.inputNames =  { "input0" };
	sessConfig.outputNames = { "output0", "463", "462" };

	/* set input information  */
	AWNNTensorDesc input;
	input.dims.w = 480;
	input.dims.h = 384;
	input.dims.c = 3;
	input.size = 480 * 384 * 3;
	input.data = (void*)inputBuffer;
	input.layout = LAYOUT_HWC;
	input.dataType = DATA_TYPE_INT8;	
	sessConfig.inputTensors.push_back(input);

	/* set output information  */
	AWNNTensorDesc output1, output2, output3;
	output1.data = (void*)outputBuffer;
	output1.layout = LAYOUT_CHW;
	output1.dataType = DATA_TYPE_FP32;
	output2.data = (void*)(outputBuffer + 15120 * 4 * sizeof(float));
	output2.layout = LAYOUT_CHW;
	output2.dataType = DATA_TYPE_FP32;
	output3.data = (void*)(outputBuffer + (15120 * 4 + 15120 * 2) * sizeof(float));
	output3.layout = LAYOUT_CHW;
	output3.dataType = DATA_TYPE_FP32;
	sessConfig.outputTensors.push_back(output1);
	sessConfig.outputTensors.push_back(output2);
	sessConfig.outputTensors.push_back(output3);

	/* create AWNNInstance */
	AWNNInstance caseNet;
	int createFlag = caseNet.create(config);

	/* inference pipeline */
	int loopNumber = 500;
	if (createFlag == 0)
	{
		double timeMin = DBL_MAX;
		double timeMax = -DBL_MAX;
		double timeAvg = 0;		
		for (int index = 0; index < loopNumber; index++)
		{
			printf("===========================testRetinaFace: No %7d==============================\n", index);
			// load data
			const char* binPath = "../data/model_test/awnn_retinaface/data_fake_inputs_hwc.bin";
			loadFromBin(binPath, 480 * 384 * 3 * sizeof(signed char), inputBuffer);

			double start = getCurrentTime();

			// inference
			int inferenceFlag = caseNet.inference(sessConfig);

			double end = getCurrentTime();
			double time = end - start;
			timeMin = std::min(timeMin, time);
			timeMax = std::max(timeMax, time);
			timeAvg += time;

			// compare
			if (inferenceFlag == 0)
			{
				std::vector<std::string> resultPaths;
				resultPaths.push_back("../data/model_test/awnn_retinaface/output0_chw.bin");
				resultPaths.push_back("../data/model_test/awnn_retinaface/463_chw.bin");
				resultPaths.push_back("../data/model_test/awnn_retinaface/462_chw.bin");
				for (size_t i = 0; i < resultPaths.size(); i++)
				{
					compareResultFP(resultPaths[i].c_str(), (float*)sessConfig.outputTensors[i].data, sessConfig.outputTensors[i].size);
				}
			}				
		}
		timeAvg /= loopNumber;
		fprintf(stderr, "testRetinaFace:  min = %7.2f  max = %7.2f  avg = %7.2f\n", timeMin, timeMax, timeAvg);		
	}

	caseNet.destroy();

	free(inputBuffer);
	free(outputBuffer);
}

int main()
{
	AWNNInit();

	testRetinaFaceInvisible();

	testRetinaFace();

	AWNNDeinit();

	return 0;
}
