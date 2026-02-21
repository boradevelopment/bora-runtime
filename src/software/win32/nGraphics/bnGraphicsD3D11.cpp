#ifdef WIN32
#include "bnGraphicsD3D11.h"

#include <d3d11_1.h>


bnGraphicsD3D11::bnGraphicsD3D11(SysHandle& handle, IGraphicsDeviceConfig& config) : windowHandle(handle), config(config)
{
}

bool bnGraphicsD3D11::Init()
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = 0;  // Auto-size
    scd.BufferDesc.Height = 0;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = windowHandle;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DXGI_SWAP_EFFECT_FLIP_DISCARD takes control of the window, and once that happens, vulkan can't take over the window once switched, causing it to freeze unless switched.

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Adapter (nullptr = default)
        D3D_DRIVER_TYPE_HARDWARE,  // Driver type
        nullptr,                    // Software rasterizer
        D3D11_CREATE_DEVICE_DEBUG, //D3D11_CREATE_DEVICE_DEBUG                          // Debugging is pretty bad because of Spir-v mismatches.
        nullptr, 0,                 // Feature levels (nullptr = use default list)
        D3D11_SDK_VERSION,
        &device.device, // Output device
        nullptr,                   // Feature level (optional out)
        &context.deviceC                 // Output context
    );

    if (FAILED(hr)) return false;

    hr = context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&mAnnotation);

    if (FAILED(hr)) {
        mAnnotation = nullptr;
    }

    if (config.enableMSAA) {
        for (UINT samples = config.aliasLevel; samples >= 2; samples -= 1) {
            UINT qualityLevels = 0;
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples, &qualityLevels)) &&
                qualityLevels > 0) {
                aliasLevel = samples;
                aliasQuality = qualityLevels - 1;
                enableMSAA = true;
                break;
            }
        }
    }


    // Step 2: Get DXGI factory
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()));
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));
    if (FAILED(hr)) return hr;

    //if (enableMSAA) {
    //    scd.SampleDesc.Count = aliasLevel;             
    //    scd.SampleDesc.Quality = aliasQuality;
    //}


    // Step 3: Create swap chain
    hr = dxgiFactory->CreateSwapChain(
        (ID3D11Device*)device.GetNativeHandle(),     // must be the same device
        &scd,
        &swapChain
    );

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    // MSAA Will be created when resized, which is guranteed.

    return true;
}

void bnGraphicsD3D11::BeginFrame()
{
    UpdateViewPorts();
    if (enableMSAA) {
        context->OMSetRenderTargets(1, &msaaRTV, nullptr);
        float clearColor[4] = { config.clearColor.r,  config.clearColor.g, config.clearColor.b, config.clearColor.a };
        context->ClearRenderTargetView(msaaRTV, clearColor);
    }
    else {
        context->OMSetRenderTargets(1, &rtv, nullptr);
        float clearColor[4] = { config.clearColor.r,  config.clearColor.g, config.clearColor.b, config.clearColor.a }; // Clear only visible area
        context->ClearRenderTargetView(rtv, clearColor);
    }
}

void bnGraphicsD3D11::Shutdown()
{
    mAnnotation->Release();
    if (msaaRTV) {
        msaaRTV->Release();
        msaaRTV = nullptr;
    }

    if (msaaRenderTarget) {
        msaaRenderTarget->Release();
        msaaRenderTarget = nullptr;
    }

    if (rtv) {
        rtv->Release();
        rtv = nullptr;
    }

    if (swapChain) {
        swapChain->Release();
        swapChain = nullptr;
    }

    if (context.GetNativeHandle()) {
        context->ClearState(); 
        context->Flush();      // Ensures that all commands are finished
        context->Release();
    }

    if (device.GetNativeHandle()) {
        device->Release();
    }
}

void bnGraphicsD3D11::Present()
{

    if (enableMSAA) {
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (FAILED(hr)) {
            OutputDebugStringA("Failed to get swap chain back buffer for resolve\n");
            return;
        }

        context->ResolveSubresource(
            backBuffer,  // Destination (non-MSAA texture)
            0,
            msaaRenderTarget,     // Source (MSAA texture)
            0,
            DXGI_FORMAT_R8G8B8A8_UNORM
        );
        backBuffer->Release();
    }

    HRESULT hr = swapChain->Present(config.vsync ? 1 : 0, 0);
    if (hr != 0) {
        _com_error err(hr);
        OutputDebugStringW(L"[DX11] Present failed: ");
        OutputDebugStringW(err.ErrorMessage());
        OutputDebugStringW(L"\n");

        // You can also switch on hr for known DXGI errors
        switch (hr) {
        case DXGI_ERROR_DEVICE_REMOVED:
            OutputDebugStringW(L"Device was removed.\n");
            break;
        case DXGI_ERROR_DEVICE_RESET:
            OutputDebugStringW(L"Device was reset.\n");
            break;
        default:
            break;
        }
    }

    viewports.clear();
}

ITexture* bnGraphicsD3D11::GetSwapchainImage()
{

    if (!swpBackBuffer) {
        HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&swpBackBuffer));
        if (FAILED(hr) || !swpBackBuffer)
            return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    swapChain->GetDesc(&desc);
    auto tex = new TextureD3D11();
    tex->owner = false;
    tex->mTexture = swpBackBuffer;
    tex->desc.width = width;
    tex->desc.height = height;
    tex->desc.format = FromDXGIFormat(desc.BufferDesc.Format);

    return tex;
}

D3D11_PRIMITIVE_TOPOLOGY ConvertToD3D11Topology(PrimitiveType type)
{
    switch (type)
    {
    case PrimitiveType::Triangles:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveType::TriangleStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveType::Lines:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveType::LineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case PrimitiveType::Points:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
        return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}


void bnGraphicsD3D11::Resize(long width, long height)
{
    this->width = width;
    this->height = height;
    viewports.clear();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    // Release old RTV
    if (rtv) {
        rtv->Release();
        rtv = nullptr;
    }

    if (enableMSAA) {
        if (msaaRTV) {
            msaaRTV->Release();
            msaaRTV = nullptr;
        }

        if (msaaRenderTarget) {
            msaaRenderTarget->Release();
            msaaRenderTarget = nullptr;
        }
    }

    auto addSwpBB = (ID3D11Texture2D**)&swpBackBuffer;

    if (swpBackBuffer) {

        // Recreate render target view
        swpBackBuffer->Release();
    }


    // Resize swapchain buffers
    HRESULT hr = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to resize swap chain\n");
        return;
    }
 
    ID3D11Texture2D* backBuffer = nullptr;
   
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get back buffer\n");
        return;
    }
    
    *addSwpBB = backBuffer;

    hr = device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
   /* backBuffer->Release();*/
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create RTV\n");
        return;
    }


    if (enableMSAA) {
        D3D11_TEXTURE2D_DESC colorDesc = {};
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        colorDesc.SampleDesc.Count = aliasLevel;              // MSAA
        colorDesc.SampleDesc.Quality = aliasQuality;
        colorDesc.Usage = D3D11_USAGE_DEFAULT;
        colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET;


        device->CreateTexture2D(&colorDesc, nullptr, &msaaRenderTarget);
        device->CreateRenderTargetView(msaaRenderTarget, nullptr, &msaaRTV);
    }

    UpdateViewPorts();
}

void bnGraphicsD3D11::UpdateViewPorts()
{
    if (viewports.empty())
        return;

    std::vector<D3D11_VIEWPORT> rawViewports;
    rawViewports.reserve(viewports.size());

    for (const auto& vpData : viewports) {
        if (vpData == nullptr) continue;
        rawViewports.push_back(vpData->viewport);
    }

    context->RSSetViewports(static_cast<UINT>(rawViewports.size()), rawViewports.data());

    viewports.clear();
}



ITexture* bnGraphicsD3D11::CreateTexture(const TextureDesc& desc, const void* initialData)
{
    TextureD3D11* texture = new TextureD3D11(desc);
    HRESULT hr;

    switch (desc.dimension)
    {
    case TextureDimensions::Dim1:
    {
        D3D11_TEXTURE1D_DESC texDesc{};
        texDesc.Width = desc.width;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.ArraySize = 1;
        texDesc.Format = ToDXGIFormat(desc.format);
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
       
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = initialData;
        initData.SysMemPitch = desc.width * 4; // Assuming RGBA8

        hr = device->CreateTexture1D(&texDesc, initialData ? &initData : nullptr, &texture->mTexture1D);
        texture->mTexture = texture->mTexture1D;
        break;
    }

    case TextureDimensions::Dim2:
    {
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.ArraySize = 1;
        texDesc.Format = ToDXGIFormat(desc.format);
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.SampleDesc.Count = 1; 
        texDesc.SampleDesc.Quality = 0; 

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = initialData;
        initData.SysMemPitch = desc.width * 4; // Assuming RGBA8

        hr = device->CreateTexture2D(&texDesc, initialData ? &initData : nullptr, &texture->mTexture2D);
        texture->mTexture = texture->mTexture2D;
        break;
    }

    case TextureDimensions::Dim3:
    {
        D3D11_TEXTURE3D_DESC texDesc{};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.Depth = 1;
        texDesc.Format = ToDXGIFormat(desc.format);
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
  
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = initialData;
        initData.SysMemPitch = desc.width * 4; // Assuming RGBA8

         hr = device->CreateTexture3D(&texDesc, initialData ? &initData : nullptr, &texture->mTexture3D);
         texture->mTexture = texture->mTexture1D;
        break;
    }
    }
   
    if (FAILED(hr)) {
        delete texture;
        return nullptr;
    }

    // Create SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = ToDXGIFormat(desc.format);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture->mTexture, &srvDesc, &texture->mSrv);
    if (FAILED(hr)) {
        texture->mTexture->Release();
        delete texture;
        return nullptr;
    }

    return texture;
}

IShader* bnGraphicsD3D11::CreateShader(const ShaderDesc& desc)
{
    if (!desc.bytecode)
        return nullptr;

    ShaderD3D11* shader = new ShaderD3D11();
    shader->ogData = desc.ogData;
    shader->type = desc.type;
    shader->binary.reserve(desc.bytecodeSize);
    shader->binary.insert(shader->binary.end(),
        desc.bytecode,
        desc.bytecode + desc.bytecodeSize);

    HRESULT hr = E_FAIL;

    switch (desc.type)
    {
    case ShaderDesc::Type::Vertex: {
        ID3D11VertexShader* vertexShader = nullptr;
        hr = device->CreateVertexShader(desc.bytecode, desc.bytecodeSize, nullptr, &vertexShader);
        shader->mShader = vertexShader;
        break;
    }
    case ShaderDesc::Type::Pixel: {
        ID3D11PixelShader* pixelShader = nullptr;
        hr = device->CreatePixelShader(desc.bytecode, desc.bytecodeSize, nullptr, &pixelShader);
        shader->mShader = pixelShader;
        break;
    }
    case ShaderDesc::Type::Compute: {
        ID3D11ComputeShader* computeShader = nullptr;
        hr = device->CreateComputeShader(desc.bytecode, desc.bytecodeSize, nullptr, &computeShader);
        shader->mShader = computeShader;
        break;
    }
    default:
        return nullptr;
    }

    if (FAILED(hr))
        return nullptr;


    if (desc.releaseByteCodeOnceInitalized) {
        delete[] desc.bytecode;
    }

    return shader;
}

IBuffer* bnGraphicsD3D11::CreateBuffer(const BufferDesc& desc, const void* data)
{
    BufferD3D11* buffer = new BufferD3D11();

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = desc.byteWidth == 0 ? desc.size * desc.stride : desc.byteWidth;
    if (bd.ByteWidth == 0) {
        bd.ByteWidth = desc.size;
    } // if this doesn't work, throw a bnError

        bd.BindFlags = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        size_t stride = desc.stride;
        bd.StructureByteStride = desc.stride;
        switch (desc.type) {
        case BufferType::Vertex:
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            break;

        case BufferType::Index:
            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexFormat = DXGI_FORMAT_R32_UINT; // or DXGI_FORMAT_R16_UINT depending on your use
            break;

        case BufferType::Constant:
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            break;
        case BufferType::Staging:
            bd.Usage = D3D11_USAGE_STAGING;
            bd.BindFlags = 0;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
            break;
        }

        if (desc.dynamic && desc.type != BufferType::Staging) {
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }


        D3D11_SUBRESOURCE_DATA initData = {};
        D3D11_SUBRESOURCE_DATA* pInitData = nullptr;

        if (data) {
            if (config.clipSpace == VerticesClipSpace::VULKAN) {
                if (desc.type == BufferType::Vertex) {
                    buffer->dataBuffer = std::vector<u8>(desc.size * desc.stride);
                    std::memcpy(buffer->dataBuffer.data(), data, buffer->dataBuffer.size());

                    for (size_t i = 0; i < desc.size; i++) {
                        auto* vertex = reinterpret_cast<float*>(buffer->dataBuffer.data() + i * desc.stride);

                        if (desc.stride >= sizeof(float) * 2) {
                            vertex[1] = -vertex[1]; // Flip Y
                        }
                    }
                    initData.pSysMem = buffer->dataBuffer.data();
                }
                else {
                    initData.pSysMem = data;
                }
            }
            else {
                initData.pSysMem = data;
            }
            pInitData = &initData;
        }

        ID3D11Buffer* d3dBuffer = nullptr;
        HRESULT hr = device->CreateBuffer(&bd, pInitData, &d3dBuffer);
    if (FAILED(hr)) {
        delete buffer;
        return nullptr;
    }

    buffer->mBuffer = d3dBuffer;
    buffer->type = desc.type;
    buffer->mStride = stride;
    buffer->mIndexFormat = indexFormat;
    buffer->desc = desc;
    return buffer;
}

// DXGIS are apart of ALL D3D versions, so i'll make a DXGI_UTIL in the future
DXGI_FORMAT TranslateTypeToDXGIFormat(VertexAttribType type)
{
    switch (type)
    {
    case VertexAttribType::Float:   return DXGI_FORMAT_R32_FLOAT;
    case VertexAttribType::Float2:  return DXGI_FORMAT_R32G32_FLOAT;
    case VertexAttribType::Float3:  return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexAttribType::Float4:  return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case VertexAttribType::UInt:    return DXGI_FORMAT_R32_UINT;
    case VertexAttribType::UInt2:   return DXGI_FORMAT_R32G32_UINT;
    case VertexAttribType::UInt3:   return DXGI_FORMAT_R32G32B32_UINT;
    case VertexAttribType::UInt4:   return DXGI_FORMAT_R32G32B32A32_UINT;

        // Add more as needed, e.g., normalized formats, half-floats
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

// D3D11
std::vector<D3D11_INPUT_ELEMENT_DESC> TranslateToD3D11(const InputLayoutDesc& layout) {
    std::vector<D3D11_INPUT_ELEMENT_DESC> d3dLayout;
    for (auto& elem : layout.elements) {
        D3D11_INPUT_ELEMENT_DESC d;
        d.SemanticName = elem.semanticName.c_str();
        d.SemanticIndex = elem.semanticIndex;
        d.Format = TranslateTypeToDXGIFormat(elem.type);
        d.InputSlot = elem.inputSlot;
        d.AlignedByteOffset = static_cast<UINT>(elem.offset);
        d.InputSlotClass = elem.perInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        d.InstanceDataStepRate = 0;
        d3dLayout.push_back(d);
    }
    return d3dLayout;
}


IInputLayout* bnGraphicsD3D11::CreateInputLayout(const InputLayoutDesc& desc)
{
    ID3D11InputLayout* inputLayout = nullptr;
    IShader* vertShader = nullptr;
    if (desc.vs) {
       vertShader = desc.vs->Get();
    }
   
    auto layOut = TranslateToD3D11(desc);

    HRESULT res;
    
    if (vertShader) {
        res = device->CreateInputLayout(
            layOut.data(),
            layOut.size(),
            vertShader->binary.data(),
            vertShader->binary.size(),
            &inputLayout);
    }
    else {
        res = device->CreateInputLayout(
            layOut.data(),
            layOut.size(),
            nullptr,
            0,
            &inputLayout);
    }

    if (res != 0) return nullptr;
    else {
        InputLayoutD3D11* iLayout = new InputLayoutD3D11();
        iLayout->desc = desc;
        iLayout->mLayout = inputLayout;

        return iLayout;
    }

}

ISamplerState* bnGraphicsD3D11::CreateSamplerState(const SamplerStateDesc& desc)
{
    D3D11_SAMPLER_DESC dxDesc = {};

    dxDesc.Filter = ToD3D11Filter(desc.minFilter, desc.magFilter, desc.mipFilter, desc.maxAnisotropy);
    dxDesc.AddressU = ToD3D11Address(desc.addressU);
    dxDesc.AddressV = ToD3D11Address(desc.addressV);
    dxDesc.AddressW = ToD3D11Address(desc.addressW);
    dxDesc.MipLODBias = desc.mipLODBias;
    dxDesc.MaxAnisotropy = desc.maxAnisotropy;
    dxDesc.ComparisonFunc = ToD3D11Comparison(desc.comparisonFunc);
    dxDesc.MinLOD = desc.minLOD;
    dxDesc.MaxLOD = desc.maxLOD;

    dxDesc.BorderColor[0] = desc.borderColor[0];
    dxDesc.BorderColor[1] = desc.borderColor[1];
    dxDesc.BorderColor[2] = desc.borderColor[2];
    dxDesc.BorderColor[3] = desc.borderColor[3];

    ID3D11SamplerState* samplerState = nullptr;
    HRESULT res = device->CreateSamplerState(&dxDesc, &samplerState);
    if (res != 0) return nullptr;
    else {
        SamplerStateD3D11* iSamplerState = new SamplerStateD3D11();
        iSamplerState->mSamplerState = samplerState;

        return iSamplerState;
    }

    return nullptr;
}

IViewPort* bnGraphicsD3D11::CreateViewPort(const ViewPortDesc& desc)
{
    ViewPortD3D11* viewPort = new ViewPortD3D11();

    D3D11_VIEWPORT d3dVp;
    d3dVp.TopLeftX = static_cast<FLOAT>(desc.viewport->x);
    d3dVp.TopLeftY = static_cast<FLOAT>(desc.viewport->y);
    d3dVp.Width = static_cast<FLOAT>(desc.viewport->width);
    d3dVp.Height = static_cast<FLOAT>(desc.viewport->height);
    d3dVp.MinDepth = static_cast<FLOAT>(desc.viewport->minDepth);
    d3dVp.MaxDepth = static_cast<FLOAT>(desc.viewport->maxDepth);

    viewPort->mViewPortData.viewport = d3dVp;
    for (auto& vp : viewports) {
        if (vp->viewport == viewPort->mViewPortData.viewport) {
            vp = &viewPort->mViewPortData;
            break;
        }
    }

    viewports.push_back(&viewPort->mViewPortData);

    return viewPort;
}

// DevNote: I need to move this abstract conversions into some D3D11Util header file

D3D11_FILL_MODE ToD3DFillMode(IFillMode mode) {
    switch (mode) {
    case IFillMode::Wireframe: return D3D11_FILL_WIREFRAME;
    case IFillMode::Solid:     return D3D11_FILL_SOLID;
    default:                  return D3D11_FILL_SOLID;
    }
}

D3D11_CULL_MODE ToD3DCullMode(CullMode mode) {
    switch (mode) {
    case CullMode::None:  return D3D11_CULL_NONE;
    case CullMode::Front: return D3D11_CULL_FRONT;
    case CullMode::Back:  return D3D11_CULL_BACK;
    default:              return D3D11_CULL_BACK;
    }
}

IRasterizerState* bnGraphicsD3D11::CreateRasterizerState(const RasterizerDesc& desc)
{
    D3D11_RASTERIZER_DESC dxDesc = {};
    dxDesc.FillMode = ToD3DFillMode(desc.fillMode);
    dxDesc.CullMode = ToD3DCullMode(desc.cullMode);
    dxDesc.FrontCounterClockwise = desc.frontCounterClockwise;
    dxDesc.DepthBias = desc.depthBias;
    dxDesc.DepthBiasClamp = desc.depthBiasClamp;
    dxDesc.SlopeScaledDepthBias = desc.slopeScaledDepthBias;
    dxDesc.DepthClipEnable = desc.depthClipEnable;
    dxDesc.ScissorEnable = desc.scissorEnable;
    dxDesc.MultisampleEnable = desc.multisampleEnable;
    dxDesc.AntialiasedLineEnable = desc.antialiasedLineEnable;

    ID3D11RasterizerState* state = nullptr;
    HRESULT hr = device->CreateRasterizerState(&dxDesc, &state);
    if (FAILED(hr)) {
        return nullptr;
    }

    return new RasterizerStateD3D11(state);
}

D3D11_COMPARISON_FUNC ToD3DComparison(ComparisonFunc func) {
    switch (func) {
    case ComparisonFunc::Never:        return D3D11_COMPARISON_NEVER;
    case ComparisonFunc::Less:         return D3D11_COMPARISON_LESS;
    case ComparisonFunc::Equal:        return D3D11_COMPARISON_EQUAL;
    case ComparisonFunc::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
    case ComparisonFunc::Greater:      return D3D11_COMPARISON_GREATER;
    case ComparisonFunc::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
    case ComparisonFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
    case ComparisonFunc::Always:       return D3D11_COMPARISON_ALWAYS;
    default:                           return D3D11_COMPARISON_ALWAYS;
    }
}

D3D11_STENCIL_OP ToD3DStencilOp(StencilOp op) {
    switch (op) {
    case StencilOp::Keep:    return D3D11_STENCIL_OP_KEEP;
    case StencilOp::Zero:    return D3D11_STENCIL_OP_ZERO;
    case StencilOp::Replace: return D3D11_STENCIL_OP_REPLACE;
    case StencilOp::IncrSat: return D3D11_STENCIL_OP_INCR_SAT;
    case StencilOp::DecrSat: return D3D11_STENCIL_OP_DECR_SAT;
    case StencilOp::Invert:  return D3D11_STENCIL_OP_INVERT;
    case StencilOp::Incr:    return D3D11_STENCIL_OP_INCR;
    case StencilOp::Decr:    return D3D11_STENCIL_OP_DECR;
    default:                 return D3D11_STENCIL_OP_KEEP;
    }
}


IDepthStencilState* bnGraphicsD3D11::CreateDepthStencilState(const DepthStencilDesc& desc)
{
    D3D11_DEPTH_STENCIL_DESC dxDesc = {};
    dxDesc.DepthEnable = desc.depthEnable;
    dxDesc.DepthWriteMask = desc.depthWriteMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dxDesc.DepthFunc = ToD3DComparison(desc.depthFunc);

    dxDesc.StencilEnable = desc.stencilEnable;
    dxDesc.StencilReadMask = desc.stencilReadMask;
    dxDesc.StencilWriteMask = desc.stencilWriteMask;

    dxDesc.FrontFace.StencilFailOp = ToD3DStencilOp(desc.frontFace.failOp);
    dxDesc.FrontFace.StencilDepthFailOp = ToD3DStencilOp(desc.frontFace.depthFailOp);
    dxDesc.FrontFace.StencilPassOp = ToD3DStencilOp(desc.frontFace.passOp);
    dxDesc.FrontFace.StencilFunc = ToD3DComparison(desc.frontFace.func);

    dxDesc.BackFace.StencilFailOp = ToD3DStencilOp(desc.backFace.failOp);
    dxDesc.BackFace.StencilDepthFailOp = ToD3DStencilOp(desc.backFace.depthFailOp);
    dxDesc.BackFace.StencilPassOp = ToD3DStencilOp(desc.backFace.passOp);
    dxDesc.BackFace.StencilFunc = ToD3DComparison(desc.backFace.func);

    ID3D11DepthStencilState* state = nullptr;
    HRESULT hr = device->CreateDepthStencilState(&dxDesc, &state);
    if (FAILED(hr)) {
        return nullptr;
    }

    return new DepthStencilStateD3D11(state);
}

static D3D11_BLEND ToD3DBlend(Blend b) {
    switch (b) {
    case Blend::Zero: return D3D11_BLEND_ZERO;
    case Blend::One: return D3D11_BLEND_ONE;
    case Blend::SrcColor: return D3D11_BLEND_SRC_COLOR;
    case Blend::InvSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
    case Blend::SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
    case Blend::InvSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
    case Blend::DestAlpha: return D3D11_BLEND_DEST_ALPHA;
    case Blend::InvDestAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
    case Blend::DestColor: return D3D11_BLEND_DEST_COLOR;
    case Blend::InvDestColor: return D3D11_BLEND_INV_DEST_COLOR;
    case Blend::SrcAlphaSat: return D3D11_BLEND_SRC_ALPHA_SAT;
    case Blend::BlendFactor: return D3D11_BLEND_BLEND_FACTOR;
    case Blend::InvBlendFactor: return D3D11_BLEND_INV_BLEND_FACTOR;
    case Blend::Src1Color: return D3D11_BLEND_SRC1_COLOR;
    case Blend::InvSrc1Color: return D3D11_BLEND_INV_SRC1_COLOR;
    case Blend::Src1Alpha: return D3D11_BLEND_SRC1_ALPHA;
    case Blend::InvSrc1Alpha: return D3D11_BLEND_INV_SRC1_ALPHA;
    }
    return D3D11_BLEND_ONE; // fallback
}

static D3D11_BLEND_OP ToD3DBlendOp(BlendOp op) {
    switch (op) {
    case BlendOp::Add: return D3D11_BLEND_OP_ADD;
    case BlendOp::Subtract: return D3D11_BLEND_OP_SUBTRACT;
    case BlendOp::RevSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
    case BlendOp::Min: return D3D11_BLEND_OP_MIN;
    case BlendOp::Max: return D3D11_BLEND_OP_MAX;
    }
    return D3D11_BLEND_OP_ADD;
}

IBlendState* bnGraphicsD3D11::CreateBlendState(const BlendStateDesc& desc)
{
    D3D11_BLEND_DESC bd = {};
    bd.AlphaToCoverageEnable = desc.alphaToCoverageEnable ? TRUE : FALSE;
    bd.IndependentBlendEnable = desc.independentBlendEnable ? TRUE : FALSE;

    for (int i = 0; i < 8; i++) {
        const RenderTargetBlendDesc& rt = desc.renderTarget[i];
        D3D11_RENDER_TARGET_BLEND_DESC& drt = bd.RenderTarget[i];

        drt.BlendEnable = rt.blendEnable ? TRUE : FALSE;
        drt.SrcBlend = ToD3DBlend(rt.srcBlend);
        drt.DestBlend = ToD3DBlend(rt.destBlend);
        drt.BlendOp = ToD3DBlendOp(rt.blendOp);
        drt.SrcBlendAlpha = ToD3DBlend(rt.srcBlendAlpha);
        drt.DestBlendAlpha = ToD3DBlend(rt.destBlendAlpha);
        drt.BlendOpAlpha = ToD3DBlendOp(rt.blendOpAlpha);
        drt.RenderTargetWriteMask = rt.renderTargetWriteMask;
    }

    ID3D11BlendState* dxBlendState = nullptr;
    HRESULT hr = device->CreateBlendState(&bd, &dxBlendState);
    if (FAILED(hr)) {
        return nullptr;
    }

    return new BlendStateD3D11(dxBlendState);
}

IRenderTarget* bnGraphicsD3D11::CreateRenderTarget(const RenderTargetDesc& desc)
{
    // Cast each color target
    std::vector<ID3D11RenderTargetView*> rtvs;
    for (size_t i = 0; i < desc.colorTargets.size(); i++) {
        auto d3dTexture = dynamic_cast<TextureD3D11*>(desc.colorTargets[i]);
        if (!d3dTexture) continue;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = ToDXGIFormat(d3dTexture->mDesc.format);
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        ID3D11RenderTargetView* rtv = nullptr;
        HRESULT hr = device->CreateRenderTargetView(
            (ID3D11Resource*)d3dTexture->GetNativeHandle(),
            &rtvDesc,
            &rtv
        );
        if (FAILED(hr)) {
            // Release any previously created RTVs
            for (auto r : rtvs) r->Release();
            return nullptr;
        }

        rtvs.push_back(rtv);
    }

    // Optional depth stencil
    ID3D11DepthStencilView* dsv = nullptr;
    if (desc.depth) {
        auto d3dDepth = dynamic_cast<DepthStencilD3D11*>(desc.depth);
        if (d3dDepth) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = ToDXGIFormat(d3dDepth->texture->desc.format);
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;

            HRESULT hr = device->CreateDepthStencilView(
                (ID3D11Resource*)d3dDepth->GetNativeHandle(),
                &dsvDesc,
                &dsv
            );
            if (FAILED(hr)) {
                for (auto r : rtvs) r->Release();
                return nullptr;
            }
        }
    }

    // Create the RenderTarget wrapper
    return new RenderTargetD3D11(rtvs, dsv);

}

IDepthStencil* bnGraphicsD3D11::CreateDepthStencil(ITexture* texture)
{
    TextureD3D11* d3dTexture = dynamic_cast<TextureD3D11*>(texture);
    if (!d3dTexture) {
        return nullptr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
    desc.Format = ToDXGIFormat(d3dTexture->mDesc.format);
    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;

    ID3D11DepthStencilView* dsv = nullptr;
    HRESULT hr = device->CreateDepthStencilView((ID3D11Resource*)texture->GetNativeHandle(), &desc, &dsv);
    if (FAILED(hr)) return nullptr;

    return new DepthStencilD3D11(dsv, texture);
}

void bnGraphicsD3D11::ReleaseShader(IShader** shader)
{
    if (shader && *shader) {
        (*shader)->Release();
        //delete* shader;
        *shader = nullptr;
    }
}

void bnGraphicsD3D11::ReleaseBuffer(IBuffer** buffer)
{
    if (buffer && *buffer) {
        (*buffer)->Release();
        //delete* buffer;
        *buffer = nullptr;
    }
}

void bnGraphicsD3D11::ReleaseTexture(ITexture** texture)
{
    if (texture && *texture) {
        (*texture)->Release();
        //delete* texture;
        *texture = nullptr;
    }
}


void bnGraphicsD3D11::BindShader(IShader* shader)
{
    if (!shader) {
        // unbind all shaders
        context->VSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        context->CSSetShader(nullptr, nullptr, 0);
        return;
    }

    ShaderD3D11* d3dShader = dynamic_cast<ShaderD3D11*>(shader);
    if (!d3dShader) {
        // Invalid cast 
        return;
    }

    void* nativeShader = d3dShader->GetNativeHandle();
    if (!nativeShader)
        return;

    switch (d3dShader->type) {
    case ShaderDesc::Type::Vertex:
        context->VSSetShader(static_cast<ID3D11VertexShader*>(nativeShader), nullptr, 0);
        break;
    case ShaderDesc::Type::Pixel:
        context->PSSetShader(static_cast<ID3D11PixelShader*>(nativeShader), nullptr, 0);
        break;
    case ShaderDesc::Type::Compute:
        context->CSSetShader(static_cast<ID3D11ComputeShader*>(nativeShader), nullptr, 0);
        break;
       
    }
}

void bnGraphicsD3D11::BindBuffer(IBuffer* buffer)
{
    BufferD3D11* d3dBuffer = reinterpret_cast<BufferD3D11*>(buffer);
    if (!d3dBuffer || !d3dBuffer->mBuffer) return;


    // if it's mapped, unmap it for safety.
    auto iteration = std::find(mappedPointers.begin(), mappedPointers.end(), d3dBuffer);
    if (iteration != mappedPointers.end()) {
        UnmapBufferMemory(d3dBuffer);
    }

    switch (d3dBuffer->type) {
    case BufferType::Vertex: {
 /*       auto buffer = (ID3D11Buffer*)d3dBuffer->GetNativeHandle();
        auto stride = (UINT)d3dBuffer->mStride;
        auto offset = (UINT)d3dBuffer->mOffset;*/
        context->IASetVertexBuffers(0, 1, &d3dBuffer->mBuffer, &d3dBuffer->mStride, &d3dBuffer->mOffset);
        break;
    }
    case BufferType::Index:
        context->IASetIndexBuffer(d3dBuffer->mBuffer, d3dBuffer->mIndexFormat, 0);
        break;
    case BufferType::Constant:
        // Might need to update it for more stages
        context->VSSetConstantBuffers(0, 1, &d3dBuffer->mBuffer);
        context->PSSetConstantBuffers(0, 1, &d3dBuffer->mBuffer);

        break;
    }

}

void bnGraphicsD3D11::BindTexture(ITexture* texture, u8 slot)
{
    TextureD3D11* d3dTexture = dynamic_cast<TextureD3D11*>(texture);
    if (!d3dTexture || !d3dTexture->mSrv) {
        // Invalid cast 
        return;
    }

    context->PSSetShaderResources(slot != 0 ? slot : d3dTexture->slot, 1, &d3dTexture->mSrv);
}

void bnGraphicsD3D11::BindInputLayout(IInputLayout* layout)
{
    InputLayoutD3D11* d3dLayout = reinterpret_cast<InputLayoutD3D11*>(layout);
    if (!d3dLayout || !d3dLayout->mLayout) return;

    context->IASetInputLayout(d3dLayout->mLayout);
}

void bnGraphicsD3D11::BindSamplerState(ISamplerState* samplerState, u8 slot)
{
    SamplerStateD3D11* d3dSamplerState = reinterpret_cast<SamplerStateD3D11*>(samplerState);
    if (!d3dSamplerState || !d3dSamplerState->mSamplerState) return;

    context->PSSetSamplers(slot , 1, &d3dSamplerState->mSamplerState);
}

void bnGraphicsD3D11::BindViewPort(IViewPort* viewPort)
{
    ViewPortD3D11* d3dViewPort = reinterpret_cast<ViewPortD3D11*>(viewPort);
    if (!d3dViewPort) return;

    if (d3dViewPort->mViewPortData.viewport.Width < 0) {
        return;
    }

    if (viewports.size() == 0) return;

    context->RSSetViewports(viewports.size()-1, &d3dViewPort->mViewPortData.viewport);
}

void bnGraphicsD3D11::BindRasterizerState(IRasterizerState* state)
{
    if (!state) {
        context->RSSetState(nullptr);
        return;
    }

    auto d3dState = static_cast<RasterizerStateD3D11*>(state);
    context->RSSetState((ID3D11RasterizerState*)d3dState->GetNativeHandle());
}

void bnGraphicsD3D11::BindDepthStencilState(IDepthStencilState* state, UINT stencilRef)
{
    if (!state) {
        context->OMSetDepthStencilState(nullptr, stencilRef);
        return;
    }

    auto d3dState = static_cast<DepthStencilStateD3D11*>(state);
    context->OMSetDepthStencilState((ID3D11DepthStencilState*)d3dState->GetNativeHandle(), stencilRef);
}

void bnGraphicsD3D11::BindBlendState(IBlendState* state, const float blendFactor[4], UINT sampleMask)
{
    if (!state) {
        context->OMSetBlendState(nullptr, blendFactor, sampleMask);
        return;
    }

    auto d3dState = reinterpret_cast<BlendStateD3D11*>(state);
    context->OMSetBlendState((ID3D11BlendState*)d3dState->GetNativeHandle(), blendFactor, sampleMask);
}

void bnGraphicsD3D11::BindRenderTarget(IRenderTarget* rt, IDepthStencil* ds)
{
    ID3D11RenderTargetView* rtv = rt ? (ID3D11RenderTargetView*)(rt)->GetNativeHandle() : nullptr;
    ID3D11DepthStencilView* dsv = ds ? (ID3D11DepthStencilView*)(ds)->GetNativeHandle() : nullptr;

    context->OMSetRenderTargets(1, &rtv, dsv);
}

void bnGraphicsD3D11::ClearRenderTarget(IRenderTarget* target, const float color[4])
{
    context->ClearRenderTargetView((ID3D11RenderTargetView * )target->GetNativeHandle(), color);
}

void bnGraphicsD3D11::ClearDepthStencil(IDepthStencil* target, float depth, UINT8 stencil)
{
    context->ClearDepthStencilView((ID3D11DepthStencilView*)target->GetNativeHandle(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

/// TIP - Bind before dispatching!
void bnGraphicsD3D11::DispatchCompute(UINT x, UINT y, UINT z)
{
    context->Dispatch(x, y, z);
}

void bnGraphicsD3D11::CopyToBuffer(IBuffer* buffer, void* data, size_t size)
{
    auto* dxBuffer = dynamic_cast<BufferD3D11*>(buffer);
    if (!dxBuffer) {
        // Handle the error: wrong buffer type passed
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(dxBuffer->mBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, data, size);
    context->Unmap(dxBuffer->mBuffer, 0);
}



void bnGraphicsD3D11::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset)
{
    D3D11_PRIMITIVE_TOPOLOGY topology = ConvertToD3D11Topology(type);
    context->IASetPrimitiveTopology(topology);
    context->Draw(static_cast<UINT>(vertexCount), 0);
}

void bnGraphicsD3D11::DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset)
{
    auto* dxIndexBuffer = dynamic_cast<BufferD3D11*>(indexBuffer);

    if (!dxIndexBuffer) {
        // Handle invalid cast (wrong buffer type)
        return;
    }


    // Bind index buffer
    ID3D11Buffer* ib = (ID3D11Buffer*)dxIndexBuffer->GetNativeHandle();
    DXGI_FORMAT format = dxIndexBuffer->mIndexFormat; // e.g., DXGI_FORMAT_R16_UINT or R32_UINT
    context->IASetIndexBuffer(ib, format, 0);

    // Set primitive topology
    context->IASetPrimitiveTopology(ConvertToD3D11Topology(type));

    // Issue draw call
    context->DrawIndexed(static_cast<UINT>(indexCount), static_cast<UINT>(indexOffset), 0);

}
#endif


// todo: re-evaluate this and createbuffer design
void bnGraphicsD3D11::MapBufferMemory(IBuffer* buffer, void** dataPtr)
{
    auto D3DBUFFER = (BufferD3D11*)buffer;
    ID3D11Buffer* d3dBuffer = static_cast<ID3D11Buffer*>(buffer->GetNativeHandle());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr;
    
    hr= context->Map(d3dBuffer, 0, D3DBUFFER->desc.dynamic ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr))
        return;

    *dataPtr = mapped.pData;

    mappedPointers.push_back(D3DBUFFER);
}

void bnGraphicsD3D11::UnmapBufferMemory(IBuffer* buffer)
{
    mappedPointers.erase(
        std::remove_if(
            mappedPointers.begin(),
            mappedPointers.end(),
            [=](BufferD3D11* b) { return b == (BufferD3D11*)buffer; } // condition for removal
        ),
        mappedPointers.end()
    );
    ID3D11Buffer* d3dBuffer = static_cast<ID3D11Buffer*>(buffer->GetNativeHandle());
    context->Unmap(d3dBuffer, 0);
}

void bnGraphicsD3D11::CopyBufferToImage(IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc)
{
    ID3D11Texture2D* tex = static_cast<ID3D11Texture2D*>(dstTexture->GetNativeHandle());
    ID3D11Buffer* buf = static_cast<ID3D11Buffer*>(srcBuffer->GetNativeHandle());

    // Map buffer to read back data
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(buf, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
        return;

    if (desc.bufferRowLength == desc.imageExtent.width) {
        desc.bufferRowLength = desc.imageExtent.width * 4;
    }

    if (desc.bufferImageHeight > desc.imageExtent.height) {
        desc.bufferImageHeight = desc.imageExtent.height;
    }

    // Describe subresource box (region of texture to update)
    D3D11_BOX box = {};
    box.left = desc.imageOffset.x;
    box.top = desc.imageOffset.y;
    box.front = desc.imageOffset.z;
    box.right = desc.imageOffset.x + desc.imageExtent.width;
    box.bottom = desc.imageOffset.y + desc.imageExtent.height;
    box.back = 1;

    // Copy from buffer memory into texture
    context->UpdateSubresource(
        tex,
        0,         // subresource index (mip level 0, array slice 0)
        &box,      // destination region
        mapped.pData,
        desc.bufferRowLength > 0 ? desc.bufferRowLength : desc.imageExtent.width * 4,   // pitch: rowLength * bytesPerPixel
        0                          // depthPitch (unused for 2D)
    );

        context->Unmap(buf, 0);
}

void bnGraphicsD3D11::CopyImageToImage(ITexture* srcBuffer, ITexture* dstBuffer, ImageCopyDesc desc)
{
    TextureD3D11* texSrcD3D = static_cast<TextureD3D11*>(srcBuffer);
    TextureD3D11* texDstD3D = static_cast<TextureD3D11*>(dstBuffer);

    D3D11_BOX srcBox = {};
    srcBox.left = desc.srcOffset.x;
    srcBox.top = desc.srcOffset.y;
    srcBox.front = desc.srcOffset.z;
    srcBox.right = desc.srcOffset.x + desc.extent.width;
    srcBox.bottom = desc.srcOffset.y + desc.extent.height;
    srcBox.back = desc.srcOffset.z + desc.extent.depth;

    // Compute the subresource index
    UINT srcSubresource = D3D11CalcSubresource(desc.srcSubresource.mipLevel,
        desc.srcSubresource.baseArrayLayer,
        texSrcD3D->mDesc.mipLevels);
    UINT dstSubresource = D3D11CalcSubresource(desc.dstSubresource.mipLevel,
        desc.dstSubresource.baseArrayLayer,
        texDstD3D->mDesc.mipLevels);

    context->CopySubresourceRegion(
        texDstD3D->mTexture,  // destination resource
        dstSubresource,            // destination subresource
        desc.dstOffset.x,          // dest X
        desc.dstOffset.y,          // dest Y
        desc.dstOffset.z,          // dest Z
        texSrcD3D->mTexture,  // source resource
        srcSubresource,            // source subresource
        &srcBox                    // source box (nullptr to copy entire subresource)
    );
}

void bnGraphicsD3D11::PushGroup(const char *name, uint32_t color) {
    if (mAnnotation) {
        mAnnotation->BeginEvent(Utf8ToWide(name).c_str());
    }
}

void bnGraphicsD3D11::PopGroup() {
    if (mAnnotation) {
        mAnnotation->EndEvent();
    }
}

void bnGraphicsD3D11::SetMarker(const char *name, uint32_t color) {
    if (mAnnotation) {
        mAnnotation->SetMarker(Utf8ToWide(name).c_str());
    }
}