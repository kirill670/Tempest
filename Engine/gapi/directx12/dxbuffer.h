#if defined(TEMPEST_BUILD_DIRECTX11)
#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "comptr.h"
#include "dxallocator.h"

namespace Tempest {
namespace Detail {

class DxDevice;

class DxBuffer : public AbstractGraphicsApi::Buffer {
  public:
    DxBuffer() = default;
    DxBuffer(DxDevice* dev, UINT sizeInBytes, UINT appSize);
    DxBuffer(DxBuffer&& other);
    ~DxBuffer();

    DxBuffer& operator=(DxBuffer&& other);

    void  fill  (uint32_t    data, size_t off, size_t size);
    void  update(const void* data, size_t off, size_t size) override;
    void  read  (      void* data, size_t off, size_t size) override;

    void  uploadS3TC(const uint8_t* d, uint32_t w, uint32_t h, uint32_t mip, UINT blockSize);

    DxDevice*               dev = nullptr;
    DxAllocator::Allocation page={};

    ComPtr<ID3D11Resource>  impl;
    NonUniqResId            nonUniqId   = NonUniqResId::I_None;
    UINT                    sizeInBytes = 0;
    UINT                    appSize     = 0;

  protected:
    void  updateByStaging(DxBuffer* stage, const void* data, size_t offDst, size_t offSrc, size_t size);
    void  updateByMapped (DxBuffer& stage, const void* data, size_t off, size_t size);

    void  fillByStaging  (DxBuffer* stage, uint32_t data, size_t offDst, size_t offSrc, size_t size);
    void  fillByMapped   (DxBuffer& stage, uint32_t data, size_t off, size_t size);

    void  readFromStaging(DxBuffer& stage, void* data, size_t off, size_t size);
    void  readFromMapped (DxBuffer& stage, void* data, size_t off, size_t size);
  };

}
}


#endif
