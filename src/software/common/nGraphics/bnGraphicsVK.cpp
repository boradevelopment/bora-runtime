#pragma once
#include "bnGraphicsVK.h"

static PFN_vkCmdBeginDebugUtilsLabelEXT fpCmdBeginDebugUtilsLabelEXT = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT   fpCmdEndDebugUtilsLabelEXT   = nullptr;
static PFN_vkCmdInsertDebugUtilsLabelEXT fpCmdInsertDebugUtilsLabelEXT = nullptr;

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() const {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        // Check for graphics support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (indices.graphicsFamily < 0)
                indices.graphicsFamily = i;
        }

        // Check for present support
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            if (indices.presentFamily < 0)
                indices.presentFamily = i;
        }

        // Stop early if both found
        if (indices.isComplete())
            break;
    }

    if (!indices.isComplete())
        return {};

    return indices;
}


VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    // Redirect to std::cout
    std::cout << "[Vulkan] " << pCallbackData->pMessage << std::endl;

    return VK_FALSE; // return true to abort the call that triggered the validation message
}

VkDebugUtilsMessengerEXT debugMessenger;

//#ifdef _DEBUG
void setupDebugMessenger(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        if (func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
}
//#endif

bnGraphicsVK::bnGraphicsVK(SysHandle& handle, IGraphicsDeviceConfig& config) : handle(handle), config(config)
{
  
}
#pragma optimize("", off)
bool bnGraphicsVK::Init()
{
    fpCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
    fpCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
    fpCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT");

    if (!config.enableMSAA) {
        config.msaaSamples = 1;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Bora Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "BNGL";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    // TODO: add extensions and validation layers
    std::vector<const char*> extensions;
    extensions.push_back("VK_KHR_surface");
#ifdef _WIN32
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
    extensions.push_back("VK_KHR_xlib_surface"); // or VK_KHR_wayland_surface
    // i will have to probably confirm with linux desktop
#endif
#ifdef _DEBUG
    if (config.enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    std::vector<const char*> validationLayers;
#ifdef _DEBUG
    if (config.enableValidation) {
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif

    instanceInfo.enabledLayerCount = (uint32_t)validationLayers.size();
    instanceInfo.ppEnabledLayerNames = validationLayers.empty() ? nullptr : validationLayers.data();

    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();


    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }

#ifdef _DEBUG
    setupDebugMessenger(instance);
#endif

#ifdef WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = handle;

    if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS) {
        return false;
    }
#endif

    // --- 2. Select Physical Device ---
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // pick preferredGPUIndex or first devi ce
    physicalDevice = devices[0];

    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts =
        physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    config.msaaSamples = 1; // fallback
    if (counts & 8) config.msaaSamples = 8;
    else if (counts & 4) config.msaaSamples = 4;
    else if (counts & 2) config.msaaSamples = 2;

    VkDeviceQueueCreateInfo queueInfo{};
    float queuePriority = 1.0f;
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = 0; // TODO: select graphics queue family properly
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    // TODO: enable device features (MSAA, anisotropy, etc.)
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    std::vector<const char*> deviceLayers;
    deviceLayers.push_back("VK_LAYER_KHRONOS_validation");

    deviceInfo.enabledLayerCount = static_cast<uint32_t>(deviceLayers.size());
    deviceInfo.ppEnabledLayerNames = deviceLayers.data();

    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device.device) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(device, 0, 0, &graphicsQueue);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    config.framesInFlight--;

    if (surfaceCapabilities.minImageCount > config.framesInFlight) {
        config.framesInFlight = surfaceCapabilities.minImageCount;
    }

    // --- 4. Create Command Pool ---

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0; // same as graphics queue
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    commandBuffer = std::vector<DeviceContextVK>(config.framesInFlight);
    commandLists = std::vector<CommandListVK*>(config.framesInFlight);


    for (int i = 0; i < config.framesInFlight; i++) {
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandBuffer[i].commandPool) != VK_SUCCESS) {
            return false;
        }
    }


    copyPool = (CommandPoolVK*)CreateCommandPool({
            0, true, true
        });

    for (int i = 0; i < config.framesInFlight; i++) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandBuffer[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary or secondary
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer[i].deviceC) != VK_SUCCESS) {
            return false;
        }
    }

    imageAvailableSemaphores = std::vector<VkSemaphore>(config.framesInFlight);
    renderFinishedSemaphores = std::vector<VkSemaphore>(config.framesInFlight);
    inFlightFences = std::vector<VkFence>(config.framesInFlight);
    currentFrame = 0;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled so first frame doesn't block

    for (int i = 0; i < config.framesInFlight; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores/fence!");
        }
    }

    return true;
}

#pragma optimize("", on)


ICommandList* bnGraphicsVK::GetCommandList()
{
    if (!commandBuffer[currentFrame].deviceC) return nullptr;
    if (!commandLists[currentFrame]) {
        commandLists[currentFrame] = new CommandListVK(device, commandBuffer[currentFrame], &releaseCommandBuffers, &pDraws);
    }
    return commandLists[currentFrame];
}

void bnGraphicsVK::DestroyPending()
{
    WaitTillImFree();
    ClearPendingReleases();
}

void bnGraphicsVK::BeginFrame()
{
   
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    vkDeviceWaitIdle(device);
    ClearPendingReleases();
    free = true;
   
    // now allow waitforframe to finish

    auto result = vkAcquireNextImageKHR(
        device,
        swapChain,
        UINT64_MAX,            // timeout
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result != VK_SUCCESS) {
        return;
    }
   

    vkResetCommandPool(device, commandBuffer[imageIndex], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // good for setup operations

    vkBeginCommandBuffer(commandBuffer[imageIndex], &beginInfo);
    free = false;
}

void TransitionImageLayout(
    VkCommandBuffer cmd,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    // Determine aspect
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        // handle stencil if needed
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Source and destination access masks
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        // handle other transitions as needed
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(
        cmd,
        srcStage, dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}


void bnGraphicsVK::EndFrame()
{
    TransitionImageLayout(
        commandBuffer[imageIndex].deviceC,
        swapChainImages[currentFrame],
        swapchainImageFormat,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    {
        std::array<VkClearValue, 3> clearValues{};
        if (config.enableMSAA) {
            clearValues[0].color = { {config.clearColor.r, config.clearColor.g, config.clearColor.b, 1.0f} };
            clearValues[2].depthStencil.depth = 1.0f;  // far plane
            clearValues[2].depthStencil.stencil = 0;
        }
        else {
            clearValues[0].color = { {config.clearColor.r, config.clearColor.g, config.clearColor.b, 1.0f} };
            clearValues[1].depthStencil.depth = 1.0f;  // far plane
            clearValues[1].depthStencil.stencil = 0;
        }


        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = swpchTarget->renderPass;  // your render pass
        renderPassInfo.framebuffer = swapChainFrameBuffers[imageIndex]; // framebuffer tied to this swapchain image
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Start render pass (this performs the clear!)
        vkCmdBeginRenderPass(commandBuffer[imageIndex].deviceC, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkRenderPass cRenderPass = swpchTarget->renderPass;
   
    for (auto draw : pDraws) {
        bool hasCHanged = false;
        if (draw.pipeline) {
            PipelineVK* vkPipeline = static_cast<PipelineVK*>(draw.pipeline);

            if (!vkPipeline || !vkPipeline->pipeline) return;
            //if (cRenderPass != swpchTarget->renderPass) {
                if (vkPipeline->renderTarget && vkPipeline->renderTarget->GetNativeHandle() && cRenderPass != (VkRenderPass)vkPipeline->renderTarget->GetNativeHandle()) {
                    cRenderPass = (VkRenderPass)vkPipeline->renderTarget->GetNativeHandle();
                    std::array<VkClearValue, 3> clearValues{};
                    hasCHanged = true;
                    if(config.enableMSAA){
                        clearValues[0].color = { {config.clearColor.r, config.clearColor.g, config.clearColor.b, 1.0f} };
                        clearValues[2].depthStencil.depth = 1.0f;  // far plane
                        clearValues[2].depthStencil.stencil = 0;
                    }
                    else {
                        clearValues[0].color = { {config.clearColor.r, config.clearColor.g, config.clearColor.b, 1.0f} };
                        clearValues[1].depthStencil.depth = 1.0f;  // far plane
                        clearValues[1].depthStencil.stencil = 0;
                    }


                    VkRenderPassBeginInfo renderPassInfo{};
                    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    renderPassInfo.renderPass = cRenderPass;  // your render pass
                    renderPassInfo.framebuffer = swapChainFrameBuffers[imageIndex]; // framebuffer tied to this swapchain image
                    renderPassInfo.renderArea.offset = { 0, 0 };
                    renderPassInfo.renderArea.extent = swapChainExtent;
                    renderPassInfo.clearValueCount = 2;
                    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                    renderPassInfo.pClearValues = clearValues.data();

                    // Start render pass (this performs the clear!)
                    vkCmdBeginRenderPass(commandBuffer[imageIndex].deviceC, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                }
         //   }
        
        }

        if (draw.buffer) {
            BufferVK* vkBuffer = static_cast<BufferVK*>(draw.buffer);
          VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(draw.cmdBuffer, 0, 1, &vkBuffer->buffer, offsets);
        }
        if (draw.ds) {
            DescriptorSetVK* vkSet = static_cast<DescriptorSetVK*>(draw.ds);

            if (!vkSet || !vkSet->pipeline) return;
        
            vkCmdBindDescriptorSets(draw.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vkSet->pipeline->layout,
                        0, 1, &vkSet->descriptorSet, 0, nullptr);
              
        }

        if (draw.pipeline) {
            PipelineVK* vkPipeline = static_cast<PipelineVK*>(draw.pipeline);

            if (!vkPipeline || !vkPipeline->pipeline) return;


            vkCmdBindPipeline(draw.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vkPipeline->pipeline);
        }
    
        if (draw.viewport) {
            auto* viewport = static_cast<ViewportVK*>(draw.viewport);
            if (!viewport) return;

            vkCmdSetViewport(draw.cmdBuffer, 0, 1, &viewport->viewport);
        }
        if (draw.scissor) {
            auto* viewport = static_cast<ViewportVK*>(draw.scissor);
            if (!viewport) return;

            vkCmdSetScissor(draw.cmdBuffer, 0, 1, &viewport->scissor);
        }
        if (draw.indexBuffer) {
            BufferVK* vkBuffer = static_cast<BufferVK*>(draw.indexBuffer);

            vkCmdBindIndexBuffer(draw.cmdBuffer, vkBuffer->buffer, 0,
                VK_INDEX_TYPE_UINT32);

            VkPrimitiveTopology topology = ToVkPrimitiveTopology(draw.type);

            vkCmdSetPrimitiveTopology(draw.cmdBuffer, topology);

            vkCmdDrawIndexed(draw.cmdBuffer, static_cast<uint32_t>(draw.indexCount), 1,
                static_cast<uint32_t>(draw.indexOffset), 0, 0);
        }
        else {
            VkPrimitiveTopology topology = ToVkPrimitiveTopology(draw.type);

            // Topology is usually dynamic if pipeline allows
            vkCmdSetPrimitiveTopology(draw.cmdBuffer, topology);


            vkCmdDraw(draw.cmdBuffer, static_cast<uint32_t>(draw.vertexCount),
                1, static_cast<uint32_t>(draw.vertexOffset), 0);
        }
        if (cRenderPass != swpchTarget->renderPass) {
            vkCmdEndRenderPass(commandBuffer[imageIndex].deviceC);
        }

    }

    if (cRenderPass == swpchTarget->renderPass) {
        vkCmdEndRenderPass(commandBuffer[imageIndex].deviceC);
    }
    pDraws.clear();

    TransitionImageLayout(
        commandBuffer[imageIndex].deviceC,
        swapChainImages[currentFrame],
        swapchainImageFormat,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    vkEndCommandBuffer(commandBuffer[imageIndex]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait for the image to be available
    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    // Command buffer
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer[imageIndex].deviceC;

    // Signal when rendering finished
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

}

void bnGraphicsVK::Present()
{
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // wait for rendering to finish

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;


    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    // Optional: handle VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Recreate swapchain
        Resize(width, height);
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    
    // Advance current frame index
    currentFrame = (currentFrame + 1) % config.framesInFlight;
}

void bnGraphicsVK::Resize(long width, long height)
{
    if (width == this->width && height == this->height) return;

    this->width = width;
    this->height = height;
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkDeviceWaitIdle(device);
  

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
    }
    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depthImageMemory, nullptr);
    }

    for (auto fb : swapChainFrameBuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    swapChainFrameBuffers.clear();

    for (auto view : swapchainImageViews) {
        vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();

    if (msaaSWPCHTarget) {
        msaaSWPCHTarget->Release();
        delete msaaSWPCHTarget;
    }


    if (depthTexture) {
        depthTexture->Release();
        delete depthTexture;
    }

    if (depthStencil) {
        vkDestroyImageView(device, ((DepthStencilVK*)depthStencil)->depthView, nullptr);
        delete depthStencil;
    }


    CreateSwapChain();
    CreateRenderPass();
    CreateFrameBuffers();

    currentFrame = 0;
}

void bnGraphicsVK::ReleaseShader(IShader** shader)
{
    auto vkShader = dynamic_cast<ShaderVK*>(*shader);
    if (!vkShader) return;

    shaderRelease.push_back(shader);
}

void bnGraphicsVK::ReleaseBuffer(IBuffer** vBuffer)
{
  //  auto vkBuffer = dynamic_cast<BufferVK**>(vBuffer);
    if (!vBuffer) return;

    bufferRelease.push_back(vBuffer);
}

void bnGraphicsVK::ReleaseTexture(ITexture** texture)
{
    if (!texture) return;

    textureRelease.push_back(texture);
}

void bnGraphicsVK::ReleaseCommandPool(ICommandPool** pool)
{
    if (!pool) return;
    poolRelease.push_back(pool);
}

void bnGraphicsVK::ReleaseDescriptorPool(IDescriptorPool** pool)
{
    if (!pool) return;
    descriptorPoolRelease.push_back(pool);
}

void bnGraphicsVK::ReleaseDescriptorSetLayout(IDescriptorSetLayout** layout)
{
    if (!layout) return;
    descriptorSetLayoutRelease.push_back(layout);
}

void bnGraphicsVK::ReleaseOnPend(void* v)
{
    if (!v) return;
    pendVoids.push_back(v);
}

void bnGraphicsVK::Shutdown()
{
    dontClearYet = false;
    
    vkDeviceWaitIdle(device);

    // Destroy
    for (auto fence : inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device, fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
    }

    for (auto seph : renderFinishedSemaphores) {
        vkDestroySemaphore(device, seph, nullptr);
        seph = VK_NULL_HANDLE;
    }

    for (auto seph : imageAvailableSemaphores) {
        vkDestroySemaphore(device, seph, nullptr);
         seph = VK_NULL_HANDLE;
    }

    ClearPendingReleases();

    // 2. Destroy framebuffers
    for (auto fb : swapChainFrameBuffers) {
        if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(device, fb, nullptr);
    }
    swapChainFrameBuffers.clear();

    // 3. Destroy image views
    for (auto view : swapchainImageViews) {
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();

    if (currentSwapChainImage) {
        vkDestroyImageView(device, currentSwapChainImage->imageView, nullptr);
        delete currentSwapChainImage;
    }

    // 4. Destroy depth image, view, and free memory
    if (depthImageView != VK_NULL_HANDLE)
        vkDestroyImageView(device, depthImageView, nullptr);

    if (depthImage != VK_NULL_HANDLE)
        vkDestroyImage(device, depthImage, nullptr);

    if (depthImageMemory != VK_NULL_HANDLE)
        vkFreeMemory(device, depthImageMemory, nullptr);

    // 5. Destroy swapchain
    if (swapChain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, swapChain, nullptr);

    for (auto pool : commandBuffer) {
        vkFreeCommandBuffers(
            device,            // VkDevice
            pool,       // VkCommandPool that allocated them
            1, // number of buffers
            &pool.deviceC // pointer to the buffers
        );
        if (pool.commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, pool, nullptr);
    }

    if (depthTexture) {
        depthTexture->Release();
        delete depthTexture;
    }

    if (depthStencil) {
        vkDestroyImageView(device, ((DepthStencilVK*)depthStencil)->depthView, nullptr);
        delete depthStencil;
    }

    for (auto& list : commandLists) {
        delete list;
    }
    
    commandBuffer.clear();

    // Destroy descriptor pool
    //if (descriptorPool != VK_NULL_HANDLE) {
    //    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    //    descriptorPool = VK_NULL_HANDLE;
    //}

    //// Destroy descriptor set layout
    //if (textureSetLayout != VK_NULL_HANDLE) {
    //    vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
    //    textureSetLayout = VK_NULL_HANDLE;
    //}

    if (copyPool->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, copyPool->commandPool, nullptr);
        delete copyPool;
        copyPool = VK_NULL_HANDLE;
    }

    if (swpchTarget->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, swpchTarget->renderPass, nullptr);
        swpchTarget->Release();
        delete swpchTarget;
    }

    // 10. Finally destroy the Vulkan device
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device.device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    // Debug messenger (validation layers)
    if (debugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, nullptr);
        }
    }

    // Optionally destroy the Vulkan instance if you created one
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

VkFormat ToVkFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::RGBA8_UNorm:        return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_UNorm_SRGB:   return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureFormat::BGRA8_UNorm:        return VK_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8_UNorm_SRGB:   return VK_FORMAT_B8G8R8A8_SRGB;

    case TextureFormat::RGBA16_Float:       return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::R16_Float:          return VK_FORMAT_R16_SFLOAT;
    case TextureFormat::R16_UNorm:          return VK_FORMAT_R16_UNORM;

    case TextureFormat::R32_Float:          return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG32_Float:         return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGBA32_Float:       return VK_FORMAT_R32G32B32A32_SFLOAT;

    case TextureFormat::D24_UNorm_S8_UInt:  return VK_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32_Float:          return VK_FORMAT_D32_SFLOAT;
    case TextureFormat::D32_Float_S8X24_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;

    default:
        throw std::runtime_error("Unsupported TextureFormat for Vulkan!");
    }
}

TextureFormat FromVkFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:       return TextureFormat::RGBA8_UNorm;
    case VK_FORMAT_R8G8B8A8_SRGB:        return TextureFormat::RGBA8_UNorm_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:       return TextureFormat::BGRA8_UNorm;
    case VK_FORMAT_B8G8R8A8_SRGB:        return TextureFormat::BGRA8_UNorm_SRGB;

    case VK_FORMAT_R16G16B16A16_SFLOAT:  return TextureFormat::RGBA16_Float;
    case VK_FORMAT_R16_SFLOAT:           return TextureFormat::R16_Float;
    case VK_FORMAT_R16_UNORM:            return TextureFormat::R16_UNorm;

    case VK_FORMAT_R32_SFLOAT:           return TextureFormat::R32_Float;
    case VK_FORMAT_R32G32_SFLOAT:        return TextureFormat::RG32_Float;
    case VK_FORMAT_R32G32B32A32_SFLOAT:  return TextureFormat::RGBA32_Float;

    case VK_FORMAT_D24_UNORM_S8_UINT:    return TextureFormat::D24_UNorm_S8_UInt;
    case VK_FORMAT_D32_SFLOAT:           return TextureFormat::D32_Float;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:   return TextureFormat::D32_Float_S8X24_UInt;

    default:
        throw std::runtime_error("Unsupported VkFormat for TextureFormat!");
    }
}



IPipelineBuilder* bnGraphicsVK::CreatePipelineBuilder()
{
    return new PipelineBuilderVK(config, &releasePipelines, device, (RenderTargetVK*)swpchTarget);
}

bool IsDepthFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

VkImageAspectFlags GetAspectFlags(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}



ITexture* bnGraphicsVK::CreateTexture(const TextureDesc& desc, const void* initialData)
{
    auto tex = new TextureVK();
    tex->desc = desc;
    tex->slot = desc.slot;
    tex->device = device.device;
    tex->explicitLayout = ImageLayout::Undefined;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = (desc.dimension == TextureDimensions::Dim1) ? 1 : desc.height;
    imageInfo.extent.depth = (desc.dimension == TextureDimensions::Dim3) ? desc.depth : 1;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = ToVkFormat(desc.format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = IntToVkSampleCount(desc.samples);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (IsDepthFormat(imageInfo.format))
    {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else
    {
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }


    switch (desc.dimension) {
    case TextureDimensions::Dim1: imageInfo.imageType = VK_IMAGE_TYPE_1D; break;
    case TextureDimensions::Dim2: imageInfo.imageType = VK_IMAGE_TYPE_2D; break;
    case TextureDimensions::Dim3: imageInfo.imageType = VK_IMAGE_TYPE_3D; break;
    }

    auto res = vkCreateImage(device, &imageInfo, nullptr, &tex->image);

    if (res != VK_SUCCESS) {
        delete tex;
        return nullptr;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, tex->image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &tex->memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory!");

    vkBindImageMemory(device, tex->image, tex->memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange.aspectMask = GetAspectFlags(imageInfo.format);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &tex->imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view!");
    return tex;
}

IShader* bnGraphicsVK::CreateShader(const ShaderDesc& desc)
{
    return new ShaderVK(device, desc);
}

IBuffer* bnGraphicsVK::CreateBuffer(const BufferDesc& desc, const void* data)
{
    auto bufferVK = new BufferVK(device, desc);
    bufferVK->slot = desc.slot;

    VkBufferUsageFlags usage = 0;
    switch (desc.type)
    {
    case BufferType::Vertex:   usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
    case BufferType::Index:    usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
    case BufferType::Constant: usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
    case BufferType::Staging:
        usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    }

    if (data && desc.type != BufferType::Staging)
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.byteWidth == 0 ? desc.size * desc.stride : desc.byteWidth;
    if (bufferInfo.size == 0) {
        bufferInfo.size = desc.size;
    }
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &bufferVK->buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryPropertyFlags props;
    if (desc.type == BufferType::Staging) {
        props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else {
        if (desc.dynamic) {
            props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        } else props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }



    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, bufferVK->buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memRequirements.memoryTypeBits,
        props
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferVK->memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(device, bufferVK->buffer, bufferVK->memory, 0);

    // Upload initial data
    if (data)
    {
        if (config.clipSpace == VerticesClipSpace::D3D) {
            if (desc.type == BufferType::Vertex) {
                bufferVK->dataBuffer = std::vector<u8>(bufferInfo.size);
                std::memcpy(bufferVK->dataBuffer.data(), data, bufferVK->dataBuffer.size());

                for (size_t i = 0; i < desc.size; i++) {
                    float* vertex = reinterpret_cast<float*>(bufferVK->dataBuffer.data() + i * desc.stride);

                    if (desc.stride >= sizeof(float) * 2) {
                        vertex[1] = -vertex[1]; // Flip Y
                    }
                }
            }
        }

        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            void* mapped;
            vkMapMemory(device, bufferVK->memory, 0, bufferInfo.size, 0, &mapped);
            memcpy(mapped, bufferVK->dataBuffer.empty() ? data : bufferVK->dataBuffer.data(), bufferInfo.size);
            vkUnmapMemory(device, bufferVK->memory);
        }
        else {
            // Add create staging buffer and copy here
            VkBufferCreateInfo stagingInfo{};
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingInfo.size = bufferInfo.size;
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &stagingInfo, nullptr, &bufferVK->stagingBuffer) != VK_SUCCESS)
                throw std::runtime_error("Failed to create staging buffer");

            VkMemoryRequirements stagingMemReq;
            vkGetBufferMemoryRequirements(device, bufferVK->stagingBuffer, &stagingMemReq);

            VkMemoryAllocateInfo stagingAlloc{};
            stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            stagingAlloc.allocationSize = stagingMemReq.size;
            stagingAlloc.memoryTypeIndex = FindMemoryType(
                stagingMemReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );

            if (vkAllocateMemory(device, &stagingAlloc, nullptr, &bufferVK->stagingMemory) != VK_SUCCESS)
                throw std::runtime_error("Failed to allocate staging buffer memory");

            vkBindBufferMemory(device, bufferVK->stagingBuffer, bufferVK->stagingMemory, 0);

            void* mapped;
            vkMapMemory(device, bufferVK->stagingMemory, 0, bufferInfo.size , 0, &mapped);
            memcpy(mapped, bufferVK->dataBuffer.empty() ? data : bufferVK->dataBuffer.data(), bufferInfo.size);
            vkUnmapMemory(device, bufferVK->stagingMemory);

            auto copyCmd = (CommandBufferVK*)BeginSingleTimeCommands(copyPool);

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = bufferInfo.size;
            vkCmdCopyBuffer(copyCmd->buffer, bufferVK->stagingBuffer, bufferVK->buffer, 1, &copyRegion);

            EndSingleTimeCommands(copyCmd);
        }
    }

    return bufferVK;
}

IInputLayout* bnGraphicsVK::CreateInputLayout(const InputLayoutDesc& desc)
{
    return new InputLayoutVK(desc);
}

ISamplerState* bnGraphicsVK::CreateSamplerState(const SamplerStateDesc& desc)
{
    return new SamplerVK(device, desc, &samplerRelease);
}

IViewPort* bnGraphicsVK::CreateViewPort(const ViewPortDesc& desc)
{
    return new ViewportVK(desc);
}

IRasterizerState* bnGraphicsVK::CreateRasterizerState(const RasterizerDesc& desc)
{
    return new RasterizerVK(desc);
}

IDepthStencilState* bnGraphicsVK::CreateDepthStencilState(const DepthStencilDesc& desc)
{
    return new DepthStencilStateVK(desc);
}

IBlendState* bnGraphicsVK::CreateBlendState(const BlendStateDesc& desc)
{
    return new BlendStateVK(desc);
}

VkImageUsageFlags ToVkUsage(ImageUsage usage) {
    VkImageUsageFlags flags = 0;
    if ((usage & ImageUsage::ColorAttachment) != ImageUsage::None) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((usage & ImageUsage::DepthStencil) != ImageUsage::None) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((usage & ImageUsage::Sampled) != ImageUsage::None) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((usage & ImageUsage::Storage) != ImageUsage::None) flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if ((usage & ImageUsage::TransferSrc) != ImageUsage::None) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((usage & ImageUsage::TransferDst) != ImageUsage::None) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((usage & ImageUsage::InputAttachment) != ImageUsage::None) flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return flags;
}

VkImageLayout ToVkLayout(ImageLayout layout) {
    switch (layout) {
    case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageLayout::RenderTarget: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageLayout::DepthStencil: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageLayout::ShaderRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case ImageLayout::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageLayout::CopyDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}


IRenderTarget* bnGraphicsVK::CreateRenderTarget(const RenderTargetDesc& desc)
{
    auto renderTarget = new RenderTargetVK();
    renderTarget->desc = desc;

    size_t colorCount = desc.colorTargets.size();
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs(colorCount);
    std::vector<VkAttachmentReference> resolveRefs(colorCount);

    for (size_t i = 0; i < colorCount; ++i) {
        auto vkTexture = dynamic_cast<TextureVK*>(desc.colorTargets[i]);
        if (!vkTexture) continue;

        renderTarget->textures.push_back(vkTexture);

        // --- MSAA / multisampled attachment ---
        VkAttachmentDescription attach{};
        attach.format = ToVkFormat(vkTexture->desc.format);
        attach.samples = IntToVkSampleCount(vkTexture->desc.samples);
        attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attach.finalLayout = ToVkLayout(desc.colorLayout[i]);;

        attachments.push_back(attach);

        VkAttachmentReference ref{};
        ref.attachment = static_cast<uint32_t>(attachments.size() - 1);
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRefs[i] = ref;

        // --- Resolve attachment for MSAA (only if samples > 1) ---
        if (vkTexture->desc.samples > 1) {
            VkAttachmentDescription resolveAttach{};
            resolveAttach.format = ToVkFormat(vkTexture->desc.format);
            resolveAttach.samples = VK_SAMPLE_COUNT_1_BIT;
            resolveAttach.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            resolveAttach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            resolveAttach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveAttach.finalLayout = ToVkLayout(desc.colorLayout[i]);

            attachments.push_back(resolveAttach); // <-- must push into main attachments

            VkAttachmentReference resolveRef{};
            resolveRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            resolveRefs[i] = resolveRef;
        }
        else {
            resolveRefs[i].attachment = VK_ATTACHMENT_UNUSED;
            resolveRefs[i].layout = VK_IMAGE_LAYOUT_UNDEFINED; // optional
        }
   
    }

    // ---------------------- Depth Attachment ----------------------
    VkAttachmentReference depthRef{};
    bool hasDepth = false;
    if (desc.depth) {
        auto vkDepth = dynamic_cast<DepthStencilVK*>(desc.depth);
        if (vkDepth) {
            hasDepth = true;

            VkAttachmentDescription attach{};
            attach.format = ToVkFormat(vkDepth->texture->desc.format);
            attach.samples = IntToVkSampleCount(vkDepth->texture->desc.samples);
            attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attach.finalLayout = ToVkLayout(desc.depthLayout);

            attachments.push_back(attach);

            depthRef.attachment = static_cast<uint32_t>(attachments.size() - 1);
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            renderTarget->depth = vkDepth;
        }
    }

    // ---------------------- Subpass ----------------------
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    if (hasDepth) subpass.pDepthStencilAttachment = &depthRef;
    if (!resolveRefs.empty()) {
        subpass.pResolveAttachments = resolveRefs.data();
    }

    // ---------------------- Render Pass ----------------------
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderTarget->renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    // ---------------------- Framebuffer ----------------------
    if (desc.makeFramebuffer) {
        std::vector<VkImageView> fbAttachments;
        for (auto tex : renderTarget->textures)
            fbAttachments.push_back(tex->imageView);
        if (hasDepth)
            fbAttachments.push_back(renderTarget->depth->depthView);

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderTarget->renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
        fbInfo.pAttachments = fbAttachments.data();
        fbInfo.width = desc.width ? desc.width : renderTarget->textures[0]->desc.width;
        fbInfo.height = desc.height ? desc.height : renderTarget->textures[0]->desc.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &renderTarget->framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    return renderTarget;
}

IDepthStencil* bnGraphicsVK::CreateDepthStencil(ITexture* texture)
{
    auto vkTexture = dynamic_cast<TextureVK*>(texture);
    if (!vkTexture) return nullptr;

    auto format = ToVkFormat(texture->desc.format);

    if (format != VK_FORMAT_D32_SFLOAT && format != VK_FORMAT_D24_UNORM_S8_UINT) {
        return nullptr;
    }

    auto depthStencil = new DepthStencilVK();
    depthStencil->texture = texture;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = vkTexture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format == VK_FORMAT_D24_UNORM_S8_UINT)
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &depthStencil->depthView);
    if (res != VK_SUCCESS) {
        delete depthStencil;
        return nullptr;
    }

    return depthStencil;
}

ICommandPool* bnGraphicsVK::CreateCommandPool(CommandPoolDesc desc)
{
    return new CommandPoolVK(device, desc);
}

IDescriptorPool* bnGraphicsVK::CreateDescriptorPool(DescriptorPoolDesc desc)
{
    return new DescriptorPoolVK(device, desc);
}

IDescriptorSetLayout* bnGraphicsVK::CreateDescriptorSetLayout(DescriptorSetLayoutDesc desc)
{
    return new DescriptorSetLayoutVK(device, desc);
}

void bnGraphicsVK::WaitForNewFrame()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkDeviceWaitIdle(device);

    return;
}

ICommandBuffer* bnGraphicsVK::BeginSingleTimeCommands(ICommandPool* pool)
{
    auto vPool = dynamic_cast<CommandPoolVK*>(pool);
    if (!vPool) return nullptr;


   VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vPool->commandPool;
    allocInfo.commandBufferCount = 1;

  
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);

    return new CommandBufferVK(cmd, vPool->commandPool);
}

void bnGraphicsVK::EndSingleTimeCommands(ICommandBuffer* buffer)
{
    auto vCmdBuffer = dynamic_cast<CommandBufferVK*>(buffer);
    if (!vCmdBuffer) return;

    vkEndCommandBuffer(vCmdBuffer->buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vCmdBuffer->buffer;



    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

    cbRelease.push_back(vCmdBuffer); 

    //vkQueueWaitIdle(graphicsQueue);

    //vkFreeCommandBuffers(device, vCmdBuffer->pool, 1, &vCmdBuffer->buffer);
    //delete buffer;
    //buffer = nullptr;
}

void bnGraphicsVK::CopyToBuffer(IBuffer* buffer, ICommandBuffer* cmd, void* data, size_t size)
{
    auto vBuffer = dynamic_cast<BufferVK*>(buffer);
    auto vCommand = dynamic_cast<ICommandBuffer*>(cmd);

    if (!vBuffer || !vCommand) return;

    if (vBuffer->type == BufferType::Staging) {
        void* mapped = nullptr;
        vkMapMemory(device, vBuffer->memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(device, vBuffer->memory);
    }
    else {
        BufferDesc stagingDesc = {};
        stagingDesc.size = size;
        stagingDesc.type = BufferType::Staging;

        vBuffer->mStagingBuffer = CreateBuffer(stagingDesc);

        // Upload data to staging
        void* mapped;
        vkMapMemory(device, ((BufferVK*)vBuffer->mStagingBuffer)->memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(device, ((BufferVK*)vBuffer->mStagingBuffer)->memory);

        // --- Record copy command ---
        VkCommandBuffer cmdBuf = (VkCommandBuffer)cmd->GetNativeHandle();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(cmdBuf, ((BufferVK*)vBuffer->mStagingBuffer)->buffer, vBuffer->buffer, 1, &copyRegion);
        //Release the buffer now
        ReleaseBuffer(&vBuffer->mStagingBuffer);
        /*vBuffer->mStagingBuffer->Release();
        delete vBuffer->mStagingBuffer;
        vBuffer->mStagingBuffer = nullptr;*/
    }
}

void bnGraphicsVK::MapBufferMemory(IBuffer* buffer, void** dataPtr)
{
    auto vkBuffer = dynamic_cast<BufferVK*>(buffer);
    if (!vkBuffer) return;

    vkMapMemory(device, vkBuffer->memory, 0, VK_WHOLE_SIZE, 0, dataPtr);
}

void bnGraphicsVK::UnmapBufferMemory(IBuffer* buffer)
{
    auto vkBuffer = dynamic_cast<BufferVK*>(buffer);
    if (!vkBuffer) return;

    vkUnmapMemory(device, vkBuffer->memory);
}

void bnGraphicsVK::CopyBufferToImage(ICommandBuffer* cBuffer, IBuffer* srcBuffer, ITexture* dstTexture, BufferImageCopyDesc desc)
{
    auto vkCBuffer = dynamic_cast<CommandBufferVK*>(cBuffer);
    auto vkSrcBuffer = dynamic_cast<BufferVK*>(srcBuffer);
    auto vkDstTexture = dynamic_cast<TextureVK*>(dstTexture);

    if (!vkCBuffer || !vkSrcBuffer || !vkDstTexture) return;

    if (desc.bufferRowLength > desc.imageExtent.width) {
        desc.bufferRowLength = desc.imageExtent.width;
    }

    if (desc.bufferImageHeight > desc.imageExtent.height) {
        desc.bufferImageHeight = desc.imageExtent.height;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = desc.bufferOffset;
    region.bufferRowLength = desc.bufferRowLength;
    region.bufferImageHeight = desc.bufferImageHeight;

    // Subresource
    region.imageSubresource.aspectMask = desc.imageSubresource.aspectMask;
    region.imageSubresource.mipLevel = desc.imageSubresource.mipLevel;
    region.imageSubresource.baseArrayLayer = desc.imageSubresource.baseArrayLayer;
    region.imageSubresource.layerCount = desc.imageSubresource.layerCount;

    // Offsets
    region.imageOffset.x = desc.imageOffset.x;
    region.imageOffset.y = desc.imageOffset.y;
    region.imageOffset.z = desc.imageOffset.z;

    // Extents
    region.imageExtent.width = desc.imageExtent.width;
    region.imageExtent.height = desc.imageExtent.height;
    region.imageExtent.depth = desc.imageExtent.depth;


    vkCmdCopyBufferToImage(
        vkCBuffer->buffer,
        vkSrcBuffer->buffer,
        vkDstTexture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

}

void bnGraphicsVK::CopyImageToImage(ICommandBuffer* cBuffer, ITexture* srcTexture, ITexture* dstTexture, ImageCopyDesc desc)
{
    auto vkCBuffer = dynamic_cast<CommandBufferVK*>(cBuffer);
    auto vkSrcTexture = dynamic_cast<TextureVK*>(srcTexture);
    auto vkDstTexture = dynamic_cast<TextureVK*>(dstTexture);

    if (!vkCBuffer || !vkSrcTexture || !vkDstTexture) return;

    VkImageCopy region{};
    region.dstOffset.x = desc.dstOffset.x;
    region.dstOffset.y = desc.dstOffset.y;
    region.dstOffset.z = desc.dstOffset.z;
    region.srcOffset.x = desc.srcOffset.x;
    region.srcOffset.y = desc.srcOffset.y;
    region.srcOffset.z = desc.srcOffset.z;

    // Subresource
    region.dstSubresource.aspectMask = desc.dstSubresource.aspectMask;
    region.dstSubresource.mipLevel = desc.dstSubresource.mipLevel;
    region.dstSubresource.baseArrayLayer = desc.dstSubresource.baseArrayLayer;
    region.dstSubresource.layerCount = desc.dstSubresource.layerCount;

    region.srcSubresource.aspectMask = desc.srcSubresource.aspectMask;
    region.srcSubresource.mipLevel = desc.srcSubresource.mipLevel;
    region.srcSubresource.baseArrayLayer = desc.srcSubresource.baseArrayLayer;
    region.srcSubresource.layerCount = desc.srcSubresource.layerCount;

    region.extent.depth = desc.extent.depth;
    region.extent.width = desc.extent.width;
    region.extent.height = desc.extent.height;

    vkCmdCopyImage(
        vkCBuffer->buffer,
        vkSrcTexture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vkDstTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
}

ITexture* bnGraphicsVK::GetSwapchainImage()
{
    auto text = new TextureVK(false);
    text->explicitLayout = ImageLayout::Present;
    text->owner = false;
    text->device = device;
    text->desc.format = FromVkFormat(swapchainImageFormat);
    text->desc.width = width;
    text->desc.height = height;
    text->image = swapChainImages[currentFrame];
    text->slot = 0;
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = text->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = ToVkFormat(text->desc.format); // convert back to VkFormat
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &text->imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swapchain image view!");
    }
    return text;
}

void bnGraphicsVK::ClearPendingReleases()
{
    //if (dontClearYet) return;
    for (auto* cmdBuf : cbRelease) {
        vkFreeCommandBuffers(device, cmdBuf->pool, 1, &cmdBuf->buffer);
        delete cmdBuf;
    }
    cbRelease.clear();

    for (auto pipeline : releasePipelines) {
        auto it = std::find_if(pDraws.begin(), pDraws.end(),
            [=](const PendingDrawVK& p) { return p.pipeline == pipeline; });

        if (it != pDraws.end()) {
            continue;
        }

        for (auto ds : pipeline->descriptorSets) { 
            if (device && ds.second->descriptorSet != VK_NULL_HANDLE && pipeline->descriptorPool->pool != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(device, pipeline->descriptorPool->pool, 1, &ds.second->descriptorSet);
                ds.second->descriptorSet = VK_NULL_HANDLE;
            }

            //ds.second->~DescriptorSetVK();
            delete ds.second;
        }
        pipeline->descriptorSets.clear();

        if (pipeline->pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline->pipeline, nullptr);
            pipeline->pipeline = VK_NULL_HANDLE;
        }
        if (pipeline->layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline->layout, nullptr);
            pipeline->layout = VK_NULL_HANDLE;
        }

        //pipeline->~PipelineVK();
        delete pipeline;
    }
    releasePipelines.clear();
    for (auto bufPtr : bufferRelease) {
        if (bufPtr && *bufPtr) {
            auto buffer = (BufferVK*)*bufPtr;
            delete buffer;
            *bufPtr = nullptr;
        }
    }
    bufferRelease.clear();

    for (auto bufPtr : shaderRelease) {
        if (bufPtr && *bufPtr) {
            auto shader = (ShaderVK*)*bufPtr;
            delete shader;
            *bufPtr = nullptr;
        }
    }
    shaderRelease.clear();

    for (auto tex : textureRelease) {
        if (tex && *tex) {
            auto texture = (TextureVK*)*tex;
            if (!texture->owner && texture->imageView) { //std::find(swapChainImages.begin(), swapChainImages.end(), texture->image) != swapChainImages.end()
                    vkDestroyImageView(device, texture->imageView, nullptr);                
            }
            delete texture;
            *tex = nullptr;
        }
    }


    textureRelease.clear();

    for (auto cmdPool : poolRelease) {
        if (cmdPool && *cmdPool) {
            (*cmdPool)->Release();
            delete* cmdPool;
            *cmdPool = nullptr;
        }
    }

    poolRelease.clear();

    for (auto cmdBuffer : releaseCommandBuffers) {
        if (cmdBuffer) {
            auto it = std::find_if(pDraws.begin(), pDraws.end(), 
                [=](const PendingDrawVK& p) { return p.cmdBuffer == cmdBuffer->cmdBuffer; });

            if (it != pDraws.end()) {
                continue;
            }
            delete cmdBuffer;
            auto iteration = std::find(commandLists.begin(), commandLists.end(), cmdBuffer);
            if (iteration != commandLists.end()) {
                *iteration = nullptr;
            }
        }
    }

    releaseCommandBuffers.clear();

    for (auto pool : descriptorPoolRelease) {

        if (pool && *pool) {
            auto it = std::find_if(pDraws.begin(), pDraws.end(),
                [=](const PendingDrawVK& p) { return ((PipelineVK*)p.pipeline)->descriptorPool == (IDescriptorPool*)pool; });

            if (it != pDraws.end()) {
                continue;
            }

            (*pool)->Release();
            delete* pool;
            *pool = nullptr;
        }
    }

    descriptorPoolRelease.clear();

    for (auto layout : descriptorSetLayoutRelease) {
        if (layout && *layout) {
            auto it = std::find_if(pDraws.begin(), pDraws.end(),
                [=](const PendingDrawVK& p) { return ((PipelineVK*)p.pipeline)->descriptorSetLayout == (IDescriptorSetLayout*)layout; });

            if (it != pDraws.end()) {
                continue;
            }
            auto vkLayout = ((DescriptorSetLayoutVK*)*layout);
            //vkLayout->~DescriptorSetLayoutVK();
            delete vkLayout;
            *layout = nullptr;
        }
    }
    descriptorSetLayoutRelease.clear();

    for (auto sampler : samplerRelease) {
        if (sampler) {
            if (sampler->sampler != VK_NULL_HANDLE)
                vkDestroySampler(device, sampler->sampler, nullptr);

            delete sampler;
        }
    }

    samplerRelease.clear();

    for (auto voids : pendVoids) {
        if (voids) {
            delete voids;
            voids = nullptr;
        }
    }

    pendVoids.clear();

}

void bnGraphicsVK::WaitTillImFree()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkDeviceWaitIdle(device);
}

void bnGraphicsVK::PushGroup(const char *name, uint32_t color) {
    if (!fpCmdBeginDebugUtilsLabelEXT) return;

    VkDebugUtilsLabelEXT labelInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    labelInfo.pLabelName = name;

    // Vulkan colors are 4 floats (RGBA). Unpack your uint32_t.
    labelInfo.color[0] = ((color >> 16) & 0xFF) / 255.0f; // R
    labelInfo.color[1] = ((color >> 8) & 0xFF) / 255.0f;  // G
    labelInfo.color[2] = (color & 0xFF) / 255.0f;         // B
    labelInfo.color[3] = ((color >> 24) & 0xFF) / 255.0f; // A (if 0, use 1.0)
    if (labelInfo.color[3] == 0.0f) labelInfo.color[3] = 1.0f;

    fpCmdBeginDebugUtilsLabelEXT(commandBuffer[currentFrame], &labelInfo);
}

void bnGraphicsVK::PopGroup() {
    if (fpCmdEndDebugUtilsLabelEXT) {
        fpCmdEndDebugUtilsLabelEXT(commandBuffer[currentFrame]);
    }
}

void bnGraphicsVK::SetMarker(const char *name, uint32_t color) {
    if (!fpCmdInsertDebugUtilsLabelEXT) return;

    VkDebugUtilsLabelEXT labelInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    labelInfo.pLabelName = name;
    // Unpack color same as above ^^^ - should be moved to a func?
    labelInfo.color[0] = ((color >> 16) & 0xFF) / 255.0f;
    labelInfo.color[1] = ((color >> 8) & 0xFF) / 255.0f;
    labelInfo.color[2] = (color & 0xFF) / 255.0f;
    labelInfo.color[3] = 1.0f;

    fpCmdInsertDebugUtilsLabelEXT(commandBuffer[currentFrame], &labelInfo);
}

void bnGraphicsVK::CreateSwapChain()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);


    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    // Choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed to exist
    for (auto& pm : presentModes) {
        
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { // preferred
            presentMode = pm;
            break;
        }
        else if (!config.vsync) {
            presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    // Swapchain extent (size)
    VkExtent2D extent;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
        extent = surfaceCapabilities.currentExtent;
    }
    else {
        extent.width = width;
        extent.height = height;
        extent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, extent.height));
    }

    int imageCount = config.framesInFlight;
    //if (config.framesInFlight > 0 && imageCount < config.framesInFlight) {
    //    // ensure enough images for in-flight frames
    //}
    
    
    if (surfaceCapabilities.minImageCount > imageCount) {
        imageCount = surfaceCapabilities.minImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // frozen frame capture

    // Queue family handling
    auto familyIndices = FindQueueFamilies(physicalDevice, surface);

    uint32_t queueFamilyIndices[] = { familyIndices.graphicsFamily, familyIndices.presentFamily };
    vkGetDeviceQueue(device, familyIndices.presentFamily, 0, &presentQueue);
    if (familyIndices.graphicsFamily != familyIndices.presentFamily) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    auto oldSwapChain = swapChain;
    swapchainInfo.oldSwapchain = oldSwapChain; // use previous swapchain when recreating

    if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapChain) != VK_SUCCESS) {
        return;
    }

    if (oldSwapChain) {
        vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
    }

    // Get swapchain images
    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, nullptr);
    swapChainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, swapChainImages.data());

    // Save format & extent
    swapchainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    swapchainImageViews.resize(swapChainImages.size());




    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;  // same as swapchain format
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
           // throw std::runtime_error("Failed to create image view for swapchain image!");
        }
    }


    //if (config.enableDepth) {
    //    VkImageCreateInfo depthImageInfo{};
    //    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    //    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    //    depthImageInfo.extent.width = swapChainExtent.width;
    //    depthImageInfo.extent.height = swapChainExtent.height;
    //    depthImageInfo.extent.depth = 1;
    //    depthImageInfo.mipLevels = 1;
    //    depthImageInfo.arrayLayers = 1;
    //    depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
    //    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    //    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    //    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // or MSAA samples
    //    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    //    //VkImage depthImage;
    //    VkResult res = vkCreateImage(device, &depthImageInfo, nullptr, &depthImage);
    //    VkMemoryRequirements memRequirements;
    //    vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

    //    VkMemoryAllocateInfo allocInfo{};
    //    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    //    allocInfo.allocationSize = memRequirements.size;
    //    allocInfo.memoryTypeIndex = FindMemoryType(
    //        memRequirements.memoryTypeBits,
    //        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    //    );

    //    //  VkDeviceMemory depthImageMemory;
    //    vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory);
    //    vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    //    VkImageViewCreateInfo viewInfo{};
    //    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    //    viewInfo.image = depthImage;
    //    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    //    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    //    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    //    viewInfo.subresourceRange.baseMipLevel = 0;
    //    viewInfo.subresourceRange.levelCount = 1;
    //    viewInfo.subresourceRange.baseArrayLayer = 0;
    //    viewInfo.subresourceRange.layerCount = 1;

    //    //VkImageView depthImageView;
    //    vkCreateImageView(device, &viewInfo, nullptr, &depthImageView);
    //}


}

void bnGraphicsVK::CreateRenderPass()
{
    //VkImageCreateInfo imageInfo{};
    //imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    //imageInfo.imageType = VK_IMAGE_TYPE_2D;
    //imageInfo.extent.width = width;
    //imageInfo.extent.height = height;
    //imageInfo.extent.depth = 1;
    //imageInfo.mipLevels = 1;
    //imageInfo.arrayLayers = 1;
    //imageInfo.format = VK_FORMAT_D32_SFLOAT;  // e.g., VK_FORMAT_D32_SFLOAT
    //imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    //imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    //imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // or MSAA
    //imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    //if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
    //   // throw std::runtime_error("Failed to create depth image!");
    //}

    //// Allocate memory
    //VkMemoryRequirements memRequirements;
    //vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

    //VkMemoryAllocateInfo allocInfo{};
    //allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    //allocInfo.allocationSize = memRequirements.size;
    //allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
    //    //throw std::runtime_error("Failed to allocate depth image memory!");
    //}

    //vkBindImageMemory(device, depthImage, depthImageMemory, 0);

    //if (depthStencil) {
    //    depthStencil->Release();
    //}

    auto oldTarget = swpchTarget;
    if (swpchTarget != VK_NULL_HANDLE) {
        //swpchTarget->Release();
        //vkDestroyImageView(device, swpchTarget->textures[0]->imageView, nullptr);
        vkDestroyRenderPass(device, swpchTarget->renderPass, nullptr);
    }


    TextureDesc depthFormat{};
    depthFormat.width = width;
    depthFormat.height = height;
    depthFormat.samples = config.enableMSAA ? config.msaaSamples : 1;
    depthFormat.format = TextureFormat::D32_Float;
    depthTexture = CreateTexture(depthFormat);
    depthStencil = CreateDepthStencil(depthTexture);

    if (currentSwapChainImage) {
            vkDestroyImageView(device, currentSwapChainImage->imageView, nullptr);
            delete currentSwapChainImage; 
    }

    currentSwapChainImage = (TextureVK*)GetSwapchainImage();

        if (config.enableMSAA) {
        TextureDesc msaaFormat{};
        msaaFormat.width = width;
        msaaFormat.height = height;
        msaaFormat.samples = config.msaaSamples;
        msaaFormat.format = FromVkFormat(swapchainImageFormat);
        msaaSWPCHTarget = CreateTexture(msaaFormat);

        swpchTarget = (RenderTargetVK*)CreateRenderTarget({
               .colorTargets = { msaaSWPCHTarget },
               .depth = depthStencil,
               .makeFramebuffer = false,
               .colorLayout = { ImageLayout::RenderTarget, ImageLayout::Present }
            });
    }
    else {

        swpchTarget = (RenderTargetVK*)CreateRenderTarget({
               .colorTargets = { currentSwapChainImage },
               .depth = depthStencil,
               .makeFramebuffer = false,
               .colorLayout = { ImageLayout::Present }
            });

    }


        if (oldTarget) {
            oldTarget->depth = swpchTarget->depth;
            oldTarget->framebuffer = swpchTarget->framebuffer;
            oldTarget->desc = swpchTarget->desc;
            oldTarget->renderPass = swpchTarget->renderPass;
            oldTarget->textures = swpchTarget->textures;

            delete swpchTarget;
            swpchTarget = oldTarget;
        }
    

    //std::vector<VkAttachmentDescription> attachments;

    //// --- Color Attachment ---
    //VkAttachmentDescription colorAttachment{};
    //colorAttachment.format = swapchainImageFormat;   // swapchain format
    //colorAttachment.samples = config.enableMSAA ? VK_SAMPLE_COUNT_8_BIT : VK_SAMPLE_COUNT_1_BIT;
    //colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    //attachments.push_back(colorAttachment);

    //// --- Depth Attachment (optional) ---
    //VkAttachmentReference depthAttachmentRef{};
    //if (config.enableDepth) {
    //    VkAttachmentDescription depthAttachment{};
    //    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    //    depthAttachment.samples = config.enableMSAA ? VK_SAMPLE_COUNT_8_BIT : VK_SAMPLE_COUNT_1_BIT;
    //    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //    depthAttachmentRef.attachment = static_cast<uint32_t>(attachments.size());
    //    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //    attachments.push_back(depthAttachment);
    //}

    //// --- Color attachment reference ---
    //VkAttachmentReference colorAttachmentRef{};
    //colorAttachmentRef.attachment = 0; // first in attachments array
    //colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //// --- Subpass ---
    //VkSubpassDescription subpass{};
    //subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    //subpass.colorAttachmentCount = 1;
    //subpass.pColorAttachments = &colorAttachmentRef;
    //if (config.enableDepth)
    //    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    //// --- Subpass dependency ---
    //VkSubpassDependency dependency{};
    //dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    //dependency.dstSubpass = 0;
    //dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.srcAccessMask = 0;
    //dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //// --- Render Pass Create Info ---
    //VkRenderPassCreateInfo renderPassInfo{};
    //renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    //renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    //renderPassInfo.pAttachments = attachments.data();
    //renderPassInfo.subpassCount = 1;
    //renderPassInfo.pSubpasses = &subpass;
    //renderPassInfo.dependencyCount = 1;
    //renderPassInfo.pDependencies = &dependency;

    //if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
    //    //throw std::runtime_error("Failed to create Vulkan render pass!");
    //    return;
    //}

    return;
}

void bnGraphicsVK::CreateFrameBuffers()
{
    swapChainFrameBuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
    /*    if(i == 0) swapChainFrameBuffers[i] = swpchTarget->framebuffer;
        else {*/
            std::vector<VkImageView> attachments;

            //// Color attachment
    

            if (config.enableMSAA) {
                attachments.push_back(((TextureVK*)msaaSWPCHTarget)->imageView);
            }
          
            attachments.push_back(swapchainImageViews[i]);

            //// Depth attachment if enabled
            if (depthStencil != VK_NULL_HANDLE) {
                attachments.push_back(((DepthStencilVK*)depthStencil)->depthView);
            }

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = swpchTarget->renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
                return;
            }
        //}
    }


}

u32 bnGraphicsVK::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
}

void CommandListVK::BindPipeline(IPipeline* pipeline) {
    PipelineVK* vkPipeline = static_cast<PipelineVK*>(pipeline);

    if (!vkPipeline || !vkPipeline->pipeline) return;

    //vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    vkPipeline->pipeline);

    draw.pipeline = pipeline;
}

void CommandListVK::BindDescriptorSet(IDescriptorSet* set, uint32_t index) {
    DescriptorSetVK* vkSet = static_cast<DescriptorSetVK*>(set);

    if (!vkSet || !vkSet->pipeline) return;

    draw.ds = set;
    draw.dsIndex = index;
}

void CommandListVK::BindViewPort(IViewPort* viewport2) {
    auto* viewport = static_cast<ViewportVK*>(viewport2);
    if (!viewport) return;

    //vkCmdSetViewport(cmdBuffer, 0, 1, &viewport->viewport);
    draw.viewport = viewport2;
}

void CommandListVK::BindScissor(IViewPort* viewport2) {
    auto* viewport = static_cast<ViewportVK*>(viewport2);
    if (!viewport) return;

    //vkCmdSetScissor(cmdBuffer, 0, 1, &viewport->scissor);
    draw.scissor = viewport2;
}

void CommandListVK::BindBuffer(IBuffer* buffer)
{
    BufferVK* vkBuffer = static_cast<BufferVK*>(buffer);
    if (!vkBuffer) return;

    if (vkBuffer->type == BufferType::Vertex) {
        //VkDeviceSize offsets[] = { 0 };
        //vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkBuffer->buffer, offsets);
        draw.buffer = buffer;
    }

}

void CommandListVK::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset) {
    VkPrimitiveTopology topology = ToVkPrimitiveTopology(type);

    // Topology is usually dynamic if pipeline allows
    //vkCmdSetPrimitiveTopology(cmdBuffer, topology);


    //vkCmdDraw(cmdBuffer, static_cast<uint32_t>(vertexCount),
    //    1, static_cast<uint32_t>(vertexOffset), 0);

    draw.type = type;
    draw.vertexCount = vertexCount;
    draw.vertexOffset = vertexOffset;

    pDraws->push_back(draw);
}

void CommandListVK::DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset) {
    BufferVK* vkBuffer = static_cast<BufferVK*>(indexBuffer);

    //vkCmdBindIndexBuffer(cmdBuffer, vkBuffer->buffer, 0,
    //    VK_INDEX_TYPE_UINT32);

    //VkPrimitiveTopology topology = ToVkPrimitiveTopology(type);
    //
    //vkCmdSetPrimitiveTopology(cmdBuffer, topology);

    //vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(indexCount), 1,
    //    static_cast<uint32_t>(indexOffset), 0, 0);

    draw.type = type;
    draw.indexBuffer = indexBuffer;
    draw.indexCount = indexCount;
    draw.indexCount = indexOffset;

    pDraws->push_back(draw);
}

void CommandListVK::CopyToBuffer(IBuffer* buffer, void* data, size_t size) {
    BufferVK* vkBuffer = static_cast<BufferVK*>(buffer);

    void* mapped = nullptr;
    vkMapMemory(device, vkBuffer->memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, vkBuffer->memory);
}

//void CommandListVK::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer buffer)
//{
//    if (!buffer) {
//        buffer = cmdBuffer;
//    }
//
//    VkImageMemoryBarrier barrier{};
//    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//    barrier.oldLayout = oldLayout;
//    barrier.newLayout = newLayout;
//    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//    barrier.image = image;
//
//    // Define which parts of the image we transition
//    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // For color images
//    barrier.subresourceRange.baseMipLevel = 0;
//    barrier.subresourceRange.levelCount = 1;
//    barrier.subresourceRange.baseArrayLayer = 0;
//    barrier.subresourceRange.layerCount = 1;
//
//    // Source/destination stage masks (depends on layouts)
//    VkPipelineStageFlags sourceStage;
//    VkPipelineStageFlags destinationStage;
//
//    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
//        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
//        barrier.srcAccessMask = 0;
//        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//
//        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//    }
//    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
//        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
//        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//
//        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//    }
//    else {
//        // Handle other cases you care about (DEPTH_STENCIL, etc.)
//        throw std::runtime_error("Unsupported layout transition!");
//    }
//
//    vkCmdPipelineBarrier(
//        buffer,
//        sourceStage, destinationStage,
//        0,
//        0, nullptr,
//        0, nullptr,
//        1, &barrier
//    );
//}


DescriptorSetVK::~DescriptorSetVK()
{
    
}

void DescriptorSetVK::SetBuffer(uint32_t slot, IBuffer* buffer) {
    if (!buffer) return;

    // Ensure the vector is large enough
    if (slot >= buffers.size()) buffers.resize(slot + 1, nullptr);

    buffers[slot] = static_cast<BufferVK*>(buffer);
}

void DescriptorSetVK::SetTexture(uint32_t slot, ITexture* texture) {
    if (!texture) return;

    if (slot >= textures.size()) textures.resize(slot + 1, nullptr);

    textures[slot] = static_cast<TextureVK*>(texture);
}

void DescriptorSetVK::SetSampler(uint32_t slot, ISamplerState* sampler) {
    if (!sampler) return;

    if (slot >= samplers.size()) samplers.resize(slot + 1, nullptr);

    samplers[slot] = static_cast<SamplerVK*>(sampler);
}

void DescriptorSetVK::Update() {
    std::vector<VkWriteDescriptorSet> writes;

    // Update buffers
    for (uint32_t i = 0; i < buffers.size(); ++i) {
        BufferVK* buf = buffers[i];
        if (!buf) continue;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buf->buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = buf->desc.size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;

        writes.push_back(write);
    }

    uint32_t textureBindingOffset = static_cast<uint32_t>(buffers.size());

    // Update textures (combined image sampler)
    for (uint32_t i = 0; i < textures.size(); ++i) {
        TextureVK* tex = textures[i];
        if (!tex) continue;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = tex->imageView;
        // Use the bound sampler if exists
        imageInfo.sampler = (i < samplers.size() && samplers[i]) ? samplers[i]->sampler : VK_NULL_HANDLE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = i;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        writes.push_back(write);
    }

    // Flush all writes
    if (!writes.empty()) {
        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0, nullptr);
    }
}
