
/* This file showcase a sand falling simutation run throught vulkan compute shaders.*/

/* Step :
 *   - Draw a quad 
 *   - Add 2D perspective and camera + size.
 *   - Draw many quad.
 *   - Discover the secondary buffer mystery.
 *   - Create a color array base on the type of the pixel.
 *   - Draw each quad as their corresponding color in the color buffer.
 *   - ... make them fall :)
 *   - happy noita !
 */

#include <iostream>
#include <memory>
#include <utility>
#include <filesystem>
#include <chrono>
#include <thread>

#define HEAVY_DEBUG false     // flush every log entries (python speed but back-trace segfault).
#define VKI_ENABLE_DEBUG_LOGS // extra logs to find some nasty bugs.

#include "VKInterface.hpp"
#include "logger.hpp"    // A custom logger for this exemple.
#include "utilities.hpp" // VKI isn't meant to be stand-alone.

//Helper functions.
void drawCall(VKI::VulkanContext &vContext, VKI::WindowContext &wContext, std::vector<VkBuffer> vertexBuffers, VkBuffer indexBuffer);
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
    RENDER_FINISH            = 2, // Since there are two images in-flight the indices must be offset by 2.
};
enum fenceNames : uint32_t
{
    SUBMIT_END = 0
};
enum my_buffers : uint32_t
{
    VERTEX_BUFFER   = 0,
    INDEX_BUFFER    = 1
};
enum my_queues : uint32_t
{
    GRAPHICS_QUEUE = 0,
    TRANSFER_QUEUE = 1,
    COMPUTE_QUEUE  = 1
};
enum computeQueueUsage : uint32_t
{
    TRANSFER_USAGE = 0,
    COMPUTE_USAGE  = 1
};

struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;
};
const std::array<Vertex, 4> quadGeometry   = {
    Vertex{{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    Vertex{{ 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    Vertex{{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    Vertex{{ 0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};
const std::array<uint16_t, 6> quadIndices = {
    0, 1, 2,
    1, 2, 3
};

const std::string progName = "VKI demo";
std::shared_ptr<GU::Logger> globalLogger = std::make_shared<GU::Logger>(std::filesystem::temp_directory_path()/progName/"logs/", HEAVY_DEBUG);

GU::LogInterface mainScope(globalLogger, "MainScope"), 
                 vkiLogger(globalLogger, "VKI")
                #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
                 ,vkiVLLogger(globalLogger, "Validation Layer")
                #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS
                 ;

int main(int argc, char** argv)
{
    bool drawOnce=false;
    if (argc>1)
        drawOnce = 0 == strncmp(argv[1], "--drawOnce", 11);

    // Set VKI logging callback.
    VKI::registerLogsVerboseCallback    ([](const char* msg){vkiLogger.logv(msg);});
    VKI::registerLogsInfoCallback       ([](const char* msg){vkiLogger.logi(msg);});
    VKI::registerLogsWarningCallback    ([](const char* msg){vkiLogger.logw(msg);});
    VKI::registerLogsErrorCallback      ([](const char* msg){vkiLogger.loge(msg);});
    VKI::registerLogsFatalErrorCallback ([](const char* msg){vkiLogger.logf(msg);});
   #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    // Set VKI validation layers logging callback.
    VKI::registerLogsValidationLayerVerboseCallback ([](const char* msg){vkiVLLogger.logv(msg);});
    VKI::registerLogsValidationLayerInfoCallback    ([](const char* msg){vkiVLLogger.logi(msg);});
    VKI::registerLogsValidationLayerWarningCallback ([](const char* msg){vkiVLLogger.logw(msg);});
    VKI::registerLogsValidationLayerErrorCallback   ([](const char* msg){vkiVLLogger.loge(msg);});
   #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

    // Register minimals requirements.
    VKI::PhysicalDeviceMinimalRequirement minRqd;
    VKI::resetPhysicalDeviceMinimalRequirement(minRqd); // Set all spec to 0 or VK_FALSE, so we only enable what we need.

    minRqd.swapChainInfo.capabilities.minImageCount = 2; // By default VKI create the maximum images available.
    minRqd.swapChainInfo.capabilities.maxImageCount = 0; // =inf : If gpu max is also 0 : VKI will create VKI_MAX_IMAGE_COUNT_IF_UNLIMITED (16 imgs).
    minRqd.swapChainInfo.capabilities.supportedTransforms     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    minRqd.swapChainInfo.capabilities.currentTransform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    minRqd.swapChainInfo.capabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;// Ignore alpha.
    minRqd.swapChainInfo.currentCompositeAlpha                = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    minRqd.swapChainInfo.capabilities.supportedUsageFlags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
                                                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                                              ;
    minRqd.swapChainInfo.formats.resize(1);
    minRqd.swapChainInfo.formats[0] = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    minRqd.swapChainInfo.presentModes.resize(3);                          // (note : presents modes are ordered from best to worst).
    minRqd.swapChainInfo.presentModes[0] = VK_PRESENT_MODE_FIFO_KHR;      // v-sync (always available).
    minRqd.swapChainInfo.presentModes[1] = VK_PRESENT_MODE_MAILBOX_KHR;   // aka immediate mode without tearing (not mandatory by the standard).
    minRqd.swapChainInfo.presentModes[2] = VK_PRESENT_MODE_IMMEDIATE_KHR; // unlimited frame rate : may cause tearing (guarantee of being available).
                                                                          // IMMEDIATE and FIFO are guarantee so it's a good idea to include
                                                                          // them even in last position to be sure that VKI can select at
                                                                          // least one present mode.
    //minRqd.swapChainInfo.queueFamilyIndicesSharingTheSwapChain = ?; // Auto filled with all presentable queues (in queueInfos (see below)).
    // TODO -> this way, no transfere queue can copy images to the swapchain ?!

    // Queues.
    // note : You should query the physical device's queue families (with getPhysicalQueueInfos) before 
    //        populating theses structures. For exemple nvidia (seems to) have a GRAPHICS + COMPUTE 
    //        queue family whereas AMD (seems to) have two separate families for these two operations.
    minRqd.queueInfos.resize(2);

    // GRAPHICS_QUEUE
    minRqd.queueInfos[GRAPHICS_QUEUE].count = 1;
    minRqd.queueInfos[GRAPHICS_QUEUE].priorities[0] = 1.0f;
    minRqd.queueInfos[GRAPHICS_QUEUE].operations = VK_QUEUE_GRAPHICS_BIT;
    minRqd.queueInfos[GRAPHICS_QUEUE].isPresentable = true;

    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos.resize(1);
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].poolsCount = 1;
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].poolsFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].commandBufferInfos.resize(1);
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].primaryCount = 2; // Two primary command buffer are required
                                                                                              // since there are two in flight frame.
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].secondaryCount = 0;
    minRqd.queueInfos[GRAPHICS_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].poolIndex = 0;

    /*
    // TRANSFER_QUEUE (which is probably going to be a compute queue (just to show what vki is capable of)).
    minRqd.queueInfos[TRANSFER_QUEUE].count = 1;
    minRqd.queueInfos[TRANSFER_QUEUE].priorities[0] = 1.0f;
    minRqd.queueInfos[TRANSFER_QUEUE].operations = VK_QUEUE_TRANSFER_BIT ;
    minRqd.queueInfos[TRANSFER_QUEUE].isPresentable = false;

    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos.resize(1);
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].poolsCount = 1;
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].poolsFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].commandBufferInfos.resize(1);
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].primaryCount = 1;
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].secondaryCount = 0;
    minRqd.queueInfos[TRANSFER_QUEUE].cmdPoolInfos[0].commandBufferInfos[0].poolIndex = 0;
    */

    // COMPUTE_QUEUE which will also be used as transfer queue.
    minRqd.queueInfos[COMPUTE_QUEUE].count = 2;
    minRqd.queueInfos[COMPUTE_QUEUE].priorities[0] = 1.0f;
    minRqd.queueInfos[COMPUTE_QUEUE].priorities[1] = 1.0f;
    minRqd.queueInfos[COMPUTE_QUEUE].operations = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    minRqd.queueInfos[COMPUTE_QUEUE].isPresentable = false;

    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos.resize(2); // One for compute and another one for transfer.
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].poolsCount = 1; // Aka how many thread will use those command buffers.
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].poolsFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].commandBufferInfos.resize(1);
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].commandBufferInfos[0].primaryCount = 1;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].commandBufferInfos[0].secondaryCount = 0;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[COMPUTE_USAGE].commandBufferInfos[0].poolIndex = 0;

    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].poolsCount = 1; // Mono thread.
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].poolsFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].commandBufferInfos.resize(1);
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].commandBufferInfos[0].primaryCount = 1;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].commandBufferInfos[0].secondaryCount = 0;
    minRqd.queueInfos[COMPUTE_QUEUE].cmdPoolInfos[TRANSFER_USAGE].commandBufferInfos[0].poolIndex = 0;

    // extensions :

    minRqd.instanceExtensions = {/*VK_REQUIRED_INSTANCE_EXTENSION_NAME*/};
    minRqd.deviceExtensions   = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VKI::registerGpuRequirement(minRqd, false);

    // Initialise VKI (glfw).
    VKI::init();

    /* Logs Information about the environment.
    mainScope.logv("Listing validation layers + extensions :");
    VKI::logAvailableValidationLayers();
    VKI::logAvailableExtension();*/

    // Bootstrap.
    VKI::WindowContext wContext = VKI::createWindowContext(1980, 1080, progName);
    VKI::VulkanContext vContext = VKI::createVulkanContext(VKI::createInstance(), 
                                                           wContext, 
                                                           4,   // 2*2 semaphores,
                                                           3,   // 2 graphics fences + 1 for transfer, 
                                                           true // Fences signaled by default,
                                                           );   // Auto best device. 

    // (not part of VKI) Register a simple callback to save in wContext if the window have been resized.
    registerFrameResizeEventCallback(&wContext);

    // Loading shaders :
    mainScope.logv("Loading quad.vert.spv .");
    std::vector<char> quadVertex    = GU::readFile("./shaders/quad.vert.spv");
    mainScope.logv("Loading quad.frag.spv .");
    std::vector<char> quadFragment  = GU::readFile("./shaders/quad.frag.spv");

    // meshes :

    VkVertexInputBindingDescription meshVertexBindingDescription;
    meshVertexBindingDescription.binding   = 0;              // Id of the binding (i.g. vkCmdBindVertexBuffer).
    meshVertexBindingDescription.stride    = sizeof(Vertex); // How much data per vertex shader.
    meshVertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;   // Move to the next [Vertex] after each verticies (vertex shader call).
                                        //!= VK_VERTEX_INPUT_RATE_INSTANCE; // Move to the next [Vertex] after each instances.

    std::array<VkVertexInputAttributeDescription, 2> meshVertexAttributeDescriptions; // 2 attr => 2 layout.
    meshVertexAttributeDescriptions[0].location = 0 ; // position layout location.
    meshVertexAttributeDescriptions[0].binding  = 0 ; // From which binding to pull data from.
    meshVertexAttributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT; // 2x4 bytes signed float.
    meshVertexAttributeDescriptions[0].offset   = offsetof(Vertex, position); // Offet of the field position in Vertex.
    meshVertexAttributeDescriptions[1].location = 1 ; // color layout location.
    meshVertexAttributeDescriptions[1].binding  = 0 ;
    meshVertexAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT; // 3x4 bytes signed float RGB.
    meshVertexAttributeDescriptions[1].offset   = offsetof(Vertex, color);

    // Buffer :

    mainScope.logv("Logging info about the available memory.");
    VKI::logMemoryInfo(vContext.physicalDeviceMemoryProperties);

    // Vertex buffer :
    VKI::BufferInfo vertexBufferInfo{};
    vertexBufferInfo.size  = sizeof(Vertex) * quadGeometry.size();
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertexBufferInfo.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vertexBufferInfo.queueFamilyIndicesSharingTheBuffer = {vContext.queueFamilies[GRAPHICS_QUEUE].info.familyIndex,
                                                           vContext.queueFamilies[TRANSFER_QUEUE].info.familyIndex};

    VKI::BufferMirror vertexBuffers = VKI::createBufferMirror(vContext.device, 
                                                              vertexBufferInfo,
                                                              true, // Allow write from host to device.
                                                              false // Don't allow read from device to host.
                                                             );

    for (auto& buffer : std::vector<std::shared_ptr<VKI::Buffer>>{vertexBuffers.hostBuffer, vertexBuffers.deviceBuffer})
    {
        if (!VKI::allocateBuffer(vContext.device, 
                                 *buffer,
                                 vContext.physicalDeviceMemoryProperties
                                ))
        {
            mainScope.logf("mainScope unable to find suitable memory for the vertex buffer !");
            std::cerr<<"Failed to find a suitable heap for the vertex memory, "
                       "please change the requirement for the vertex buffer\n"
                       "PS : The available memory properties should have been written to the logg file.";
            throw std::logic_error("main : unable to find suitable memory for the vertex buffer !");
        }
    }

    vContext.buffers.emplace_back(vertexBuffers.hostBuffer);
    vContext.buffers.emplace_back(vertexBuffers.deviceBuffer);

    mainScope.logv("loading vertex data to the device.");
    VKI::writeBuffer(vContext.device, *vertexBuffers.hostBuffer, quadGeometry.data()); // since no maxSize have been set : quadGeometry.size() must be >= buffer.info.size;
    VKI::unmapBuffer(vContext.device, *vertexBuffers.hostBuffer);

    // Transfering stagging vertex buffer to device vertex buffer.
    auto cmdCopyBuffer = vContext.queueFamilies[TRANSFER_QUEUE].commands[0].PBuffers[0];          // Extract the command buffer.

    VKI::cmdBeginRecordCommandBuffer(cmdCopyBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // Begin record.
    VKI::recordPushBufferMirror(vertexBuffers, cmdCopyBuffer);                                    // Record copy.
    VKI::cmdEndRecordCommandBuffer(cmdCopyBuffer);                                                // End record.

    {
    VKI::SubmitInfo transferSubmitOrder{};
    transferSubmitOrder.commandBuffers.push_back(cmdCopyBuffer);

    VKI::resetFence(vContext.device, vContext.fences[2]); // reset the fence before submit.
    VKI::queueSubmit(vContext.queueFamilies[TRANSFER_QUEUE].queues[0], {transferSubmitOrder}, vContext.fences[2]);
    VKI::waitFence(vContext.device, vContext.fences[2], UINT64_MAX, true); // true => enable logs.
    }

    // Indices Buffer :
    VKI::BufferInfo indicesBufferInfo{};
    indicesBufferInfo.size  = sizeof(uint16_t) * quadIndices.size();
    indicesBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    indicesBufferInfo.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    indicesBufferInfo.queueFamilyIndicesSharingTheBuffer = {vContext.queueFamilies[GRAPHICS_QUEUE].info.familyIndex,
                                                           vContext.queueFamilies[TRANSFER_QUEUE].info.familyIndex};

    VKI::BufferMirror indicesBuffers = VKI::createBufferMirror(vContext.device, 
                                                               indicesBufferInfo,
                                                               true, // Allow write from host to device.
                                                               false // Don't enable read from device to host.
                                                              );

    for (auto& buffer : std::vector<std::shared_ptr<VKI::Buffer>>{indicesBuffers.hostBuffer, indicesBuffers.deviceBuffer})
    {
        if (!VKI::allocateBuffer(vContext.device, 
                                 *buffer,
                                 vContext.physicalDeviceMemoryProperties
                                ))
        {
            mainScope.logf("mainScope unable to find suitable memory for the indices buffer !");
            std::cerr<<"Failed to find a suitable heap for the indices memory, "
                       "please change the requirement for the indices buffer\n"
                       "PS : The available memory properties should have been written to the logg file.";
            throw std::logic_error("main : unable to find suitable memory for the index buffer !");
        }
    }

    vContext.buffers.emplace_back(indicesBuffers.hostBuffer);
    vContext.buffers.emplace_back(indicesBuffers.deviceBuffer);

    mainScope.logv("loading index data to the device.");
    VKI::writeBuffer(vContext.device, *indicesBuffers.hostBuffer, quadIndices.data()); // since no maxSize have been set : quadIndices.size() must be >= buffer.info.size;
    VKI::unmapBuffer(vContext.device, *indicesBuffers.hostBuffer);

    // Transfering stagging indices buffer to device index buffer.
    cmdCopyBuffer = vContext.queueFamilies[TRANSFER_QUEUE].commands[0].PBuffers[0];       // Extract the command buffer.

    VKI::cmdBeginRecordCommandBuffer(cmdCopyBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // Begin record.
    VKI::recordPushBufferMirror(indicesBuffers, cmdCopyBuffer);                                    // Record copy.
    VKI::cmdEndRecordCommandBuffer(cmdCopyBuffer);                                                // End record.

    VKI::SubmitInfo transferSubmitOrder{};
    transferSubmitOrder.commandBuffers.push_back(cmdCopyBuffer);

    VKI::resetFence(vContext.device, vContext.fences[2]); // reset the fence before submit.
    VKI::queueSubmit(vContext.queueFamilies[TRANSFER_QUEUE].queues[0], {transferSubmitOrder}, vContext.fences[2]);
    VKI::waitFence(vContext.device, vContext.fences[2], UINT64_MAX, true); // enable logs.

    // Creating the graphics pipeline and render pass.
    VKI::RenderPass renderPass;

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
    pipelineInfo.vertexInputBindingDescriptions.resize(1);
    pipelineInfo.vertexInputBindingDescriptions[0] = meshVertexBindingDescription;
    pipelineInfo.vertexInputAttributeDescriptions.assign(meshVertexAttributeDescriptions.begin(), 
                                                         meshVertexAttributeDescriptions.end()
                                                        );
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

    renderPass.pipelines.resize(1);
    renderPass.pipelines[0].info = pipelineInfo;
    renderPass.renderPassInfo = renderPassInfo;

    vContext.renderPasses.resize(1);
    vContext.renderPasses[0] = renderPass;

    /// Create the VkPipelineLarenderPassenderPass/VkPipeline
    vContext.renderPasses[0].pipelines[0].layout = VKI::createPipelineLayout(vContext.device, 
                                                            vContext.renderPasses[0].pipelines[0].info);
    vContext.renderPasses[0].renderPass = VKI::createRenderPass(vContext.device, 
                                                            vContext.renderPasses[0].renderPassInfo);
    vContext.renderPasses[0].pipelines[0].pipeline = VKI::createGraphicsPipeline(vContext.device, 
                                                            vContext.renderPasses[0].pipelines[0].layout,
                                                            vContext.renderPasses[0].renderPass,
                                                            vContext.renderPasses[0].pipelines[0].info);

    vContext.swapchainFramebuffers.resize(vContext.swapchainImageViews.size());
    for (size_t i(0) ; i<vContext.swapchainFramebuffers.size() ; i++)
    {
        vContext.swapchainFramebuffers[i] = VKI::createFramebuffer(vContext.device,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.width,
                                                                   vContext.swapChainInfo.capabilities.currentExtent.height,
                                                                   vContext.renderPasses.front().renderPass,
                                                                   { vContext.swapchainImageViews[i] }
                                                                  );
    }

    // Disable vki logs durring frame drawings. 
    disableVkiInfoLogs();

    mainScope.logv("Main loop begin.");

    uint32_t frameCount = 0;
    do
    {
        glfwPollEvents();

        if (wContext.eventWindowResized)
        {
            reCreateSwapchain(vContext, wContext); // TODO their is a error occuring when forcing full screen where a semaphore is signaled
                                                   // before passing it to acquireNextImage ... idk check the logs.
            wContext.eventWindowResized = false;
        }

        drawCall(vContext, wContext, {vertexBuffers.deviceBuffer->buffer}, indicesBuffers.deviceBuffer->buffer);

        // display frame rate every 16 frames.
        if (frameCount >= 16)
        {
            double frameRate;
            GU::countFrameRate(&frameRate);
            std::cout<<frameRate<<" fps\r"<<std::flush; // Yes I know.
            frameCount=0;
        }
        else
        {
            GU::countFrameRate();
            frameCount++;
        }

        //std::this_thread::sleep_for(std::chrono::duration<double, std::milli>{16});
    }
    while (!VKI::windowShouldClose(wContext) && !drawOnce);
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

void drawCall(VKI::VulkanContext &vContext, VKI::WindowContext &wContext, std::vector<VkBuffer> vertexBuffers, VkBuffer indexBuffer)
{
    static std::vector<VKI::SubmitInfo> submitInfos(2);
    static std::vector<VkDeviceSize>    vertexBufferOffsets = {0};
    VKI::SwapchainStatusFlags           swapchainErrors;
    const VKI::SwapchainStatusFlags     supportedSwapchainErrors = VKI::SWAPCHAIN_STATUS_OUT_OF_DATE_BIT    |
                                                                 //VKI::SWAPCHAIN_STATUS_SURFACE_LOST_BIT   |
                                                                   VKI::SWAPCHAIN_STATUS_SUBOPTIMAL_BIT     ;

    static uint32_t currentFrame = 1; // Tell which of the two in flight frame buffer are use.
    currentFrame = (currentFrame+1)%2; // use & 2 ?? TODO

    VkQueue         &queue      = vContext.queueFamilies[0].queues[0];            // only one queue is used.
    VkPipeline      &pipeline   = vContext.renderPasses[0].pipelines[0].pipeline; // since only one pipeline exist.
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
                         vContext.renderPasses[0].renderPass, 
                         vContext.swapchainFramebuffers[scImageIndex],
                         {0,0,vContext.swapChainInfo.capabilities.currentExtent.width, vContext.swapChainInfo.capabilities.currentExtent.height},
                         { {{{0.0f, 0.0f, 0.0f, 1.0f}}} },
                         VK_SUBPASS_CONTENTS_INLINE
                        );

    VKI::cmdBindPipeline(cmdBuffer, pipeline);

    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers.data(), vertexBufferOffsets.data());
    vkCmdBindIndexBuffer  (cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(quadIndices.size()), 1, 0, 0, 0);
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
    {
        reCreateSwapchain(vContext, wContext);
    }
}

void reCreateSwapchain(VKI::VulkanContext &vContext, VKI::WindowContext &wContext)
{
    // Logs surface size change :
    static std::pair<unsigned, unsigned> dims = {vContext.swapChainInfo.capabilities.currentExtent.width, vContext.swapChainInfo.capabilities.currentExtent.height};
    std::stringstream ss;
    ss<<"Recreating the swapchain begin : from ";
    ss<<dims.first<<"x"<<dims.second<<" to ";
    ss<<vContext.swapChainInfo.capabilities.currentExtent.width<<"x";
    ss<<vContext.swapChainInfo.capabilities.currentExtent.height;
    mainScope.logi(ss.str().c_str());
    dims = {vContext.swapChainInfo.capabilities.currentExtent.width, vContext.swapChainInfo.capabilities.currentExtent.height};

    enableVkiLogs();      //
    disableVkiInfoLogs(); // Only capture warnings/error.

    VKI::waitDeviceBecomeIdle(vContext.device);

    // Destroy all swapchain related objects.
    for (size_t i(0) ; i<vContext.swapchainFramebuffers.size() ; i++)
        VKI::destroyFrameBuffer(vContext.device, vContext.swapchainFramebuffers[i]);

    for (VkImageView &imageView : vContext.swapchainImageViews)
        VKI::destroyImageView(vContext.device, imageView);

    vkDestroySwapchainKHR(vContext.device, vContext.swapChain, nullptr);

    // create a new swapchain.

RESIZE_SWAPCHAIN: // Store the new extend :
    updateSwapchainExtents(vContext.physicalDevice, vContext.surface, wContext, vContext.swapChainInfo); 
    if ((vContext.swapChainInfo.capabilities.currentExtent.height==0) || (vContext.swapChainInfo.capabilities.currentExtent.width==0))
    {
        glfwWaitEvents(); // Since there is no surface to render to, we wait for a resize event.
        goto RESIZE_SWAPCHAIN;
    }
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
                                                                   vContext.renderPasses.front().renderPass,
                                                                   { vContext.swapchainImageViews[i] }
                                                                  );
    }

    disableVkiInfoLogs();
    mainScope.logi("Recreating the swapchain end.");
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
