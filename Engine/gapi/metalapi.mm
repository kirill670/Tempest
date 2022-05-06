#include "metalapi.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/Log>
#include <Tempest/Pixmap>

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"
#include "gapi/metal/mtcommandbuffer.h"
#include "gapi/metal/mttexture.h"
#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtdescriptorarray.h"
#include "gapi/metal/mtsync.h"
#include "gapi/metal/mtswapchain.h"
#include "gapi/metal/mtaccelerationstructure.h"

#import  <Metal/MTLDevice.h>
#import  <Metal/MTLCommandQueue.h>
#import  <AppKit/AppKit.h>

using namespace Tempest;
using namespace Tempest::Detail;

MetalApi::MetalApi(ApiFlags f) {
  if((f & ApiFlags::Validation)==ApiFlags::Validation) {
    setenv("METAL_DEVICE_WRAPPER_TYPE","1",1);
    setenv("METAL_DEBUG_ERROR_MODE",   "5",0);
    setenv("METAL_ERROR_MODE",         "5",0);
    validation = true;
    }
  }

MetalApi::~MetalApi() {
  }

std::vector<AbstractGraphicsApi::Props> MetalApi::devices() const {
  NSArray<id<MTLDevice>>* dev = MTLCopyAllDevices();
  try {
    std::vector<AbstractGraphicsApi::Props> p(dev.count);
    for(size_t i=0; i<p.size(); ++i)
      MtDevice::deductProps(p[i],dev[i]);
    return p;
    }
  catch(...) {
    [dev release];
    throw;
    }
  }

AbstractGraphicsApi::Device* MetalApi::createDevice(const char *gpuName) {
  return new MtDevice(gpuName,validation);
  }

void MetalApi::destroy(AbstractGraphicsApi::Device *d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w,
                                                          AbstractGraphicsApi::Device* d) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);

  NSObject* obj = reinterpret_cast<NSObject*>(w);
  if([obj isKindOfClass : [NSWindow class]])
    return new MtSwapchain(dev,reinterpret_cast<NSWindow*>(w));
  return nullptr;
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st, size_t stride,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::PipelineLay &ulayImpl,
                                                        const AbstractGraphicsApi::Shader*const* sh,
                                                        size_t cnt) {
  auto& dx  = *reinterpret_cast<MtDevice*>(d);
  auto& lay = reinterpret_cast<const MtPipelineLay&>(ulayImpl);
  const Detail::MtShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::MtShader*>(sh[i]);
  return PPipeline(new MtPipeline(dx,tp,st,stride,lay, shader,cnt));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d,
                                                                   const AbstractGraphicsApi::PipelineLay& ulayImpl,
                                                                   AbstractGraphicsApi::Shader *cs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<const MtShader*>(cs);
  auto& lay = reinterpret_cast<const MtPipelineLay&>(ulayImpl);
  return PCompPipeline(new MtCompPipeline(dx,lay,cx));
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return PShader(new MtShader(dx,source,src_size));
  }

AbstractGraphicsApi::Fence *MetalApi::createFence(AbstractGraphicsApi::Device*) {
  return new MtSync();
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                    const void *mem, size_t count, size_t sz, size_t alignedSz,
                                                    MemUsage /*usage*/, BufferHeap flg) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  MTLResourceOptions opt = 0;
  // https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html#//apple_ref/doc/uid/TP40016642-CH17-SW1
  switch(flg) {
    case BufferHeap::Device:
      opt |= MTLResourceStorageModePrivate;
      break;
    case BufferHeap::Upload:
#ifndef __IOS__
      if(count*alignedSz>PAGE_SIZE)
        opt |= MTLResourceStorageModeManaged; else
        opt |= MTLResourceStorageModeShared;
#else
      opt |= MTLResourceStorageModeShared;
#endif
      opt |= MTLResourceCPUCacheModeWriteCombined;
      break;
    case BufferHeap::Readback:
      opt |= MTLResourceStorageModeManaged;
      opt |= MTLResourceCPUCacheModeDefaultCache;
      break;
    }

  return PBuffer(new MtBuffer(dx,mem,count,sz,alignedSz,opt));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  @autoreleasepool {
    auto& dev = *reinterpret_cast<MtDevice*>(d);
    return PTexture(new MtTexture(dev,p,mips,frm));
    }
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  @autoreleasepool {
    auto& dev = *reinterpret_cast<MtDevice*>(d);
    return PTexture(new MtTexture(dev,w,h,mips,frm,false));
    }
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  @autoreleasepool {
    auto& dev = *reinterpret_cast<MtDevice*>(d);
    return PTexture(new MtTexture(dev,w,h,mips,frm,true));
    }
  }

AbstractGraphicsApi::AccelerationStructure*
MetalApi::createBottomAccelerationStruct(Device* d,
                                         Buffer* vbo, size_t vboSz, size_t stride,
                                         Buffer* ibo, size_t iboSz, size_t ioffset, Detail::IndexClass icls) {
  @autoreleasepool {
    auto& dev = *reinterpret_cast<MtDevice*>(d);
    auto& vx  = *reinterpret_cast<MtBuffer*>(vbo);
    auto& ix  = *reinterpret_cast<MtBuffer*>(ibo);
    return new MtAccelerationStructure(dev,vx,vboSz,stride, ix,iboSz,ioffset,icls);
    }
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  @autoreleasepool {
    auto& dev = *reinterpret_cast<MtDevice*>(d);
    return new MtTopAccelerationStructure(dev,inst,as,size);
    }
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device*,
                          Pixmap& out, const AbstractGraphicsApi::PTexture t,
                          TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto& tx = *reinterpret_cast<MtTexture*>(t.handler);
  tx.readPixels(out,frm,w,h,mip);
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::Desc *MetalApi::createDescriptors(AbstractGraphicsApi::Device* d,
                                                       AbstractGraphicsApi::PipelineLay& layP) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  auto& lay = reinterpret_cast<MtPipelineLay&>(layP);
  return new MtDescriptorArray(dev,lay);
  }

AbstractGraphicsApi::PPipelineLay MetalApi::createPipelineLayout(AbstractGraphicsApi::Device*,
                                                                 const AbstractGraphicsApi::Shader*const*sh,
                                                                 size_t cnt) {
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const MtShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PPipelineLay(new MtPipelineLay(lay,cnt));
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain *sw) {
  auto& s   = *reinterpret_cast<MtSwapchain*>(sw);
  s.present();
  }

void MetalApi::submit(AbstractGraphicsApi::Device *d,
                      AbstractGraphicsApi::CommandBuffer *cmd,
                      AbstractGraphicsApi::Fence *doneCpu) {
  this->submit(d,&cmd,1,doneCpu);
  }

void MetalApi::submit(AbstractGraphicsApi::Device*,
                      AbstractGraphicsApi::CommandBuffer **pcmd, size_t count,
                      AbstractGraphicsApi::Fence *doneCpu) {
  auto& fence = *reinterpret_cast<MtSync*>(doneCpu);
  fence.signal();
  for(size_t i=0; i<count; ++i) {
    auto& cx = *reinterpret_cast<MtCommandBuffer*>(pcmd[i]);
    id<MTLCommandBuffer> cmd = cx.impl;

    [cmd addCompletedHandler:^(id<MTLCommandBuffer> c) {
      MTLCommandBufferStatus s = c.status;
      if(s==MTLCommandBufferStatusNotEnqueued ||
         s==MTLCommandBufferStatusEnqueued ||
         s==MTLCommandBufferStatusCommitted ||
         s==MTLCommandBufferStatusScheduled)
        return;

      if(s==MTLCommandBufferStatusCompleted)
        fence.reset(); else
        fence.reset(s);
      }];
    [cmd commit];
    }
  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  caps = dx.prop;
  }
