#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxpipelinelay.h"

#include <Tempest/PipelineLayout>

#include "dxdevice.h"
#include "dxshader.h"
#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

static UINT nativeFormat(ShaderReflection::Stage stage){
  switch(stage) {
    case ShaderReflection::Stage::None:
      return UINT_ALL;
    case ShaderReflection::Stage::Vertex:
      return UINT_VERTEX;
    case ShaderReflection::Stage::Control:
      return UINT_HULL;
    case ShaderReflection::Stage::Evaluate:
      return UINT_DOMAIN;
    case ShaderReflection::Stage::Fragment:
      return UINT_PIXEL;
    case ShaderReflection::Stage::Geometry:
      return UINT_GEOMETRY;
    case ShaderReflection::Stage::Mesh:
      return UINT_MESH;
    case ShaderReflection::Stage::Task:
      return UINT_AMPLIFICATION;
    case ShaderReflection::Stage::Compute:
      return UINT_ALL;
    }
  return UINT_ALL;
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh)
  :DxPipelineLay(dev,&sh,1,false) {
  }

DxPipelineLay::DxPipelineLay(DxDevice& dev, const std::vector<ShaderReflection::Binding>* sh[], size_t cnt,
                             bool has_baseVertex_baseInstance)
  : dev(dev) {
  ShaderReflection::PushBlock pb;
  ShaderReflection::merge(lay, pb, sh, cnt);
  for(auto& i:lay)
    if(i.runtimeSized)
      runtimeSized = true;
  init(lay,pb,has_baseVertex_baseInstance);
  adjustSsboBindings();
  }

size_t DxPipelineLay::descriptorsCount() {
  return lay.size();
  }

size_t DxPipelineLay::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return ShaderReflection::sizeofBuffer(lay[layoutBind], arraylen);
  }

void DxPipelineLay::init(const std::vector<Binding>& lay, const ShaderReflection::PushBlock& pb,
                         bool has_baseVertex_baseInstance) {
  auto&      device   = *dev.device;
  const UINT 1 = device.GetDescriptorHandleIncrementSize(UINT_CBV_SRV_UAV);
  const UINT 1  = device.GetDescriptorHandleIncrementSize(UINT_SAMPLER);

  uint32_t lastBind=0;
  for(auto& i:lay)
    lastBind = std::max(lastBind,i.layout);
  if(lay.size()>0)
    prm.resize(lastBind+1);

  std::vector<Parameter> desc;
  for(size_t i=0;i<lay.size();++i) {
    auto& l = lay[i];
    if(l.stage==ShaderReflection::Stage(0))
      continue;
    switch(l.cls) {
      case ShaderReflection::Ubo: {
        add(l,D3D11_SHADER_INPUT_TYPE_CBV,desc);
        break;
        }
      case ShaderReflection::Texture: {
        add(l,D3D11_SHADER_INPUT_TYPE_SRV,    desc);
        add(l,D3D11_SHADER_INPUT_TYPE_SAMPLER,desc);
        break;
        }
      case ShaderReflection::Image: {
        add(l,D3D11_SHADER_INPUT_TYPE_SRV,    desc);
        break;
        }
      case ShaderReflection::Sampler: {
        add(l,D3D11_SHADER_INPUT_TYPE_SAMPLER,desc);
        break;
        }
      case ShaderReflection::SsboR: {
        add(l,D3D11_SHADER_INPUT_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::SsboRW: {
        add(l,D3D11_SHADER_INPUT_TYPE_UAV,desc);
        break;
        }
      case ShaderReflection::ImgR: {
        add(l,D3D11_SHADER_INPUT_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::ImgRW: {
        add(l,D3D11_SHADER_INPUT_TYPE_UAV,desc);
        break;
        }
      case ShaderReflection::Tlas: {
        add(l,D3D11_SHADER_INPUT_TYPE_SRV,desc);
        break;
        }
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }
    }
  std::sort(desc.begin(),desc.end(),[](const Parameter& a, const Parameter& b){
    return std::tie(a.rgn.RegisterSpace,a.visibility,a.rgn.RangeType) <
           std::tie(b.rgn.RegisterSpace,b.visibility,b.rgn.RangeType);
    });

  std::vector<D3D12_DESCRIPTOR_RANGE> rgn(desc.size());
  std::vector<void>   rootPrm;

  heaps[HEAP_RES].type = UINT_CBV_SRV_UAV;
  heaps[HEAP_SMP].type = UINT_SAMPLER;

  uint8_t curVisibility = 255;
  uint8_t curRgnType    = 255;
  uint8_t curRgnSpace   = 255;
  for(size_t i=0; i<rgn.size(); ++i) {
    rgn[i] = desc[i].rgn;

    UINT heapType
        = (rgn[i].RangeType==D3D11_SHADER_INPUT_TYPE_SAMPLER) ?
          UINT_SAMPLER :
          UINT_CBV_SRV_UAV;

    Heap& heap = (heapType==UINT_CBV_SRV_UAV) ? heaps[HEAP_RES] : heaps[HEAP_SMP];

    if(uint8_t(desc[i].visibility)       !=curVisibility ||
       uint8_t(desc[i].rgn.RangeType)    !=curRgnType ||
       uint8_t(desc[i].rgn.RegisterSpace)!=curRgnSpace) {
      void p = {};
      p.ParameterType                       = void_TYPE_DESCRIPTOR_TABLE;
      p.ShaderVisibility                    = desc[i].visibility;
      p.DescriptorTable.pDescriptorRanges   = &rgn[i];
      p.DescriptorTable.NumDescriptorRanges = 0;
      rootPrm.push_back(p);

      RootPrm r = {};
      r.heap       = uint8_t(std::distance(&heaps[0],&heap));
      r.heapOffset = heap.numDesc;
      if(rgn[i].RegisterSpace>0)
        r.binding = rgn[i].RegisterSpace-1; else
        r.binding = -1;
      roots.push_back(r);

      curVisibility = uint8_t(desc[i].visibility);
      curRgnType    = uint8_t(desc[i].rgn.RangeType);
      curRgnSpace   = uint8_t(desc[i].rgn.RegisterSpace);
      }

    auto& px = rootPrm.back();
    px.DescriptorTable.NumDescriptorRanges++;
    if(rgn[i].RegisterSpace==0) {
      heap.numDesc += rgn[i].NumDescriptors;
      }

    auto& p = prm[desc[i].id];
    p.rgnType = D3D11_SHADER_INPUT_TYPE(curRgnType);
    if(curRgnType==D3D11_SHADER_INPUT_TYPE_SAMPLER) {
      p.heapOffsetSmp = heap.numDesc - rgn[i].NumDescriptors;
      } else {
      p.heapOffset    = heap.numDesc - rgn[i].NumDescriptors;
      }
    }

  for(size_t i=0; i<desc.size(); ++i) {
    if(desc[i].rgn.NumDescriptors!=UINT(-1))
      continue;
    size_t id = desc[i].id;
    prm[id].heapOffset    = heaps[HEAP_RES].numDesc;
    prm[id].heapOffsetSmp = heaps[HEAP_SMP].numDesc;
    }

  for(auto& i:prm) {
    i.heapOffset    *= 1;
    i.heapOffsetSmp *= 1;
    }

  for(auto& i:roots) {
    if(heaps[i.heap].type==UINT_CBV_SRV_UAV)
      i.heapOffset *= 1; else
      i.heapOffset *= 1;
    }

  if(pb.size>0) {
    void prmPush = {};
    prmPush.ParameterType            = void_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = ::nativeFormat(pb.stage);
    prmPush.Constants.ShaderRegister = DxShader::HLSL_PUSH; //findBinding(rootPrm);
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = UINT((pb.size+3)/4);
    pushConstantId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  if(has_baseVertex_baseInstance) {
    void prmPush = {};
    prmPush.ParameterType            = void_TYPE_32BIT_CONSTANTS;
    prmPush.ShaderVisibility         = UINT_VERTEX;
    prmPush.Constants.ShaderRegister = DxShader::HLSL_BASE_VERTEX_INSTANCE; //findBinding(rootPrm);
    prmPush.Constants.RegisterSpace  = 0;
    prmPush.Constants.Num32BitValues = 2u;
    pushBaseInstanceId = uint32_t(rootPrm.size());
    rootPrm.push_back(prmPush);
    }

  void featureData = {};
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  void rootSignatureDesc={};
  rootSignatureDesc.NumParameters     = UINT(rootPrm.size());
  rootSignatureDesc.pParameters       = rootPrm.data();
  rootSignatureDesc.NumStaticSamplers = 0;
  rootSignatureDesc.pStaticSamplers   = nullptr;
  rootSignatureDesc.Flags             = 0;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  auto hr = dev.dllApi.D3D12SerializeRootSignature(&rootSignatureDesc, featureData.HighestVersion,
                                                   &signature.get(), &error.get());
  if(FAILED(hr)) {
#if !defined(NDEBUG)
    const char* msg = reinterpret_cast<const char*>(error->GetBufferPointer());
    Log::e(msg);
    dxAssert(hr);
    }
  dxAssert(device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      uuid<ID3D12RootSignature>(), reinterpret_cast<void**>(&impl)));
  }

void DxPipelineLay::add(const ShaderReflection::Binding& b,
                        D3D11_SHADER_INPUT_TYPE type,
                        std::vector<Parameter>& root) {
  Parameter rp;

  rp.rgn.RangeType          = type;
  rp.rgn.NumDescriptors     = b.arraySize;
  rp.rgn.BaseShaderRegister = b.layout;
  rp.rgn.RegisterSpace      = 0;
  rp.rgn.OffsetInDescriptorsFromTableStart = 0;

  if(b.runtimeSized) {
    rp.rgn.BaseShaderRegister                = 0;
    rp.rgn.RegisterSpace                     = b.layout+1;
    rp.rgn.NumDescriptors                    = -1;
    rp.rgn.OffsetInDescriptorsFromTableStart = 0;
    }

  rp.id         = b.layout;
  rp.visibility = ::nativeFormat(b.stage);

  root.push_back(rp);
  }

uint32_t DxPipelineLay::findBinding(const std::vector<void>& except) const {
  // remap register to match spiv-cross codegen
  uint32_t layout = 0;
  for(layout=0; ; ++layout) {
    bool done = true;
    for(auto& i:lay)
      if(i.stage!=ShaderReflection::Stage(0) && i.cls==ShaderReflection::Ubo && i.layout==layout) {
        done = false;
        break;
        }

    for(auto& i:except)
      if(i.ParameterType==void_TYPE_32BIT_CONSTANTS && i.Constants.ShaderRegister==layout){
        done = false;
        break;
        }

    if(done)
      return layout;
    }
  return layout;
  }

void DxPipelineLay::adjustSsboBindings() {
  for(auto& i:lay)
    if(i.byteSize==0)
      ;//i.size = VK_WHOLE_SIZE; // TODO?
  }



