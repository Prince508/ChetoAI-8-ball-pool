# ChetoAI - Level 1 (Extended Guidelines for 8 Ball Pool)

ChetoAI is a C++ educational project that replicates **Cheto Aim Tool** features for the mobile game **8 Ball Pool**, beginning with **Level 1: Extended Guidelines** using object detection and physics simulation.

## 🎯 Project Goal

To build an overlay-based assistance tool using:
- **YOLOv11m-seg ONNX** model for real-time object detection
- **DirectX 11 overlay** for visual feedback
- **OpenCV** for image processing
- **ONNX Runtime** for inference
- **Custom physics module** to draw accurate guidelines

---

## ✅ Current Status (Level 1)

The following are implemented:
- Capture the game window in real time
- Run ONNX inference on captured frames to detect:
  - Balls, Guidelines, Holes, Play Area, Force, Spin
- Compute the projected path of the cue ball
- Render extended guideline overlay directly on the game


---

## 🧰 Prerequisites

1. **Windows 10/11 x64**
2. **Visual Studio 2022** with C++ Desktop Development workload
3. **DirectX SDK (June 2010)** (for legacy headers if needed)
4. **ONNX Runtime 1.21.0** ([Download](https://github.com/microsoft/onnxruntime/releases))
5. **OpenCV 4.x** installed and configured
6. **Clone or download YOLOv11m-seg model** (already placed in `onnx_model/`)

---

## 🛠️ Build Instructions

1. Open `ChetoAl.sln` in Visual Studio.
2. Ensure build configuration is **x64 - Debug**.
3. Make sure `onnxruntime.dll` and `onnxruntime.lib` are in:
   - `x64/Debug/` (for runtime and linker)
4. Make sure OpenCV is configured correctly:
   - Include directories (e.g., `C:\opencv\build\include`)
   - Library directories (e.g., `C:\opencv\build\x64\vc15\lib`)
5. Build the solution: **Ctrl + Shift + B**

---

## ▶️ How to Run

1. Launch **8 Ball Pool**, window name that is opening the game should have name **8 Ball Pool** or you can change this in main.cpp, Line 70.
2. Run `ChetoAl.exe` from `x64/Debug/`.
3. The overlay should appear on top of the game window.
4. Move the cue – the extended guideline should reflect predicted paths based on current layout.

> ⚠️ Ensure the emulator window is in the foreground.

---

## 🧪 Features (Level 1)

- [x] Object Detection using ONNX
- [x] Real-time Overlay using DirectX 11
- [x] Cue Ball Path Prediction
- [x] Hole Proximity Detection

---

## 📅 Planned Features (Next Levels)

### 🔹 Level 2 – Advanced Cushion Shots
- Add ricochet prediction for 1-cushion bank shots
- Predict ball-to-ball deflections

### 🔹 Level 3 – Full Cheto Clone
- Add spin and force estimation
- Predict multi-collision outcomes
- UI element detection and angle helper

---

## 🧠 Tech Stack

- **Language**: C++17
- **Rendering**: DirectX 11
- **Machine Learning**: ONNX Runtime + YOLOv11m-seg
- **Computer Vision**: OpenCV
- **Window Management**: WinAPI + pygetwindow logic port

---

## 🤝 Credits

- **YOLOv11m-seg** trained via Roboflow
- ONNX Runtime maintained by Microsoft
- Original concept inspired by Cheto Aim tool (reverse-engineered for education)

---

## 📝 Disclaimer

> **This project is for educational purposes only.**
> Using overlays or auto-aim tools in online games may violate the game’s terms of service and could lead to account bans. The developer does not endorse cheating in any form.

---

## 📝NOTE

- This project is still under prototyping stage, and i donot have coding knowledge all this is made using chatgpt it may not be fully optimizedsorry about that feel free to make any changes and all the help to make this possible is appriciated.

---