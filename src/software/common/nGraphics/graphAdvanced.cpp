#include "graphAdvanced.h"
#include "nWindow/bnWindow.h"

// lists don't get reused so 
GraphicAdvancedCommandList* GraphicsAdvanced::GetCommandList() {
	return new GraphicAdvancedCommandList(commands);
}

ResourceHandle<IPipelineBuilder>* GraphicsAdvanced::CreatePipelineBuilder(ResourceHandle<IPipelineBuilder>* resource)
{
	ResourceHandle<IPipelineBuilder>* pipe;

	if (!resource) pipe = new ResourceHandle<IPipelineBuilder>();
	else pipe = resource;

	//auto cmd = new CreatePipelineBuilderCommand(pipe, commands);
	//cmd->Execute(window.rootDevice);
	//delete cmd;

	commands->push_back(std::make_unique<CreatePipelineBuilderCommand>(pipe, commands));

	return pipe;
}

ResourceHandle<ICommandPool>* GraphicsAdvanced::CreateCommandPool(CommandPoolDesc desc, ResourceHandle<ICommandPool>* resource)
{
	ResourceHandle<ICommandPool>* pool;

	if (!resource) pool = new ResourceHandle<ICommandPool>();
	else pool = resource;

	auto cmd = new CreateCommandPoolCommand(pool, desc);
	cmd->Execute(window.rootDevice);
	delete cmd;

	return pool;
}

ResourceHandle<IDescriptorPool>* GraphicsAdvanced::CreateDescriptorPool(DescriptorPoolDesc desc, ResourceHandle<IDescriptorPool>* resource)
{
	ResourceHandle<IDescriptorPool>* pool;

	if (!resource) pool = new ResourceHandle<IDescriptorPool>();
	else pool = resource;

	auto cmd = new CreateDescriptorPoolCommand(pool, desc);
	cmd->Execute(window.rootDevice);
	delete cmd;

	return pool;
}

ResourceHandle<IDescriptorSetLayout>* GraphicsAdvanced::CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc, ResourceHandle<IDescriptorSetLayout>* resource)
{
	ResourceHandle<IDescriptorSetLayout>* layout;

	if (!resource) layout = new ResourceHandle<IDescriptorSetLayout>();
	else layout = resource;

	auto cmd = new CreateDescriptorSetLayoutCommand(layout, desc);
	cmd->Execute(window.rootDevice);
	delete cmd;

	return layout;
}

ResourceHandle<IPipeline>* GraphicsAdvanced::CreatePipeline(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IPipeline>* resource)
{

	ResourceHandle<IPipeline>* pipeline;
	if (resource) pipeline = resource;
	else pipeline = new ResourceHandle<IPipeline>();

	commands->push_back(std::make_unique<PipelineBuildCommand>(builder, pipeline));

	return pipeline;
}

ResourceHandle<IDescriptorSet>* GraphicsAdvanced::CreateDescriptorSet(ResourceHandle<IPipeline>* pipeline, u32 slot, ResourceHandle<IDescriptorSet>* resource)
{

	ResourceHandle<IDescriptorSet>* descriptorSet;
	if (resource) descriptorSet = resource;
	else descriptorSet = new ResourceHandle<IDescriptorSet>();

	commands->push_back(std::make_unique<PipelineCreateDescriptorSetCommand>(pipeline, slot, descriptorSet));

	return descriptorSet;

}

void GraphicsAdvanced::SetDescriptorSetBuffer(ResourceHandle<IDescriptorSet>* set, ResourceHandle<IBuffer>* buffer, u32 slot)
{
	commands->push_back(std::make_unique<DescriptorSetSetBuffer>(set, slot, buffer));
}

void GraphicsAdvanced::SetDescriptorSetTexture(ResourceHandle<IDescriptorSet>* set, ResourceHandle<ITexture>* buffer, u32 slot)
{
	commands->push_back(std::make_unique<DescriptorSetSetTexture>(set, slot, buffer));
}

void GraphicsAdvanced::SetDescriptorSetSamplerState(ResourceHandle<IDescriptorSet>* set, ResourceHandle<ISamplerState>* buffer, u32 slot)
{
commands->push_back(std::make_unique<DescriptorSetSetSampler>(set, slot, buffer));
}

void GraphicsAdvanced::UpdateDescriptorSet(ResourceHandle<IDescriptorSet>* set)
{
	commands->push_back(std::make_unique<UpdateDescriptorSetCommand>(set));
}

ResourceHandle<ICommandBuffer>* GraphicsAdvanced::BeginSingleTimeCommands(ResourceHandle<ICommandPool>* pool, ResourceHandle<ICommandBuffer>* resource)
{
	ResourceHandle<ICommandBuffer>* buffer;

	if (!resource) buffer = new ResourceHandle<ICommandBuffer>();
	else buffer = resource;

	//auto cmd = new BeginSingleTimeCommandsCommand(buffer, pool);
	//cmd->Execute(window.rootDevice);
	//delete cmd;

	commands->push_back(std::make_unique<BeginSingleTimeCommandsCommand>(buffer, pool));
	return buffer;
}

void GraphicsAdvanced::EndSingleTimeCommands(ResourceHandle<ICommandBuffer>* buffer)
{
	commands->push_back(std::make_unique<EndSingleTimeCommandsCommand>(buffer));

}

void GraphicsAdvanced::ReleaseCommandPool(ResourceHandle<ICommandPool>* pool)
{
}

void GraphicsAdvanced::ReleaseDescriptorPool(ResourceHandle<IDescriptorPool>* pool)
{
	commands->push_back(std::make_unique<ReleaseDescriptorPoolCommand>(pool));
}

void GraphicsAdvanced::ReleaseDescriptorSetLayout(ResourceHandle<IDescriptorSetLayout>* layout)
{
	commands->push_back(std::make_unique<ReleaseDescriptorSetLayoutCommand>(layout));
}

void GraphicsAdvanced::ReleasePipeline(ResourceHandle<IPipeline>* pipeline)
{
	commands->push_back(std::make_unique<ReleasePipelineCommand>(pipeline));
}

void GraphicsAdvanced::ReleaseCommandList(GraphicAdvancedCommandList* list)
{
	commands->push_back(std::make_unique<ReleaseCommandListCommand>(list));
}

void GraphicsAdvanced::ReleaseDescriptorSet(ResourceHandle<IDescriptorSet>* set) {
	commands->push_back(std::make_unique<ReleaseDescriptorSetCommand>(set));
}

void GraphicsAdvanced::CopyToBuffer(ResourceHandle<IBuffer>* buffer, ResourceHandle<ICommandBuffer>* pool, void* data, size_t size)
{
	commands->push_back(std::make_unique<CopyToBufferCommandAdvanced>(buffer, pool, data, size));
}

void GraphicsAdvanced::MapBufferMemory(ResourceHandle<IBuffer>* buffer, ResourceHandle<void>* dataPtr) {
	//auto cmd = new MapBufferMemoryCommand(buffer, dataPtr);
	//cmd->Execute(window.rootDevice);
	//delete cmd;

	commands->push_back(std::make_unique<MapBufferMemoryCommand>(buffer, dataPtr));
}
void GraphicsAdvanced::UnmapBufferMemory(ResourceHandle<IBuffer>* buffer) {
	//auto cmd = new UnmapBufferMemoryCommand(buffer);
	//cmd->Execute(window.rootDevice);
	//delete cmd;

	commands->push_back(std::make_unique<UnmapBufferMemoryCommand>(buffer));
}

// void GraphicsAdvanced::MemoryCopy(ResourceHandle<void>* dataPtr, const void* src, size_t srcSize, size_t offset)
// {
// 	commands->push_back(std::make_unique<MemoryCopyCommand>(dataPtr, src, srcSize, offset));
// } - moved to memory commands

void GraphicsAdvanced::CopyBufferToImage(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<IBuffer>* srcBuffer, ResourceHandle<ITexture>* dstTexture, BufferImageCopyDesc desc)
{
	commands->push_back(std::make_unique<CopyBufferToImageCommandAdvanced>(cBuffer, srcBuffer, dstTexture, desc));
}

void GraphicsAdvanced::CopyImageToImage(ResourceHandle<ICommandBuffer>* cBuffer, ResourceHandle<ITexture>* srcBuffer, ResourceHandle<ITexture>* dstBuffer, ImageCopyDesc desc)
{
	commands->push_back(std::make_unique<CopyImageToImageCommandAdvanced>(cBuffer, srcBuffer, dstBuffer, desc));
}

void GraphicsAdvanced::BuilderAddShader(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IShader>* shader)
{
	commands->push_back(std::make_unique<PipelineAddShaderCommand>(shader, builder));
}

void GraphicsAdvanced::BuilderSetInputLayout(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IInputLayout>* layout)
{
	commands->push_back(std::make_unique<PipelineSetInputLayoutCommand>(layout, builder));
}

void GraphicsAdvanced::BuilderSetRasterizer(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IRasterizerState>* raster)
{
	commands->push_back(std::make_unique<PipelineSetRasterizerCommand>(raster, builder));
}

void GraphicsAdvanced::BuilderSetDepthStencil(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDepthStencilState>* depth)
{
	commands->push_back(std::make_unique<PipelineSetDepthStencilCommand>(depth, builder));
}

void GraphicsAdvanced::BuilderSetBlendState(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IBlendState>* blend)
{
	commands->push_back(std::make_unique<PipelineSetBlendStateCommand>(blend, builder));
}

void GraphicsAdvanced::BuilderSetDescriptorPool(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDescriptorPool>* pool)
{
	commands->push_back(std::make_unique<PipelineSetDescriptorPoolCommand>(pool, builder));
}

void GraphicsAdvanced::BuilderSetDescriptorSetLayout(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IDescriptorSetLayout>* layout)
{
	commands->push_back(std::make_unique<PipelineSetDescriptorSetLayoutCommand>(layout, builder));
}

void GraphicsAdvanced::BuilderSetRenderTarget(ResourceHandle<IPipelineBuilder>* builder, ResourceHandle<IRenderTarget>* target)
{
	//commands->push_back(std::make_unique<PipelineSetRenderTargetCommand>(builder, target));
}

PipelineBuilderImmediate& PipelineBuilderImmediate::AddShader(IShader* shader)
{
	/*vector->push_back(std::make_unique<PipelineAddShaderCommand>(shader, this));*/
	shaders.push_back(shader);
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetInputLayout(IInputLayout* layout)
{
	inputLayout = layout;
	//vector->push_back(std::make_unique<PipelineSetInputLayoutCommand>(layout, this));
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetRasterizer(IRasterizerState* raster)
{
	//vector->push_back(std::make_unique<PipelineSetRasterizerCommand>(raster, this));
	rasterizer = raster;
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetDepthStencil(IDepthStencilState* depth)
{
	//vector->push_back(std::make_unique<PipelineSetDepthStencilCommand>(depth, this));
	depthStencil = depth;
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetBlendState(IBlendState* blend)
{
	blendState = blend;
	//vector->push_back(std::make_unique<PipelineSetBlendStateCommand>(blend, this));
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetDescriptorPool(IDescriptorPool* pool)
{
	descriptorPool = pool;
	//vector->push_back(std::make_unique<PipelineSetDescriptorPoolCommand>(pool, this));
	return *this;
}

PipelineBuilderImmediate& PipelineBuilderImmediate::SetDescriptorSetLayout(IDescriptorSetLayout* layout)
{
	//vector->push_back(std::make_unique<PipelineSetDescriptorSetLayoutCommand>(layout, this));
	descriptorSetLayout = layout;
	return *this;
}

IPipeline* PipelineBuilderImmediate::Build()
{
	if (!inputLayout || !rasterizer || !depthStencil || !blendState) {
		delete this;
		return nullptr;
	}
;

	auto pl = new PipelineImmediate(
		descriptorSetLayout,
		descriptorPool,
		shaders,
		inputLayout,
		rasterizer,
		depthStencil,
		blendState,
		renderPass,
		vector
	);

	delete this;
	return pl;

}

IDescriptorSet* PipelineImmediate::CreateDescriptorSet(u32 slot)
{
	auto ds = new DescriptorSetImmediate(this);
	descriptorSets[slot] = ds;
	return ds;
}
