#if defined(TEMPEST_BUILD_DIRECTX11)
#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxDevice;

class DxTexture : public AbstractGraphicsApi::Texture {
  public:
    DxTexture();
    DxTexture(ComPtr<ID3D11Resource>&& b, DXGI_FORMAT frm, NonUniqResId nonUniqId,
              UINT mipCnt, UINT sliceCnt, bool is3D, bool isFilterable);
    DxTexture(DxTexture&& other);

    uint32_t mipCount() const override { return mipCnt; }

    UINT     bitCount() const;
    UINT     bytePerBlockCount() const;

    ComPtr<ID3D11Resource> impl;
    DXGI_FORMAT            format       = DXGI_FORMAT_UNKNOWN;
    NonUniqResId           nonUniqId    = NonUniqResId::I_None;
    UINT                   mipCnt       = 1;
    UINT                   sliceCnt     = 1;
    bool                   is3D         = false;
    bool                   isFilterable = false;
  };

class DxTextureWithRT : public DxTexture {
  public:
    DxTextureWithRT(DxDevice& dev, DxTexture&& base);

    ComPtr<void> heap;
    void*  handle;
    void*  handleR;
  };

}
}


#endif
