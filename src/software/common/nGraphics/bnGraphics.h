#pragma once
#include "GraphicsSystem.h"
#include "graphSimple.h"
#include "graphAdvanced.h"

class bnWindow;

class bnGraphics
{
public:
    bnGraphics(bnWindow* Window);
    ~bnGraphics();
public:
    ResourceHandle<ITexture>* CreateTexture(const TextureDesc& desc, const void* initialData = nullptr, ResourceHandle<ITexture>* resource = nullptr) {
        ResourceHandle<ITexture>* tex;

        if (!resource) tex = new ResourceHandle<ITexture>();
        else tex = resource;


        tex->status = ResourceStatus::TO_BE_CREATED;
        commands.push_back(std::make_unique<CreateTextureCommand>(desc, initialData, tex));
        return tex;
    }
    ResourceHandle<IShader>* CreateShader(const ShaderDesc& desc, ResourceHandle<IShader>* resource = nullptr);
    ResourceHandle<IBuffer>* CreateBuffer(const BufferDesc& desc, const void* data = nullptr, ResourceHandle<IBuffer>* resource = nullptr);
    ResourceHandle<IInputLayout>* CreateInputLayout(const InputLayoutDesc& desc, ResourceHandle<IInputLayout>* resource = nullptr) {
        ResourceHandle<IInputLayout>* inp;

        if (!resource) inp = new ResourceHandle<IInputLayout>();
        else inp = resource;
        
        commands.push_back(std::make_unique<CreateInputLayoutCommand>(desc, inp));
        return inp;
    }
    ResourceHandle<ISamplerState>* CreateSamplerState(const SamplerStateDesc& desc, ResourceHandle<ISamplerState>* resource = nullptr) {
        ResourceHandle<ISamplerState>* samp;

        if (!resource) samp = new ResourceHandle<ISamplerState>();
        else samp = resource;

        commands.push_back(std::make_unique<CreateSamplerStateCommand>(desc, samp));
        return samp;
    }
    IViewPort* CreateViewPort(const ViewPortDesc& desc, ResourceHandle<IBuffer>* resource = nullptr) {
        IViewPort* view = nullptr;
        commands.push_back(std::make_unique<CreateViewPortCommand>(desc, &view));
        return view;
    }
    ResourceHandle<IRasterizerState>* CreateRasterizerState(const RasterizerDesc& desc, ResourceHandle<IRasterizerState>* resource = nullptr) {
        ResourceHandle<IRasterizerState>* rast;

        if (!resource) rast = new ResourceHandle<IRasterizerState>();
        else rast = resource;

        commands.push_back(std::make_unique<CreateRasterizerStateCommand>(desc, rast));
        return rast;
    }
    ResourceHandle<IDepthStencilState>* CreateDepthStencilState(const DepthStencilDesc& desc, ResourceHandle<IDepthStencilState>* resource = nullptr) {
        ResourceHandle<IDepthStencilState>* rast = nullptr;

        if (!resource) rast = new ResourceHandle<IDepthStencilState>();
        else rast = resource;

        commands.push_back(std::make_unique<CreateDepthStencilStateCommand>(desc, rast));
        return rast;
    }
    ResourceHandle<IBlendState>* CreateBlendState(const BlendStateDesc& desc, ResourceHandle<IBlendState>* resource = nullptr) {
        ResourceHandle<IBlendState>* rast = nullptr;

        if (!resource) rast = new ResourceHandle<IBlendState>();
        else rast = resource;

        commands.push_back(std::make_unique<CreateBlendStateCommand>(desc, rast));
        return rast;
    }
    IRenderTarget* CreateRenderTarget(ITexture* texture) {

    }
    IDepthStencil* CreateDepthStencil(ITexture* texture) {

    }

    GraphicsSimple* makeSimple() {
        vectorGraphics.push_back(std::make_unique<GraphicsSimple>(&commands, *window, &relVariables));
        return vectorGraphics.back().get();
    }

    GraphicsAdvanced* makeAdvanced() {
        vectorGraphicsAdv.push_back(std::make_unique<GraphicsAdvanced>(&commands, *window, &relVariables));
        return vectorGraphicsAdv.back().get();
    }
  
    void DispatchCompute(UINT x, UINT y, UINT z) {

    }

    /// <summary>
    /// Only use this in ResourceHandles that are pointers
    /// </summary>
    template<typename T>
    void DestroyResource(T*& buffer);    
    void ReleaseShader(ResourceHandle<IShader>* buffer);
    void ReleaseBuffer(ResourceHandle<IBuffer>* buffer);
    void ReleaseTexture(ResourceHandle<ITexture>* texture);
    void ReleaseInputLayout(ResourceHandle<IInputLayout>* layout);
    void ReleaseSamplerState(ResourceHandle<ISamplerState>* sampler);
    void CallFunctionInSubmission(GPUFunction func) {
        commands.push_back(std::make_unique<FunctionCommand>(func));
    }

    void PushGroup(const char* name, uint32_t color = 0xFFFFFFFF);
    void PopGroup();
    void SetMarker(const char* name, uint32_t color = 0xFFFFFFFF);

    void Submit();
public: 
    bnWindow* window;
    CommandVector commands;
protected: // Variables

    std::vector<IResourceHandle**> relVariables;
    std::vector<std::unique_ptr<GraphicsSimple>> vectorGraphics;
    std::vector<std::unique_ptr<GraphicsAdvanced>> vectorGraphicsAdv;
	friend class bnWindow;

};

#include "bnGraphics.tpp"
