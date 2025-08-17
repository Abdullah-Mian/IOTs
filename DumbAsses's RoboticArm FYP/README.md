# cnn_model_segmentation
# U-Net CNN Cotton Segmentation (Pre-trained Model)

This repository contains a pre-trained U-Net model for cotton segmentation in images.

## 🚀 Quick Start

### Prerequisites
Make sure you have Python 3.7+ installed with required packages:

```bash
pip install tensorflow opencv-python matplotlib pillow numpy
```

### Running the Model
Execute the inference script:
```bash
python run_inference.py
```

## 📁 Project Structure
```
├── run_inference.py         # Main inference script
├── unet_model.tflite       # Pre-trained TensorFlow Lite model
└── README.md               # This file
```

## 🎯 Features

### Inference Script (`run_inference.py`)
- Test on custom images
- Real-time webcam inference
- Visualization of segmentation results
- Easy-to-use interactive menu

## 📊 Model Details
- **Input Size**: 128x128x3 (RGB images)
- **Output**: 128x128x1 (Binary segmentation mask)
- **Architecture**: U-Net with encoder-decoder structure
- **Format**: TensorFlow Lite (.tflite)

## 💡 Usage

Run the script and choose from the menu:
1. **Test on Image**: Provide path to any image file
2. **Webcam Mode**: Real-time segmentation using your camera
3. **Exit**: Close the application

### Commands to Run:
```bash
# Navigate to the project directory
cd c:\Users\Abdullah\Desktop\cnn_model_segmentation

# Run the inference script
python run_inference.py
```

## 🔧 Requirements
- TensorFlow
- OpenCV
- Matplotlib
- PIL (Pillow)
- NumPy

## 📝 Notes
- Ensure `unet_model.tflite` is in the same directory as the script
- For webcam mode, make sure your camera is not being used by other applications
- Press 'q' to exit webcam mode
