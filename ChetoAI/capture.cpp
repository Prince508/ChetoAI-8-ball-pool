#include "capture.h"
#include <Windows.h>

// Helper: Convert HBITMAP to cv::Mat
cv::Mat hbitmapToMat(HBITMAP hBitmap) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    cv::Mat mat(bmp.bmHeight, bmp.bmWidth, CV_8UC4);

    HDC hdc = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdc);
    SelectObject(hdcMem, hBitmap);
    GetDIBits(hdcMem, hBitmap, 0, bmp.bmHeight, mat.data, (BITMAPINFO*)&bmp, DIB_RGB_COLORS);

    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdc);

    cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR); // Remove alpha
    cv::flip(mat, mat, 0); // Flip vertically
    return mat;
}

cv::Mat captureScreen() {
    HWND hwnd = GetDesktopWindow();
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

    cv::Mat mat = hbitmapToMat(hBitmap);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);

    return mat;
}

cv::Mat captureWindow(const std::wstring& windowName) {
    HWND hwnd = FindWindowW(NULL, windowName.c_str());
    if (!hwnd) return cv::Mat(); // Return empty matrix if window not found

    HDC hdcWindow = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWindow);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right;
    int height = rc.bottom;

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    cv::Mat mat = hbitmapToMat(hBitmap);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWindow);

    return mat;
}