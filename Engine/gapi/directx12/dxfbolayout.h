#if defined(TEMPEST_BUILD_DIRECTX11)
#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>

namespace Tempest {
namespace Detail {

struct DxFboLayout {
  UINT        NumRenderTargets = 0;
  DXGI_FORMAT RTVFormats[MaxFramebufferAttachments] = {};
  DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;

  bool        equals(const DxFboLayout& l) const;
  };

}
}


#endif
