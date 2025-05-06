#include "overlay.h"
#include <d3dcompiler.h>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND InitializeOverlay(HINSTANCE hInstance, OverlayData* pOverlayData) {
    const wchar_t CLASS_NAME[] = L"ChetoOverlay";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        CLASS_NAME,
        L"Overlay",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);

    if (!InitDirectX(hwnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), pOverlayData))
        return nullptr;

    pOverlayData->hwnd = hwnd;
    return hwnd;
}

bool InitDirectX(HWND hwnd, int width, int height, OverlayData* pOverlayData) {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &scd,
        &pOverlayData->swapChain, &pOverlayData->device, nullptr, &pOverlayData->deviceContext)))
        return false;

    ID3D11Texture2D* backBuffer;
    pOverlayData->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    pOverlayData->device->CreateRenderTargetView(backBuffer, nullptr, &pOverlayData->renderTargetView);
    backBuffer->Release();

    // Simple passthrough shaders
    const char* vsCode =
        "struct VS_INPUT { float2 pos : POSITION; float4 col : COLOR; };"
        "struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };"
        "PS_INPUT VSMain(VS_INPUT input) { PS_INPUT output; output.pos = float4(input.pos, 0.0f, 1.0f); output.col = input.col; return output; }";

    const char* psCode =
        "struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };"
        "float4 PSMain(PS_INPUT input) : SV_TARGET { return input.col; }";

    ID3DBlob* vsBlob;
    ID3DBlob* psBlob;
    CompileShader(vsCode, "VSMain", "vs_4_0", &vsBlob);
    CompileShader(psCode, "PSMain", "ps_4_0", &psBlob);

    pOverlayData->device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &pOverlayData->vertexShader);
    pOverlayData->device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pOverlayData->pixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    pOverlayData->device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pOverlayData->inputLayout);

    vsBlob->Release();
    psBlob->Release();

    return true;
}

bool CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob) {
    ID3DBlob* error;
    HRESULT hr = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr, entryPoint, target, 0, 0, blob, &error);
    if (FAILED(hr)) {
        if (error) error->Release();
        return false;
    }
    return true;
}

void ClearOverlay(OverlayData* pOverlayData) {
    float clearColor[4] = { 0, 0, 0, 0 };
    pOverlayData->deviceContext->OMSetRenderTargets(1, &pOverlayData->renderTargetView, nullptr);
    pOverlayData->deviceContext->ClearRenderTargetView(pOverlayData->renderTargetView, clearColor);
}

void PresentOverlay(OverlayData* pOverlayData) {
    pOverlayData->swapChain->Present(1, 0);
}

struct Vertex {
    float x, y;
    float r, g, b, a;
};

void DrawLine(float x1, float y1, float x2, float y2, float color[4], OverlayData* pOverlayData) {
    float screenWidth = (float)GetSystemMetrics(SM_CXSCREEN);
    float screenHeight = (float)GetSystemMetrics(SM_CYSCREEN);

    float sx1 = (x1 / screenWidth) * 2.0f - 1.0f;
    float sy1 = 1.0f - (y1 / screenHeight) * 2.0f;
    float sx2 = (x2 / screenWidth) * 2.0f - 1.0f;
    float sy2 = 1.0f - (y2 / screenHeight) * 2.0f;

    Vertex vertices[2] = {
        { sx1, sy1, color[0], color[1], color[2], color[3] },
        { sx2, sy2, color[0], color[1], color[2], color[3] }
    };

    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    ID3D11Buffer* vertexBuffer;
    pOverlayData->device->CreateBuffer(&bd, &initData, &vertexBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pOverlayData->deviceContext->IASetInputLayout(pOverlayData->inputLayout);
    pOverlayData->deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    pOverlayData->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    pOverlayData->deviceContext->VSSetShader(pOverlayData->vertexShader, nullptr, 0);
    pOverlayData->deviceContext->PSSetShader(pOverlayData->pixelShader, nullptr, 0);
    pOverlayData->deviceContext->Draw(2, 0);

    vertexBuffer->Release();
}

void DrawCircle(float cx, float cy, float radius, float color[4], OverlayData* pOverlayData) {
    const int segments = 64;
    std::vector<Vertex> vertices;

    float screenWidth = (float)GetSystemMetrics(SM_CXSCREEN);
    float screenHeight = (float)GetSystemMetrics(SM_CYSCREEN);

    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * 3.14159265f;
        float x = cx + radius * cosf(theta);
        float y = cy + radius * sinf(theta);

        float sx = (x / screenWidth) * 2.0f - 1.0f;
        float sy = 1.0f - (y / screenHeight) * 2.0f;

        vertices.push_back({ sx, sy, color[0], color[1], color[2], color[3] });
    }

    D3D11_BUFFER_DESC bd = { UINT(vertices.size() * sizeof(Vertex)), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE };
    D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
    ID3D11Buffer* vertexBuffer;
    pOverlayData->device->CreateBuffer(&bd, &initData, &vertexBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    pOverlayData->deviceContext->IASetInputLayout(pOverlayData->inputLayout);
    pOverlayData->deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    pOverlayData->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    pOverlayData->deviceContext->VSSetShader(pOverlayData->vertexShader, nullptr, 0);
    pOverlayData->deviceContext->PSSetShader(pOverlayData->pixelShader, nullptr, 0);
    pOverlayData->deviceContext->Draw((UINT)vertices.size(), 0);

    vertexBuffer->Release();
}

void CleanupOverlay(OverlayData* pOverlayData) {
    if (pOverlayData->vertexBuffer) pOverlayData->vertexBuffer->Release();
    if (pOverlayData->inputLayout) pOverlayData->inputLayout->Release();
    if (pOverlayData->vertexShader) pOverlayData->vertexShader->Release();
    if (pOverlayData->pixelShader) pOverlayData->pixelShader->Release();
    if (pOverlayData->renderTargetView) pOverlayData->renderTargetView->Release();
    if (pOverlayData->swapChain) pOverlayData->swapChain->Release();
    if (pOverlayData->deviceContext) pOverlayData->deviceContext->Release();
    if (pOverlayData->device) pOverlayData->device->Release();
}
