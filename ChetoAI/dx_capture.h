#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

bool initializeDxCapture();
cv::Mat captureDxFrame();
cv::Mat captureDxWindow(const std::wstring& windowName);
void releaseDxCapture();
