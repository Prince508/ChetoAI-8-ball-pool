#pragma once

#include <string>
#include <opencv2/opencv.hpp>

// Function declarations
cv::Mat captureScreen(); // Captures the entire screen
cv::Mat captureWindow(const std::wstring& windowName); // Captures a specified window