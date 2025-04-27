#ifndef VKINTERFACE_HEADER 
#define VKINTERFACE_HEADER 

/* If you're here, it might be cause I didn't made a clean docs.
 * To get an overview of this lib, read the structure defined right after.
 * Then search for all '///'. Thoses are chapter-like section that group functions by topics (e.g. /// Queue Family).
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#include <glm/vec4.hpp>
//#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

#include "libgmp/gmpxx.h" // The GMP library is overkill for it use in VKI. 
                          // It's used to compute the score/ranking of every 
                          // GPU on the system. And since there is a lot of 
                          // parameter to count, an uint64_t integer isn't 
                          // enought.

#include <cstdint>
#include <atomic>
#include <memory>
#include <functional>
#include <initializer_list>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <set>
#include <optional>
#include <utility> // <3 std::pair;
#include <bitset>

#define VKI_MAX_QUEUE_COUNT_PER_FAMILY 16 /*uint8_t  If >255 : UB */
#define VKI_IMAGE_COUNT_IF_UNLIMITED   16 /*uint32_t How many images to create in the swapchain has unlimited imgs count.*/

/* Macro-options :
#define VKI_DISABLE_LOGS_CALL // If defined, all call to the logs functions are remove. // TODO !
#define VKI_ENABLE_DEBUG_LOGS // If defined, this include more logs calls to inspect some functions behavior.
*/

/**@brief VulKanInterface : All you need to draw a quad to the screen.
 *        *Info structure are intented to be fill by the user (unless specified).
 * @note VKI use glfw to be cross platform but can be changed if needed (the required change have been keept at a minimum).
 * @note The registerLogs* callback should be called before any other function (even init).
 * @warning VKI doesn't perform any test on user provided value (almost).
 *          i.e. if you set minImageCount=0 (in the swapchainInfo minimal requirement) no warnings will be displayed 
 *          and the behavior is UB !
 *
 * 
 *
 *
 */
namespace VKI
{
    void registerLogsVerboseCallback(std::function<void(const char*)>);
    void registerLogsInfoCallback(std::function<void(const char*)>);
    void registerLogsWarningCallback(std::function<void(const char*)>);
    void registerLogsErrorCallback(std::function<void(const char*)>);
    void registerLogsFatalErrorCallback(std::function<void(const char*)>);

    void registerLogsValidationLayerVerboseCallback(std::function<void(const char*)>);
    void registerLogsValidationLayerInfoCallback(std::function<void(const char*)>);
    void registerLogsValidationLayerWarningCallback(std::function<void(const char*)>);
    void registerLogsValidationLayerErrorCallback(std::function<void(const char*)>);

    // Types :

    /**@brief Callback signature to manualy select the physical device to use.
     * By default VKI::pickBestPhysicalDevice will select the best device available. But is no device meet the 
     * requirement, or 'letMeChooseTheDevice' is set to true, this function will be called.
     */
    typedef std::function<VkPhysicalDevice(const std::vector<VkPhysicalDevice>&)> PhysicalDeviceSelectorCallback;

    /**@brief Groups every related information about a window. */
	struct WindowContext
	{
        GLFWwindow*  window = nullptr;
        GLFWmonitor* monitor= nullptr;

        bool manualShouldClose = false; // Allow user to manually make windowShouldClose return true if set.
        bool forceWindowOpen   = true;  // If set to false, windowShouldClose won't ever return true.
        // Computed as : (glfwWindowShouldClose() || manualShouldClose) && forceWindowOpen;

        bool eventWindowResized=false; // Set to true to indicate that the window need to be resized.
	};

    struct SubmitInfo
    {
        std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>> waitSemaphoresAtStages;
        std::vector<VkCommandBuffer>                              commandBuffers;
        std::vector<VkSemaphore>                                  signalSemaphores;
    };

    struct Command
    {
        std::vector<VkCommandPool> pools;

        std::vector<VkCommandBuffer> PBuffers; // Primary   buffers.
        std::vector<VkCommandBuffer> SBuffers; // Secondary buffers.
    };

    struct CommandBufferInfo
    {
        uint32_t primaryCount   = 0;
        uint32_t secondaryCount = 0;
        uint32_t poolIndex      = 0;
    };

    struct CommandInfo
    {
        VkCommandPoolCreateFlags poolsFlags = 0;
        uint32_t                 poolsCount = 1;

        std::vector<CommandBufferInfo> commandBufferInfos;
    };

    /**@note QueueInfo::priorities size is limited to VKI_MAX_QUEUE_COUNT_PER_FAMILY (default 16) */
    struct QueueInfo 
    {
        uint32_t familyIndex = -1; // Filled by findQueuefamilyIndices.
        uint8_t  count       =  1; // 0=UB. /* TODO 0 == max.*/ // if count>VKI_MAX_QUEUE_COUNT_PER_FAMILY -> UB.
        float    priorities[VKI_MAX_QUEUE_COUNT_PER_FAMILY] = {0.0f};

        VkQueueFlags operations     = 0;
        bool         isPresentable  = false; // aka shareTheSwapchain. // TODO

        std::vector<CommandInfo> cmdPoolInfos;
    };

    struct QueueFamily
    {
        QueueInfo info;
        std::vector<VkQueue> queues;
        std::vector<Command> commands;
    };

    /**@param capabilities, At least minImageCount is created, if possible maxImageCount is created. */
    struct SwapChainInfo
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;

        VkImageUsageFlags           imageUsage              = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkSharingMode               sharingMode             = VK_SHARING_MODE_EXCLUSIVE;
        VkCompositeAlphaFlagBitsKHR currentCompositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        VkBool32                    enableClipping          = VK_TRUE; // Ignore pixels that are covered by another window.

        // SubresourceRange for the images views in the context (default is no Mip map and only color).
        VkImageSubresourceRange subresourceRange = { 
            /*aspectMask*/     VK_IMAGE_ASPECT_COLOR_BIT,
            /*baseMipLevel*/   0,
            /*levelCount*/     1,
            /*baseArrayLayer*/ 0,
            /*layerCount*/     1
        };

        // Optional if sharingMode == VK_SHARING_MODE_CONCURRENT;
        std::set<uint32_t> queueFamilyIndicesSharingTheSwapChain;
    };

    typedef std::bitset<7> SwapchainStatusFlags;
    enum SwapchainStatusFlagBits : uint8_t
    {
        SWAPCHAIN_STATUS_TIMEOUT_BIT                         = 0x01,
        SWAPCHAIN_STATUS_NOT_READY_BIT                       = 0x02,
        SWAPCHAIN_STATUS_SUBOPTIMAL_BIT                      = 0x04,
        SWAPCHAIN_STATUS_OUT_OF_DATE_BIT                     = 0x08,
        SWAPCHAIN_STATUS_SURFACE_LOST_BIT                    = 0x10,
        SWAPCHAIN_STATUS_DEVICE_LOST_BIT                     = 0x20,
        SWAPCHAIN_STATUS_FULL_SCREEN_EXCLUSIVE_MODE_LOST_BIT = 0x40,

        // Index to previous defined bits position.
        SWAPCHAIN_STATUS_TIMEOUT_INDEX                         = 0,
        SWAPCHAIN_STATUS_NOT_READY_INDEX                       = 1,
        SWAPCHAIN_STATUS_SUBOPTIMAL_INDEX                      = 2,
        SWAPCHAIN_STATUS_OUT_OF_DATE_INDEX                     = 3,
        SWAPCHAIN_STATUS_SURFACE_LOST_INDEX                    = 4,
        SWAPCHAIN_STATUS_DEVICE_LOST_INDEX                     = 5,
        SWAPCHAIN_STATUS_FULL_SCREEN_EXCLUSIVE_MODE_LOST_INDEX = 6
    };

    /**@brief Pack all required information to create a pipeline layout and a graphics pipeline. */
    struct PipelineInfo
    {
        VkPipelineCreateFlags flags = 0;
        uint32_t              subpassIndex; // Which subpass in the VkRenderPass this pipeline is use for.

        // Shader module are required to create the pipeline (when calling createPipeline), but not after.
        VkShaderModule vertexShader                 = VK_NULL_HANDLE, 
                       fragmentShader               = VK_NULL_HANDLE, 
                       //computeShader              = VK_NULL_HANDLE, // Optional.
                       geometryShader               = VK_NULL_HANDLE, // Optional.
                       tessellationControlShader    = VK_NULL_HANDLE, // Optional.
                       tessellationEvaluationShader = VK_NULL_HANDLE; // Optional.

        const char* vertexShaderEntryPoint                  = "main";
        const char* fragmentShaderEntryPoint                = "main";
        const char* geometryShaderEntryPoint                = "main";
        const char* tessellationControlShaderEntryPoint     = "main";
        const char* tessellationEvaluationShaderEntryPoint  = "main";

        std::optional<VkSpecializationInfo> vertexSpecializationInfo;
        std::optional<VkSpecializationInfo> fragmentSpecializationInfo;
        std::optional<VkSpecializationInfo> geometrySpecializationInfo;
        std::optional<VkSpecializationInfo> tessellationControlSpecializationInfo;
        std::optional<VkSpecializationInfo> tessellationEvaluationSpecializationInfo;

        std::vector<VkDynamicState> dynamicStates /*= {
                //VK_DYNAMIC_STATE_VIEWPORT,
                //VK_DYNAMIC_STATE_SCISSOR
            }*/;

        std::vector<VkVertexInputBindingDescription>   vertexInputBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;

        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool enablePrimitiveRestart  = false; // If enabled, index -1 will break *_STRIP primitives.

        // Warnings, a feature must be enabled to use multiple viewport.
        std::vector<VkViewport> viewports; // Can be empty if set as dynamic state.
        std::vector<VkRect2D>   scissors;  // Same.
                                           // TODO : ?? Is scissors's size must be eq to viewport.size() ??
        // Rasterization options:
        //physicalDeviceMinimalRequirement.features.depthClamp; // to enable depthClamp.
        bool rasterizerDiscardEnable  = false; // Disable rasterizer (disable writing fragments to a frame buffer).
        VkPolygonMode   polygonMode   = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode      = VK_CULL_MODE_BACK_BIT; // Cull back-facing fragments.
        VkFrontFace     frontFace     = VK_FRONT_FACE_CLOCKWISE;
        bool  depthBiasEnable         = false;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp          = 0.0f;
        float depthBiasSlopeFactor    = 0.0f;
        float lineWidth               = 1.0f; // A feature must be enabled for lineWith>1.0f;

        // multisampling is currently unavailable and hardwired to be disable.
        // Depth and stencil is currently unavailable and hardwired to be disable.

        // Color blending.
        // > Default config is (disabled) for ONE framebuffer to blend the color according to alpha.
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates = {{ // Within the same frambuffer.
            /*blend_enable = */         VK_FALSE,
            /*srcColorBlendFactor  = */ VK_BLEND_FACTOR_SRC_ALPHA,
            /*destColorBlendFactor = */ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            /*colorBlendOP = */         VK_BLEND_OP_ADD,
            /*srcAlphaBlendFactor  = */ VK_BLEND_FACTOR_ONE,
            /*destAlphaBlendFactor = */ VK_BLEND_FACTOR_ZERO,
            /*alphaBlendOP = */         VK_BLEND_OP_ADD,
            /*colorWriteMask = */       VK_COLOR_COMPONENT_R_BIT   | VK_COLOR_COMPONENT_G_BIT 
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        }};

        // > How colors are blend from frambuffer to another.
        VkBool32    colorBlendEnableLogicOp     = VK_FALSE;
        VkLogicOp   colorBlendLogicOp           = VK_LOGIC_OP_COPY;
        float       colorBlendBlendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        VkPipelineColorBlendStateCreateFlags colorBlendFlags = 0;

        // Pipeline layout.
        // Other value than zero require : VK_EXT_graphics_pipeline_library.
      #ifdef VK_EXT_graphics_pipeline_library
        VkPipelineLayoutCreateFlags      pipelineLayoutFlags = 0;
      #else
        const int32_t                    pipelineLayoutFlags = 0;
      #endif//VK_EXT_graphics_pipeline_library
        VkDescriptorSetLayoutCreateFlags pipelineLayoutDescriptorSetFlags = 0;

        std::vector<VkDescriptorSetLayout> pipelineDescriptorSetLayouts; // Use createPipelineDescriptorSetLayout.
        std::vector<VkPushConstantRange>   pipelinePushConstantRanges;

        // tessellator.
        uint32_t tessellatorPatchControlPoints = 1; // !=0 , if >1 tessellator will create more geometry.

        // Performance.
        VkPipeline basePipeline = VK_NULL_HANDLE; // Specify a pipeline to derivate from at creation time.
    };

    /**@brief Groups information about a subPass
     * @warning If resolveAttachmentReference isn't empty it MUST be the same size as colorAttachmentReference.
     */
    struct RenderSubPassInfo
    {
        VkSubpassDescriptionFlags          flags = 0;
        VkPipelineBindPoint                pipelineBindPoint               = VK_PIPELINE_BIND_POINT_GRAPHICS;
        std::vector<VkAttachmentReference> inputAttachmentReferences;
        std::vector<VkAttachmentReference> colorAttachmentReferences;
        std::vector<VkAttachmentReference> resolveAttachmentReferences;
        VkAttachmentReference              depthStencilAttachmentReference = {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED};
        std::vector<uint32_t>              preserveAttachmentReferences;
    };

    struct RenderPassInfo
    {
        VkRenderPassCreateFlags              flags = 0;
        std::vector<VkAttachmentDescription> attachmentDescs;
        std::vector<RenderSubPassInfo>       subPassInfos;
        std::vector<VkSubpassDependency>     subPassDependencies;
    };

    struct RenderPass_pipeline
    {
        PipelineInfo       info;
        VkPipelineLayout   layout;
        VkPipeline         pipeline;
    };

    struct RenderPass
    {
        RenderPassInfo                   renderPassInfo;
        VkRenderPass                     renderPass = VK_NULL_HANDLE;
        std::vector<RenderPass_pipeline> pipelines;
    };

    /*
    struct ComputePipelineContext // :D
    {
    };
    */

    struct BufferInfo
    {
        VkDeviceSize          size;
        VkDeviceSize          offset  = 0; // Offset from the memory origin.
        VkDeviceSize          alignment = -1; // Alignment of the memory region.
        VkBufferUsageFlags    usage   = 0;
        VkBufferCreateFlags   flags   = 0;
    #ifdef VK_VERSION_1_4
        VkBufferUsageFlags2   usage2    = 0;
        bool                  useUsage2 = false;
    #else
        const uint32_t        usage2    = 0;
        const bool            useUsage2 = false;
    #endif

        VkMemoryPropertyFlags memoryProperties=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        std::vector<uint32_t> queueFamilyIndicesSharingTheBuffer;
        bool                  exclusiveMode=true;// () ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    };

    struct Buffer
    {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;

        void*          mappedAddr = nullptr; // Only if info.memoryProperties contain HOST_VISIBLE.
        BufferInfo     info;
    };

    struct BufferMirror // Link a Buffer (device_local) and a 'mirror' (host_visible).
    {
        std::shared_ptr<Buffer> deviceBuffer,
                                hostBuffer; // Both buffer must be identical (except for memoryProperties).
    };

    /**@brief OPTIONAL struct that bundle all resources that your application could/would need.*/
    struct VulkanContext
    {
        VkInstance instance;

        VkPhysicalDevice physicalDevice  = VK_NULL_HANDLE;
        VkDevice         device          = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;

        std::vector<QueueFamily> queueFamilies;

        VkSurfaceKHR surface;
        SwapChainInfo swapChainInfo; // Infos of the swap chain.
                                     // format.size() is quaranty to be equal to 1, same for presentMode;
        VkSwapchainKHR              swapChain = VK_NULL_HANDLE;
        std::vector<VkImage>        swapchainImages;
        std::vector<VkImageView>    swapchainImageViews;
        std::vector<VkFramebuffer>  swapchainFramebuffers; // createVulkanContext won't initialize this !

        std::vector<VkSemaphore>    semaphores;
        std::vector<VkFence>        fences;

        // Below this point, createVulkanContext won't initialise more field.

        std::vector<RenderPass> renderPasses; // Fill the renderPasses.renderPassInfo and pipelines[x].info;

        std::vector<std::shared_ptr<Buffer>> buffers;

        // Those two lines below are only used by VKI, you should ignore them.
       #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        // Require VK_EXT_debug_utils
        VkDebugUtilsMessengerEXT debugCallbackHandle;
        bool isDebugCallbackHandleValid =false; // debugCallbackHandle can fail at creation,
                                                // So it must not be destroyed if it happend.
                                                // TODO Can't we just test for VK_NULL_HANDLE ?
       #endif
    };

    /**@brief Lists the GPU minimal requirements.
     */
    struct PhysicalDeviceMinimalRequirement
    { 
        VkPhysicalDeviceLimits      limits;
        VkPhysicalDeviceFeatures    features;
        std::vector<QueueInfo>      queueInfos; // queue info to be created (.index are ignored);
                                                // note : VKI doesn't support protected queue.

    #ifdef VK_VERSION_1_4
        // TODO add a VkDeviceQueueGlobalPriority to set systeme wide priority.
        // vulkan spec says that priorities above MEDIUM might require higher priviledge.
        // This will cause the VK_ERROR_NOT_PERMITED (check the queue creation process accordingly).
    #endif

        SwapChainInfo swapChainInfo; // minimal requirement for the swapchain.
                                     // note : it is possible (but untested) to set maxImageCount to 0.

        std::vector<const char*> instanceExtensions;
        std::vector<const char*> deviceExtensions;
    };

    /**@brief Save GPU preference when choosing a physical device. 
     * @param letMeChooseTheDevice force the call to PhysicalDeviceSelectorCallback.*/
    void registerGpuRequirement(const PhysicalDeviceMinimalRequirement&, bool letMeChooseTheDevice=false);
    /**@brief Save a function to call if the physical device automatic selection fail. */
    void registerPhysicalDeviceSelector(const PhysicalDeviceSelectorCallback);

    // Vulkan Functions :

    void init();      // Initialize glfw /!\  Must be the FIRST function to be called.
    void terminate(); // Terminate glfw / ! \ Must be the LAST  function to be called.

    /// WindowContext :
    WindowContext createWindowContext(unsigned width, 
                                      unsigned height, 
                                      std::string title,
                                      GLFWmonitor* monitor = nullptr // nullptr make call to glfwGetPrimaryMonitor;
                                     );
    void destroyWindowContext(WindowContext&);
    VkSurfaceKHR createWindowSurface(const VkInstance, const WindowContext&);
    VkExtent2D getWindowFramebufferSize(const WindowContext&); // bind glfwGetFrameBufferSize();
    bool windowShouldClose(const WindowContext&);

    /// VulkanContext :
    VkInstance createInstance();
    VkDebugUtilsMessengerEXT  setupInstanceDebugCallback();
    VulkanContext createVulkanContext(
            const                           VkInstance, 
            const                           WindowContext,
            const uint32_t                  semaphoreCount = 0,
            const uint32_t                  fenceCount     = 0,
            const bool                      fenceSignaled  = true,
            const VkPhysicalDevice          physicalDevice = VK_NULL_HANDLE // VK_NULL_HANDLE => auto select best device.
            );
    void destroyVulkanContext(VulkanContext&, bool waitDeviceIdle=true);

    std::vector<VkQueue> getQueueFromDevice(const VkDevice, const QueueInfo&); // Fetch queues from a device.

    uint32_t getInstanceApiVersion();

    /// Extensions :
    std::vector<VkExtensionProperties> listSupportedPhysicalDeviceExtensions(const VkPhysicalDevice, const char* pLayerName=nullptr);
    bool isPhysicalDeviceExtensionSupported(const VkPhysicalDevice, const std::vector<const char*> &extensionNames, const char* pLayerName=nullptr);

    std::vector<VkExtensionProperties> listInstanceExtensionProperties(const char* pLayerName=nullptr);
    bool isInstanceExtensionSupported(const std::vector<VkExtensionProperties>& extensions, const char* pLayerName=nullptr);
    bool isAllInstanceExtensionSupported(); // fetch extension from getRequiredExtension(...);

    /*constexpr*/ VkExtensionProperties createVkExtensionProperties(const char* name, uint32_t specVersion=0);

    bool isGlfwExtensionAvailable();
    void logAvailableExtension(const char* pLayerName=nullptr); // if pLayerName==nullptr -> log available extension for
                                                                // all available validation layer.

    std::vector<const char*> getRequiredExtensions();

    /// Validation layers :
    /*constexpr*/ VkLayerProperties createVkLayerProperties(const char* name, uint32_t specVersion=0, uint32_t implementationVersion=0);

#ifndef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    const bool isValidationLayersEnabled = false;

    const std::array<VkLayerProperties, 0> validationLayers; // implement listValidationLayersNames to remove those.
    const std::array<const char*, 0> validationLayersNames;
#else
    const bool isValidationLayersEnabled = true;

    #ifndef VKI_NB_VULKAN_VALIDATION_LAYERS
    #define VKI_NB_VULKAN_VALIDATION_LAYERS 1
    #endif

    const std::array<VkLayerProperties, VKI_NB_VULKAN_VALIDATION_LAYERS> validationLayers = {
        createVkLayerProperties(u8"VK_LAYER_KHRONOS_validation", 0),
        };

    const std::array<const char*, VKI_NB_VULKAN_VALIDATION_LAYERS> validationLayersNames = {
        u8"VK_LAYER_KHRONOS_validation",
        };
#endif

    bool isAllValidationLayersAvailable();
    std::vector<VkLayerProperties> listInstanceLayersProperties();

    void logAvailableValidationLayers();

    //const char** listValidationLayersNames(uint32_t* count); // TODO define this function
                                                               // To remove validiationLayersNames.

    /// Physical device :
    std::vector<VkPhysicalDevice> listInstancePhysicalDevices(const VkInstance);

    // Return a score for a certain PhysicalDevice based on its perf (higher is better).
    // This function use the GMP lib to compute the score.
    mpz_class ratePhysicalDevice(const VkPhysicalDevice, const bool logInfo=true) noexcept; 
    bool isPhysicalDeviceMatchMinimalRequirements(const VkPhysicalDevice device, const VkSurfaceKHR surface) noexcept;
    // This function use the GMP lib to compare scores.
    VkPhysicalDevice pickBestPhysicalDevice(const std::vector<VkPhysicalDevice>& devices, const VkSurfaceKHR, bool logInfo=true);

    /// Queue Family :
    std::vector<VkQueueFamilyProperties> listPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice, bool logInfo=true);
    std::vector<QueueInfo> getPhysicalQueueInfos(const VkPhysicalDevice, 
                                                 const VkSurfaceKHR      surface=VK_NULL_HANDLE, // surface is optional.
                                                 bool logInfo=true);
    // The next two function are helper function you can ignore them.
    constexpr   bool     isQueueFamilySuitable(const QueueInfo& physicalQueueFamily, const QueueInfo& requiredQueue);
  /*constexpr*/ unsigned getDistanceBetweenQueueOperations(const VkQueueFlags, const VkQueueFlags); // until C++20.
    /**@brief Assign a bunch of specified queue to a family available on the physical device.*/
    std::vector<QueueInfo> findQueueFamilyIndices(const VkPhysicalDevice, 
                                                  const std::vector<QueueInfo>&,
                                                  const VkSurfaceKHR surface=VK_NULL_HANDLE,// Offline rendering.
                                                  const bool         logInfo = true
                                                 );
    bool areRequiredFamilyQueueAvailable(const VkPhysicalDevice, const VkSurfaceKHR);
    VkDeviceQueueCreateInfo populateDeviceQueueCreateInfo(const QueueInfo& info);

    std::set<uint32_t> listQueueSharingTheSwapchain(const std::vector<QueueFamily>&) noexcept;
    std::string queueFlagBitsToString(VkQueueFlags flags) noexcept;

    void queueSubmit(const VkQueue, const std::vector<SubmitInfo>&, const VkFence fence=VK_NULL_HANDLE);
    /**@brief Present an image to the display engine. @note pErrorFlags and supportedErrorFlags are used the same way than in acquireNextImage*/
    void queuePresent(const VkQueue                     queue, 
                      const uint32_t                    imageIndices, 
                      const VkSwapchainKHR              swapchains,
                      const std::vector<VkSemaphore> &  semaphores,
                      SwapchainStatusFlags* const       pErrorFlags=nullptr,
                      const SwapchainStatusFlags        supportedErrorFlags = SwapchainStatusFlags(0)
                     );
    bool waitQueueBecomeIdle(const VkQueue) noexcept;// return true if the queue is empty.
    /* Not supported.
    // Present to multiple swapchains.
    void queuePresent(const VkQueue, 
                      const std::vector<std::pair<VkSwapchainKHR, uint32_t>> &swapchainImagesIndices, 
                      const std::vector<VkSemaphore>&, // Wait for semaphores.
                      SwapchainStatusFlags* const pErrorFlags=nullptr,
                      const SwapchainStatusFlags supportedErrorFlags = SwapchainStatusFlags(0)
                     );
    */

    /// Logical devices:

    VkDevice createLogicalDevice(const VkPhysicalDevice, const std::vector<QueueInfo>, const std::vector<const char*> &deviceExtensionsNames);
    bool waitDeviceBecomeIdle(const VkDevice) noexcept; // return true if device have no more operation in its queues.

    /// SwapChain
    SwapChainInfo getPhysicalDeviceSwapChainInfo(const VkPhysicalDevice, const VkSurfaceKHR, bool logInfo=true);
    bool isSwapChainInfoMatchMinimalRequirement(const SwapChainInfo&);
    /**@brief Clamp the currentExtent to the framebuffer of the windows.*/
    void updateSwapchainExtents(const VkPhysicalDevice, 
                                const VkSurfaceKHR,
                                const WindowContext&,
                                SwapChainInfo&
                               );
    /**@brief Remove unsupported format (from swapchainInfo.formats) and unsupported present mode (from swapchainInfo.presentModes).
     *        This allow createSwapchain to call safely .front() on those two fields.
     * @note This function doesn't clamp current extent to the minmax extent of the device.
     *        Call updateSwapChainExtents for that.
    */
    void discardUnsuportedFormatsAndPresentModes(SwapChainInfo&, const VkPhysicalDevice, const VkSurfaceKHR);
    VkSwapchainKHR createSwapChain(const VkDevice, 
                                   const SwapChainInfo&,
                                   const VkSurfaceKHR,
                                   VkSwapchainKHR oldSwapchain=VK_NULL_HANDLE
                                  );
    std::vector<VkImage> fetchImagesFromSwapChain(const VkDevice&, const VkSwapchainKHR&);
    std::vector<VkImageView> createImageViews(const VkDevice,
                                              const std::vector<VkImage>&, 
                                              const VkFormat,
                                              const VkImageSubresourceRange,
                                              const VkImageViewType viewType=VK_IMAGE_VIEW_TYPE_2D
                                             );
    void destroyImageView(const VkDevice, VkImageView&) noexcept;
    inline SwapChainInfo& clearNullMaxImageCount(SwapChainInfo& swapchainInfo);
    inline std::pair<uint32_t, uint32_t> getNonNullMaxImageCount(const SwapChainInfo&);
    /**@brief Compute the intersection of the device.minmaxImageCount and required.minmaxImageCount.
     * @return If the two image count ranges intersect.
     */
    inline bool intersectMinMaxImageCount(const SwapChainInfo& deviceSwapchainInfo,
                                          const SwapChainInfo& requiredSwapchainInfo,
                                          std::pair<uint32_t, uint32_t>* intersection
                                         );
    inline bool intersectMinMaxImageCount(const std::pair<uint32_t, uint32_t>& device_minmax_imageCount,
                                          const std::pair<uint32_t, uint32_t>& required_minmax_imageCount,
                                          std::pair<uint32_t, uint32_t>* intersection
                                         );

    /**@brief Get the image index in the swapchain to use for the next frame.
     * @return Index of the image in the swapchain (-1 if timeout).
     * @param pErrorFlags : A pointer to a statusFlags to handle/notify swapchain errors like out_of_date.
     *                      By specifying a valid pointer (and if the error/flags is set in supportedErrorFlags),
     *                      this function can be treated as noexcept.
     *                      By default, its nullptr which is equivalent to supportedErrorFlags(0).
     * @param supportedErrorFlags : A SwapchainStatusFlags to inform the function which error are handled and
     *                              which aren't (i.g. if device_lost is false : the func will throw an error when the
     *                                                 actual VK_ERROR_DEVICE_LOST will be encountered).
     *                              By default, no flags are supported.
     */
    uint32_t acquireNextImage(const VkDevice, 
                              const VkSwapchainKHR, 
                              const VkSemaphore, 
                              const VkFence, 
                              uint64_t timeout=UINT64_MAX,
                              SwapchainStatusFlags* pErrorFlags=nullptr,
                              const SwapchainStatusFlags supportedErrorFlags = SwapchainStatusFlags(0)
                             );

    /// Pipeline & RenderPass :

    /**@brief Create a full pipeline from scratch.
     * @note VKI doesn't support basePipelineIndex derivation but basePipelineHandle is available (untested).*/
    VkPipeline createGraphicsPipeline(const VkDevice, 
                              const VkPipelineLayout, 
                              const VkRenderPass, 
                              const PipelineInfo&
                             );
    //TODO Add a function std::vector<VkPipelines> createGraphicsPipelines(...); // and pack pipelines creation into a 
    //     single vkCreateGraphicsPipelines call. 
    VkPipelineLayout createPipelineLayout(const VkDevice, const PipelineInfo&);
    void destroyPipelineInfoShaderModules(const VkDevice, PipelineInfo&) noexcept;
    void destroyShaderModule(const VkDevice, std::shared_ptr<VkShaderModule>&) noexcept;
    void destroyShaderModule(const VkDevice, VkShaderModule&) noexcept;
    VkShaderModule createShaderModule(const VkDevice, const std::vector<char>& spirvCode);
    VkDescriptorSetLayout createPipelineDescriptorSetLayout(const VkDevice, 
                                                            std::vector<VkDescriptorSetLayoutBinding>,
                                                            VkDescriptorSetLayoutCreateFlags flags=0
                                                           );
    VkSubpassDescription createVkSubpassDescription(const RenderSubPassInfo&);
    VkRenderPass createRenderPass(const VkDevice, const RenderPassInfo&);
    void destroyRenderPass(const VkDevice, VkRenderPass&) noexcept;

    void destroyPipelineLayout(const VkDevice, VkPipelineLayout&) noexcept;
    void destroyPipeline(const VkDevice, VkPipeline&) noexcept;
    void destroyRenderPass(const VkDevice, RenderPass&) noexcept;

    /// Framebuffers.
    VkFramebuffer createFramebuffer(const VkDevice,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const VkRenderPass,
                                    const std::vector<VkImageView> &attachments,
                                    const VkFramebufferCreateFlags flags=0);
    void destroyFrameBuffer(const VkDevice, VkFramebuffer&) noexcept;

    /// Buffers.
    
    // Warnings : createBuffer won't allocate nor bind memory to the buffer : call allocateBuffers (and if needed bindBufferMemory).
    std::vector<Buffer> createBuffers(const VkDevice, const std::vector<BufferInfo>&);
    void destroyBuffer(const VkDevice, Buffer&, const bool free_memory=true) noexcept;
    void destroyBuffer(const VkDevice, VkBuffer&) noexcept;

    // TODO review this function since it's not compatible with allocateBuffers
    // |-> May update it to take a 'device' buffer and create a mirror to the host side.
    /**@brief Create two Buffers : a 'Buffer' (device_local) and its 'Mirror' (host_visible).
     * The two Buffers are identical except that one is created with DEVICE_LOCAL and the other one with HOST_VISIBLE.
     * @param write, add TRANSFER_SRC to host and TRANSFER_DET to device.
     * @param read , add TRANSFER_DST to host and TRANSFER_SRC to device.
     * @param deviceFamilyIndexAccessing if specified, REPLACE the list of family index that will access the device Buffer.
     * @param hostFamilyIndexAccessing   if specified, REPLACE the list of family index that will access the host Buffer.
     * @param removeCoherenceOnDevice will remove (if exist) the HOST_COHERENT_BIT on the device Buffer.
     * @param setupForHostToDeviceBond add VISIBLE to host and LOCAL to device (If false, it allow device to device mirror).
     * @note  Create the host buffer and then the device buffer (Good to know if you need to identify the buffer in the logs).
     */
    BufferMirror createBufferMirror(const VkDevice,
                                    const BufferInfo&, 
                                    const bool write=true, // to device.
                                    const bool read =true, // from device.
                                    const std::optional<std::vector<uint32_t>> hostFamilyIndexAccessing=std::nullopt,
                                    const std::optional<std::vector<uint32_t>> deviceFamilyIndexAccessing=std::nullopt,
                                    const bool setupForHostToDeviceBond = true,
                                    const bool removeCoherenceOnDevice  = true
                                    );

    //TODO create a new function : 
    //BufferMirror createBufferMirrorFromDeviceBuffer(const VkDevice,
    //                                                const Buffer&  deviceBuffer,
    //                                                const bool write = true,
    //                                                const bool read  = true,
    //                                                const std::optional<std::vector<uint32_t>>
    //                                                ...

    /**@brief Allocate memory to a buffer.
     * @return true on success.
     * @return false if no suitable heap is found or if the targetMemoryIndex doesn't support the buffer.
     * @throw  runtime_error if out of memory.
     * @warning Doesn't support multi-instance memory pages.
     * @note For better performance (by incresing cache-frendly data) take a look at allocateBuffers(...);
     */
    bool allocateBuffer(const VkDevice, 
                              Buffer&, 
                        const VkPhysicalDeviceMemoryProperties&, 
                        const uint32_t  targetMemoryIndex = -1,  // -1 => take first founded suitable memory index.
                        const bool      bindMemoryToBuffer=true, // call bindBufferMemory(...);
                        const bool      logInfo=true
                       );
    /**@brief Allocate a bunch of buffers to a single memory allocation.
     */
    bool allocateBuffers(const VkDevice,
                         std::vector<Buffer*>,
                         const VkPhysicalDeviceMemoryProperties&, 
                         const VkDeviceSize customAlignment= -1,  // -1 let VKI figure it out.
                         const uint32_t  targetMemoryIndex = -1,  // -1 => take first founded suitable memory index.
                         const bool      bindMemoryToBuffer=true, // call bindBufferMemory(...);
                         const bool      logInfo=true
                       );
    /**@brief Find a set of memory index that are suitable for the memory requirements and properties.
     */
    std::set<uint32_t> getMemoryIndexFromMemoryRequirement(const VkMemoryRequirements  ,// Size, alignment, ...
                                                           const VkMemoryPropertyFlags ,// DEVICE_LOCAL / HOST_VISIBLE / ...
                                                           const VkPhysicalDeviceMemoryProperties& // Device's memories.
                                                          );
    VkDeviceMemory allocateMemory(const VkDevice,
                                  const VkDeviceSize,
                                  const uint32_t memoryTypeIndex
                                 );
    void freeMemory(const VkDevice, VkDeviceMemory&) noexcept;

    void bindBufferMemory(const VkDevice, VkBuffer, VkDeviceMemory, const VkDeviceSize memoryOffset);
    //TODO
  //void bindBufferMemory(const VkDevice, const std::vector<const Buffer*>);

    size_t getGlobalMemoryAllocationCount() noexcept; // Return how many allocations simultaneously exist.
    void logMemoryHeapInfo(const VkPhysicalDeviceMemoryProperties&, const uint32_t heapIndex); // log infos about a heap.
    void logMemoryInfo(const VkPhysicalDeviceMemoryProperties&); // log infos about all heaps.
    void logMemoryRequirements(const VkMemoryRequirements, const void* addr);

    void* mapMemory(const VkDevice           device,
                    const VkDeviceMemory     memory,
                    const VkDeviceSize       offset=0,
                    const VkDeviceSize       size=VK_WHOLE_SIZE
                   );
    void mapBuffer(const VkDevice, Buffer&);
    void unmapBuffer(const VkDevice, Buffer&);
    // void vkUnmapMemory(VkDevice,VkDeviceMemory);

    /**@brief Write arbitrary data to a HOST_VISIBLE buffer (otherwise take a look at recordPushBuffer).
     * @note  writebuffer will map the buffer if not already mapped.
     * @warning If the buffer isn't HOST_COHERENT you should use flushBuffer function afterwards.
     * @except Raise std::logic_error if the buffer is not HOST_VISIBLE.
     */
    void writeBuffer(const VkDevice,
                           Buffer&,
                     const void*  data,
                     const size_t maxSize=VK_WHOLE_SIZE // WHOLE_SIZE => buffer's size.
                    );
    /**@brief Flush HOST_VISIBLE buffers.
     * @note This function isn't required is the buffer is HOST_COHERENT.
     */
    void flushBuffers(const VkDevice,
                      const std::vector<Buffer>&,
                      const bool flushRead=true,
                      const bool flushWrite=true
                     );

    // host to device.
    void recordPushBufferMirror(const BufferMirror&,
                                const VkCommandBuffer,
                                std::vector<VkBufferCopy> regions={{0,0,VK_WHOLE_SIZE}} // whole_size == buffer.host.size;
                               );
    // device to host.
    void recordPullBufferMirror(const BufferMirror&,
                                const VkCommandBuffer,
                                std::vector<VkBufferCopy> regions={{0,0,VK_WHOLE_SIZE}} // whole_size == buffer.device.size;
                               );

    /// Commands.
    void createCommandFromCommandInfo(const VkDevice, const uint32_t queueFamilyIndex, const CommandInfo&, Command&);

    VkCommandPool createCommandPool(const VkDevice, const VkCommandPoolCreateFlags, const uint32_t queueFamilyIndex);
    void destroyCommandPool(const VkDevice, VkCommandPool&) noexcept;
    /*// This function below is really bad bc it could re-create a pool with different flags that the commandInfo.
    void reCreateCommandPool(const VkDevice, 
                             const VkCommandPoolCreateFlags, 
                             const uint32_t queueFamilyIndex,
                             VkCommandPool&
                            );
    */
    std::vector<VkCommandBuffer> createNCommandBuffer(const VkDevice, 
                                                      const VkCommandPool, 
                                                      const uint32_t count, 
                                                      const bool isSecondary
                                                     );

    /**@brief Begin the record of a command buffer.
     * @warning This function has an UB if VkCommandBuffer is a SECONDARY buffer.*/
    void cmdBeginRecordCommandBuffer(VkCommandBuffer,
                                     const VkCommandBufferUsageFlags &flags=0
                                    );
    /**@brief Begin the record of a command buffer.*/
    void cmdBeginRecordCommandBuffer(VkCommandBuffer, 
                                     const VkCommandBufferInheritanceInfo&,
                                     const VkCommandBufferUsageFlags &flags=0
                                    );
    void cmdResetCommandBuffer(VkCommandBuffer, bool willBeReuse=true);
    void cmdEndRecordCommandBuffer(VkCommandBuffer);
    void cmdBeginRenderPass(VkCommandBuffer,
                            const VkRenderPass, 
                            const VkFramebuffer, 
                            const VkRect2D renderArea, 
                            const std::vector<VkClearValue>&,
                            const VkSubpassContents
                           );
    void cmdBindPipeline(      VkCommandBuffer, 
                         const VkPipeline, 
                         const VkPipelineBindPoint bindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS
                        );
    //void cmdBindVertexBuffers(VkCommandBuffer, const std::vector<VkBuffer>&); // just use vkCmdBindVertexBuffer.
    void cmdDraw(VkCommandBuffer, 
                 uint32_t vertexCount, 
                 uint32_t instanceCount, 
                 uint32_t firstVertexIndex, 
                 uint32_t firstInstanceIndex
                );
    void cmdEndRenderPass(VkCommandBuffer cmdBuffer);

    /// Synchronisation :

    VkSemaphore createSemaphore (const VkDevice);
    VkFence     createFence     (const VkDevice, bool startSignaled=false);
    void        destroySemaphore(const VkDevice, VkSemaphore&) noexcept;
    void        destroyFence    (const VkDevice, VkFence&)     noexcept;

    /**@brief Return if the fence is signaled or not (throw runtime_error on device lost).*/
    bool isFenceSignaled(const VkDevice, const VkFence);

    /**@brief Wait for All|One fence(s) to be signaled to continue execution.
     * @return bool return false if the timeout reach zero, true if a/all fence(s) have been signaled.
     */
    bool waitFences(const VkDevice, const std::vector<VkFence>&, bool waitAll=true, uint64_t timeout=UINT64_MAX, bool logInfo=false);

    /**@brief Wait for one fence to be signaled to continue execution.
     * @return bool return false if the timeout reach zero, true if the fence have been signaled.
     */
    bool waitFence (const VkDevice, const VkFence&, uint64_t timeout=UINT64_MAX, bool logInfo=false);

    void resetFence(const VkDevice, const std::vector<VkFence>&);
    void resetFence(const VkDevice, const VkFence);

    /// Others :
    constexpr VkBool32 boolToVkBool32(const bool b) // TODO create a custom type cast to use static_cast<>. 
    { return (b) ? VK_TRUE : VK_FALSE;}
  /*constexpr*/ bool operator==(const QueueInfo&,const QueueInfo&);

    /*@brief Fill a PhysicalDeviceMinimalRequirement struct with 0 and VK_FALSE so you don't have to.*/
    // This function is 203 line of "xxx.xxx = 0;" so constexpr have been removed.
    /*constexpr*/ void resetPhysicalDeviceMinimalRequirement(PhysicalDeviceMinimalRequirement&);

}

#endif//VKINTERFACE_HEADER 

