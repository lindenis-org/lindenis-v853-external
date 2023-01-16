#  Overview
YOLOV3 Demo presents the detailed steps of detecting objects within an image via the AWNN APIs (based on YOLOV3, of course).

# Structure
The YOLOV3 Demo folder contains 5 subfolders ("3rdparty", "include", "src", "data", "model"), a makefile and an executable file. 

- YOLOV3_Demo/3rdparty/lib/ : some 3rd-party libraries.
- YOLOV3_Demo/include/ : header files.
- YOLOV3_Demo/src/ : source codes.
- YOLOV3_Demo/data/ : a test image inside.
- YOLOV3_Demo/model/ : the YOLOV3 ".param" and ".bin" file.
- YOLOV3_Demo/Makefile : the compiling file.
- YOLOV3_Demo/yolov3 : the executable file.

# Functions

## 1. main.cpp >> main()

The function implements yolov3 detection function based on AWNN APIs. It includes initialization, test and deinitialization. 

Warnings: 

Before you run the executable file, you must set the library path as follows:
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/sdcard/YOLOV3_Demo/3rdparty/lib`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/sdcard/com_libs`

## 2. main.cpp >> AWNNInit()

The function is used to initialize the functions of hardware module and AWNN.
Please refer to AWNN guideline 4.1.1 and 4.3.2 for details.


## 3. main.cpp >> testYoloV3()

The function is used to illustrate the detailed steps of detecting objects within an image via the AWNN APIs, as described below.

### (1) Image Preprocessing

* Read an image ("jpg" or "bmp") and convert it to AWNN image format.
* Resize the image to a specified size.
* Normalize the image by mean values and variance values.
* Pad (32-aligned) the width/height of the image.
* Quantize float data to int8.


### (2) Network Setting

please refer to AWNN guideline 4.3.3 for details.

### (3) Session Setting

please refer to AWNN guideline 4.3.4 for details.

### (4) IPU Inference

Please refer to AWNN guideline 4.3.5 for details.

### (5) Dequantization

Dequantize the output results (from INT8 to FP32).

### (6) Detection Outputs

* Set parameters for yolov3 detection head.
* Initialize detection outputs.
* Infer the detection results according to the dequantized data.
* Print the predicted class, the confidence score and the box coordinates.

### (7) Deinitialization

* Free the AWNN image memory, the input data memeory and the output data memory.
* Deinitialize the detection outputs memory.
* Destroy the network.

## 4. main.cpp >> AWNNDeinit()

The function is used to free the functions of hardware module and AWNN. Please refer to AWNN guideline 4.1.2 for details.


