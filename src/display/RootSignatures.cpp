#include "RootSignatures.h"
#include "../utils/Logger.h"
#include <d3dx12.h>
#include <stdexcept>

bool RootSignatures::init(ID3D12Device* device) {
    if (!createNV12ToRGB(device))     return false;
    if (!createHDRTonemap(device))    return false;
    if (!createLanczos(device))       return false;
    if (!createChromaUpsample(device)) return false;
    if (!createFramePacing(device))   return false;
    LOG_INFO("RootSignatures: All {} root signatures created", (int)RS::Count);
    return true;
}

void RootSignatures::release() {
    for (auto& rs : m_rs) rs.Reset();
}

bool RootSignatures::finalise(ID3D12Device* device,
                               const D3D12_ROOT_SIGNATURE_DESC& desc,
                               RS slot) {
    ComPtr<ID3DBlob> serialised, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &serialised, &error);
    if (FAILED(hr)) {
        LOG_ERROR("RootSignatures: Serialize failed for slot {}: 0x{:X}", (int)slot, (uint32_t)hr);
        if (error) LOG_ERROR("{}", (char*)error->GetBufferPointer());
        return false;
    }
    hr = device->CreateRootSignature(0, serialised->GetBufferPointer(),
                                      serialised->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rs[(int)slot]));
    if (FAILED(hr)) {
        LOG_ERROR("RootSignatures: CreateRootSignature failed for slot {}: 0x{:X}",
                  (int)slot, (uint32_t)hr);
        return false;
    }
    return true;
}

bool RootSignatures::createNV12ToRGB(ID3D12Device* device) {
    // Slot 0: descriptor table (Y SRV + UV SRV)
    // Slot 1: root constant (colour space: 0=BT709, 1=BT2020)
    // Static sampler: linear clamp
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0, t1

    CD3DX12_ROOT_PARAMETER1 params[2];
    params[0].InitAsDescriptorTable(1, ranges, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL); // b0

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(2, params, 1, &sampler,
                  D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, err;
    D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &blob, &err);
    if (!blob) {
        // Fallback to v1.0
        D3D12_ROOT_SIGNATURE_DESC desc10 = {};
        D3D12_ROOT_PARAMETER p[2] = {};
        D3D12_DESCRIPTOR_RANGE r[1] = {{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0 }};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p[0].DescriptorTable = { 1, r };
        p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p[1].Constants = { 0, 0, 1 };
        p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC s = {};
        s.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.ShaderRegister = 0; s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        desc10 = { 2, p, 1, &s,
                   D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
        return finalise(device, desc10, RS::NV12ToRGB);
    }
    HRESULT hr = device->CreateRootSignature(0, blob->GetBufferPointer(),
        blob->GetBufferSize(), IID_PPV_ARGS(&m_rs[(int)RS::NV12ToRGB]));
    return SUCCEEDED(hr);
}

bool RootSignatures::createHDRTonemap(ID3D12Device* device) {
    D3D12_DESCRIPTOR_RANGE r = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0 };
    D3D12_ROOT_PARAMETER p[2] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].DescriptorTable = { 1, &r };
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[1].Constants = { 0, 0, 1 }; // peak luminance nits
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC s = {};
    s.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc = { 2, p, 1, &s,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    return finalise(device, desc, RS::HDRTonemap);
}

bool RootSignatures::createLanczos(ID3D12Device* device) {
    D3D12_DESCRIPTOR_RANGE r = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0 };
    D3D12_ROOT_PARAMETER p[2] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].DescriptorTable = { 1, &r };
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[1].Constants = { 0, 0, 4 }; // srcW, srcH, dstW, dstH
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC s = {};
    s.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc = { 2, p, 1, &s,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    return finalise(device, desc, RS::Lanczos);
}

bool RootSignatures::createChromaUpsample(ID3D12Device* device) {
    D3D12_DESCRIPTOR_RANGE r = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0 };
    D3D12_ROOT_PARAMETER p[2] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].DescriptorTable = { 1, &r };
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[1].Constants = { 0, 0, 2 };
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC s = {};
    s.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc = { 2, p, 1, &s,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    return finalise(device, desc, RS::ChromaUpsample);
}

bool RootSignatures::createFramePacing(ID3D12Device* device) {
    D3D12_DESCRIPTOR_RANGE r = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0 };
    D3D12_ROOT_PARAMETER p[2] = {};
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[0].DescriptorTable = { 1, &r };
    p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[1].Constants = { 0, 0, 1 }; // blendFactor float
    p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC s = {};
    s.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc = { 2, p, 1, &s,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
    return finalise(device, desc, RS::FramePacing);
}
