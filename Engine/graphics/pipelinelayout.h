#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;

class PipelineLayout final {
  public:
    PipelineLayout(PipelineLayout&& other)=default;
    PipelineLayout& operator = (PipelineLayout&& other)=default;

    size_t sizeOfBuffer(size_t layoutBind, size_t arraylen = 0) const;

  private:
    PipelineLayout()=default;
    PipelineLayout(Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& lay);
    Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*> impl;

  friend class Device;
  friend class RenderPipeline;
  friend class ComputePipeline;
  };

}
