#include "bnGraphics.h"
#include "nWindow/bnWindow.h"
#include <thread>

//std::vector<IResourceHandle*> relVariables;

bnGraphics::bnGraphics(bnWindow* Window) : window(Window)
{
}

bnGraphics::~bnGraphics()
{
    for (int i = static_cast<int>(vectorGraphics.size()) - 1; i >= 0; --i) {
        auto& var = vectorGraphics[i];
        if (var->alrRel) {
            for (auto*& pipes : var->pipes) {
                delete pipes;
                pipes = nullptr;
            }
            if (var->pipe) {
                delete var->pipe;
            }
            vectorGraphics.erase(vectorGraphics.begin() + i);

        }
    }
    vectorGraphicsAdv.clear();
    vectorGraphics.shrink_to_fit();
}

ResourceHandle<IShader>* bnGraphics::CreateShader(const ShaderDesc& desc, ResourceHandle<IShader>* resource)
{
    ResourceHandle<IShader>* sha;

    if (!resource) sha = new ResourceHandle<IShader>();
    else sha = resource;


    sha->status = ResourceStatus::TO_BE_CREATED;
    //auto cmd = new CreateShaderCommand(desc, sha);
    //cmd->Execute(window->rootDevice);
    //delete cmd;
    commands.push_back(std::make_unique<CreateShaderCommand>(desc, sha));

    return sha;
}

ResourceHandle<IBuffer>* bnGraphics::CreateBuffer(const BufferDesc& desc, const void* data, ResourceHandle<IBuffer>* resource)
{
    ResourceHandle<IBuffer>* buf;

    if (!resource) buf = new ResourceHandle<IBuffer>();
    else buf = resource;

    buf->status = ResourceStatus::TO_BE_CREATED;
    commands.push_back(std::make_unique<CreateBufferCommand>(desc, data, buf));
    return buf;

    //if (desc.type == BufferType::Staging) {
    //    buf->status = ResourceStatus::CREATED;
    //    auto cmd = new CreateBufferCommand(desc, data, buf);
    //    cmd->Execute(window->rootDevice);
    //    delete cmd;
    //}
    //else {
    //    buf->status = ResourceStatus::TO_BE_CREATED;
    //    commands.push_back(std::make_unique<CreateBufferCommand>(desc, data, buf));
    //    return buf;
    //}
}

void bnGraphics::ReleaseShader(ResourceHandle<IShader>* buffer)
{
    //if (buffer->status != ResourceStatus::CREATED) return;
    //if (immediate) {
        //buffer->status = ResourceStatus::NOTHING;
        //auto cmd = new ReleaseShaderCommand(buffer);
        //cmd->Execute(window->rootDevice);
        //delete cmd;
    //}
    //else {
        buffer->status = ResourceStatus::NOTHING;
        commands.push_back(std::make_unique<ReleaseShaderCommand>(buffer));

    //}
}

void bnGraphics::ReleaseBuffer(ResourceHandle<IBuffer>* buffer)
{
    if (!buffer->PlannedToExist()) return;
    
    /*if (immediate) {
        buffer->status = ResourceStatus::NOTHING;
        auto cmd = new ReleaseBufferCommand(buffer);
        cmd->Execute(window->rootDevice);
        delete cmd;
    }*/
    //else {
        buffer->status = ResourceStatus::NOTHING;
        commands.push_back(std::make_unique<ReleaseBufferCommand>(buffer));
    //}
}

void bnGraphics::ReleaseTexture(ResourceHandle<ITexture>* texture)
{
    if (!texture->PlannedToExist()) return;
    //if (immediate) {
    //    texture->status = ResourceStatus::NOTHING;
    //    auto cmd = new ReleaseTextureCommand(texture);
    //    cmd->Execute(window->rootDevice);
    //    delete cmd;
    //}
    //else {
        texture->status = ResourceStatus::NOTHING;
        commands.push_back(std::make_unique<ReleaseTextureCommand>(texture));
    //}
}

void bnGraphics::ReleaseInputLayout(ResourceHandle<IInputLayout>* layout)
{
    if (!layout) return;

    layout->status = ResourceStatus::NOTHING;
    commands.push_back(std::make_unique<ReleaseInputLayoutCommand>(layout));
}

void bnGraphics::ReleaseSamplerState(ResourceHandle<ISamplerState> *sampler) {
    if (!sampler) return;

    sampler->status = ResourceStatus::NOTHING;
    commands.push_back(std::make_unique<ReleaseSamplerStateCommand>(sampler));
}

void bnGraphics::PushGroup(const char *name, uint32_t color) {
    commands.push_back(std::make_unique<PushGroupCommand>(PushGroupCommandData(name, color)));
}

void bnGraphics::PopGroup() {
    commands.push_back(std::make_unique<PopGroupCommand>());
}

void bnGraphics::SetMarker(const char *name, uint32_t color) {
    commands.push_back(std::make_unique<SetMarkerCommand>(name, color));
}

void bnGraphics::Submit()
{
   for (int i = static_cast<int>(vectorGraphics.size()) - 1; i >= 0; --i) {
        auto& var = vectorGraphics[i];
        if (var->alrRel) {
            //for (auto*& pipes : var->pipes) {
            //    delete pipes;
            //    pipes = nullptr;
            //}
            //if (var->pipe) {
            //    delete var->pipe;
            //}
            vectorGraphics.erase(vectorGraphics.begin() + i);
           
        }
    }
    
    vectorGraphics.shrink_to_fit();

    commands.access([&](auto& cmds) {
        for (auto& cmd : cmds) {
            if (cmd)
                cmd->Execute(window->rootDevice);
        }
        cmds.clear();
        });
    
        for (auto& var : relVariables) {
            if (var && *var) {
                (*var)->Release();
                delete* var;
                *var = nullptr;
            }
        }

        for (auto& var : vectorGraphics) {
            if (var.get()) {
                var->Release();
            }
        }

        vectorGraphicsAdv.clear();
    
        relVariables.clear();
  }
