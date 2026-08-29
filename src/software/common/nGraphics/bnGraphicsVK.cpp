#include "bnGraphicsVK.h"
#include <cstring>
#include <ranges>

#include "nWindow/linuxAbstracts.h"
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#elif defined(__apple__)
#endif

#if defined(Always)
  #undef Always
#endif

#if defined(None)
  #undef None
#endif
static PFN_vkCmdBeginDebugUtilsLabelEXT fpCmdBeginDebugUtilsLabelEXT = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT   fpCmdEndDebugUtilsLabelEXT   = nullptr;
static PFN_vkCmdInsertDebugUtilsLabelEXT fpCmdInsertDebugUtilsLabelEXT = nullptr;

namespace
{
    struct QueueFamilyIndices {
        u32 graphicsFamily = 0;
        u32 presentFamily = 0;

        [[nodiscard]] bool isComplete() const {
            return graphicsFamily > 0 && presentFamily > 0;
        }
    };
}

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
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


VKAPI_ATTR static VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    // Redirect to std::cout
    std::cout << "[Vulkan] " << pCallbackData->pMessage << std::endl;

    return VK_FALSE; // return true to abort the call that triggered the validation message
}

static VkDebugUtilsMessengerEXT debugMessenger;

//#ifdef _DEBUG
static void setupDebugMessenger(VkInstance instance) {
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

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func != nullptr) {
        if (func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
}
//#endif

bnGraphicsVK::bnGraphicsVK(SysHandle& handle, IGraphicsDeviceConfig& config) : config(config), handle(handle),
                                                                               device(),
                                                                               swapchainImageFormat(),
                                                                               msaaSamples()
{
}

#pragma optimize("", off)
bool bnGraphicsVK::Init()
{
    // fpCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
    // fpCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
    // fpCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT");

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

    instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
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
#elif defined(__linux__)
    if (handle->type == DisplayServerType::Wayland)
    {
        VkWaylandSurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.display = handle->wayland.display;
        surfaceInfo.surface = handle->wayland.surface;
        surfaceInfo.pNext   = nullptr;
        surfaceInfo.flags   = 0;
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;

        if (vkCreateWaylandSurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
        {
            return false;
        }
    }

    if (handle->type == DisplayServerType::X11)
    {
        VkXlibSurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.window = handle->x11.window;
        surfaceInfo.pNext  = nullptr;
        surfaceInfo.flags  = 0;
        surfaceInfo.dpy    = handle->x11.display;
        surfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;

        if (vkCreateXlibSurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
        {
            return false;
        }
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


    copyPool = dynamic_cast<CommandPoolVK*>(CreateCommandPool({
        0, true, true
    }));

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

static void TransitionImageLayout(
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
        // bool hasCHanged = false;
        if (draw.pipeline) {
            auto* vkPipeline = dynamic_cast<PipelineVK*>(draw.pipeline);

            if (!vkPipeline || !vkPipeline->pipeline) return;
            //if (cRenderPass != swpchTarget->renderPass) {
                if (vkPipeline->renderTarget && vkPipeline->renderTarget->GetNativeHandle() && cRenderPass != static_cast<VkRenderPass>(vkPipeline->renderTarget->GetNativeHandle())) {
                    cRenderPass = static_cast<VkRenderPass>(vkPipeline->renderTarget->GetNativeHandle());
                    std::array<VkClearValue, 3> clearValues{};
                    // hasCHanged = true;
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
            auto* vkBuffer = dynamic_cast<BufferVK*>(draw.buffer);
          VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(draw.cmdBuffer, 0, 1, &vkBuffer->buffer, offsets);
        }
        if (draw.ds) {
            auto vkSet = dynamic_cast<DescriptorSetVK*>(draw.ds);

            if (!vkSet || !vkSet->pipeline) return;
        
            vkCmdBindDescriptorSets(draw.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        vkSet->pipeline->layout,
                        0, 1, &vkSet->descriptorSet, 0, nullptr);
              
        }

        if (draw.pipeline) {
            auto* vkPipeline = dynamic_cast<PipelineVK*>(draw.pipeline);

            if (!vkPipeline || !vkPipeline->pipeline) return;


            vkCmdBindPipeline(draw.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vkPipeline->pipeline);
        }
    
        if (draw.viewport) {
            auto* viewport = dynamic_cast<ViewportVK*>(draw.viewport);
            if (!viewport) return;

            vkCmdSetViewport(draw.cmdBuffer, 0, 1, &viewport->viewport);
        }
        if (draw.scissor) {
            auto* viewport = dynamic_cast<ViewportVK*>(draw.scissor);
            if (!viewport) return;

            vkCmdSetScissor(draw.cmdBuffer, 0, 1, &viewport->scissor);
        }
        if (draw.indexBuffer) {
            auto vkBuffer = dynamic_cast<BufferVK*>(draw.indexBuffer);

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

void bnGraphicsVK::Resize(long changedWidth, long changedHeight)
{
    if (changedWidth == this->width && changedHeight == this->height) return;

    this->width = changedWidth;
    this->height = changedHeight;
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
        vkDestroyImageView(device, dynamic_cast<DepthStencilVK*>(depthStencil)->depthView, nullptr);
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

    for (const auto& pool : commandBuffer) {
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
        vkDestroyImageView(device, dynamic_cast<DepthStencilVK*>(depthStencil)->depthView, nullptr);
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
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
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

IPipelineBuilder* bnGraphicsVK::CreatePipelineBuilder()
{
    return new PipelineBuilderVK(config, &releasePipelines, device, (RenderTargetVK*)swpchTarget);
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
    default:
        break;
        ;
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
                    auto vertex = reinterpret_cast<float*>(bufferVK->dataBuffer.data() + i * desc.stride);

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

            auto copyCmd = dynamic_cast<CommandBufferVK*>(BeginSingleTimeCommands(copyPool));

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
        if (auto vkDepth = dynamic_cast<DepthStencilVK*>(desc.depth)) {
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
        fbAttachments.reserve(renderTarget->textures.size());
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

    auto localDepthStencil = new DepthStencilVK();
    localDepthStencil->texture = texture;

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

    VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &localDepthStencil->depthView);
    if (res != VK_SUCCESS) {
        delete localDepthStencil;
        return nullptr;
    }

    return localDepthStencil;
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

        vBuffer->mStagingBuffer = CreateBuffer(stagingDesc, nullptr);

        // Upload data to staging
        void* mapped;
        vkMapMemory(device, dynamic_cast<BufferVK*>(vBuffer->mStagingBuffer)->memory, 0, size, 0, &mapped);
        memcpy(mapped, data, size);
        vkUnmapMemory(device, dynamic_cast<BufferVK*>(vBuffer->mStagingBuffer)->memory);

        // --- Record copy command ---
        auto cmdBuf = static_cast<VkCommandBuffer>(cmd->GetNativeHandle());
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;

        vkCmdCopyBuffer(cmdBuf, dynamic_cast<BufferVK*>(vBuffer->mStagingBuffer)->buffer, vBuffer->buffer, 1, &copyRegion);
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
        auto it = std::ranges::find_if(pDraws,
                                       [=](const PendingDrawVK& p) { return p.pipeline == pipeline; });

        if (it != pDraws.end()) {
            continue;
        }

        for (auto set : pipeline->descriptorSets | std::views::values) {
            if (device && set->descriptorSet != VK_NULL_HANDLE && pipeline->descriptorPool->pool != VK_NULL_HANDLE) {
                vkFreeDescriptorSets(device, pipeline->descriptorPool->pool, 1, &set->descriptorSet);
                set->descriptorSet = VK_NULL_HANDLE;
            }

            //ds.second->~DescriptorSetVK();
            delete set;
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
            auto buffer = dynamic_cast<BufferVK*>(*bufPtr);
            delete buffer;
            *bufPtr = nullptr;
        }
    }
    bufferRelease.clear();

    for (auto bufPtr : shaderRelease) {
        if (bufPtr && *bufPtr) {
            auto shader = dynamic_cast<ShaderVK*>(*bufPtr);
            delete shader;
            *bufPtr = nullptr;
        }
    }
    shaderRelease.clear();

    for (auto tex : textureRelease) {
        if (tex && *tex) {
            auto texture = dynamic_cast<TextureVK*>(*tex);
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
            auto it = std::ranges::find_if(pDraws,
                                           [=](const PendingDrawVK& p) { return p.cmdBuffer == cmdBuffer->cmdBuffer; });

            if (it != pDraws.end()) {
                continue;
            }
            delete cmdBuffer;
            auto iteration = std::ranges::find(commandLists, cmdBuffer);
            if (iteration != commandLists.end()) {
                *iteration = nullptr;
            }
        }
    }

    releaseCommandBuffers.clear();

    for (auto pool : descriptorPoolRelease) {

        if (pool && *pool) {
            auto it = std::ranges::find_if(pDraws,
                                           [=](const PendingDrawVK& p) { return dynamic_cast<PipelineVK*>(p.pipeline)->descriptorPool == reinterpret_cast<IDescriptorPool*>(pool); });

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
            auto it = std::ranges::find_if(pDraws,
                                           [=](const PendingDrawVK& p) { return dynamic_cast<PipelineVK*>(p.pipeline)->descriptorSetLayout == reinterpret_cast<IDescriptorSetLayout*>(layout); });

            if (it != pDraws.end()) {
                continue;
            }
            auto vkLayout = dynamic_cast<DescriptorSetLayoutVK*>(*layout);
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

    // unsafe.
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

    u32 imageCount = config.framesInFlight;
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
    depthTexture = CreateTexture(depthFormat, nullptr);
    depthStencil = CreateDepthStencil(depthTexture);

    if (currentSwapChainImage) {
            vkDestroyImageView(device, currentSwapChainImage->imageView, nullptr);
            delete currentSwapChainImage; 
    }

    currentSwapChainImage = dynamic_cast<TextureVK*>(GetSwapchainImage());

    if (config.enableMSAA) {
        TextureDesc msaaFormat{};
        msaaFormat.width = width;
        msaaFormat.height = height;
        msaaFormat.samples = config.msaaSamples;
        msaaFormat.format = FromVkFormat(swapchainImageFormat);
        msaaSWPCHTarget = CreateTexture(msaaFormat, nullptr);

        swpchTarget = dynamic_cast<RenderTargetVK*>(CreateRenderTarget({
            .colorTargets = {msaaSWPCHTarget},
            .depth = depthStencil,
            .makeFramebuffer = false,
            .colorLayout = {ImageLayout::RenderTarget, ImageLayout::Present}
        }));
    }
    else {

        swpchTarget = dynamic_cast<RenderTargetVK*>(CreateRenderTarget({
            .colorTargets = {currentSwapChainImage},
            .depth = depthStencil,
            .makeFramebuffer = false,
            .colorLayout = {ImageLayout::Present}
        }));

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
                attachments.push_back(dynamic_cast<TextureVK*>(msaaSWPCHTarget)->imageView);
            }
          
            attachments.push_back(swapchainImageViews[i]);

            //// Depth attachment if enabled
            if (depthStencil != VK_NULL_HANDLE) {
                attachments.push_back(dynamic_cast<DepthStencilVK*>(depthStencil)->depthView);
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

u32 bnGraphicsVK::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
}

InputLayoutVK::InputLayoutVK(const InputLayoutDesc &desc) {
    this->desc = desc;
    bindings.clear();
    attributes.clear();

    // Single vertex buffer binding (for simplicity)
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = desc.stride; // stride of one vertex
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings.push_back(binding);

    for (const auto & elem : desc.elements)
    {
        // your vertex elements
        VkVertexInputAttributeDescription attr{};
        attr.location = elem.semanticIndex;
        attr.binding = elem.inputSlot;
        attr.format = TranslateTypeToVkFormat(elem.type);
        attr.offset = elem.offset;
        attributes.push_back(attr);
    }

    vkVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vkVertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vkVertexInputInfo.pVertexBindingDescriptions = bindings.data();
    vkVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vkVertexInputInfo.pVertexAttributeDescriptions = attributes.data();
}

ShaderVK::ShaderVK(VkDevice device, const ShaderDesc &desc): device(device) {
    type = desc.type;
    ogData = desc.ogData;
    binary.reserve(desc.bytecodeSize);
    binary.insert(binary.end(),
                  desc.bytecode,
                  desc.bytecode + desc.bytecodeSize);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = desc.bytecodeSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.bytecode);

    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
}

VkPipelineShaderStageCreateInfo ShaderVK::GetStageInfo() const {
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // default
    stageInfo.module = shaderModule;
    stageInfo.pName = "main"; // entry point

    // Set stage according to shader type
    switch (type)
    {
        case ShaderDesc::Type::Vertex:
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ShaderDesc::Type::Pixel:  // fragment shader
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case ShaderDesc::Type::Compute:
            stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        case ShaderDesc::Type::Geometry:
            stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ShaderDesc::Type::TessControl:
            stageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        case ShaderDesc::Type::TessEval:
            stageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;
        case ShaderDesc::Type::RayGen:
            stageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            break;
        case ShaderDesc::Type::ClosestHit:
            stageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            break;
        case ShaderDesc::Type::AnyHit:
            stageInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            break;
        case ShaderDesc::Type::Miss:
            stageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            break;
        case ShaderDesc::Type::Intersection:
            stageInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            break;
        case ShaderDesc::Type::Callable:
         stageInfo.stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
            break;
    }

    return stageInfo;
}

ViewportVK::ViewportVK(const ViewPortDesc &desc) {
    auto vp = desc.viewport;

    viewport.x = vp->x;
    viewport.y = vp->y;
    viewport.width = vp->width;
    viewport.height = vp->height;
    viewport.minDepth = vp->minDepth;
    viewport.maxDepth = vp->maxDepth;

    scissor.offset = { static_cast<i32>(vp->x), static_cast<i32>(vp->y) };
    scissor.extent = { static_cast<u8>(vp->width), static_cast<u8>(vp->height) };
}

RasterizerVK::RasterizerVK(const RasterizerDesc &desc) {
    vkRaster.lineWidth = 1.0f;
    vkRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vkRaster.polygonMode = (desc.fillMode == IFillMode::Solid) ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE;
    vkRaster.cullMode = (desc.cullMode == CullMode::Back) ? VK_CULL_MODE_BACK_BIT :
                            (desc.cullMode == CullMode::Front) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;
    vkRaster.frontFace = desc.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    vkRaster.depthBiasEnable = VK_FALSE;
}

DepthStencilStateVK::DepthStencilStateVK(const DepthStencilDesc &desc) {
    vkDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    vkDepthStencil.depthTestEnable = desc.depthEnable ? VK_TRUE : VK_FALSE;
    vkDepthStencil.depthWriteEnable = desc.depthWriteMask ? VK_TRUE : VK_FALSE;
    vkDepthStencil.depthCompareOp = TranslateComparisonFunc(desc.depthFunc);
    vkDepthStencil.stencilTestEnable = desc.stencilEnable ? VK_TRUE : VK_FALSE;
    // Fill front/back stencil ops
}

BlendStateVK::BlendStateVK(const BlendStateDesc &desc) {
    attachments.resize(1); // max render targets
    for (int i = 0; i < 1; ++i) {
        auto& a = attachments[i];
        a.blendEnable = desc.renderTarget[i].blendEnable ? VK_TRUE : VK_FALSE;
        a.srcColorBlendFactor = TranslateBlend(desc.renderTarget[i].srcBlend);
        a.dstColorBlendFactor = TranslateBlend(desc.renderTarget[i].destBlend);
        a.colorBlendOp = TranslateBlendOp(desc.renderTarget[i].blendOp);
        a.srcAlphaBlendFactor = TranslateBlend(desc.renderTarget[i].srcBlendAlpha);
        a.dstAlphaBlendFactor = TranslateBlend(desc.renderTarget[i].destBlendAlpha);
        a.alphaBlendOp = TranslateBlendOp(desc.renderTarget[i].blendOpAlpha);
        a.colorWriteMask = desc.renderTarget[i].renderTargetWriteMask;
    }
    vkBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    vkBlend.attachmentCount = static_cast<uint32_t>(attachments.size());
    vkBlend.pAttachments = attachments.data();
}

SamplerVK::SamplerVK(VkDevice dev, const SamplerStateDesc &desc, sVec<SamplerVK *> *samplerReleasePointer): device(dev), samplerReleasePointer(samplerReleasePointer) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // Convert filters
    samplerInfo.minFilter = ToVkFilter(desc.minFilter);
    samplerInfo.magFilter = ToVkFilter(desc.magFilter);
    samplerInfo.mipmapMode = ToVkMipmapMode(desc.mipFilter);

    // Convert address modes
    samplerInfo.addressModeU = ToVkAddressMode(desc.addressU);
    samplerInfo.addressModeV = ToVkAddressMode(desc.addressV);
    samplerInfo.addressModeW = ToVkAddressMode(desc.addressW);

    // LOD
    samplerInfo.mipLodBias = desc.mipLODBias;
    samplerInfo.minLod = desc.minLOD;
    samplerInfo.maxLod = desc.maxLOD;

    // Anisotropy
    samplerInfo.anisotropyEnable = (desc.maxAnisotropy > 1) ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = static_cast<float>(desc.maxAnisotropy);

    // Comparison function
    if (desc.comparisonFunc != ComparisonFunc::Always) {
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = ToVkCompareOp(desc.comparisonFunc);
    }
    else {
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    }

    // Border color
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; // default
    if (desc.addressU == TextureAddressMode::Border ||
        desc.addressV == TextureAddressMode::Border ||
        desc.addressW == TextureAddressMode::Border) {
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
        // Or map your borderColor to closest VK_BORDER_COLOR enum
    }

    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan sampler!");
    }
}

void CommandListVK::BindPipeline(IPipeline* pipeline) {
    auto* vkPipeline = dynamic_cast<PipelineVK*>(pipeline);

    if (!vkPipeline || !vkPipeline->pipeline) return;

    //vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    vkPipeline->pipeline);

    draw.pipeline = pipeline;
}

void CommandListVK::BindDescriptorSet(IDescriptorSet* set, uint32_t index) {
    auto* vkSet = dynamic_cast<DescriptorSetVK*>(set);

    if (!vkSet || !vkSet->pipeline) return;

    draw.ds = set;
    draw.dsIndex = index;
}

void CommandListVK::BindViewPort(IViewPort* viewport2) {
    auto* viewport = dynamic_cast<ViewportVK*>(viewport2);
    if (!viewport) return;

    //vkCmdSetViewport(cmdBuffer, 0, 1, &viewport->viewport);
    draw.viewport = viewport2;
}

void CommandListVK::BindScissor(IViewPort* viewport2) {
    auto* viewport = dynamic_cast<ViewportVK*>(viewport2);
    if (!viewport) return;

    //vkCmdSetScissor(cmdBuffer, 0, 1, &viewport->scissor);
    draw.scissor = viewport2;
}

void CommandListVK::BindBuffer(IBuffer* buffer)
{
    auto* vkBuffer = dynamic_cast<BufferVK*>(buffer);
    if (!vkBuffer) return;

    if (vkBuffer->type == BufferType::Vertex) {
        //VkDeviceSize offsets[] = { 0 };
        //vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkBuffer->buffer, offsets);
        draw.buffer = buffer;
    }

}

void CommandListVK::Draw(PrimitiveType type, size_t vertexCount, size_t vertexOffset) {
    draw.type = type;
    draw.vertexCount = vertexCount;
    draw.vertexOffset = vertexOffset;

    pDraws->push_back(draw);
}

void CommandListVK::DrawIndexed(PrimitiveType type, IBuffer* indexBuffer, size_t indexCount, size_t indexOffset) {
    draw.type = type;
    draw.indexBuffer = indexBuffer;
    draw.indexCount = indexCount;
    draw.indexCount = indexOffset;

    pDraws->push_back(draw);
}

void CommandListVK::CopyToBuffer(IBuffer* buffer, void* data, size_t size) {
    auto* vkBuffer = dynamic_cast<BufferVK*>(buffer);
    void* mapped = nullptr;
    vkMapMemory(device, vkBuffer->memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, vkBuffer->memory);
}

VkImageMemoryBarrier CommandBufferVK::BarrierCreator(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
    ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) {
    auto vkTexture = dynamic_cast<TextureVK*>(image);
    if (!vkTexture) return {};


    if (oldLayout != image->explicitLayout) {
        oldLayout = image->explicitLayout;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = ToVkImageLayout(oldLayout);
    barrier.newLayout = ToVkImageLayout(newLayout);
    barrier.srcAccessMask = ToVkAccessFlags(srcAccessMask);
    barrier.dstAccessMask = ToVkAccessFlags(dstAccessMask);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkTexture->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    return barrier;
}

void CommandBufferVK::PipelineBarrierBatched(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
    ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) {
    auto vkTexture = dynamic_cast<TextureVK*>(image);
    if (!vkTexture) return;

    auto barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);
    pendingBarriers.push_back(barrier);

    image->explicitLayout = newLayout;
}

void CommandBufferVK::FlushBatchedBarriers() {

    if (!pendingBarriers.empty()) {
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkCmdPipelineBarrier(
            buffer,
            srcStage,    // src stage
            dstStage, // dst stage
            0,                                    // dependency flags
            0, nullptr,                            // memory barriers
            0, nullptr,                            // buffer barriers
            static_cast<uint32_t>(pendingBarriers.size()), pendingBarriers.data() // image barriers
        );

        pendingBarriers.clear();
    }
}

void CommandBufferVK::PipelineBarrier(ITexture *image, ImageLayout oldLayout, ImageLayout newLayout,
    ImageAccessLayout srcAccessMask, ImageAccessLayout dstAccessMask) {

    auto vkTexture = dynamic_cast<TextureVK*>(image);
    if (!vkTexture) return;


    if (oldLayout != image->explicitLayout) {
        oldLayout = image->explicitLayout;
    }

    VkImageMemoryBarrier barrier = BarrierCreator(image, oldLayout, newLayout, srcAccessMask, dstAccessMask);

    //Todo: Determine source/destination stage masks
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    vkCmdPipelineBarrier(
        this->buffer, // VkCommandBuffer
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    image->explicitLayout = newLayout;
}

DescriptorSetLayoutVK::DescriptorSetLayoutVK(VkDevice device, const DescriptorSetLayoutDesc& desc): device(device), desc(desc) {
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(desc.bindings.size());

    for (auto& b : desc.bindings) {
        vkBindings.push_back({ b.binding, ToVkDescriptorType(b.type), b.count, ToVkShaderStageFlags(b.stageFlags), nullptr });
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.flags = ToVkFlags(desc.flags);
    info.bindingCount = static_cast<uint32_t>(vkBindings.size());
    info.pBindings = vkBindings.data();

    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

DescriptorPoolVK::DescriptorPoolVK(VkDevice device, const DescriptorPoolDesc& desc): device(device) {
    std::vector<VkDescriptorPoolSize> vkSizes;
    vkSizes.reserve(desc.poolSizes.size());

    for (auto& s : desc.poolSizes) {
        vkSizes.push_back({ ToVkDescriptorType(s.first), s.second });
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = ToVkDescriptorPoolFlags(desc.flags);
    info.maxSets = desc.maxSets;
    info.poolSizeCount = static_cast<uint32_t>(vkSizes.size());
    info.pPoolSizes = vkSizes.data();

    if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

DescriptorSetVK::~DescriptorSetVK() = default;

void DescriptorSetVK::SetBuffer(uint32_t slot, IBuffer* buffer) {
    if (!buffer) return;

    // Ensure the vector is large enough
    if (slot >= buffers.size()) buffers.resize(slot + 1, nullptr);
    buffers[slot] = dynamic_cast<BufferVK*>(buffer);
}

void DescriptorSetVK::SetTexture(uint32_t slot, ITexture* texture) {
    if (!texture) return;

    if (slot >= textures.size()) textures.resize(slot + 1, nullptr);

    textures[slot] = dynamic_cast<TextureVK*>(texture);
}

void DescriptorSetVK::SetSampler(uint32_t slot, ISamplerState* sampler) {
    if (!sampler) return;

    if (slot >= samplers.size()) samplers.resize(slot + 1, nullptr);

    samplers[slot] = dynamic_cast<SamplerVK*>(sampler);
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

    // uint32_t textureBindingOffset = static_cast<uint32_t>(buffers.size());
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

PipelineVK::PipelineVK(sVec<PipelineVK *> *releaseVec, VkDevice dev, DescriptorSetLayoutVK *setLayout,
    DescriptorPoolVK *pool, const std::vector<ShaderVK *>& shaders, InputLayoutVK *inputLayout, RasterizerVK *rasterizer,
    DepthStencilStateVK *depthStencil, BlendStateVK *blendState, RenderTargetVK *renderTarget,
    IGraphicsDeviceConfig &config) : config(config), device(dev), descriptorSetLayout(setLayout), releaseVec(releaseVec), descriptorPool(pool), renderTarget(renderTarget) {
    // 1. Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout->layout;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    std::vector<VkPipelineShaderStageCreateInfo> vkStages;
    vkStages.reserve(shaders.size());
    for (auto shader : shaders) vkStages.push_back(shader->GetStageInfo());

    pipelineInfo.stageCount = static_cast<uint32_t>(vkStages.size());
    pipelineInfo.pStages = vkStages.data();
    pipelineInfo.pVertexInputState = &inputLayout->vkVertexInputInfo;
    pipelineInfo.pRasterizationState = &rasterizer->vkRaster;
    pipelineInfo.pDepthStencilState = &depthStencil->vkDepthStencil;
    pipelineInfo.pColorBlendState = &blendState->vkBlend;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    pipelineInfo.pInputAssemblyState = &inputAssembly;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicInfo.pDynamicStates = dynamicStates.data();
    pipelineInfo.pDynamicState = &dynamicInfo;

    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderTarget->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = pipeline;
    pipelineInfo.basePipelineIndex = 0;

    // Viewport + scissor must always be defined, even if dynamic
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; // Must be >0
    viewportState.pViewports = nullptr; // ignored if dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;  // ignored if dynamic
    pipelineInfo.pViewportState = &viewportState;

    // Multisampling (even if you don�t use MSAA)
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = IntToVkSampleCount(config.enableMSAA ? config.msaaSamples : 1);
    multisample.sampleShadingEnable = VK_FALSE;
    pipelineInfo.pMultisampleState = &multisample;

    auto res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline!");
}

IDescriptorSet * PipelineVK::CreateDescriptorSet(u32 slot) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool->pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout->layout;

    VkDescriptorSet vkSetHandle;
    auto result = vkAllocateDescriptorSets(device, &allocInfo, &vkSetHandle);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set!");

    auto* ds = new DescriptorSetVK(device, vkSetHandle, this);
    descriptorSets[slot] = ds;
    return ds;
}

IDescriptorSet* PipelineVK::GetDescriptorSet(u32 slot)
{
    auto it = descriptorSets.find(slot);
    if (it != descriptorSets.end()) {
        return it->second;
    }
    else return nullptr;
}

PipelineBuilderVK & PipelineBuilderVK::From(const IPipelineBuilder &builder) {
    if (const auto* src = dynamic_cast<const PipelineBuilderVK*>(&builder)) {
        this->shaders          = src->shaders;
        this->inputLayout      = src->inputLayout;
        this->renderPass     = src->renderPass;
        this->rasterizer       = src->rasterizer;
        this->depthStencil     = src->depthStencil;
        this->blendState       = src->blendState;
        this->descriptorPool   = src->descriptorPool;
        this->descriptorSetLayout = src->descriptorSetLayout;
    }

    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::AddShader(IShader *shader) {
    auto vShader = dynamic_cast<ShaderVK*>(shader);

    if (!vShader) return *this;

    shaders.push_back(vShader);
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetInputLayout(IInputLayout *layout) {
    auto vLayout = dynamic_cast<InputLayoutVK*>(layout);

    if (!vLayout) return *this;

    inputLayout = vLayout;
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetRasterizer(IRasterizerState *raster) {
    auto vRaster = dynamic_cast<RasterizerVK*>(raster);

    if (!vRaster) return *this;

    rasterizer = vRaster;
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetDepthStencil(IDepthStencilState *depth) {
    auto vDepth = dynamic_cast<DepthStencilStateVK*>(depth);

    if (!vDepth) return *this;

    depthStencil = vDepth;
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetBlendState(IBlendState *blend) {
    auto vBlend = dynamic_cast<BlendStateVK*>(blend);

    if (!vBlend) return *this;

    blendState = vBlend;
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetDescriptorPool(IDescriptorPool *pool) {
    auto vPool = dynamic_cast<DescriptorPoolVK*>(pool);

    if (!vPool) return *this;

    descriptorPool = vPool;
    return *this;
}

IPipelineBuilder & PipelineBuilderVK::SetRenderTarget(IRenderTarget *target) {
    auto vRenderTarget = dynamic_cast<RenderTargetVK*>(target);

    if (!vRenderTarget) return *this;

    renderPass = vRenderTarget;
    return *this;
}

PipelineBuilderVK & PipelineBuilderVK::SetDescriptorSetLayout(IDescriptorSetLayout *layout) {
    auto vLayout = dynamic_cast<DescriptorSetLayoutVK*>(layout);

    if (!vLayout) return *this;

    descriptorSetLayout = vLayout;
    return *this;
}

sVec<IShader *> * PipelineBuilderVK::GetShaders() {

    if (shaders.size() != interfaceShaders.size()) {
        interfaceShaders.clear();
        for (auto* s : shaders) {
            interfaceShaders.push_back(static_cast<IShader*>(s));
        }
    }

    return &interfaceShaders;
}

IPipeline * PipelineBuilderVK::Build() {
    if (!inputLayout || !rasterizer || !depthStencil || !blendState || !renderPass) {
        return nullptr;
    }

    auto* pipeline = new PipelineVK(
        releaseVec,
        device,
        descriptorSetLayout,
        descriptorPool,
        shaders,
        inputLayout,
        rasterizer,
        depthStencil,
        blendState,
        renderPass,
        config
    );

    return pipeline;
}
