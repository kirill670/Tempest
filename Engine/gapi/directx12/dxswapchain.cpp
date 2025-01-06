#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxswapchain.h"
#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxSwapchain::DxSwapchain(DxDevice& dev, IDXGIFactory4& dxgi, SystemApi::Window* hwnd)
  :dev(dev), fence(dev) {
  auto& device = *dev.device;

  auto rect = SystemApi::windowClientRect(hwnd);
  imgW = uint32_t(rect.w);
  imgH = uint32_t(rect.h);
  imgCount = 3;// TODO

  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width  = imgW;
  sd.Height = imgH;
  sd.Format = frm;
  sd.Stereo = false;

  sd.SampleDesc.Count = 1;
  sd.BufferUsage  = 0;
  sd.BufferCount  = imgCount;
  sd.Scaling      = DXGI_SCALING_STRETCH;
  sd.SwapEffect   = 0;
  sd.AlphaMode    = DXGI_ALPHA_MODE_UNSPECIFIED;
  /*
  sd.Flags        = 0;
  */

  dxAssert(dxgi.CreateSwapChainForHwnd(
      dev.cmdQueue.get(),        // Swap chain needs the queue so that it can force a flush on it.
      HWND(hwnd),
      &sd,
      nullptr,
      nullptr,
      &swapChain.get()
      ));

  dxAssert(dxgi.MakeWindowAssociation(HWND(hwnd), DXGI_MWA_NO_ALT_ENTER));
  dxAssert(swapChain->QueryInterface(uuid<IDXGISwapChain3>(), reinterpret_cast<void**>(&impl)));

  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = UINT_RTV;
  rtvHeapDesc.NumDescriptors = imgCount;
  rtvHeapDesc.Flags          = 0;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<void>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views  .reset(new ComPtr<ID3D11Resource>[imgCount]);
  handles.reset(new void*[imgCount]);

  initImages();
  }

DxSwapchain::~DxSwapchain() {
  }

void DxSwapchain::reset() {
  fence.waitValue(frameCounter); //wait for all pending frame to be finizhed

  for(uint32_t i=0; i<imgCount; ++i) {
    views[i] = nullptr;
    }

  HWND hwnd={};
  dxAssert(impl->GetHwnd(&hwnd));
  auto rect = SystemApi::windowClientRect(reinterpret_cast<SystemApi::Window*>(hwnd));
  imgW = uint32_t(rect.w);
  imgH = uint32_t(rect.h);

  dxAssert(impl->ResizeBuffers(imgCount, imgW, imgH, frm, 0));
  initImages();
  }

uint32_t DxSwapchain::currentBackBufferIndex() {
  return impl->GetCurrentBackBufferIndex();
  }

void DxSwapchain::queuePresent() {
  static int vsunc = 1;
  dxAssert(impl->Present(vsunc, 0));

  ++frameCounter;
  dev.cmdQueue->Signal(fence.impl.get(),frameCounter);
  }

void DxSwapchain::initImages() {
  auto& device  = *dev.device;
  auto  eltSize = device.GetDescriptorHandleIncrementSize(UINT_RTV);

  void* rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  for(uint32_t i=0; i<imgCount; i++) {
    dxAssert(swapChain->GetBuffer(i, uuid<ID3D11Resource>(), reinterpret_cast<void**>(&views[i])));
    device.CreateRenderTargetView(views[i].get(), nullptr, rtvHandle);
    handles[i] = rtvHandle;
    rtvHandle.ptr += eltSize;
    }
  }


