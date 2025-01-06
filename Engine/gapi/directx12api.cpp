#include "directx12api.h"

#include "directx11/dxdevice.h"
#include "directx11/dxbuffer.h"
#include "directx11/dxtexture.h"
#include "directx11/dxshader.h"
#include "directx11/dxpipeline.h"
#include "directx11/dxpipelinelay.h"
#include "directx11/dxcommandbuffer.h"
#include "directx11/dxswapchain.h"
#include "directx11/dxfence.h"
#include "directx11/dxdescriptorarray.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

struct DirectX12Api::Impl {
  Impl(bool validation) {
    CoInitialize(nullptr);
    d3d11_dll = LoadLibraryA("d3d11.dll");
     initApi(dllApi);

    if(validation && dllApi.D3D11CreateDevice!=nullptr) {
      
        // DXGI_ERROR_SDK_COMPONENT_MISSING
        Log::d("DirectX11Api: no validation layers available");
      }

    }

  ~Impl(){
    }

  std::vector<Props> devices() {
      std::vector<Props> d;
      if (dllApi.D3D11CreateDevice == nullptr)
          return d;
      
    
      ComPtr<IDXGIAdapter1> adapter;
      ComPtr<IDXGIFactory1> factory;
      dxAssert(CreateDXGIFactory1(uuid<IDXGIFactory1>(), reinterpret_cast<void**>(&factory.get())));
    
      for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter.get()); ++i)
        {
          DXGI_ADAPTER_DESC1 desc = {};
          adapter->GetDesc1(&desc);
          if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
          if (std::wcscmp(desc.Description, L"Microsoft Basic Render Driver") == 0)
              continue;
    
          ComPtr<ID3D11Device> tmpDev;
          D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

          if (FAILED(dllApi.D3D11CreateDevice(adapter.get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,featureLevels,1,
                                             D3D11_SDK_VERSION, &tmpDev,nullptr,nullptr)))
              continue;

          DxDevice::DxProps props = {};
            DxDevice::getProp(desc, *tmpDev, props);
        d.push_back(props);
        }

    return d;
    }

  AbstractGraphicsApi::Device* createDevice(std::string_view gpuName) {
      if (dllApi.D3D11CreateDevice == nullptr)
          throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    
    
      ComPtr<IDXGIAdapter1> adapter;
      ComPtr<IDXGIFactory1> factory;
      dxAssert(CreateDXGIFactory1(uuid<IDXGIFactory1>(), reinterpret_cast<void**>(&factory.get())));
    
      for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter.get()); ++i)
        {
          DXGI_ADAPTER_DESC1 desc = {};
          adapter->GetDesc1(&desc);
          if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
          if (std::wcscmp(desc.Description, L"Microsoft Basic Render Driver") == 0)
              continue;
    
          ComPtr<ID3D11Device> tmpDev;
          D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

          if (FAILED(dllApi.D3D11CreateDevice(adapter.get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,featureLevels,1,
                                             D3D11_SDK_VERSION, &tmpDev,nullptr,nullptr)))
              continue;
            
          DxDevice::DxProps props = {};
            DxDevice::getProp(desc, *tmpDev, props);
          if (!gpuName.empty() && gpuName != props.name)
              continue;
          break;
        }

      if(adapter.get()==nullptr)
          throw std::system_error(Tempest::GraphicsErrc::NoDevice);
      return new DxDevice(*adapter, dllApi);
    }

    void initApi(DxDevice::ApiEntry& a) {
        if (d3d11_dll == nullptr)
            return;
        getProcAddress(d3d11_dll, a.D3D11CreateDevice, "D3D11CreateDevice");
    }

  template<class T>
  void getProcAddress(HMODULE module, T& t, const char* name) {
    if(module==nullptr) {
      t = nullptr;
      return;
      }
    t = reinterpret_cast<T>(GetProcAddress(module,name));
    }

  HMODULE         d3d11_dll = nullptr;
  DxDevice::ApiEntry  dllApi;
  };

DirectX12Api::DirectX12Api(ApiFlags f) {
  impl.reset(new Impl(ApiFlags::Validation==(f&ApiFlags::Validation)));
  }

DirectX12Api::~DirectX12Api(){
  }

std::vector<AbstractGraphicsApi::Props> DirectX12Api::devices() const {
  return impl->devices();
  }

AbstractGraphicsApi::Device* DirectX12Api::createDevice(std::string_view gpuName) {
  return impl->createDevice(gpuName);
  }

AbstractGraphicsApi::Swapchain* DirectX12Api::createSwapchain(SystemApi::Window* w, AbstractGraphicsApi::Device* d) {
    auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
    return new Detail::DxSwapchain(*dx,w);
}

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st,
                                                            Topology tp, const PipelineLay& ulayImpl,
                                                            const Shader*const*sh, size_t cnt) {
  auto* dx   = reinterpret_cast<Detail::DxDevice*>(d);
  auto& ul   = reinterpret_cast<const Detail::DxPipelineLay&>(ulayImpl);
  const Detail::DxShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::DxShader*>(sh[i]);
  return PPipeline(new Detail::DxPipeline(*dx,st,tp,ul,shader,cnt));
}

AbstractGraphicsApi::PCompPipeline DirectX12Api::createComputePipeline(AbstractGraphicsApi::Device* d,
                                                                       const AbstractGraphicsApi::PipelineLay& ulayImpl,
                                                                       AbstractGraphicsApi::Shader* shader) {
  auto*   dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto&   ul = reinterpret_cast<const Detail::DxPipelineLay&>(ulayImpl);

  return PCompPipeline(new Detail::DxCompPipeline(*dx,ul,*reinterpret_cast<Detail::DxShader*>(shader)));
}

AbstractGraphicsApi::PShader DirectX12Api::createShader(AbstractGraphicsApi::Device*,
                                                        const void* source, size_t src_size) {
  return PShader(new Detail::DxShader(source,src_size));
}

AbstractGraphicsApi::Fence* DirectX12Api::createFence(AbstractGraphicsApi::Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxFence(*dx);
}

AbstractGraphicsApi::PBuffer DirectX12Api::createBuffer(AbstractGraphicsApi::Device* d, const void* mem, size_t size,
                                                        MemUsage usage, BufferHeap flg) {
  DxDevice& dx = *reinterpret_cast<DxDevice*>(d);
  usage = usage|MemUsage::TransferSrc|MemUsage::TransferDst;

  if(flg==BufferHeap::Upload) {
    DxBuffer buf=dx.allocator.alloc(mem,size,usage,BufferHeap::Upload);
    return PBuffer(new DxBuffer(std::move(buf)));
    }

  DxBuffer                      base = dx.allocator.alloc(nullptr,size,usage,flg);
  Detail::DSharedPtr<DxBuffer*> buf(new Detail::DxBuffer(std::move(base)));
  if(mem==nullptr && (usage&MemUsage::Initialized)==MemUsage::Initialized) {
    buf.handler->fill(0x0,0,size);
    return PBuffer(buf.handler);
    }
  if(mem!=nullptr)
    buf.handler->update(mem,0,size);
  return PBuffer(buf.handler);
}

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device*, PipelineLay& layP) {
  Detail::DxPipelineLay& u = reinterpret_cast<Detail::DxPipelineLay&>(layP);
  return new DxDescriptorArray(u);
}

AbstractGraphicsApi::PPipelineLay DirectX12Api::createPipelineLayout(Device* d, const Shader*const* sh, size_t count) {
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  bool has_baseVertex_baseInstance = false;
  for(size_t i=0; i<count; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::DxShader*>(sh[i]);
    lay[i] = &s->lay;
    has_baseVertex_baseInstance |= s->has_baseVertex_baseInstance;
    }
  auto& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  return PPipelineLay(new Detail::DxPipelineLay(dx,lay,count,has_baseVertex_baseInstance));
}

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
    if(isCompressedFormat(frm))
        return createCompressedTexture(d,p,frm,mipCnt);

  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);
  DXGI_FORMAT       format = Detail::nativeFormat(frm);
  uint32_t          row    = p.w()*p.bpp();
  const uint32_t    pith   = ((row+255)/256)*256;
  Detail::DxBuffer  stage  = dx.allocator.alloc(nullptr,p.h()*pith,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  for(uint32_t y=0; y<p.h(); ++y) {
    auto px = reinterpret_cast<const uint8_t*>(p.data());
    stage.update(px+y*row, y*pith, row);
    }

  Detail::DSharedPtr<Buffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf);
  cmd->hold(pstage); // preserve stage buffer, until gpu side copy is finished

  cmd->copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
    cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
  if(mipCnt>1)
    cmd->generateMipmap(*pbuf.handler, p.w(), p.h(), mipCnt);
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
}

AbstractGraphicsApi::PTexture DirectX12Api::createCompressedTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);

  DXGI_FORMAT    format    = Detail::nativeFormat(frm);
    UINT           blockSize = UINT(Pixmap::blockSizeForFormat(frm));

  UINT   stageBufferSize = 0;
  uint32_t w = p.w(), h = p.h();

  for(uint32_t i=0; i<mipCnt; i++) {
      Size bsz = Pixmap::blockCount(frm, w, h);
    UINT pitch = alignTo(bsz.w * blockSize, 256);
    stageBufferSize += pitch * bsz.h;
      stageBufferSize = alignTo(stageBufferSize, 256);

    w = std::max<uint32_t>(1,w/2);
      h = std::max<uint32_t>(1,h/2);
    }

  Detail::DxBuffer  stage  = dx.allocator.alloc(nullptr,stageBufferSize,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);
  Detail::DSharedPtr<Buffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::DxTexture(std::move(buf)));

    reinterpret_cast<Detail::DxBuffer*>(pstage.handler)->uploadS3TC(reinterpret_cast<const uint8_t*>(p.data()), p.w(), p.h(), mipCnt, blockSize);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
    cmd->hold(pbuf);
    cmd->hold(pstage); // preserve stage buffer, until gpu side copy is finished

    stageBufferSize = 0;
      w = p.w();
      h = p.h();
    for(uint32_t i=0; i<mipCnt; i++) {
        UINT wBlk = (w+3)/4;
        UINT hBlk = (h + 3) / 4;
        cmd->copy(*pbuf.handler,w,h,i,*pstage.handler,stageBufferSize);
        UINT pitch = wBlk * blockSize;
        pitch = alignTo(pitch, 256);
        stageBufferSize += pitch * hBlk;
          stageBufferSize = alignTo(stageBufferSize, 256);

        w = std::max<uint32_t>(4,w/2);
        h = std::max<uint32_t>(4,h/2);
    }

    cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
}

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
    DxDevice& dx = *reinterpret_cast<DxDevice*>(d);
    DxTexture base = dx.allocator.alloc(w,h,0,mipCnt,frm,false);
    DSharedPtr<DxTexture*> pbuf(new DxTextureWithRT(dx,std::move(base)));
    return PTexture(pbuf.handler);
}

AbstractGraphicsApi::PTexture DirectX12Api::createStorage(AbstractGraphicsApi::Device* d, const uint32_t w, const uint32_t h,
                                                          uint32_t mipCnt, TextureFormat frm) {
    Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

    Detail::DxTexture buf = dx.allocator.alloc(w,h,0,mipCnt,frm,true);
    Detail::DSharedPtr<Texture*> pbuf(new Detail::DxTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf);
  cmd->fill(*pbuf.handler,0);
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
}

AbstractGraphicsApi::PTexture DirectX12Api::createStorage(Device* d, const uint32_t w, const uint32_t h, const uint32_t depth,
                                                          uint32_t mipCnt, TextureFormat frm) {
    Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

    Detail::DxTexture buf = dx.allocator.alloc(w, h, depth, mipCnt, frm, true);
    Detail::DSharedPtr<Texture*> pbuf(new Detail::DxTexture(std::move(buf)));

    auto cmd = dx.dataMgr().get();
    cmd->begin();
    cmd->hold(pbuf);
    cmd->fill(*pbuf.handler,0);
  cmd->end();
    dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
}

AbstractGraphicsApi::AccelerationStructure* DirectX12Api::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
    return nullptr;
}

AbstractGraphicsApi::AccelerationStructure* DirectX12Api::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t geomSize) {
    return nullptr;
}

void DirectX12Api::readPixels(Device* d, Pixmap& out, const PTexture t,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  Detail::DxDevice&  dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxTexture& tx = *reinterpret_cast<Detail::DxTexture*>(t.handler);

  size_t           bpb    = Pixmap::blockSizeForFormat(frm);
    Size             bsz    = Pixmap::blockCount(frm,w,h);

  uint32_t         row    = bsz.w*uint32_t(bpb);
  const uint32_t   pith   = ((row+255)/256)*256;
  Detail::DxBuffer stage  = dx.allocator.alloc(nullptr, bsz.h*pith, MemUsage::TransferDst, BufferHeap::Readback);
    ResourceAccess   defLay = storageImg ? (ResourceAccess::UavReadGr | ResourceAccess::UavReadComp) : ResourceAccess::Sampler;
    if(isDepthFormat(frm))
        defLay = ResourceAccess::DepthReadOnly;

  auto cmd = dx.dataMgr().get();
  cmd->begin();
    cmd->barrier(tx, defLay, ResourceAccess::TransferSrc, mip);
  cmd->copyNative(stage,0, tx,w,h,mip);
    cmd->barrier(tx, ResourceAccess::TransferSrc, defLay, mip);
  cmd->end();
  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,frm);
  for(int32_t i=0; i<bsz.h; ++i)
    stage.read(reinterpret_cast<uint8_t*>(out.data())+i*bsz.w*bpb, i*pith, bsz.w*bpb);
}

void DirectX12Api::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  buf->read(out,0,size);
}

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
}

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw) {
  Detail::DxDevice&    dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxSwapchain& sx = *reinterpret_cast<Detail::DxSwapchain*>(sw);
  sx.queuePresent();
}

void DirectX12Api::submit(AbstractGraphicsApi::Device* d,
                          AbstractGraphicsApi::CommandBuffer* cx,
                          AbstractGraphicsApi::Fence* doneCpu) {
  auto& dx   = *reinterpret_cast<Detail::DxDevice*>(d);
  auto& sync = *reinterpret_cast<Detail::DxFence*>(doneCpu);
  auto& cmd  = *reinterpret_cast<Detail::DxCommandBuffer*>(cx);

  dx.submit(cmd, &sync);
}

void DirectX12Api::getCaps(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Props& caps) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  caps = dx.props;
}

#endif