#if defined(TEMPEST_BUILD_DIRECTX11)
#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>

#include "gapi/deviceallocator.h"

namespace Tempest {
namespace Detail {

class DxDevice;
class DxBuffer;
class DxTexture;

class DxAllocator {
  public:
    DxAllocator();

    struct Provider {
      using DeviceMemory = ID3D12Heap*;
      ~Provider();

      DxDevice*    device     = nullptr;
      DeviceMemory last       = nullptr;
      size_t       lastSize   = 0;
      uint32_t     lastTypeId = 0;

      DeviceMemory alloc(size_t size, uint32_t typeId);
      void         free(DeviceMemory m, size_t size, uint32_t typeId);
      };

    void      setDevice(DxDevice& device);

    using Allocation=typename Tempest::Detail::DeviceAllocator<Provider>::Allocation;

    DxBuffer  alloc(const void *mem,  size_t size, MemUsage usage, BufferHeap bufFlg);
    DxTexture alloc(const Pixmap &pm, uint32_t mip, DXGI_FORMAT format);
    DxTexture alloc(const uint32_t w, const uint32_t h, const uint32_t d, const uint32_t mip, TextureFormat frm, bool imageStore);
    void      free (Allocation& page);

  private:
    struct MemRequirements{};
    Allocation allocMemory(const MemRequirements& rq, const uint32_t heapId, const uint32_t typeId, bool hostVisible);
    bool       commit(ID3D12Heap* dev, std::mutex& mmapSync, ID3D11Resource*& dest,
                      const D3D11_BUFFER_DESC& resDesc, UINT state, size_t offset,
                      const void *mem, size_t size);

    DxDevice*       owner  = nullptr;
    ID3D11Device*   device = nullptr;

    Provider                          provider;
    Detail::DeviceAllocator<Provider> allocator{provider};

    D3D11_BUFFER_DESC             buferHeap[3]={};
  };

}
}


#endif
