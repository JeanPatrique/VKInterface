
/* This file is a exemple on how to draw a simple triangle with VKI.*/

#include <iostream>
#include <memory>
#include <utility>
#include <filesystem>
#include <chrono>

#include "VKInterface.hpp"
#include "logger.hpp"    // my custom logger for this exemple.
#include "utilities.hpp" // VKI isn't versatile enough to be stand-alone.

#define HEAVY_DEBUG false // flush every log entries (python speed).

//Helper functions.
void drawCall(VKI::VulkanContext &vContext, VKI::WindowContext &wContext);
void reCreateSwapchain(VKI::VulkanContext &vContext, VKI::WindowContext &wContext);

void enableVkiLogs();
void enableVkiInfoLogs();
void disableVkiLogs();
void disableVkiInfoLogs();

void registerFrameResizeEventCallback(VKI::WindowContext *wContext);

// Index names :
enum semaphoreNames : uint32_t
{
    SWAPCHAIN_IMG_AVAILABLE  = 0,
    RENDER_FINISH            = 2, // Since there is two images in-flight the indicies must be offset by 2.
};
enum fenceNames : uint32_t
{
    SUBMIT_END = 0
};

const std::string progName = "VKI demo";
std::shared_ptr<GU::Logger> globalLogger = std::make_shared<GU::Logger>(std::filesystem::temp_directory_path()/progName/"logs/", HEAVY_DEBUG);

GU::LogInterface mainScope(globalLogger, "MainScope"), 
                 vkiLogger(globalLogger, "VKI")
                #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
                 ,vkiVLLogger(globalLogger, "Validation Layer")
                #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS
                 ;

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    // Set vki logging callback.
    VKI::registerLogsVerboseCallback    ([](const char* msg){vkiLogger.logv(msg);});
    VKI::registerLogsInfoCallback       ([](const char* msg){vkiLogger.logi(msg);});
    VKI::registerLogsWarningCallback    ([](const char* msg){vkiLogger.logw(msg);});
    VKI::registerLogsErrorCallback      ([](const char* msg){vkiLogger.loge(msg);});
    VKI::registerLogsFatalErrorCallback ([](const char* msg){vkiLogger.logf(msg);});
   #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    // Set vki validation layers logging callback.
    VKI::registerLogsValidationLayerVerboseCallback ([](const char* msg){vkiVLLogger.logv(msg);});
    VKI::registerLogsValidationLayerInfoCallback    ([](const char* msg){vkiVLLogger.logi(msg);});
    VKI::registerLogsValidationLayerWarningCallback ([](const char* msg){vkiVLLogger.logw(msg);});
    VKI::registerLogsValidationLayerErrorCallback   ([](const char* msg){vkiVLLogger.loge(msg);});
   #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

    // Logs Information about the environment.
    //VKI::logAvailableValidationLayers();
    //VKI::logAvailableExtension();

    // Register minimals requirements.
    VKI::PhysicalDeviceMinimalRequirement minRqd;
    VKI::getNullPhysicalDeviceMinimalRequirement(minRqd); // Set all spec to 0 or VK_FALSE, so we only enable what we need.

    minRqd.swapChainInfo.capabilities.minImageCount = 2; // By default VKI create the maximum images available.
    minRqd.swapChainInfo.capabilities.maxImageCount = 3;
    minRqd.swapChainInfo.capabilities.supportedTransforms     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    minRqd.swapChainInfo.capabilities.currentTransform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    minRqd.swapChainInfo.capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;// Ignore alpha.
    minRqd.swapChainInfo.currentCompositeAlpha                = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    minRqd.swapChainInfo.capabilities.supportedUsageFlags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT

    // Note on format nomenclature : https://stackoverflow.com/questions/59628956/what-is-the-difference-between-normalized-scaled-and-integer-vkformats#answer-59630187
    minRqd.swapChainInfo.formats.resize(1);
    minRqd.swapChainInfo.formats[0] = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    minRqd.swapChainInfo.presentModes.resize(3);
    minRqd.swapChainInfo.presentModes[0] = VK_PRESENT_MODE_FIFO_KHR;      // v-sync.
    minRqd.swapChainInfo.presentModes[1] = VK_PRESENT_MODE_MAILBOX_KHR;   // aka no-tearing immediate mode.
    minRqd.swapChainInfo.presentModes[2] = VK_PRESENT_MODE_IMMEDIATE_KHR; // unlimit frame rate : may cause tearing.

    // Queues.
    minRqd.queueInfos.resize(1);
    minRqd.queueInfos[0].count = 1;
    minRqd.queueInfos[0].priorities[0] = 1.0f;
    minRqd.queueInfos[0].operations = VK_QUEUE_GRAPHICS_BIT |
                                      VK_QUEUE_COMPUTE_BIT  |
                                      VK_QUEUE_TRANSFER_BIT ;
    minRqd.queueInfos[0].isPresentable = true;
    minRqd.swapChainInfo.queueFamilyIndicesSharingTheSwapChain = {0};

    minRqd.queueInfos[0].cmdPoolInfos.resize(1);
    minRqd.queueInfos[0].cmdPoolInfos[0].poolsCount = 1;
    minRqd.queueInfos[0].cmdPoolInfos[0].poolsFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    minRqd.queueInfos[0].cmdPoolInfos[0].commandBufferInfos.resize(1);
    minRqd.queueInfos[0].cmdPoolInfos[0].commandBufferInfos[0].primaryCount = 2; // Two primary command buffer are required.
    minRqd.queueInfos[0].cmdPoolInfos[0].commandBufferInfos[0].secondaryCount = 0;
    minRqd.queueInfos[0].cmdPoolInfos[0].commandBufferInfos[0].poolIndex = 0; // i.e. From wich pool to allocate from.

    minRqd.instanceExtensions = {/*VK_REQUIRED_INSTANCE_EXTENSION_NAME*/};
    minRqd.deviceExtensions   = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VKI::registerGpuRequirement(minRqd, false);

    // Initialise VKI (glfw).
    VKI::init();

    // Simple bootstrap.
    VKI::WindowContext wContext = VKI::createWindowContext(1960, 1080, progName);
    VKI::VulkanContext vContext = VKI::createVulkanContext(VKI::createInstance(), wContext, 4); // 2*2 semaphores,
                                                                                                // no fences.
    vContext.fences.resize(2); // Here is a great exemple of not using VKI because the default behavior isn't desired.
    vContext.fences[0] = VKI::createFence(vContext.device, true); // By default, createVulkanContext create fence unsignaled.
    vContext.fences[1] = VKI::createFence(vContext.device, true); // Which will block execution indefinitly on first
                                                                  // drawCall() call.

    registerFrameResizeEventCallback(&wContext);// register a simple callback to save in wContext if the window have been resized.

    // Loading resources :
    mainScope.logv("Loading quad.vert.spv .");
    std::vector<char> quadVertex    = GU::readFile("Build/src/Cellular_Automata/runtime/shaders/quad.vert.spv");
    mainScope.logv("Loading quad.frag.spv .");
    std::vector<char> quadFragment  = GU::readFile("Build/src/Cellular_Automata/runtime/shaders/quad.frag.spv");

    // Creating the graphics pipeline and render pass.
    VKI::GraphicsContext gContext;

    VKI::PipelineInfo pipelineInfo; 
    pipelineInfo.subpassIndex             = 0;
    pipelineInfo.vertexShader             = VKI::createShaderModule(vContext.device, quadVertex);
    pipelineInfo.fragmentShader           = VKI::createShaderModule(vContext.device, quadFragment);
    pipelineInfo.vertexShaderEntryPoint   = "main";
    pipelineInfo.fragmentShaderEntryPoint = "main";
    pipelineInfo.topology                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInfo.enablePrimitiveRestart   = false;
    pipelineInfo.viewports.resize(1);
    pipelineInfo.viewports[0].x        = 0.0f;
    pipelineInfo.viewports[0].y        = 0.0f;
    pipelineInfo.viewports[0].width    = static_cast<float>(vContext.swapChainInfo.capabilities.currentExtent.width);
    pipelineInfo.viewports[0].height   = static_cast<float>(vContext.swapChainInfo.capabilities.currentExtent.height);
    pipelineInfo.viewports[0].minDepth = 0.0f;
    pipelineInfo.viewports[0].maxDepth = 1.0f;
    pipelineInfo.scissors.resize(1);
    pipelineInfo.scissors[0].offset = {0, 0};
    pipelineInfo.scissors[0].extent = vContext.swapChainInfo.capabilities.currentExtent;
    pipelineInfo.cullMode           = VK_CULL_MODE_NONE; // Discard no triangle.

    VKI::RenderPassInfo renderPassInfo = {};
    VKI::RenderSubPassInfo renderSubPassInfo = {};
    renderSubPassInfo.colorAttachmentReferences.resize(1);
    renderSubPassInfo.colorAttachmentReferences[0].attachment = 0;
    renderSubPassInfo.colorAttachmentReferences[0].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attDsc = {};
     attDsc.format          = vContext.swapChainInfo.formats.front().format;
     attDsc.samples         = VK_SAMPLE_COUNT_1_BIT;
     attDsc.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
     attDsc.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
     attDsc.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
     attDsc.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
     attDsc.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
     attDsc.finalLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    renderPassInfo.attachmentDescs.resize(1);
    renderPassInfo.attachmentDescs[0] = attDsc;
    renderPassInfo.subPassInfos.resize(1);
    renderPassInfo.subPassInfos[0] = renderSubPassInfo;
    renderPassInfo.subPassDependencies.resize(1);
    renderPassInfo.subPassDependencies[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    renderPassInfo.subPassDependencies[0].dstSubpass    = 0;
    renderPassInfo.subPassDependencies[0].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    renderPassInfo.subPassDependencies[0].srcAccessMask = 0;
    renderPassInfo.subPassDependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    renderPassInfo.subPassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    renderPassInfo.subPassDependencies[0].dependencyFlags = 0;

    
    gContext.pipelines.resize(1);
    gContext.pipelines[0].info = pipelineInfo;
    gContext.renderPassInfo = renderPassInfo;

    vContext.graphicsContexts.resize(1);
    vContext.graphicsContexts[0] = gContext;

    /// Create the VkPipelineLayout/VKRenderPass/VkPipeline
    vContext.graphicsContexts[0].pipelines[0].layout = VKI::createPipelineLayout(vContext.device, 
                                                            vContext.graphicsContexts[0].pipelines[0].info);
    vContext.graphicsContexts[0].renderPass = VKI::createRenderPass(vContext.device, 
                                                            vContext.graphicsContexts[0].renderPassInfo);
    vContext.graphicsContexts[0].pipelines[0].pipeline = VKI::createGraphicsPipeline(vContext.device, 
                                                            vContext.graphicsContexts[0].pipelines[0].layout,
                                                            vContext.graphicsContexts[0].renderPass,
                                                            vContext.graphicsContexts[0].pipelines[0].info);

    vContext.swapchainFramebuffers.resize(vContext.swapchainImageViews.size());
    for (size_t i(0) ; i<vContext.swapchainFramebuffers.size() ; i++)
    {
        vContext.swapchainFramebuffers[i] = VKI::createFramebuffer(vContext.device,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.width,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.height,
                                                                   vContext.graphicsContexts.front().renderPass,
                                                                   { vContext.swapchainImageViews[i] }
                                                                  );
    }

    // Disable vki logs durring frame drawings. 
    disableVkiInfoLogs();

    mainScope.logv("Main loop begin.");

    uint32_t frameCount = 0;
    while (!VKI::windowShouldClose(wContext))
    {
        glfwPollEvents();

        if (wContext.eventWindowResized)
            reCreateSwapchain(vContext, wContext);
        drawCall(vContext, wContext);

        // display frame rate.
        if (frameCount >= 16)
        {
            double frameRate;
            GU::countFrameRate(&frameRate);
            std::cout<<frameRate<<" fps\r"<<std::flush;
            frameCount=0;
        }
        else
        {
            GU::countFrameRate();
            frameCount++;
        }
    }
    std::cout<<"\n";

    mainScope.logv("Main loop end.");

    // Re-enable VKI logs callbacks for destruction.
    enableVkiLogs();

    VKI::destroyVulkanContext(vContext);

    // Terminate VKI (glfw).
    VKI::terminate();
    std::cout<<"Pure success !"<<std::endl;
    return EXIT_SUCCESS;
}

void drawCall(VKI::VulkanContext &vContext, VKI::WindowContext &wContext)
{
    static std::vector<VKI::SubmitInfo> submitInfos(2);
    const VKI::SwapchainStatusFlags supportedSwapchainErrors = VKI::SWAPCHAIN_STATUS_OUT_OF_DATE_BIT |
                                                               VKI::SWAPCHAIN_STATUS_SUBOPTIMAL_BIT;
    VKI::SwapchainStatusFlags swapchainErrors;

    static uint32_t currentFrame = 1; // Tell which of the two in flight frame buffer are use.
    currentFrame = (currentFrame+1)%2;

    VkQueue         &queue      = vContext.queueFamilies[0].queues[0];                // only one queue is used.
    VkPipeline      &pipeline   = vContext.graphicsContexts[0].pipelines[0].pipeline; // as only one pipeline exist.
    VkCommandBuffer &cmdBuffer  = vContext.queueFamilies[0].commands[0].PBuffers[currentFrame];


    VKI::waitFence(vContext.device, vContext.fences[currentFrame]);

    // If v-sync (swapchain.presentMode==FIFO) is enabled, this function below will block execution to the frame rate.
    uint32_t scImageIndex = VKI::acquireNextImage(vContext.device, 
                                                  vContext.swapChain, 
                                                  vContext.semaphores[SWAPCHAIN_IMG_AVAILABLE + currentFrame],
                                                  VK_NULL_HANDLE,
                                                  UINT64_MAX,
                                                  &swapchainErrors,
                                                  supportedSwapchainErrors
                                                 );
    if (scImageIndex == static_cast<uint32_t>(-1))
    {
        throw std::runtime_error("failed to acquire the next image.");
    }
    else if (swapchainErrors.any())
    {
        reCreateSwapchain(vContext, wContext);
        return;
    }
    else
        VKI::resetFence(vContext.device, vContext.fences[currentFrame]); // Reset the fence ONLY if the command can be submited.

    // Record the command buffer.
    VKI::cmdResetCommandBuffer(cmdBuffer);
    VKI::cmdBeginRecordCommandBuffer(cmdBuffer);

    VKI::cmdBeginRenderPass(cmdBuffer,
                         vContext.graphicsContexts[0].renderPass, 
                         vContext.swapchainFramebuffers[scImageIndex],
                         {0,0,vContext.swapChainInfo.capabilities.currentExtent.width, vContext.swapChainInfo.capabilities.currentExtent.height},
                         { {{{0.0f, 0.0f, 0.0f, 1.0f}}} },
                         VK_SUBPASS_CONTENTS_INLINE
                        );

    VKI::cmdBindPipeline(cmdBuffer, pipeline);
    VKI::cmdDraw(cmdBuffer, 3, 1, 0, 0);
    VKI::cmdEndRenderPass(cmdBuffer);
    VKI::cmdEndRecordCommandBuffer(cmdBuffer);

    submitInfos[currentFrame].waitSemaphoresAtStages = { {vContext.semaphores[SWAPCHAIN_IMG_AVAILABLE + currentFrame], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}};
    submitInfos[currentFrame].commandBuffers = {cmdBuffer};
    submitInfos[currentFrame].signalSemaphores = {vContext.semaphores[RENDER_FINISH + currentFrame]};

    VKI::queueSubmit(queue, {submitInfos[currentFrame]}, vContext.fences[SUBMIT_END + currentFrame]);

    VKI::queuePresent(queue, scImageIndex, vContext.swapChain, {vContext.semaphores[RENDER_FINISH + currentFrame]},
                      &swapchainErrors,
                      supportedSwapchainErrors
                     );

    if (swapchainErrors.any())
        reCreateSwapchain(vContext, wContext);
}

void reCreateSwapchain(VKI::VulkanContext &vContext, VKI::WindowContext &wContext)
{
    //mainScope.logi("Recreating the swapchain begin.");
    //enableVkiInfoLogs();

    //VKI::resetFence(vContext.device, vContext.fences[SUBMIT_END + currentFrameIndex]);// Reset the fence so the next draw call won't block.
    VKI::waitDeviceBecomeIdle(vContext.device);

    // Destroy all swapchain related objects.
    for (size_t i(0) ; i<vContext.swapchainFramebuffers.size() ; i++)
        VKI::destroyFrameBuffer(vContext.device, vContext.swapchainFramebuffers[i]);

    for (VkImageView &imageView : vContext.swapchainImageViews)
        VKI::destroyImageView(vContext.device, imageView);

    vkDestroySwapchainKHR(vContext.device, vContext.swapChain, nullptr);

    // create a new swapchain.

    // Store the new extend :
    updateSwapchainExtents(vContext.physicalDevice, vContext.surface, wContext, vContext.swapChainInfo); 
    vContext.swapChain = createSwapChain(vContext.device, vContext.swapChainInfo, vContext.surface);

    vContext.swapchainImages = VKI::fetchImagesFromSwapChain(vContext.device, vContext.swapChain);

    vContext.swapchainImageViews = VKI::createImageViews(vContext.device,
                                                         vContext.swapchainImages,
                                                         vContext.swapChainInfo.formats[0].format,
                                                         vContext.swapChainInfo.subresourceRange
                                                        );

    vContext.swapchainFramebuffers.resize(vContext.swapchainImageViews.size());
    for (size_t i(0) ; i<vContext.swapchainFramebuffers.size() ; i++)
    {
        vContext.swapchainFramebuffers[i] = VKI::createFramebuffer(vContext.device,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.width,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.height,
                                                                   vContext.graphicsContexts.front().renderPass,
                                                                   { vContext.swapchainImageViews[i] }
                                                                  );
    }

    //disableVkiInfoLogs();
    //mainScope.logi("Recreating the swapchain end.");
}

inline void enableVkiInfoLogs()
{
    VKI::registerLogsInfoCallback([](const char* msg){vkiLogger.logi(msg);});
}
inline void disableVkiInfoLogs()
{
    VKI::registerLogsInfoCallback([](const char*){});
}
void enableVkiLogs()
{
    VKI::registerLogsVerboseCallback    ([](const char* msg){vkiLogger.logv(msg);});
    VKI::registerLogsInfoCallback       ([](const char* msg){vkiLogger.logi(msg);});
    VKI::registerLogsWarningCallback    ([](const char* msg){vkiLogger.logw(msg);});
    VKI::registerLogsErrorCallback      ([](const char* msg){vkiLogger.loge(msg);});
    VKI::registerLogsFatalErrorCallback ([](const char* msg){vkiLogger.logf(msg);});
}

void disableVkiLogs()
{
    VKI::registerLogsVerboseCallback    ([](const char*){});
    VKI::registerLogsInfoCallback       ([](const char*){});
    VKI::registerLogsWarningCallback    ([](const char*){});
    VKI::registerLogsErrorCallback      ([](const char*){});
    VKI::registerLogsFatalErrorCallback ([](const char*){});
}

void registerFrameResizeEventCallback(VKI::WindowContext *wContext)
{
    glfwSetWindowUserPointer(wContext->window, wContext);
    glfwSetFramebufferSizeCallback(wContext->window, 
            [](GLFWwindow* window, int, int)
            {
                VKI::WindowContext *wContext = reinterpret_cast<VKI::WindowContext*>(glfwGetWindowUserPointer(window));
                if (wContext==nullptr) 
                    throw std::logic_error("Resize callback got a nullptr insted of a WindowContext.");
                else
                    wContext->eventWindowResized=true;
            }
    );
}
