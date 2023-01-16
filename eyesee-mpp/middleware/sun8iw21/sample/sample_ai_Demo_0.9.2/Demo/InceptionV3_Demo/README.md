#  Overview
InceptionV3 Demo presents the detailed steps of classification within an image via the AWNN APIs.

# Structure
The InceptionV3 Demo folder contains 5 subfolders ("3rdparty", "include", "src", "data", "model"), a makefile and an executable file. 

- InceptionV3_Demo/3rdparty/lib/ : some 3rd-party libraries.
- InceptionV3_Demo/include/ : header files.
- InceptionV3_Demo/src/ : source codes.
- InceptionV3_Demo/data/ : a test image inside.
- InceptionV3_Demo/model/ : the InceptionV3 ".param" and ".bin" file.
- InceptionV3_Demo/Makefile : the compiling file.
- InceptionV3_Demo/inceptionv3 : the executable file.

# Functions

## 1. main.cpp >> main()

The function implements inceptionv3 image classification function based on AWNN APIs. It includes initialization, test and deinitialization. 

Warnings: 

Before you run the executable file, you must set the library path as follows:
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/sdcard/InceptionV3_Demo/3rdparty/lib`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/sdcard/com_libs`

## 2. main.cpp >> AWNNInit()

The function is used to initialize the functions of hardware module and AWNN.
Please refer to AWNN guideline 4.1.1 and 4.3.2 for details.


## 3. main.cpp >> testInceptionV3()

The function presents the detailed steps of classification within an image via the AWNN APIs, as described below.

### (1) Image Preprocessing

* Read an image ("jpg" or "bmp") and convert it to AWNN image format.
* Resize the original image by setting the longer edge to size(299) and setting the shorter edge accordingly.
* Normalize the image by mean values and variance values.
* Pad the shorter edge to size(299).
* Quantize float data to int8.

### (2) Network Setting

Please refer to AWNN guideline 4.3.3 for details.

### (3) Session Setting

Please refer to AWNN guideline 4.3.4 for details.

### (4) IPU Inference

Please refer to AWNN guideline 4.3.5 for details.

### (5) Dequantization

Dequantize the output results (from INT8 to FP32).

### (6) Classification Outputs

* Get the indexes and the sorted scores made by the output results in descending order.
* Print the top5 labels and scores.

### (7) Deinitialization

* Free the AWNN image memory, the input data memory and the output data memory.
* Deinitialize the classification outputs memory.
* Destroy the network.

## 4. main.cpp >> AWNNDeinit()

The function is used to free the functions of hardware module and AWNN. Please refer to AWNN guideline 4.1.2 for details.


