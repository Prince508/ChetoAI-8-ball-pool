#include <Windows.h>
#include "overlay.h"
//#include "capture.h"
#include "dx_capture.h"
#include "onnx_inference.h"
#include "physics.h"
#include "enums.h"

// Convert YOLO detections to Ball and Table structs
void processDetections(const std::vector<Detection>& detections, Ball& cueBall, Ball& targetBall, Table& table, int screenWidth, int screenHeight) {
    table.pockets.clear();
    bool cueFound = false, targetFound = false;

    for (const auto& det : detections) {
        cv::Point2f center(det.box.x + det.box.width / 2.0f, det.box.y + det.box.height / 2.0f);
        float radius = std::min(det.box.width, det.box.height) / 2.0f;

        // Scale coordinates to overlay resolution
        center.x = (center.x / screenWidth) * 1920.0f;
        center.y = (center.y / screenHeight) * 1080.0f;
        radius = (radius / screenWidth) * 1920.0f;

        switch (static_cast<ObjectType>(det.class_id)) {
        case ObjectType::White:
            cueBall = { center, radius, BallType::Cue };
            cueFound = true;
            break;
        case ObjectType::Ball:
            if (!targetFound) {
                targetBall = { center, radius, BallType::Target };
                targetFound = true;
            }
            break;
        case ObjectType::Hole:
            table.pockets.push_back(center);
            break;
        case ObjectType::PlayArea:
            table.bounds = cv::Rect(
                static_cast<int>((det.box.x / static_cast<float>(screenWidth)) * 1920.0f),
                static_cast<int>((det.box.y / static_cast<float>(screenHeight)) * 1080.0f),
                static_cast<int>((det.box.width / static_cast<float>(screenWidth)) * 1920.0f),
                static_cast<int>((det.box.height / static_cast<float>(screenHeight)) * 1080.0f)
            );
            break;
        default:
            break;
        }
    }

    // Optional debug
    if (!cueFound || !targetFound || table.pockets.empty()) {
        OutputDebugStringA("Warning: Missing cue/target ball or pockets.\n");
    }
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    OutputDebugStringA("Main Called\n");
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize overlay
    OverlayData overlayData;
    HWND overlayHwnd = InitializeOverlay(hInstance, &overlayData);
    if (!overlayHwnd) {
        MessageBoxA(nullptr, "Failed to initialize overlay!", "Error", MB_OK);
        return 1;
    }

    // Initialize ONNX inference
    ONNXInference detector("D:/AimBotAI/ChetoAI/ChetoAI/onnx_model/yolov11m-seg.onnx");
    if (!detector.isSessionValid()) {
        MessageBoxA(nullptr, "Failed to load ONNX model!", "Error", MB_OK);
        return 1;
    }

    // Initialize DirectX Capture
    if (!initializeDxCapture()) {
        MessageBoxA(nullptr, "Failed to initialize DirectX Capture!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg = { 0 };
    float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f }; // Line color

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Capture the game frame (you can switch to nullptr for full screen)
        cv::Mat frame = captureDxFrame();
        //cv::Mat frame = captureDxWindow(L"image.jpg");
        cv::imwrite("debug_frame.jpg", frame);
        OutputDebugStringA("Saved debug_frame.jpg\n");
        if (frame.empty()){
            OutputDebugStringA("Frame is empty!\n"); // Add this line
            continue;
        }
        OutputDebugStringA("Frame captured!\n");

        int frameWidth = frame.cols;
        int frameHeight = frame.rows;

        std::vector<Detection> detections = detector.runInference(frame);
        if (detections.empty()) {
            OutputDebugStringA("No detections found.\n");
        }
        else {
            OutputDebugStringA("Detections found.\n");
        }

        Ball cueBall, targetBall;
        Table table;
        processDetections(detections, cueBall, targetBall, table, frameWidth, frameHeight);

        ClearOverlay(&overlayData);
        OutputDebugStringA("Cleared overlay.\n");

        overlayData.deviceContext->OMSetRenderTargets(1, &overlayData.renderTargetView, nullptr);

        OutputDebugStringA("Manually Drawing Test Line\n");
        //Test
        DrawLine(100, 100, 600, 600, red, &overlayData);

        std::vector<LineSegment> guide = calculateGuideline(cueBall, targetBall, table);
        for (const auto& segment : guide) {
            DrawLine(segment.start.x, segment.start.y, segment.end.x, segment.end.y, red, &overlayData);
            OutputDebugStringA("Guideline segment drawn.\n");
        }
        PresentOverlay(&overlayData);

        if (GetAsyncKeyState(VK_END) & 1) break;

        Sleep(16); // ~60 FPS
    }

	OutputDebugStringA("Exiting...\n");
    releaseDxCapture();
    CleanupOverlay(&overlayData);
    return 0;
}
