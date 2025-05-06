#pragma once

#include <Windows.h>
#include <d3d11.h>
#include "physics.h" // Include for LineSegment struct

// Structure to hold DirectX data
struct OverlayData {
    HWND hwnd;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11Buffer* vertexBuffer;
};

// Function declarations
HWND InitializeOverlay(HINSTANCE hInstance, OverlayData* pOverlayData);
void DrawLine(float startX, float startY, float endX, float endY, float color[4], OverlayData* pOverlayData);
void DrawCircle(float centerX, float centerY, float radius, float color[4], OverlayData* pOverlayData); // New
void ClearOverlay(OverlayData* pOverlayData);
void PresentOverlay(OverlayData* pOverlayData);
void CleanupOverlay(OverlayData* pOverlayData);
bool InitDirectX(HWND hwnd, int width, int height, OverlayData* pOverlayData);
bool CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob);