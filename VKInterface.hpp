#ifndef VKINTERFACE_HEADER 
#define VKINTERFACE_HEADER 


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/*
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
*/

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
#include <set>
#include <optional>
#include <utility> // <3 std::pair;
#include <bitset>

#ifndef VKI_MAX_QUEUE_COUNT_PER_FAMILY 
#define VKI_MAX_QUEUE_COUNT_PER_FAMILY 16 /*uint8_t (aka 0-255 otherwise UB)*/
#endif//VKI_MAX_QUEUE_COUNT_PER_FAMILY

#ifndef VKI_IMAGE_COUNT_IF_UNLIMITED
#define VKI_IMAGE_COUNT_IF_UNLIMITED 64 /*uint32_t how many images to create in the swapchain if unlimited (max=0).*/
#endif//VKI_IMAGE_COUNT_IF_UNLIMITED 

/**@brief VulKanInterface : All you need to draw a quad to the screen.
 *        *Info structure are intented to be fill by the user (unless specified).
 * @note VKI isn't fully thread-safe yet.
 * @note VKI use glfw to be cross platform but can be changed if needed (the required change have been keept at a minimum).
 * @note The registerLogs* callback should be called before any other function (even init).
 * @warning VKI doesn't perform any test on user provided value (almost).
 *          i.e. if you set minImageCount=0 (in the swapchainInfo minimal requirement) no warnings will be displayed 
 *          and the behavior is UB !
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
        GLFWwindow* window;

        bool manualShouldClose      = false; // Allow user to manually make windowShouldClose return true if set.
        bool forceWindowNotClose    = true;  // If set to false, windowShouldClose won't EVER return true.
        // Computed as : (glfwWindowShouldClose() || manualShouldClose) && forceWindowNotClose;

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
        VkCommandPoolCreateFlags poolsFlags = 0; // Flags from which the commandPools.
        uint32_t                 poolsCount = 1;

        std::vector<CommandBufferInfo> commandBufferInfos;
    };

    /**@note QueueInfo::priorities size is limited to VKI_MAX_QUEUE_COUNT_PER_FAMILY (default 16) */
    struct QueueInfo 
    {
        uint32_t familyIndex; // Filled by findQueuefamilyIndicies.
        uint8_t count  = 1;   // 0=UB.
        float priorities[VKI_MAX_QUEUE_COUNT_PER_FAMILY] = {0.0f};

        VkQueueFlags operations = 0;
        bool isPresentable  = false; // Does the queue must be presentable.

        std::vector<CommandInfo> cmdPoolInfos;
    };

    struct QueueFamily
    {
        QueueInfo info;
        std::vector<VkQueue> queues; // (size=info.count);
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
        uint32_t              subpassIndex; // Which subpass in the VkRenderPass the pipeline is use for.

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
        bool enablePrimitiveRestart = true; // If enabled, index -1 will break *_STRIP primitives.

        // Warnings, a feature must be enabled to use multiple viewport.
        std::vector<VkViewport> viewports;
        std::vector<VkRect2D>   scissors; // TODO : TOFIND : is size must be eq to viewport.size() ?

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
        VkPipelineColorBlendStateCreateFlags colorBlendFlags = 0; // Too specific for this small lib (ignore it).

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

    struct GraphicsContext_pipeline
    {
        PipelineInfo       info;
        VkPipelineLayout   layout;
        VkPipeline         pipeline;
    };

    struct GraphicsContext // Should it be renamed to RenderPass (RenderPass::renderPass doesn't sound good).
    {
        RenderPassInfo                          renderPassInfo;
        VkRenderPass                            renderPass = VK_NULL_HANDLE;
        std::vector<GraphicsContext_pipeline>   pipelines;
    };

    /**@brief OPTIONAL struct that bundle all resources that your application could/would need.*/
    struct VulkanContext
    {
        VkInstance instance;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device                 = VK_NULL_HANDLE;

        std::vector<QueueFamily> queueFamilies;

        VkSurfaceKHR surface;
        SwapChainInfo swapChainInfo; // Infos of the swap chain.
                                     // format.size() is quaranty to be equal to 1, same for presentMode;
        VkSwapchainKHR              swapChain = VK_NULL_HANDLE;
        std::vector<VkImage>        swapchainImages;
        std::vector<VkImageView>    swapchainImageViews;
        std::vector<VkFramebuffer>  swapchainFramebuffers; // createVulkanContext won't initialize this,
                                                           // unless graphicsContexts isn't empty (TODO).

        // TODO create a function createGraphicsContext to let createVulkanContext call it.
        std::vector<GraphicsContext> graphicsContexts; // Fill the GraphicsContext.renderPassInfo and pipelines[x].info;

        std::vector<VkSemaphore> semaphores;
        std::vector<VkFence>     fences;

        // Those two lines below are only used by VKI, you should ignore them.
       #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        // Require VK_EXT_debug_utils
        VkDebugUtilsMessengerEXT debugCallbackHandle;
        bool isDebugCallbackHandleValid =false; // debugCallbackHandle can fail at creation,
                                                // So it must not be destroyed if it happend.
       #endif
    };

    /**@brief Lists the GPU minimal requirement.
     */
    struct PhysicalDeviceMinimalRequirement
    { 
        VkPhysicalDeviceLimits      limits;
        VkPhysicalDeviceFeatures    features;
        std::vector<QueueInfo>      queueInfos; // queue info to be created (.index are ignored);
                                                // note : VKI doesn't support protected queue.
        SwapChainInfo swapChainInfo; // minimal requirement for the swapchain.
                                     // note : it is possible (but untested) to set maxImageCount to 0.

        std::vector<const char*> instanceExtensions;
        std::vector<const char*> deviceExtensions;
    };

    /**@brief Save GPU preference when choosing physical device. 
     * @param letMeChooseTheDevice force the call to PhysicalDeviceSelectorCallback.*/
    void registerGpuRequirement(const PhysicalDeviceMinimalRequirement&, bool letMeChooseTheDevice=false);
    /**@brief Save a function to call if the physical device automatic selection fail. */
    void registerPhysicalDeviceSelector(const PhysicalDeviceSelectorCallback);

    // Vulkan Functions :

    void init();      // Initialize glfw /!\  Must be the FIRST function to be called.
    void terminate(); // Terminate glfw / ! \ Must be the LAST  function to be called.

    /// WindowContext :
    WindowContext createWindowContext(unsigned width, unsigned height, std::string title);
    void destroyWindowContext(WindowContext&);
    VkSurfaceKHR createWindowSurface(const VkInstance, const WindowContext&);
    VkExtent2D getWindowFramebufferSize(const WindowContext&); // bind glfwGetFrameBufferSize();
    bool windowShouldClose(const WindowContext&);

    /// VulkanContext :
    VkInstance createInstance();
    VkDebugUtilsMessengerEXT  setupInstanceDebugCallback();
    VulkanContext createVulkanContext(
            const VkInstance, 
            const WindowContext,
            const uint32_t semaphoreCount = 0,
            const uint32_t fenceCount = 0,
            const VkPhysicalDevice physicalDevice = VK_NULL_HANDLE // VK_NULL_HANDLE => auto best device.
            );
    void destroyVulkanContext(VulkanContext&, bool waitDeviceIdle=true);

    //void setupVkQueues(VulkanContext&);// Deprecated !
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

    #define VKI_NB_VULKAN_VALIDATION_LAYERS 1

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

    /// Family queues:
    std::vector<VkQueueFamilyProperties> listPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice, bool logInfo=true);
    std::vector<QueueInfo> findQueueFamilyIndices(const VkPhysicalDevice, const VkSurfaceKHR, const std::vector<QueueInfo>);
    bool areRequiredFamilyQueueAvailable(const VkPhysicalDevice, const VkSurfaceKHR) noexcept;
    VkDeviceQueueCreateInfo populateDeviceQueueCreateInfo(const uint32_t index, const float priorities[VKI_MAX_QUEUE_COUNT_PER_FAMILY], const uint8_t count=1);

    std::string VkQueueFlagBitsToString(VkQueueFlags flags);

    void queueSubmit(const VkQueue, const std::vector<SubmitInfo>&, const VkFence fence=VK_NULL_HANDLE);
    /**@brief Present an image to the display engine. @note pErrorFlags and supportedErrorFlags are used the same way than in acquireNextImage*/
    void queuePresent(const VkQueue                     queue, 
                      const uint32_t                    imageIndicies, 
                      const VkSwapchainKHR              swapchains,
                      const std::vector<VkSemaphore> &  semaphores,
                      SwapchainStatusFlags* const       pErrorFlags=nullptr,
                      const SwapchainStatusFlags        supportedErrorFlags = SwapchainStatusFlags(0)
                     );
    /* Support EOL.
    // Present to multiple swapchains.
    void queuePresent(const VkQueue, 
                      const std::vector<std::pair<VkSwapchainKHR, uint32_t>> &swapchainImagesIndicies, 
                      const std::vector<VkSemaphore>&, // Wait for semaphores.
                      SwapchainStatusFlags* const pErrorFlags=nullptr,
                      const SwapchainStatusFlags supportedErrorFlags = SwapchainStatusFlags(0)
                     );
    */

    /// Logical devices:

    VkDevice createLogicalDevice(const VkPhysicalDevice, const std::vector<QueueInfo>, const std::vector<const char*> &deviceExtensionsNames);

    /// SwapChain
    SwapChainInfo getPhysicalDeviceSwapChainInfo(const VkPhysicalDevice, const VkSurfaceKHR, bool logInfo=true);
    bool isSwapChainInfoMatchMinimalRequirement(const SwapChainInfo&);
    /**@brief Clamp the currentExtent to the framebuffer of the windows.*/
    void updateSwapchainExtents(const VkPhysicalDevice, 
                                const VkSurfaceKHR,
                                const WindowContext&,
                                SwapChainInfo&
                               );
    /**@brief Adapt the swapchainInfo to the device (i.e. format/presentMode)*/
    void adaptSwapChainInfoToDevice(SwapChainInfo&, const VkPhysicalDevice, const VkSurfaceKHR);
    //VkSwapchainKHR createSwapChain(const VulkanContext& context, VkSwapchainKHR oldSwapchain=VK_NULL_HANDLE);
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
    //TODO Add a function createGraphicsContext(GraphicsContext&);
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
    void destroyGraphicsContext(const VkDevice, GraphicsContext&) noexcept;

    // Framebuffers.
    VkFramebuffer createFramebuffer(const VkDevice,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const VkRenderPass,
                                    const std::vector<VkImageView> &attachments,
                                    const VkFramebufferCreateFlags flags=0);
    void destroyFrameBuffer(const VkDevice, VkFramebuffer&) noexcept;

    // Commands.
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

    // Command calls and draw calls.
    /**@brief Begin the record of a command buffer.
     * @warning This function has a UB if VkCommandBuffer is a SECONDARY buffer.*/
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
    void cmdBindPipeline(VkCommandBuffer, const VkPipeline, const VkPipelineBindPoint bindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS);
    void cmdDraw(VkCommandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertexIndex, uint32_t firstInstanceIndex);
    void cmdEndRenderPass(VkCommandBuffer cmdBuffer);

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
    bool waitDeviceBecomeIdle(const VkDevice); // i.e. wait until no more operation/commands are running on the device.
    constexpr VkBool32 boolToVkBool32(const bool b) // TODO create a custom type converter to use static_cast<>. 
    { return (b) ? VK_TRUE : VK_FALSE;}

    /*@brief Fill a PhysicalDeviceMinimalRequirement struct with 0 and VK_FALSE so you don't have to.*/
    /*constexpr*/ void getNullPhysicalDeviceMinimalRequirement(PhysicalDeviceMinimalRequirement&);
    // This function is 203 line of "xxx.xxx = 0;" so constexpr have been removed.

}

#endif//VKINTERFACE_HEADER 

