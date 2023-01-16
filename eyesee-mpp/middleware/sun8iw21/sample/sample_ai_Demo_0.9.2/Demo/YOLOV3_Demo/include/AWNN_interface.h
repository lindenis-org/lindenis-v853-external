// Copyright (c) 2008-2020 Allwinner Technology Co. Ltd. All rights reserved.
// File : AWNN_interface.h
// Description : AWNN Interface
// Version: 0.9.2

#ifndef AWNN_INTERFACE_H
#define AWNN_INTERFACE_H

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

enum AWNNTensorLayout
{
	//default hwc
	LAYOUT_HWC,
	
	//LAYOUT_CHW
};

enum AWNNTensorDataType
{
	//default int8
	DATA_TYPE_INT8,
	
	//DATA_TYPE_FP32
};

enum AWNNInferenceType
{
	//default ipu
	AWNN_FORWARD_IPU = 0,
	
	//AWNN_FORWARD_AUTO = 1
};

struct AWNNTensorDims
{
	uint32_t h = 0;
	uint32_t w = 0;
	uint32_t c = 0;
};

struct AWNNTensorDesc
{
	AWNNTensorLayout layout = LAYOUT_HWC;
	AWNNTensorDataType dataType = DATA_TYPE_INT8;
	AWNNTensorDims dims;
	std::string tensorName;
	int tensorID = -1;
	// tensor address
	void* data = NULL;
	// tensor size
	uint32_t size = 0;
	// quantization scale of tensor
	float tensorScale = 1.0;
};

struct AWNNConfig
{
	bool paramInvisible = false;

	// network model path
	std::string paramPath;
	std::string modelPath;

	unsigned char* paramBuffer = NULL;
	unsigned char* modelBuffer = NULL;	
};

struct AWNNSessionConfig
{
	// inference device type
	AWNNInferenceType type = AWNN_FORWARD_IPU;

	// input/output tensor names of model
	std::vector<std::string> inputNames;
	std::vector<std::string> outputNames;
	// input/output tensor indexes of model
	std::vector<int> inputIDs;
	std::vector<int> outputIDs;

	// input/output tensors description
	std::vector<AWNNTensorDesc> inputTensors;
	std::vector<AWNNTensorDesc> outputTensors;
};

class AWNNInstance
{
public:
	AWNNInstance();

	~AWNNInstance();

	int create(const AWNNConfig& Config);

	int inference(AWNNSessionConfig& sessConfig);

	void destroy();

public:
	int getTensorScale(const std::string& tensorName, float& tensorScale);

	int getTensorScale(const int& tensorID, float& tensorScale);

private:
	void* graph;
	void* option;
	bool  invisibleFlag;
};

// init ipu & AWNN
void AWNNInit();

// deinit ipu & AWNN
void AWNNDeinit();

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // !AWNN_INTERFACE_H
