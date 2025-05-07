#include "dx_capture.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

static ComPtr<ID3D11Device> d3dDevice;
static ComPtr<ID3D11DeviceContext> d3dContext;
static ComPtr<IDXGIOutputDuplication> deskDupl;

bool initializeDxCapture() {
    ComPtr<IDXGIFactory1> dxgiFactory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgiFactory))) return false;

    ComPtr<IDXGIAdapter1> adapter;
    dxgiFactory->EnumAdapters1(0, &adapter);

    D3D_FEATURE_LEVEL level;
    if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice, &level, &d3dContext))) return false;

    ComPtr<IDXGIOutput> output;
    adapter->EnumOutputs(0, &output);

    ComPtr<IDXGIOutput1> output1;
    output.As(&output1);

    if (FAILED(output1->DuplicateOutput(d3dDevice.Get(), &deskDupl))) return false;

    return true;
}

cv::Mat captureDxFrame() {
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    ComPtr<IDXGIResource> desktopResource;

    if (FAILED(deskDupl->AcquireNextFrame(500, &frameInfo, &desktopResource)))
        return cv::Mat();  // Timeout or failure

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(desktopResource.As(&tex))) return cv::Mat();

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC cpuDesc = desc;
    cpuDesc.Usage = D3D11_USAGE_STAGING;
    cpuDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    cpuDesc.BindFlags = 0;
    cpuDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> cpuTex;
    if (FAILED(d3dDevice->CreateTexture2D(&cpuDesc, nullptr, &cpuTex))) return cv::Mat();

    d3dContext->CopyResource(cpuTex.Get(), tex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(d3dContext->Map(cpuTex.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return cv::Mat();

    cv::Mat img(desc.Height, desc.Width, CV_8UC4, mapped.pData, mapped.RowPitch);
    cv::Mat bgr;
    cv::cvtColor(img.clone(), bgr, cv::COLOR_BGRA2BGR);

    d3dContext->Unmap(cpuTex.Get(), 0);
    deskDupl->ReleaseFrame();

    return bgr;
}

void releaseDxCapture() {
    deskDupl.Reset();
    d3dContext.Reset();
    d3dDevice.Reset();
}
