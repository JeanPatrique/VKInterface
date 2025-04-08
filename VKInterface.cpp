#include "VKInterface.hpp"

#include <stdexcept>
#include <iostream>  // Used for default log callback.
#include <mutex>     // See registerLogs/*Specifier*/Callback
#include <cstring>   // std::strcmp
#include <map>       // See pickBestPhysicalDevice(...)
#include <limits>    // See updateSwapchainExtent;
#include <algorithm> // std::clamp/std::min/std::max
#include <list>

// Quick note : if you see rqd* it mean ReQuireD*

namespace VKI
{
    // Callbacks and global variables:

    // TODO : replace every 'lastApiError = ', by 'VkResult apiError = '.
    std::atomic<VkResult> lastApiError; 
    // This is an artefact for a previous design, and it need to be removed (potential race condition).
    // Any other function running in another thread will override this.

    std::atomic<size_t> vkiGlobalMemoryAllocationCount = 0;

    std::function<void(const char*)> logVerboseCB    = []([[maybe_unused]]const char* msg){};
    std::function<void(const char*)> logInfoCB       = logVerboseCB;
    std::function<void(const char*)> logWarningCB    = logVerboseCB;
    std::function<void(const char*)> logErrorCB		 = logVerboseCB;
    std::function<void(const char*)> logFatalErrorCB = [](const char* msg){ std::cerr<<msg<<std::endl;};

    std::function<void(const char*)> logValidationLayerVerboseCB = logInfoCB;
    std::function<void(const char*)> logValidationLayerInfoCB    = logInfoCB;
    std::function<void(const char*)> logValidationLayerWarningCB = logInfoCB;
    std::function<void(const char*)> logValidationLayerErrorCB   = [](const char* msg) { std::cerr<<"Validation Layers : "<<msg<<std::endl; };

    PhysicalDeviceMinimalRequirement physicalDeviceMinimalRequirement;

    // Default make the program stop.
    PhysicalDeviceSelectorCallback physicalDeviceSelectorCallback = 
        [](const std::vector<VkPhysicalDevice>&) -> VkPhysicalDevice
        { return VK_NULL_HANDLE; };

    bool disableGpuAutoSelectAndCallSelectPhysicalDeviceCallback = false; // I've learn some lessons from vulkan.

    // register functions :

    void registerLogsVerboseCallback(std::function<void(const char*)> logVerboseCb)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);
        logVerboseCB = logVerboseCb;
        //logVerboseCB("registerLogsVerboseCallback called to this new logger.");
    }

    void registerLogsInfoCallback(std::function<void(const char*)> logInfoCb)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);
        logInfoCB = logInfoCb;
        //logInfoCB("registerLogsInfoCallback called to this new logger.");
    }

    void registerLogsWarningCallback(std::function<void(const char*)> logWarnCb)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);
        logWarningCB = logWarnCb;
        //logWarningCB("no warning -> registerLogsWarningCallback called to this new logger.");
    }

    void registerLogsErrorCallback(std::function<void(const char*)> logErrCb)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);
        logErrorCB = logErrCb;
        //logErrorCB("no error happended -> registerLogsErrorCallback called to this new logger.");
    }

    void registerLogsFatalErrorCallback(std::function<void(const char*)> logFECb)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);
        logFatalErrorCB = logFECb;
        //logFatalErrorCB("No Fatal Error happended -> registerLogsFatalErrorCallback called to this new logger.");
    }

    void registerLogsValidationLayerVerboseCallback(std::function<void(const char*)> logVCb)
	{
		static std::mutex myMut;
		std::lock_guard<std::mutex> guard(myMut);
        logValidationLayerVerboseCB = logVCb;
        //logValidationLayerVerboseCB("registerLogsValidationLayerVerboseCallback called to this logger.");
    }

    void registerLogsValidationLayerInfoCallback(std::function<void(const char*)> logICb)
	{
		static std::mutex myMut;
		std::lock_guard<std::mutex> guard(myMut);
		logValidationLayerInfoCB = logICb;
		//logValidationLayerInfoCB("registerLogsValidationLayerInfoCallback called to this logger.");
    }

    void registerLogsValidationLayerWarningCallback(std::function<void(const char*)> logWCb)
	{
		static std::mutex myMut;
		std::lock_guard<std::mutex> guard(myMut);
		logValidationLayerWarningCB = logWCb;
		//logValidationLayerWarningCB("No warning -> registerLogsValidationLayerWarningCallback called to this logger.");
    }

    void registerLogsValidationLayerErrorCallback(std::function<void(const char*)> logECb)
	{
		static std::mutex myMut;
		std::lock_guard<std::mutex> guard(myMut);
		logValidationLayerErrorCB = logECb;
		//logValidationLayerErrorCB("No Error -> registerLogsValidationLayerErrorCallback called to this logger.");
    }

    void registerGpuRequirement(const PhysicalDeviceMinimalRequirement& minRequirement, bool letMeChooseTheDevice)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);

        physicalDeviceMinimalRequirement = minRequirement;

        disableGpuAutoSelectAndCallSelectPhysicalDeviceCallback = letMeChooseTheDevice;
    }

    void registerPhysicalDeviceSelector(std::function<uint32_t(const std::vector<VkPhysicalDevice>)>)
    {
        static std::mutex myMut;
        std::lock_guard<std::mutex> guard(myMut);

        std::function<uint32_t(std::vector<VkPhysicalDevice>)> selectPhysicalDeviceCallback;
    }

    /**@brief The callback used to retrive error from the validation layer.
     *
     */
    VKAPI_ATTR VkBool32 VKAPI_CALL validationLayerDebugCallBack(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            [[maybe_unused]] void* pUserData
            )
    {
        std::stringstream ss;

        switch (type)
        {
            case (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT):
                {
                    ss<<"General : "; // Aka no specific type.
                    break;
                }
            case (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT):
                {
                    ss<<"Validation : "; // Called during check for invalid use of vulkan api.
                    break;
                }
            case (VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT):
                {
                    ss<<"Performance : "; // Non optimal use of vulkan.
                    break;
                }
            /* Provided by VK_EXT_device_address_binding_report
            case (VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT):
                {
                    ss<<"ERROR : ";
                    break;
                }*/
            default:
                {
                    ss<<"type = UNKOWN : "; // Uncomment the previous case if the extension is used.
                    break;
                }
        }

        ss<<pCallbackData->pMessage;
        // Note -> if the logging system use deferred write method 
        // then the pMessage must be duplicated since it's only valid
        // during this callback scope.

        switch (severity)
        {
            case (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT):
                {
                    logValidationLayerVerboseCB(ss.str().c_str());
                    break;
                }
            case (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT):
                {
                    logValidationLayerInfoCB(ss.str().c_str());
                    break;
                }
            case (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT):
                {
                    logValidationLayerWarningCB(ss.str().c_str());
                    break;
                }
            case (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT):
                {
                    //logErrorCB(ss.str().c_str());
                    logValidationLayerErrorCB(ss.str().c_str()); // Error may indicate crashes.
                    break;
                }
            default:
                {
                    logErrorCB(ss.str().c_str());
                    break;
                }
        }

        return VK_FALSE; // you can ingore the return value.
    }

    // Initialize the interface with vulkan.
    void init()
    {
        static bool initialized(false);

        if (not initialized)
        {
            initialized=true;

            int error = glfwInit();
            if (error != GLFW_TRUE)
            {
                logFatalErrorCB("init -> glfwInit returned false.");
            }
            if (glfwVulkanSupported() != GL_TRUE)
            {
                logFatalErrorCB("init -> glfwVulkanSupported");
            }

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Disable OpenGL api.

            lastApiError = VkResult::VK_SUCCESS;
            logInfoCB("VKI::init initialized successfully.");


        }
        else
        {
            logWarningCB("VKI::init have been called more than once (that may be normal if a object must be sure that VKI::init have been called).");
        }
    }

    // Terminate the VKInterface.
    void terminate()
    {
        static bool terminated(false);

        if (not terminated)
        {
            glfwTerminate();
            logInfoCB("VKI::terminate have been called.");
        }
        else
        {
            logErrorCB("VKI::terminate have been called more than once.");
            //throw std::logic_error("Fatal error : VKInterface::terminate have been called more than one.");
        }
        terminated=true;

        auto nullCB = []([[maybe_unused]]const char* msg){};
        registerLogsInfoCallback(       nullCB);
        registerLogsWarningCallback(    nullCB);
        registerLogsErrorCallback(      nullCB);
        registerLogsFatalErrorCallback( nullCB);
    }

    WindowContext createWindowContext(unsigned width, 
                                      unsigned height, 
                                      std::string title,
                                      GLFWmonitor* monitor
                                     )
    {
        WindowContext wc{};
        wc.window              = glfwCreateWindow(width, height, title.c_str(), monitor, nullptr);

        logInfoCB("A new Window has been created.");

        return wc;
    }

    void destroyWindowContext(WindowContext& wc)
    {
        if (wc.window == nullptr)
        {
            logWarningCB("destroyWindowContext has been called on an nullptr window.");
            return;
        }

        glfwDestroyWindow(wc.window); // Close the window.
        wc.window = nullptr;
        logInfoCB("A window context has been destroyed.");
    }

    VkSurfaceKHR createWindowSurface(const VkInstance instance, const WindowContext& windowContext)
    {
        VkSurfaceKHR surface;
        VkResult apiError = glfwCreateWindowSurface(instance, windowContext.window, nullptr, &surface);

        if (apiError == VkResult::VK_SUCCESS)
        {
            logInfoCB("Successfully created a window surface.");
        }
        else if ( apiError == GLFW_PLATFORM_ERROR)
        {
            logFatalErrorCB("createWindowSurface failed to create a surface : GLFW_PLATFORM_ERROR have been returned.");
            throw std::runtime_error("VKI::createWindowSurface failed to create a surface : GLFW_PLATFORM_ERROR have been returned.");
        }
        else if ( apiError == GLFW_INVALID_VALUE)
        {
            logFatalErrorCB("createWindowSurface failed to create a surface : GLFW_INVALID_VALUE have been returned.");
            throw std::invalid_argument("VKI::createWindowSurface failed to create a surface : GLFW_INVALID_VALUE have been returned.");
        }
        else
        {
            logFatalErrorCB("createWindowSurface an unkown error happened during the surface creation.");
            throw std::runtime_error("VKI::createWindowSurface an unkown error happened during the surface creation.");
        }
        
        return surface;
    }

    // bind glfwGetFrameBufferSize();
    VkExtent2D getWindowFramebufferSize(const WindowContext& wc) 
    {
        VkExtent2D size;

        int width, height;
        glfwGetFramebufferSize(wc.window, &width, &height);

        size.height = height;
        size.width  = width;

        return size;
    }

    /**@brief Return true if the window should be close.*/
    bool windowShouldClose(const WindowContext& context)
    {
        const bool glfwClose = glfwWindowShouldClose(context.window);

        return (glfwClose || context.manualShouldClose) && context.forceWindowOpen;
    }

    VkInstance createInstance()
    {
        if (not isAllValidationLayersAvailable())
        {
            logFatalErrorCB("createInstance Post conditions failed : some validiation layers are missing.");
            throw std::runtime_error("VKI::createInstance -> Post conditions failed : some validiation layers are missing.");
        }
        else if (not isAllInstanceExtensionSupported())
        {
            logFatalErrorCB("createInstance Post conditions failed : some extensions are missing.");
            throw std::runtime_error("VKI::createInstance -> Post conditions failed : some extensions are missing.");
        }
        else
        {
            logInfoCB("createInstance successfully checked that every required extension and every validation layer are available.");
        }

        // List application info :
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = u8"Cellular Automata";
        appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.pEngineName = u8"No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.pNext = nullptr;

        // List instance creation info :
        VkInstanceCreateInfo createInstanceInfo{};
        createInstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInstanceInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> extensions = VKI::getRequiredExtensions();
        createInstanceInfo.ppEnabledExtensionNames = extensions.data();
        createInstanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

        //createInstanceInfo.ppEnabledLayerNames = VKI::listValidationLayersNames(&createInfo.enabledExtensionCount);
        createInstanceInfo.ppEnabledLayerNames = VKI::validationLayersNames.data();
        createInstanceInfo.enabledLayerCount = static_cast<uint32_t>(VKI::validationLayersNames.size());

        VkDebugUtilsMessengerCreateInfoEXT createDbgCbInfo{};
        if (isValidationLayersEnabled )
        {
            // Create a simple debug callback createinfo struct.
            createDbgCbInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createDbgCbInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    | 
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createDbgCbInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT   | 
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createDbgCbInfo.pfnUserCallback = validationLayerDebugCallBack;
            createDbgCbInfo.pUserData = nullptr;
            createDbgCbInfo.pNext=nullptr;

            createInstanceInfo.pNext = &createDbgCbInfo;
        }
        else
            createInstanceInfo.pNext = nullptr;


        VkInstance instance;
        lastApiError = vkCreateInstance(&createInstanceInfo, nullptr, &instance);
        
        switch (lastApiError)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("Successfully created a new vulkan instance.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_OUT_OF_HOST_MEMORY.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_OUT_OF_HOST_MEMORY.");
            }
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_OUT_OF_DEVICE_MEMORY.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_OUT_OF_DEVICE_MEMORY.");
            }
            case (VkResult::VK_ERROR_INITIALIZATION_FAILED):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_INITIALIZATION_FAILED.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_INITIALIZATION_FAILED.");
            }
            case (VkResult::VK_ERROR_LAYER_NOT_PRESENT):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
            }
            case (VkResult::VK_ERROR_EXTENSION_NOT_PRESENT):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
            }
            case (VkResult::VK_ERROR_INCOMPATIBLE_DRIVER):
            {
                logFatalErrorCB("createInstance(...) -> failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance : VK_ERROR_LAYER_NOT_PRESENT.");
            }
            default:
            {
                logFatalErrorCB("createInstance(...) -> vkcreateInstance return an unkown error value.");
                throw std::runtime_error("VKI::createInstance failed to create a new instance -> unkown error value.");
            }
        }

        return instance;
    }

    // See the bottom of this file to see the manually loaded funtions.
    VkResult createInstanceDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
            const VkAllocationCallbacks* pAllocator, 
            VkDebugUtilsMessengerEXT* pDebugMessenger
            );

    VkDebugUtilsMessengerEXT  setupInstanceDebugCallback(VkInstance instance)
    {
        VkDebugUtilsMessengerEXT debugMessenger;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        //createInfo.messageSeverity = -1; // Take all
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    | 
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        //createInfo.messageType = -1; // Take all.
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    | 
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        // Provided by VK_EXT_device_address_binding_report :
        //createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

        createInfo.pfnUserCallback = validationLayerDebugCallBack;
        createInfo.pUserData  = nullptr;
        createInfo.pNext = nullptr;

        lastApiError = createInstanceDebugUtilsMessengerEXT(
                            instance,
                            &createInfo,
                            nullptr,
                            &debugMessenger
                            );

        if (lastApiError == VkResult::VK_SUCCESS)
        {
            logInfoCB("setupInstanceDebugCallback successfully setup the debug callback for the validation layers.");
        }
        else if (lastApiError == VkResult::VK_ERROR_EXTENSION_NOT_PRESENT)
        {
            logErrorCB("setupInstanceDebugCallback failed to manually load the vkCreateDebugUtilsDebugUtilsMessengerEXT.");
            throw std::runtime_error(""); // will get silently catch so no .what is needed. 
        }
        else
        {
            logErrorCB("setupInstanceDebugCallback failed to setup the debug callback for unkown reason.");
            throw std::runtime_error(""); // will get silently catch so no .what is needed. 
        }

        return debugMessenger;
    }

    VulkanContext createVulkanContext(const VkInstance        instance, 
                                      const WindowContext     windowContext,
                                      const uint32_t          semaphoreCount,
                                      const uint32_t          fenceCount,
                                      const bool              fenceSignaled,
                                      const VkPhysicalDevice  physicalDevice
                                     )
    {
        logVerboseCB("Creating a new VulkanContex.");
        VulkanContext context;

        context.instance = instance;

       #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        if (isValidationLayersEnabled)
        {
            try
            {
                context.debugCallbackHandle = setupInstanceDebugCallback(context.instance);
                context.isDebugCallbackHandleValid = true;
            }
            catch (std::runtime_error &err)
            {
                logErrorCB("setupInstanceDebugCallback throw a std::runtime_error -> the debugCallback will be ignored.");
                context.isDebugCallbackHandleValid = false;
            }
        }
       #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

        context.surface = createWindowSurface(context.instance, windowContext);

        if (physicalDevice == VK_NULL_HANDLE)
            context.physicalDevice = pickBestPhysicalDevice(listInstancePhysicalDevices(context.instance), context.surface);
        else
            context.physicalDevice = physicalDevice;

        logInfoCB("createVulkanContext fetch physicalDeviceMemoryProperties."); 
        vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &context.physicalDeviceMemoryProperties);

        logInfoCB("createVulkanContext fetch queue families.");
        std::vector<QueueInfo> queueInfos(physicalDeviceMinimalRequirement.queueInfos.size());
        queueInfos = findQueueFamilyIndices(context.physicalDevice,
                                            physicalDeviceMinimalRequirement.queueInfos,
                                            context.surface
                                           );

        logInfoCB("createVulkanContext call createLogicalDevice.");
        context.device  = createLogicalDevice(
                                context.physicalDevice, 
                                queueInfos,
                                physicalDeviceMinimalRequirement.deviceExtensions
                                );

        logInfoCB("createVulkanContext retrive queue(s).");
        context.queueFamilies.resize(queueInfos.size());
        for (size_t i(0) ; i<context.queueFamilies.size(); i++)
        {
            context.queueFamilies[i].info   = queueInfos[i];
            context.queueFamilies[i].queues = getQueueFromDevice(context.device, context.queueFamilies[i].info);
        }

        logInfoCB("createVulkanContext create commands pools and allocating command buffers.");
        for (QueueFamily& queueFamily : context.queueFamilies)
        {
            const QueueInfo& qInfo = queueFamily.info;
            queueFamily.commands.resize(qInfo.cmdPoolInfos.size());

            for (size_t i(0) ; i<queueFamily.commands.size() ; i++)
            {
                const CommandInfo &cmdInfo = qInfo.cmdPoolInfos[i];
                      Command     &cmd     = queueFamily.commands[i];

                createCommandFromCommandInfo(context.device, 
                                             qInfo.familyIndex, 
                                             cmdInfo, 
                                             cmd
                                            );
            }
        }
            
        logInfoCB("createVulkanContext creating the swapchain.");

        clearNullMaxImageCount( physicalDeviceMinimalRequirement.swapChainInfo);
        context.swapChainInfo = physicalDeviceMinimalRequirement.swapChainInfo;
        updateSwapchainExtents(context.physicalDevice, context.surface, windowContext, context.swapChainInfo);
        discardUnsuportedFormatsAndPresentModes(context.swapChainInfo, context.physicalDevice, context.surface);
        context.swapChainInfo.queueFamilyIndicesSharingTheSwapChain = listQueueSharingTheSwapchain(context.queueFamilies);

        context.swapChain = createSwapChain(context.device,
                                            context.swapChainInfo,
                                            context.surface
                                           );

        context.swapchainImages = fetchImagesFromSwapChain(context.device, context.swapChain);

        context.swapchainImageViews = createImageViews( context.device,
                                                        context.swapchainImages,
                                                        context.swapChainInfo.formats[0].format,
                                                        context.swapChainInfo.subresourceRange
                                                      );

        if (semaphoreCount > 0)
        {
            logInfoCB("createVulkanContext creating semaphores.");
            context.semaphores.resize(semaphoreCount);
            for (VkSemaphore &semaphore : context.semaphores)
                semaphore = createSemaphore(context.device);
        }
        if (fenceCount > 0)
        {
            logInfoCB("createVulkanContext creating fences.");
            context.fences.resize(fenceCount);
            for (VkFence &fence : context.fences)
                fence = createFence(context.device, fenceSignaled);
        }

        return context;
    }

   
   #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    // See the manually loaded function section (at the bottom of this file).
    void destroyInstanceDebugUtilsMessengerEXT (
            VkInstance instance, 
            VkDebugUtilsMessengerEXT debugMessenger, 
            const VkAllocationCallbacks* pAllocator
            ); 
   #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

    void destroyVulkanContext(VulkanContext &context, bool waitDeviceIdle)
    {
        logVerboseCB("destroyVulkanContext have been called.");
        if (waitDeviceIdle)
            waitDeviceBecomeIdle(context.device);

       #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        if (isValidationLayersEnabled && context.isDebugCallbackHandleValid)
        {
            destroyInstanceDebugUtilsMessengerEXT(
                context.instance,
                context.debugCallbackHandle,
                nullptr
                );

            context.isDebugCallbackHandleValid = false;
        }
       #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

        // Any ressources that have been bind with the device must be destroyed here.

        for (std::shared_ptr<Buffer>& buffer : context.buffers)
            destroyBuffer(context.device, *buffer);

        for (VkSemaphore &semaphore : context.semaphores)
            destroySemaphore(context.device, semaphore);
        for (VkFence &fence : context.fences)
            destroyFence(context.device, fence);

        for (QueueFamily &queueFamily : context.queueFamilies)
        {
            for (Command &cmd : queueFamily.commands)
            {
                for (VkCommandPool &pool : cmd.pools)
                    destroyCommandPool(context.device, pool);
            }
        }

        for (size_t i(0) ; i<context.swapchainFramebuffers.size() ; i++)
            destroyFrameBuffer(context.device, context.swapchainFramebuffers[i]);

        for (size_t i(0) ; i<context.renderPasses.size() ; i++) 
            destroyRenderPass(context.device, context.renderPasses[i]);

        if (context.swapChain != VK_NULL_HANDLE)
        {
            for (VkImageView &imageView : context.swapchainImageViews)
                destroyImageView(context.device, imageView);

            vkDestroySwapchainKHR(context.device, context.swapChain, nullptr);
            logInfoCB("A swap chain have been destroyed.");
        }

        // context.physicalDevice is implicitly deleted at instance destruction.
        if (context.device != VK_NULL_HANDLE)// But a logic device must be destroyed.
        {
            vkDestroyDevice(context.device, nullptr);
            logInfoCB("A logic device have been destroyed.");
        }

        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);

        vkDestroyInstance(context.instance, nullptr);
        logInfoCB("A vulkan context has been destroyed.");
    }

    /**@brief Fetch queues from a device.*/
    std::vector<VkQueue> getQueueFromDevice(const VkDevice device, const QueueInfo &info)
    {
        std::vector<VkQueue> queues(info.count);

        for (uint32_t queueIndex(0) ; queueIndex < info.count ; queueIndex++)
        {
            vkGetDeviceQueue(device, info.familyIndex, queueIndex, &queues[queueIndex]);
        }

        return queues;
    }

    /**@brief Return the version of the api of the vulkan instance.
     * @return -1 if failed, otherwise the version of the api.
     */
    uint32_t getInstanceApiVersion()
    {
        uint32_t apiVersion;
        if (vkEnumerateInstanceVersion(&apiVersion) != VkResult::VK_SUCCESS)
            apiVersion=-1;

        return apiVersion;
    }

    std::vector<VkExtensionProperties> listSupportedPhysicalDeviceExtensions(const VkPhysicalDevice device, const char* pLayerName)
    {
        uint32_t count = 0;
        VkResult apiError = vkEnumerateDeviceExtensionProperties(device, pLayerName, &count, nullptr);
        
             if (apiError == VkResult::VK_ERROR_LAYER_NOT_PRESENT)
        {
            logErrorCB("listSupportedPhysicalDeviceExtensions called with a layer name that is not present.");
            throw std::logic_error("VKI::listSupportedPhysicalDeviceExtensions called with a layer name that is not present.");
        }
        else if (apiError == VkResult::VK_INCOMPLETE) // it can't be VK_INCOMPLETE.
        {
            logFatalErrorCB("listSupportedPhysicalDeviceExtension received VK_INCOMPLETE by fetching the number of extensions to retrieve.");
            throw std::logic_error("VKI::listSupportedPhysicalDeviceExtension received VK_INCOMPLETE by fetching the number of extensions to retrieve.");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("listSupportedPhysicalDeviceExtensions called but the program is running out of memory.");
            throw std::runtime_error("VKI::listSupportedPhysicalDeviceExtensions called but the program is running out of memory.");
        }

        std::vector<VkExtensionProperties> extensions(count);
        apiError = vkEnumerateDeviceExtensionProperties(device, pLayerName, &count, extensions.data());
        
        if (apiError == VkResult::VK_INCOMPLETE)
        {
            logWarningCB("listSupportedPhysicalDeviceExtensions failed to fetch EVERY device extensions (the count changed during the fetch).");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("listSupportedPhysicalDeviceExtensions the validation layer disappeared while retrieving the extensions.");
            throw std::logic_error("listSupportedPhysicalDeviceExtensions the validation layer disappeared while retrieving the extensions.");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("listSupportedPhysicalDeviceExtensions called but the program is running out of memory.");
            throw std::runtime_error("VKI::listSupportedPhysicalDeviceExtensions called but the program is running out of memory.");
        }

        return extensions;
    }

    bool isPhysicalDeviceExtensionSupported(VkPhysicalDevice device, const std::vector<const char*> &extensionNames, const char* pLayerName)
    {
        std::vector<VkExtensionProperties> extensions = listSupportedPhysicalDeviceExtensions(device, pLayerName);

        bool isAllExtensionsSupported = true;
        for (const char* extensionName : extensionNames)
        {
            bool isExtensionFound = false;
            for (VkExtensionProperties extension : extensions)
            {
                if (std::strcmp(extension.extensionName, extensionName) == 0)
                {
                    isExtensionFound = true;
                    break;
                }
            }

            isAllExtensionsSupported &= isExtensionFound;
        }

        return isAllExtensionsSupported;
    }

    std::vector<VkExtensionProperties> listInstanceExtensionProperties(const char* pLayerName)
    {
        uint32_t extCount=0;

        lastApiError = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, nullptr);
        if (lastApiError != VkResult::VK_SUCCESS)
        {
            logErrorCB("listInstanceExtensionProperties -> failed to read how much extension exist.");
            return {};
        }

        std::vector<VkExtensionProperties> extVector(extCount);

        lastApiError = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, extVector.data());
        if (lastApiError == VkResult::VK_INCOMPLETE)
        {
            logWarningCB("listInstanceExtensionProperties -> VK_INCOMPLETE -> failed to fetch ALL of the properties array.");
        }
        else if (lastApiError != VkResult::VK_SUCCESS)
        {
            logErrorCB("listInstanceExtensionProperties -> failed to fetch the properties array.");
        }

        return extVector;
    }

    /**@brief Check if a list of VkExtensionProperties are available and with a >= version.
     */
    bool isInstanceExtensionSupported(const std::vector<VkExtensionProperties>& extensions, const char* pLayerName)
    {
        std::vector<VkExtensionProperties> properties = listInstanceExtensionProperties(pLayerName);

        for (VkExtensionProperties isExt : extensions)
        {
            bool extensionNotFound = true;
            bool isBadVersion  = true; // if the extension have been found but the version is too old.
            uint32_t badVersion= 0;

            for (VkExtensionProperties ext : properties)
            {
                if (std::strcmp(ext.extensionName, isExt.extensionName) == 0)
                {
                    extensionNotFound = false;
                    badVersion = ext.specVersion;
                    isBadVersion = ext.specVersion < isExt.specVersion;
                }
            }
            if (extensionNotFound)
            {
                std::stringstream ss;
                ss<<"isInstanceExtensionSupported -> extension "<<isExt.extensionName<<" haven't been found.";
                logErrorCB(ss.str().c_str());

                return false;
            }
            else if (isBadVersion)
            {
                std::stringstream ss;
                ss<<"isInstanceExtensionSupported -> extension "<<
                            std::string(isExt.extensionName)<<
                            " have been found but the versions are different : "<<
                            isExt.specVersion<<" is required but only : "<<
                            badVersion<<" have been found.";
                logErrorCB(ss.str().c_str());
                return false;
            }
        }

        return true;
    }

    // fetch extension from getRequiredExtension(...);
    bool isAllInstanceExtensionSupported()
    {
        std::vector<const char*> extensionNames = getRequiredExtensions();
        for (const char* extName : physicalDeviceMinimalRequirement.instanceExtensions)
            extensionNames.push_back(extName);
        extensionNames.shrink_to_fit();

        std::vector<VkExtensionProperties> extensions(extensionNames.size());

        for (std::size_t i(0) ; i<extensionNames.size() ; i++)
        {
            extensions[i] = createVkExtensionProperties(extensionNames[i]);
        }

        return isInstanceExtensionSupported(extensions);
    }

    /**@brief Constructor for a VkExtensionProperties. 
     */
    /*constexpr*/ VkExtensionProperties createVkExtensionProperties(const char* name, uint32_t version)
    {
        VkExtensionProperties property = {"", version};

        // Copy char* to char[256];
        bool nullFound = false;
        for (uint16_t i(0) ; i<VK_MAX_EXTENSION_NAME_SIZE ; i++)
        {
            if (name[i] == '\0')
                nullFound = true;

            if (nullFound)
                property.extensionName[i] = '\0';
            else
                property.extensionName[i] = name[i];
        }

        return property;
    }


    void logAvailableExtension(const char* pLayerName)
    {
        static bool recursive = true;
        if (pLayerName == nullptr && recursive)
        {
            recursive=false;

            std::vector<VkLayerProperties> layers = listInstanceLayersProperties();

            logInfoCB("Starting to log every available extension :");
            logAvailableExtension(nullptr);
            for (VkLayerProperties layer : layers)
            {
                logAvailableExtension(layer.layerName);
            }
        }

        std::vector<VkExtensionProperties> extensions = listInstanceExtensionProperties(pLayerName);

        std::stringstream ss;
        ss<<"List of all available extension for the layer : ";
        if (pLayerName != nullptr)
            ss<<pLayerName;
        else
            ss<<"None";
        ss<<" :";

        for (VkExtensionProperties property : extensions)
        {
            ss<<"\n\t"<<property.extensionName;
            ss<<" specVersion="<<property.specVersion;
        }

        logInfoCB(ss.str().c_str());
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t count = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);

        if (glfwExtensions == NULL)
        {
            logFatalErrorCB("getRequiredExtensions -> glfwGetRequiredInstanceExtensions returned NULL.");
            throw std::runtime_error("VKI::getRequiredExtensions glfwGetRequiredInstanceExtensions returned NULL.");
        }

        std::vector<const char*> extensions(count);

        for (uint32_t i(0) ; i<count ; i++)
        {
            extensions[i] = glfwExtensions[i];
        }

        if (isValidationLayersEnabled)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    bool isGlfwExtensionAvailable()
    {
        bool extensionSupported = true;

        uint32_t size;
        const char** names = glfwGetRequiredInstanceExtensions(&size);

        for (uint32_t i(0) ; i<size ; i++)
        {
            VkExtensionProperties property = createVkExtensionProperties(names[i]);
            extensionSupported &= isInstanceExtensionSupported({property}, nullptr);
            // Really inefficient but glfw require only two extension (i think).
        }
        return extensionSupported;
    }

    /*constexpr*/ VkLayerProperties createVkLayerProperties(const char* name, uint32_t specVersion, uint32_t implementationVersion)
    {
        VkLayerProperties property = {"", specVersion, implementationVersion, ""};

        // Copy char* to char[256];
        bool nullFound = false;
        for (uint16_t i(0) ; i<VK_MAX_EXTENSION_NAME_SIZE ; i++)
        {
            if (name[i] == '\0')
                nullFound = true;

            if (nullFound)
                property.layerName[i] = '\0';
            else
                property.layerName[i] = name[i];
        }

        return property;
    }

    bool isAllValidationLayersAvailable()
    {
    #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        std::vector<VkLayerProperties> layers = listInstanceLayersProperties();
        
        for (VkLayerProperties isLayer : validationLayers)
        {
            bool layerNotFound = true;
            bool isBadSpecVersion  = true; // if the layer have been found but the spec version is too old.
            bool isBadImplVersion  = true; // if the layer have been found but the implementation version is too old.
            uint32_t badSpecVersion= 0;
            uint32_t badImplVersion= 0;

            for (VkLayerProperties layer : layers)
            {
                if (std::strcmp(layer.layerName, isLayer.layerName) == 0)
                {
                    layerNotFound = false;

                    badSpecVersion = layer.specVersion;
                    isBadSpecVersion = layer.specVersion < isLayer.specVersion;

                    badImplVersion = layer.implementationVersion;
                    isBadImplVersion = layer.implementationVersion < isLayer.implementationVersion;
                }
            }
            if (layerNotFound)
            {
                std::stringstream ss;
                ss<<"isAllValidationLayerAvailable -> layer "<<isLayer.layerName<<" haven't been found.";
                logErrorCB(ss.str().c_str());

                return false;
            }
            else if (isBadSpecVersion)
            {
                std::stringstream ss;
                ss<<"isAllValidationLayerAvailable -> layer "<<
                            std::string(isLayer.layerName)<<
                            " have been found but the spec versions are different : "<<
                            isLayer.specVersion<<" is required but only : "<<
                            badSpecVersion<<" have been found.";
                logErrorCB(ss.str().c_str());
                return false;
            }
            else if (isBadImplVersion)
            {
                std::stringstream ss;
                ss<<"isAllValidationLayerAvailable -> layer "<<
                            std::string(isLayer.layerName)<<
                            " have been found but the implmentation versions are different : "<<
                            isLayer.implementationVersion<<" is required but only : "<<
                            badImplVersion<<" have been found.";
                logErrorCB(ss.str().c_str());
                return false;
            }
        }
    #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS

        return true; // #else = #endif.
    }

    std::vector<VkLayerProperties> listInstanceLayersProperties()
    {
        uint32_t size=0;
        lastApiError = vkEnumerateInstanceLayerProperties(&size, nullptr);
        if (lastApiError != VkResult::VK_SUCCESS)
        {
            logErrorCB("listInstanceLayersProperties failed to count how many layers exist.");
            return {};
        }

        std::vector<VkLayerProperties> layersProperties(size);

        lastApiError = vkEnumerateInstanceLayerProperties(&size, layersProperties.data());
        if (lastApiError != VkResult::VK_SUCCESS)
        {
            logErrorCB("listInstanceLayersProperties failed to fetch every validation layers that exist.");
            return {};
        }

        return layersProperties;

    }

    void logAvailableValidationLayers()
    {
        std::vector<VkLayerProperties> layers = listInstanceLayersProperties();


        std::stringstream ss;
        ss<<"Validation layers are "<<((isValidationLayersEnabled) ? "enabled, and " : "disabled, but ");
        ss<<std::dec<<layers.size()<<" validation layers available :";

        for (VkLayerProperties property : layers)
        {
            ss<<"\n\t"<<property.layerName;
            ss<<", specVersion="<<property.specVersion;
            ss<<", implementationVersion="<<property.implementationVersion;
            ss<<", description="<<property.description;
            ss<<".";
        }

        logInfoCB(ss.str().c_str());
    }

    /**@brief Retrive the names from validationLayers.
     * It's supposed to replace the need for validationLayersNames.
     */
    /*
    const char** listValidationLayersNames(uint32_t* count)
    {
    #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS

        *count = validationLayers.size();
        if (not isAllValidationLayersAvailable())
        {
            logErrorCB("listValidationLayersNames called with layers enabled, but not all layers are available.");
            // count will cause createInstance to set lastApiResult to VK_ERROR_LAYER_NOT_PRESENT.
            // Which will throw a fatal error.
            return nullptr;
        }

        std::array<const char*, VKI_NB_VULKAN_VALIDATION_LAYERS> validationLayersNames;

    #else//VKI_ENABLE_VULKAN_VALIDATION_LAYERS
        *count=0;
        return nullptr;
    #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    }
    */

    std::vector<VkPhysicalDevice> listInstancePhysicalDevices(const VkInstance instance)
    {
        uint32_t count = 0;

        lastApiError = vkEnumeratePhysicalDevices(instance, &count, nullptr);

        if ((lastApiError != VkResult::VK_SUCCESS) && (lastApiError != VkResult::VK_INCOMPLETE))
        {
            logFatalErrorCB("listInstancePhysicalDevices failed to count how many physical device are available.");
            throw std::runtime_error("listInstancePhysicalDevices failed to count how many physical device are available.");
        }

        std::vector<VkPhysicalDevice> physicalDevices(count);
        lastApiError = vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());

        if (lastApiError == VkResult::VK_INCOMPLETE)
        {
            logErrorCB("listInstancePhysicalDevices failed to fetch every physical devices.");
        }
        else if (
                (lastApiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY)   ||
                (lastApiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY)
                )
        {
            logFatalErrorCB("listInstancePhysicalDevices failed to fetch physicalDevices -> out of memory.");
            throw std::runtime_error("listInstancePhysicalDevices failed to fetch physicalDevices -> out of memory."); 
        }

        return physicalDevices;
    }

    mpz_class ratePhysicalDevice(const VkPhysicalDevice device, const bool logInfo) noexcept
    {
        // This function is supposed to compute the 'score' (aka an 
        // unsigned integer) to rank a device.
        // For now the score is computed as :
        //  score = +limits +memory *features(count+importance);
        //       if (type == discret gpu) <<(score.size/2);
        //  else if (type == integrated ) >>(score.size/4);
        //  else if (type == virtual )    >>(score.size/4);
        //  else if (type == cpu     )    >>(score.size/2);
        //  else if (type == other   )    &=0; // Almost ignore these.
        //
        // The problem is that, by doing an rought estimation of the maximal 
        // score obtainable (just by concatenating all of the bytes), like :
        //  limits   =  504 Bytes;
        //  features =  220 Bytes;
        //  memory   =  520 Bytes;
        //  total    = 1244 Bytes;
        //    1244*8 = 9952 Bits;
        //   2^9952 ~= 70x10^2995;
        // Which doesn't mean anything ...
        // Even doubles can't store that monster
        // (I almost sure the score could be used as a UUID).
        // The solution that i come up with is to use the GMP lib which allow 
        // me to manipulate insanely large integers.
        //
        // While that is not the probleme here (the probleme is my score 
        // compute method that is really bad), it allow me to focus on making 
        // VKI functional.
        //
        // + gmp don't encapsulate their declarations
        // into a namespace so it's a bad lib >:(


        mpz_class score(0);

        // Properties score :
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        //score += properties.apiVersion;   // versions doesn't represent a performance indicator.
        //score += properties.driverVersion;// versions doesn't represent a performance indicator.
        
        //properties.vendorID isn't relevant here.
        //properties.deviceID isn't relevant here.
        //properties.deviceType is too important -> it's added at the end.
        //properties.deviceName isn't relevant here.
        //properties.pipelineCacheUUID isn't relevant here (i think ..).

        // Add each limit to the score 
        // (weights can be added if a limit is more important than others).
        VkPhysicalDeviceLimits devLimits = properties.limits;

    	score += devLimits.maxImageDimension1D;                             /*uint32_t*/
        score += devLimits.maxImageDimension2D; 							/*uint32_t*/
        score += devLimits.maxImageDimension3D; 							/*uint32_t*/
        score += devLimits.maxImageDimensionCube; 							/*uint32_t*/
        score += devLimits.maxImageArrayLayers; 							/*uint32_t*/
        score += devLimits.maxTexelBufferElements; 							/*uint32_t*/
        score += devLimits.maxUniformBufferRange; 							/*uint32_t*/
        score += devLimits.maxStorageBufferRange; 							/*uint32_t*/
        score += devLimits.maxPushConstantsSize; 							/*uint32_t*/
        score += devLimits.maxMemoryAllocationCount; 						/*uint32_t*/
        score += devLimits.maxSamplerAllocationCount; 						/*uint32_t*/
        score += devLimits.bufferImageGranularity; 							/*VkDeviceSize*/
        score += devLimits.sparseAddressSpaceSize; 							/*VkDeviceSize*/
        score += devLimits.maxBoundDescriptorSets; 							/*uint32_t*/
        score += devLimits.maxPerStageDescriptorSamplers; 					/*uint32_t*/
        score += devLimits.maxPerStageDescriptorUniformBuffers; 			/*uint32_t*/
        score += devLimits.maxPerStageDescriptorStorageBuffers; 			/*uint32_t*/
        score += devLimits.maxPerStageDescriptorSampledImages; 				/*uint32_t*/
        score += devLimits.maxPerStageDescriptorStorageImages; 				/*uint32_t*/
        score += devLimits.maxPerStageDescriptorInputAttachments; 			/*uint32_t*/
        score += devLimits.maxPerStageResources; 							/*uint32_t*/
        score += devLimits.maxDescriptorSetSamplers; 						/*uint32_t*/
        score += devLimits.maxDescriptorSetUniformBuffers; 					/*uint32_t*/
        score += devLimits.maxDescriptorSetUniformBuffersDynamic; 			/*uint32_t*/
        score += devLimits.maxDescriptorSetStorageBuffers; 					/*uint32_t*/
        score += devLimits.maxDescriptorSetStorageBuffersDynamic; 			/*uint32_t*/
        score += devLimits.maxDescriptorSetSampledImages; 					/*uint32_t*/
        score += devLimits.maxDescriptorSetStorageImages; 					/*uint32_t*/
        score += devLimits.maxDescriptorSetInputAttachments; 				/*uint32_t*/
        score += devLimits.maxVertexInputAttributes; 						/*uint32_t*/
        score += devLimits.maxVertexInputBindings; 							/*uint32_t*/
        score += devLimits.maxVertexInputAttributeOffset; 				    /*uint32_t*/
        score += devLimits.maxVertexInputBindingStride; 					/*uint32_t*/
        score += devLimits.maxVertexOutputComponents; 						/*uint32_t*/
        score += devLimits.maxTessellationGenerationLevel; 					/*uint32_t*/
        score += devLimits.maxTessellationPatchSize; 						/*uint32_t*/
        score += devLimits.maxTessellationControlPerVertexInputComponents; 	/*uint32_t*/
        score += devLimits.maxTessellationControlPerVertexOutputComponents; /*uint32_t*/
        score += devLimits.maxTessellationControlPerPatchOutputComponents; 	/*uint32_t*/
        score += devLimits.maxTessellationControlTotalOutputComponents; 	/*uint32_t*/
        score += devLimits.maxTessellationEvaluationInputComponents; 		/*uint32_t*/
        score += devLimits.maxTessellationEvaluationOutputComponents; 		/*uint32_t*/
        score += devLimits.maxGeometryShaderInvocations; 					/*uint32_t*/
        score += devLimits.maxGeometryInputComponents; 						/*uint32_t*/
        score += devLimits.maxGeometryOutputComponents; 					/*uint32_t*/
        score += devLimits.maxGeometryOutputVertices; 						/*uint32_t*/
        score += devLimits.maxGeometryTotalOutputComponents; 				/*uint32_t*/
        score += devLimits.maxFragmentInputComponents; 						/*uint32_t*/
        score += devLimits.maxFragmentOutputAttachments; 					/*uint32_t*/
        score += devLimits.maxFragmentDualSrcAttachments; 					/*uint32_t*/
        score += devLimits.maxFragmentCombinedOutputResources; 				/*uint32_t*/
        score += devLimits.maxComputeSharedMemorySize; 						/*uint32_t*/
        score += devLimits.maxComputeWorkGroupCount[0]; 					/*uint32_t[3]*/
        score += devLimits.maxComputeWorkGroupCount[1]; 					/*uint32_t[3]*/
        score += devLimits.maxComputeWorkGroupCount[2]; 					/*uint32_t[3]*/
        score += devLimits.maxComputeWorkGroupInvocations; 					/*uint32_t*/
        score += devLimits.maxComputeWorkGroupSize[0]; 						/*uint32_t[3]*/
        score += devLimits.maxComputeWorkGroupSize[1]; 						/*uint32_t[3]*/
        score += devLimits.maxComputeWorkGroupSize[2]; 						/*uint32_t[3]*/
        score += devLimits.subPixelPrecisionBits; 							/*uint32_t*/
        score += devLimits.subTexelPrecisionBits; 							/*uint32_t*/
        score += devLimits.mipmapPrecisionBits; 							/*uint32_t*/
        score += devLimits.maxDrawIndexedIndexValue; 						/*uint32_t*/
        score += devLimits.maxDrawIndirectCount; 							/*uint32_t*/
        score += devLimits.maxSamplerLodBias; 							    /*float*/
        score += devLimits.maxSamplerAnisotropy; 							/*float*/
        score += devLimits.maxViewports; 							        /*uint32_t*/
        score += devLimits.maxViewportDimensions[0]; 						/*uint32_t[2]*/
        score += devLimits.maxViewportDimensions[1]; 						/*uint32_t[2]*/
        score += devLimits.viewportBoundsRange[0]; 							/*float[2]*/
        score += devLimits.viewportBoundsRange[1]; 							/*float[2]*/
        score += devLimits.viewportSubPixelBits; 							/*uint32_t*/
        score += devLimits.minMemoryMapAlignment; 							/*size_t*/
        score += devLimits.minTexelBufferOffsetAlignment; 					/*VkDeviceSize*/
        score += devLimits.minUniformBufferOffsetAlignment; 				/*VkDeviceSize*/
        score += devLimits.minStorageBufferOffsetAlignment; 				/*VkDeviceSize*/
        score += devLimits.minTexelOffset; 							        /*int32_t*/
        score += devLimits.maxTexelOffset; 							        /*uint32_t*/
        score += devLimits.minTexelGatherOffset; 							/*int32_t*/
        score += devLimits.maxTexelGatherOffset; 							/*uint32_t*/
        score += devLimits.minInterpolationOffset; 							/*float*/
        score += devLimits.maxInterpolationOffset; 							/*float*/
        score += devLimits.subPixelInterpolationOffsetBits; 				/*uint32_t*/
        score += devLimits.maxFramebufferWidth; 							/*uint32_t*/
        score += devLimits.maxFramebufferHeight; 							/*uint32_t*/
        score += devLimits.maxFramebufferLayers; 							/*uint32_t*/
        score += devLimits.framebufferColorSampleCounts; 					/*VkSampleCountFlags*/
        score += devLimits.framebufferDepthSampleCounts; 					/*VkSampleCountFlags*/
        score += devLimits.framebufferStencilSampleCounts; 					/*VkSampleCountFlags*/
        score += devLimits.framebufferNoAttachmentsSampleCounts; 			/*VkSampleCountFlags*/
        score += devLimits.maxColorAttachments; 							/*uint32_t*/
        score += devLimits.sampledImageColorSampleCounts; 					/*VkSampleCountFlags*/
        score += devLimits.sampledImageIntegerSampleCounts; 				/*VkSampleCountFlags*/
        score += devLimits.sampledImageDepthSampleCounts; 					/*VkSampleCountFlags*/
        score += devLimits.sampledImageStencilSampleCounts; 				/*VkSampleCountFlags*/
        score += devLimits.storageImageSampleCounts; 						/*VkSampleCountFlags*/
        score += devLimits.maxSampleMaskWords; 							    /*uint32_t*/
        score += devLimits.timestampComputeAndGraphics; 				    /*VkBool32*/
        score += devLimits.timestampPeriod; 							    /*float*/
        score += devLimits.maxClipDistances; 							    /*uint32_t*/
        score += devLimits.maxCullDistances; 							    /*uint32_t*/
        score += devLimits.maxCombinedClipAndCullDistances; 				/*uint32_t*/
        score += devLimits.discreteQueuePriorities; 						/*uint32_t*/
        score += devLimits.pointSizeRange[0]; 						        /*float[2]*/
        score += devLimits.pointSizeRange[1];						        /*float[2]*/
        score += devLimits.lineWidthRange[0]; 						        /*float[2]*/
        score += devLimits.lineWidthRange[1]; 						        /*float[2]*/
        score += devLimits.pointSizeGranularity; 							/*float*/
        score += devLimits.lineWidthGranularity; 							/*float*/
        score += devLimits.strictLines; 							        /*VkBool32*/
        score += devLimits.standardSampleLocations; 					    /*VkBool32*/
        score += devLimits.optimalBufferCopyOffsetAlignment; 				/*VkDeviceSize*/
        score += devLimits.optimalBufferCopyRowPitchAlignment; 				/*VkDeviceSize*/
        score += devLimits.nonCoherentAtomSize; 							/*VkDeviceSize*/

        // Add the memory capacity.
        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(device, &memory);

        // memory type doesn't seem to be scorable.
        // Adding memory heapCount to score * heap importance.
        for (uint32_t i(0) ; i<memory.memoryHeapCount ; i++)
        {
            VkMemoryHeap heap = memory.memoryHeaps[i];

            mpz_class size = heap.size; // mpz cause uint64_t * Weight may overflow.
            if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                size*=2;
            score += size;
        }

        // Multiply the current score for each features.
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        // The following weights are (almost) randomly attributed since i currently
        // don't know anythings about graphics.
		score *= 1+(( features.robustBufferAccess )			    	     ?      500 : 0 );
		score *= 1+(( features.fullDrawIndexUint32 )				     ?        1 : 0 );
		score *= 1+(( features.imageCubeArray )						     ?        1 : 0 );
		score *= 1+(( features.independentBlend )					     ?        1 : 0 );
		score *= 1+(( features.geometryShader )						     ?   100000 : 0 );
		score *= 1+(( features.tessellationShader )					     ?       10 : 0 );
		score *= 1+(( features.sampleRateShading )					     ?        1 : 0 );
		score *= 1+(( features.dualSrcBlend )						     ?        1 : 0 );
		score *= 1+(( features.logicOp )				    		     ?    10000 : 0 );
		score *= 1+(( features.multiDrawIndirect )					     ?        1 : 0 );
		score *= 1+(( features.drawIndirectFirstInstance )			     ?        1 : 0 ); 
        score *= 1+(( features.depthClamp )   					         ?        1 : 0 );
		score *= 1+(( features.depthBiasClamp )				   		     ?        1 : 0 );
		score *= 1+(( features.fillModeNonSolid )					     ?        1 : 0 );
		score *= 1+(( features.depthBounds )						     ?        1 : 0 );
		score *= 1+(( features.wideLines )				    		     ?        1 : 0 );
		score *= 1+(( features.largePoints )			   			     ?        1 : 0 );
		score *= 1+(( features.alphaToOne )				    		     ?        1 : 0 );
		score *= 1+(( features.multiViewport )						     ?        1 : 0 );
		score *= 1+(( features.samplerAnisotropy )		    		     ?        1 : 0 );
		score *= 1+(( features.textureCompressionETC2 )				     ?        1 : 0 );
		score *= 1+(( features.textureCompressionASTC_LDR )			     ?        1 : 0 );
		score *= 1+(( features.textureCompressionBC )				     ?        1 : 0 );
		score *= 1+(( features.occlusionQueryPrecise )				     ?        1 : 0 );
		score *= 1+(( features.pipelineStatisticsQuery )			     ?        1 : 0 );
		score *= 1+(( features.vertexPipelineStoresAndAtomics )		     ?        2 : 0 );
		score *= 1+(( features.fragmentStoresAndAtomics )			     ?        2 : 0 );
		score *= 1+(( features.shaderTessellationAndGeometryPointSize )  ?        1 : 0 );
		score *= 1+(( features.shaderImageGatherExtended )			     ?        1 : 0 );
		score *= 1+(( features.shaderStorageImageExtendedFormats )       ?        1 : 0 );
		score *= 1+(( features.shaderStorageImageMultisample )		     ?        1 : 0 );
		score *= 1+(( features.shaderStorageImageReadWithoutFormat )     ?        1 : 0 );
		score *= 1+(( features.shaderStorageImageWriteWithoutFormat )    ?        1 : 0 );
		score *= 1+(( features.shaderUniformBufferArrayDynamicIndexing)  ?        1 : 0 );
		score *= 1+(( features.shaderSampledImageArrayDynamicIndexing )  ?        1 : 0 );
		score *= 1+(( features.shaderStorageBufferArrayDynamicIndexing)  ?        1 : 0 );
		score *= 1+(( features.shaderStorageImageArrayDynamicIndexing )  ?        1 : 0 );
		score *= 1+(( features.shaderClipDistance )					     ?        1 : 0 );
		score *= 1+(( features.shaderCullDistance )					     ?        1 : 0 );
		score *= 1+(( features.shaderFloat64 )						     ?   100000 : 0 );
		score *= 1+(( features.shaderInt64 )						     ?   100000 : 0 );
		score *= 1+(( features.shaderInt16 )						  	 ?    10000 : 0 );
		score *= 1+(( features.shaderResourceResidency )			  	 ?        1 : 0 );
		score *= 1+(( features.shaderResourceMinLod )		    	 	 ?        1 : 0 );
		score *= 1+(( features.sparseBinding )						  	 ?        1 : 0 );
		score *= 1+(( features.sparseResidencyBuffer )				  	 ?        1 : 0 );
		score *= 1+(( features.sparseResidencyImage2D )				     ?        1 : 0 );
		score *= 1+(( features.sparseResidencyImage3D )				     ?        1 : 0 );
		score *= 1+(( features.sparseResidency2Samples )			     ?        1 : 0 );
		score *= 1+(( features.sparseResidency4Samples )			     ?        1 : 0 );
		score *= 1+(( features.sparseResidency8Samples )			     ?        1 : 0 );
		score *= 1+(( features.sparseResidency16Samples)			     ?        1 : 0 );
		score *= 1+(( features.sparseResidencyAliased  )				 ?        1 : 0 );
		score *= 1+(( features.variableMultisampleRate )		         ?        1 : 0 );
		score *= 1+(( features.inheritedQueries)					     ?        1 : 0 );


        // End scoring by the device type.

        switch (properties.deviceType)
        {
            case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU):
            {
                score *= 2*(mpz_sizeinbase(score.get_mpz_t(), 10)/2); // aka <<= size/2;
                break;
            }
            case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU):
            {
                score /= 2*(mpz_sizeinbase(score.get_mpz_t(), 10)/4); // aka >>= size/4;
                break;
            }
            case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU):
            {
                score /= 2*(mpz_sizeinbase(score.get_mpz_t(), 10)/4); // aka >>= size/4;
                break;
            }
            case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU):
            {
                score /= 2*(mpz_sizeinbase(score.get_mpz_t(), 10)/2); // aka >>= size/2;
                break;
            }
            case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER):
            {
                score = 0; // Can it be a accelerator ?
                           // Could be intresting for compute shaders.
                break;
            }
            default: // Can't happend since every type have been case(-d).
            {
                logErrorCB("ratePhysicalDevice device's type is not reconnize.");
                score = 0; // Nobody want a bug.
            }
        }

        if (logInfo)
        {
            std::stringstream ss;
            ss<<"Physical device found : ";
            ss<<"name = "<<properties.deviceName<<"\n\t";
            ss<<"apiVerions = "<<properties.apiVersion<<"\n\t";
            ss<<"driverVersion ="<<properties.driverVersion<<"\n\t";
            ss<<"vendorID = "<<properties.vendorID<<"\n\t";
            ss<<"deviceID = "<<properties.deviceID<<"\n\t";

            ss<<"deviceType = ";
            switch (properties.deviceType)
            {
                case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU):
                {
                    ss<<"intergrated gpu";
                    break;
                }
                case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU):
                {
                    ss<<"discrete gpu";
                    break;
                }
                case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU):
                {
                    ss<<"virtual gpu";
                    break;
                }
                case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU):
                {
                    ss<<"cpu";
                    break;
                }
                case (VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER):
                {
                    ss<<"Weird unkown type"; // don't use it...
                    break;
                }
                default:
                    ss<<"The type is not handled by the vulkan api and should never appear here!"; 
                    break;
            }
            
            char *score_str = mpz_get_str(nullptr, 10, score.get_mpz_t());
            ss<<"\n\tscore = "<<score_str;

            logInfoCB(ss.str().c_str());
            //delete score_str;
        }

        return score;
    }


    bool isPhysicalDeviceMatchMinimalRequirements(const VkPhysicalDevice device, const VkSurfaceKHR surface) noexcept
    {
        if (device == VK_NULL_HANDLE)
            return false;

        const VkPhysicalDeviceLimits    targetLimits   = physicalDeviceMinimalRequirement.limits;
        const VkPhysicalDeviceFeatures  targetFeatures = physicalDeviceMinimalRequirement.features;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        const VkPhysicalDeviceLimits limits = properties.limits;

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

    #ifndef VKI_DISABLE_LOGS_CALL
        #define VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(opeA, opeB, operator, property) \
        else if (opeA.property operator opeB.property)\
            {ss<<#property; goto TESTS_FAILED;}

        std::stringstream ss;
        ss<<"isPhysicalDeviceMathMinimalRequirements Device > "<<properties.deviceName<<" < is rejected due to : ";
    #else
        #define VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(opeA, opeB, operator, property) \
        else if (opeA.property operator opeB.property)\
            {goto TESTS_FAILED;}
    #endif

        // from ~850 lines to ~170, thanks preprocessor <3.
        if constexpr (false){throw std::logic_error("sorry");} // Sorry for this ugly line ...
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxImageDimension1D)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxImageDimension2D)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxImageDimension3D)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxImageDimensionCube)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxImageArrayLayers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTexelBufferElements)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxUniformBufferRange)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxStorageBufferRange)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPushConstantsSize)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxMemoryAllocationCount)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxSamplerAllocationCount)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, bufferImageGranularity)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, sparseAddressSpaceSize)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxBoundDescriptorSets)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorSamplers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorUniformBuffers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorStorageBuffers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorSampledImages)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorStorageImages)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageDescriptorInputAttachments)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxPerStageResources)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetSamplers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetUniformBuffers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetUniformBuffersDynamic)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetStorageBuffers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetStorageBuffersDynamic)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetSampledImages)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetStorageImages)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDescriptorSetInputAttachments)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxVertexInputAttributes)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxVertexInputBindings)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxVertexInputAttributeOffset)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxVertexInputBindingStride)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxVertexOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationGenerationLevel)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationPatchSize)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationControlPerVertexInputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationControlPerVertexOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationControlPerPatchOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationControlTotalOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationEvaluationInputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTessellationEvaluationOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxGeometryShaderInvocations)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxGeometryInputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxGeometryOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxGeometryOutputVertices)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxGeometryTotalOutputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFragmentInputComponents)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFragmentOutputAttachments)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFragmentDualSrcAttachments)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFragmentCombinedOutputResources)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeSharedMemorySize)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupCount[0])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupCount[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupCount[2])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupInvocations)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupSize[0])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupSize[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxComputeWorkGroupSize[2])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, subPixelPrecisionBits)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, subTexelPrecisionBits)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, mipmapPrecisionBits)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDrawIndexedIndexValue)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxDrawIndirectCount)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxSamplerLodBias)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxSamplerAnisotropy)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxViewports)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxViewportDimensions[0])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxViewportDimensions[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, <, viewportBoundsRange[0]) // yes the operator is correct.
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, viewportBoundsRange[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, viewportSubPixelBits)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, minMemoryMapAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, minTexelBufferOffsetAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, minUniformBufferOffsetAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, minStorageBufferOffsetAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, <, minTexelOffset) // Yes the operator is correct.
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTexelOffset)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, <, minTexelGatherOffset) // Yes the operator is correct.
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxTexelGatherOffset)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, <, minInterpolationOffset)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxInterpolationOffset)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, subPixelInterpolationOffsetBits)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFramebufferWidth)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFramebufferHeight)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxFramebufferLayers)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, framebufferColorSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, framebufferDepthSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, framebufferStencilSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, framebufferNoAttachmentsSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxColorAttachments)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, sampledImageColorSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, sampledImageIntegerSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, sampledImageDepthSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, sampledImageStencilSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, storageImageSampleCounts)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxSampleMaskWords)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, timestampComputeAndGraphics)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, timestampPeriod)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxClipDistances)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxCullDistances)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, maxCombinedClipAndCullDistances)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, discreteQueuePriorities)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, pointSizeRange[0])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, pointSizeRange[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, lineWidthRange[0])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, lineWidthRange[1])
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, pointSizeGranularity)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, lineWidthGranularity)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, strictLines)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, standardSampleLocations)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, optimalBufferCopyOffsetAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, optimalBufferCopyRowPitchAlignment)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetLimits, limits, >, nonCoherentAtomSize)

        // features.
        if constexpr (false){throw std::logic_error("sorry");}
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, robustBufferAccess)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, fullDrawIndexUint32)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, imageCubeArray)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, independentBlend)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, geometryShader)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, tessellationShader)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sampleRateShading)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, dualSrcBlend)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, logicOp)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, multiDrawIndirect)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, drawIndirectFirstInstance)
        VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, depthClamp)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, depthBiasClamp)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, fillModeNonSolid)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, depthBounds)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, wideLines)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, largePoints)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, alphaToOne)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, multiViewport)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, samplerAnisotropy)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, textureCompressionETC2)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, textureCompressionASTC_LDR)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, textureCompressionBC)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, occlusionQueryPrecise)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, pipelineStatisticsQuery)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, vertexPipelineStoresAndAtomics)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, fragmentStoresAndAtomics)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderTessellationAndGeometryPointSize)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderImageGatherExtended)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageImageExtendedFormats)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageImageMultisample)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageImageReadWithoutFormat)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageImageWriteWithoutFormat)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderUniformBufferArrayDynamicIndexing)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderSampledImageArrayDynamicIndexing)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageBufferArrayDynamicIndexing)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderStorageImageArrayDynamicIndexing)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderClipDistance)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderCullDistance)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderFloat64)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderInt64)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderInt16)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderResourceResidency)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, shaderResourceMinLod)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseBinding)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidencyBuffer)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidencyImage2D)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidencyImage3D)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidency2Samples)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidency4Samples)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidency8Samples)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidency16Samples)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, sparseResidencyAliased )
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, variableMultisampleRate)
		VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO(targetFeatures, features, &&, inheritedQueries)

        #undef VKI_ISPHYSICALDEVICEMATCHMINIMALREQUIREMENTS_TESTMACRO

        if (not areRequiredFamilyQueueAvailable(device, surface))
        {
            ss<<"areRequiredFamilyQueueAvailable failed, which check if the required family queue types were available.";
            goto TESTS_FAILED;
        }

        if (not isPhysicalDeviceExtensionSupported(device, physicalDeviceMinimalRequirement.deviceExtensions))
        {
        #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS 
            bool isExtensionPartOfAValidationLayer=false;
            for (const char* layerName : validationLayersNames)
            {
                if (not isPhysicalDeviceExtensionSupported(device, physicalDeviceMinimalRequirement.deviceExtensions, layerName))
                {
                    isExtensionPartOfAValidationLayer = true;
                    break;
                }
            }
            if (not isExtensionPartOfAValidationLayer)
            {
        #endif
                ss<<"isPhysicalDeviceExtensionSupported failed, which check if the required device extensions were supported by the device.";
                goto TESTS_FAILED;
        #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS 
            }
        #endif
        }

        if (not isSwapChainInfoMatchMinimalRequirement(getPhysicalDeviceSwapChainInfo(device, surface)))
        {
            ss<<"isSwapChainInfoMatchMinimalRequirement failed, which check if the swap chain info (from the device) math the minimal requirement";
            goto TESTS_FAILED;
        }

        return true;

    TESTS_FAILED:

        logWarningCB(ss.str().c_str());

        return false;
    }

    VkPhysicalDevice pickBestPhysicalDevice(const std::vector<VkPhysicalDevice>& devices, const VkSurfaceKHR surface, bool logInfo)
    {
        // hint : you could use mpz_cmp (const mpz_t op1, const mpz_t op2)
        //        to compare two mpz.
        
        // I use map to compute for me which device have the highest score.
        // (Note that i use map and not unordered_map).

        auto cmpLambda = [](const mpz_class& left, const mpz_class& right)
        {
            return left < right;
        };

        std::map<mpz_class, VkPhysicalDevice, decltype(cmpLambda)> rankedDevices(cmpLambda);

        for (auto device : devices)
        {
            ratePhysicalDevice(device, logInfo);

            if (isPhysicalDeviceMatchMinimalRequirements(device, surface)) // This func take care of VK_NULL_HANDLE for us.
                rankedDevices.insert({ratePhysicalDevice(device, false), device});
        }

        if ((rankedDevices.size() == 0) || disableGpuAutoSelectAndCallSelectPhysicalDeviceCallback)
        {
            logWarningCB("pickBestPhysicalDevice failed to find a compliant physical device that meet the required properties."
                         "OR the automatic physical device selection have been disabled.");
            VkPhysicalDevice selectedDevice = physicalDeviceSelectorCallback(devices);

            if (selectedDevice == VK_NULL_HANDLE)
            {
                logErrorCB("pickBestPhysicalDevice failed to find a compliant physical device that meet the required properties."
                           "AND the physicalDeviceSelector callback failed to choose a physical device.");
                throw std::runtime_error("VKI::pickPhysicalDevice failed to find a compliant physical device that meet the required properties.");
            }

            std::stringstream ss;
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(selectedDevice, &properties);
            ss<<"The manualy selected physical device is "<<properties.deviceName;

            logInfoCB(ss.str().c_str());

            return selectedDevice;
        }

        auto it     = rankedDevices.begin();
        auto itNext = rankedDevices.begin();

        while (itNext != rankedDevices.end())
        {
            it = itNext;
            itNext = rankedDevices.upper_bound(itNext->first);
        }

        if (logInfo)
        {
            std::stringstream ss;
            ss<<std::dec<<rankedDevices.size()<<" physical device(s) have been found.";
            logInfoCB(ss.str().c_str());


            size_t rank=0;
            for (auto device(rankedDevices.rbegin()) ; device!=rankedDevices.rend() ; device++)
            {
                ss.str(std::string());
                rank++;
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(device->second, &properties);
                ss<<"Device "<<properties.deviceName<<" is ranked : "<<rank<<"/"<<rankedDevices.size();

                char *score_str = mpz_get_str(nullptr, 10, device->first.get_mpz_t());
                ss<<", with a score of : "<<score_str;

                logInfoCB(ss.str().c_str());
            }

            ss.str(std::string());
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(it->second, &properties);

            ss<<"The most powerfull device found is "<<properties.deviceName;
            logInfoCB(ss.str().c_str());
        }

        return it->second;
    }

    std::vector<VkQueueFamilyProperties> listPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice device, bool logInfo)
    {
        uint32_t count=0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

        if (logInfo)
        {
            std::stringstream ss;
            ss<<std::dec<<count<<" queue(s) family have been found the device.";

            uint32_t index = 0;
            for (VkQueueFamilyProperties property : properties)
            {
                ss<<"\n\t";
                ss<<"Queue family index="<<index<<" have the properties :\n\t\t";
                ss<<"max queue instancible = "<<property.queueCount<<".\n\t\t";
                ss<<"supported operations  = "<<queueFlagBitsToString(property.queueFlags);

                index++;
            }

            logInfoCB(ss.str().c_str());
        }

        return properties;
    }

    std::vector<QueueInfo> getPhysicalQueueInfos(const VkPhysicalDevice physicalDevice, 
                                                 const VkSurfaceKHR     surface, 
                                                 bool logInfo
                                                )
    {
        std::vector<VkQueueFamilyProperties> properties = listPhysicalDeviceQueueFamilyProperties(physicalDevice, logInfo);

        std::vector<QueueInfo> infos;
        infos.reserve(properties.size());
        uint32_t indexCounter=0;
        for (VkQueueFamilyProperties property : properties)
        {
            QueueInfo info;
            info.familyIndex = indexCounter;
            info.count       = property.queueCount;
            info.operations  = property.queueFlags;
            info.isPresentable = false;

            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 isPresentable = VK_FALSE;
                VkResult error = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, 
                                                                      info.familyIndex, 
                                                                      surface, 
                                                                      &isPresentable
                                                                     );
                switch(error)
                {
                    case (VkResult::VK_SUCCESS):
                    {
                        info.isPresentable = true;
                        break;//
                    }
    #ifdef VK_KHR_surface
                    case (VkResult::VK_ERROR_SURFACE_LOST_KHR):
                    {
                        logErrorCB("getPhysicalQueueInfos lost surface access ?! No queue will be presentable.");
                        [[fallthrough]];
                    }
    #endif
                    default:
                    {
                        info.isPresentable = false;
                    }
                }
            }

            infos.push_back(info);
            indexCounter++;
        }

        return infos;
    }

    // TODO : this function is now useless ...
    VkQueueFlagBits getQueueMainOperation(const VkQueueFlags flags) noexcept
    {
        VkQueueFlagBits mainOperation;
        if      (flags & VK_QUEUE_GRAPHICS_BIT)
            mainOperation = VK_QUEUE_GRAPHICS_BIT;
        else if (flags & VK_QUEUE_COMPUTE_BIT)
            mainOperation = VK_QUEUE_COMPUTE_BIT;
    #ifdef VK_KHR_video_decode_queue
        else if (flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
            mainOperation = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
    #endif
    #ifdef VK_KHR_video_encode_queue
        else if (flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
            mainOperation = VK_QUEUE_VIDEO_ENCODE_BIT_KHR;
    #endif
    #ifdef VK_NV_optical_flow
        else if (flags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
            mainOperation = VK_QUEUE_OPTICAL_FLOW_BIT_NV;
    #endif
        else if (flags & VK_QUEUE_TRANSFER_BIT) // last since most other type imply transfer.
            mainOperation = VK_QUEUE_TRANSFER_BIT;
        else
            mainOperation = static_cast<VkQueueFlagBits>(0);

        return mainOperation;
    }

    constexpr bool isQueueFamilySuitable(const QueueInfo& physicalQueueFamily, const QueueInfo& requiredQueue)
    {
        return ((0 == (requiredQueue.operations ^ (physicalQueueFamily.operations & requiredQueue.operations)))
                && (physicalQueueFamily.isPresentable || !requiredQueue.isPresentable)
                && (physicalQueueFamily.count >= requiredQueue.count)
               );
    }

  /*constexpr*/ unsigned getDistanceBetweenQueueOperations(const VkQueueFlags opA, const VkQueueFlags opB)
    {
        // replace bitset by popcount if migrate to c++20.
        const std::bitset<64> opMaskA(opA), // 32 bits count sould be enough.
                              opMaskB(opB);
        const std::bitset<64> distanceMask = opMaskA ^ opMaskB;
        return distanceMask.count();
    }

#ifdef VKI_ENABLE_DEBUG_LOGS 
    // TODO give this function a better name.
    void deleteMeImADebuggingFunction(const std::map<QueueInfo,
                                                     std::list<std::pair<QueueInfo, uint32_t>>,
                                                     std::function<bool(const QueueInfo &a, const QueueInfo &b)>
                                                    > data)
    {
        std::stringstream ss;

        ss<<"ULTRA DEBUG :\n";
        ss<<"\t\t\tThere is "<<std::dec<<data.size()<<" required queue families\n";

        for (const std::pair<QueueInfo, std::list<std::pair<QueueInfo, uint32_t>>>& rqd_available : data)
        {
            ss<<"\t\t\tA required family, with "<<static_cast<uint32_t>(rqd_available.first.count)<<" instancible queues, "
                "and supporting operations :"<<queueFlagBitsToString(rqd_available.first.operations)<<"\n"
                "\t\t\tHave "<<rqd_available.second.size()<<" suitable families :\n";

            for (const std::pair<QueueInfo, uint32_t>& phyQueue_cost : rqd_available.second)
            {
                ss<<"\t\t\t\t|-> Family index="<<static_cast<uint32_t>(phyQueue_cost.first.familyIndex)
                  <<" with operations : "<<queueFlagBitsToString(phyQueue_cost.first.operations)
                  <<"and would cost "<<std::dec<<phyQueue_cost.second<<"\n";
            }
            ss<<"\n";
        }

        logInfoCB(ss.str().c_str());

    }
#endif//VKI_ENABLE_DEBUG_LOGS 

    // TODO : this function is more complex than excpected : write some tests !
    // TODO check this error from the validations layers :
    // Validation : Validation Error: [ VUID-VkDeviceCreateInfo-queueFamilyIndex-02802 ] Object 0: handle = 0x5c869eca7080, type = VK_OBJECT_TYPE_PHYSICAL_DEVICE; | MessageID = 0x29498778 | vkCreateDevice(): pCreateInfo->pQueueCreateInfos[2].queueFamilyIndex (1) is not unique and was also used in pCreateInfo->pQueueCreateInfos[1].
    // The Vulkan spec states: The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos , except that two members can share the same queueFamilyIndex if one describes protected-capable queues and one describes queues that are not protected-capable (https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#VUID-VkDeviceCreateInfo-queueFamilyIndex-02802)
    // |-> TODO remove family from suitableFamilies if already assigned (-> let user deal with queue collision).
    std::vector<QueueInfo> findQueueFamilyIndices(const VkPhysicalDevice        device, 
                                                  const std::vector<QueueInfo>& queueTemplate,
                                                  const VkSurfaceKHR            surface,
                                                  const bool                    logInfo
                                                 )
    {
        std::vector<QueueInfo> requiredQueues(queueTemplate);
        std::vector<QueueInfo> physicalQueues = getPhysicalQueueInfos(device, surface);

        std::function<bool(const QueueInfo &a, const QueueInfo &b)> predicat = [](const QueueInfo &a, const QueueInfo &b)
        {
            return a.operations < b.operations;
        };

        std::map<QueueInfo,                                 // required queue.
                 std::list<std::pair<QueueInfo, uint32_t>>, // a list of suitable families and the distance to it.
                 decltype(predicat)                         // (with 'distance' I meaning measure 'wasted potential').
                > suitableFamilies(predicat);

        // Initialize suitableFamilies.
        {
        uint32_t requiredQueueIndex=0;
        for (QueueInfo rqdQueue : requiredQueues)
        {
            // note : In suitableFamilies, the familyIndex of required queues (which have no meaning in this context)
            // represent their index in requiredQueues vector.
            rqdQueue.familyIndex=requiredQueueIndex;
            suitableFamilies.emplace(rqdQueue, std::list<std::pair<QueueInfo, uint32_t>>{});

            requiredQueueIndex++;
        }}

        // Fill suitableFamilies.
        for (const QueueInfo& rqdQueue: requiredQueues)
        {
            bool atLeastOneFound = false;
            for (const QueueInfo& phyQueue : physicalQueues)
            {
                if (isQueueFamilySuitable(phyQueue, rqdQueue))
                {
                    atLeastOneFound=true;
                    suitableFamilies.at(rqdQueue).push_back({
                            phyQueue, 
                            getDistanceBetweenQueueOperations(phyQueue.operations, rqdQueue.operations)
                            });
                }
            }

            if (not atLeastOneFound)
            {
                logErrorCB("findQueueFamilyIndices failed to find a suitable family for a queue.");
                throw std::runtime_error("findQueueFamilyIndices failed to find a suitable family for a queue.");
            }
        }

        // Assign rqdQueues to their best fitting family.
    
        std::vector<uint32_t> familyInstanceCount(physicalQueues.size(), 0); // How many instance have already be allocate to this family.
        while (!suitableFamilies.empty())
        {
            std::optional<QueueInfo> familyToDelete, requiredQueueToDelete;

    #ifdef VKI_ENABLE_DEBUG_LOGS 
            if (logInfo)
                deleteMeImADebuggingFunction(suitableFamilies);
    #endif//VKI_ENABLE_DEBUG_LOGS 

            // First let's assign requiredQueues that have only one family available.
            for (const std::pair<QueueInfo, std::list<std::pair<QueueInfo, uint32_t>>> rqd_suitable : suitableFamilies)
            {
                const QueueInfo& requiredQueue    = rqd_suitable.first;
                const auto&      suitableFamilies = rqd_suitable.second;

                if (suitableFamilies.size() == 1)// can't be null.
                {
                    const uint32_t   queueIndex = requiredQueue.familyIndex; // aka the index in requiredQueues (see at suitableFamilies initialisation).
                    const QueueInfo& familyInfo = suitableFamilies.front().first;

                    // Test if there is enough instances left.
                    if (requiredQueue.count+familyInstanceCount[familyInfo.familyIndex] <= familyInfo.count)
                    {
                        // Save the family info.
                        requiredQueues[queueIndex].familyIndex = familyInfo.familyIndex;
                        familyInstanceCount[familyInfo.familyIndex] += requiredQueue.count;

                        if (familyInstanceCount[familyInfo.familyIndex] == familyInfo.count)// hit max family capacity.
                            familyToDelete = familyInfo;
                        requiredQueueToDelete = requiredQueue;

                        break; // exit this for loop to fall below, update the suitableFamilies map and restart.
                    }
                    else
                    {
                        logErrorCB("findQueueFamilyIndices : A family has too few instancible queues -> lower the requirements.");
                        throw std::logic_error("VKI->Queues requirements can't be met by the physical device.");
                    }
                }
            }

            // Either all queue have been assigned or all remaining queues have at least two possible families.
            if (!suitableFamilies.empty()
                && !(familyToDelete || requiredQueueToDelete)
               )
            {
                // Removing the worst assignment. 
                std::tuple<QueueInfo, // required.
                           QueueInfo, // physical.
                           uint32_t   // cost|distance.
                          >worstLink = std::tie(suitableFamilies.begin()->first,
                                                suitableFamilies.begin()->second.front().first,
                                                suitableFamilies.begin()->second.front().second
                                               );
                for (const std::pair<QueueInfo, std::list<std::pair<QueueInfo, uint32_t>>> rqd_suitable : suitableFamilies)
                {
                    for (const std::pair<QueueInfo, uint32_t>& family_cost : rqd_suitable.second)
                    {
                        if (family_cost.second < std::get<2>(worstLink))
                            worstLink = std::tie(rqd_suitable.first, family_cost.first, family_cost.second);
                    }
                }

                // delete worst assignment possible to unstuck the loop.
                suitableFamilies.at(std::get<0>(worstLink)).remove({std::get<1>(worstLink), std::get<2>(worstLink)});
            }

            if (familyToDelete)
            {
                std::function<bool(const std::pair<VKI::QueueInfo, unsigned int>&)> removePredicat = 
                    [&,familyToDelete](const std::pair<VKI::QueueInfo, unsigned int>& family)
                    {
                        return family.first == familyToDelete.value();
                    };
                for (auto i(suitableFamilies.begin()) ; i!=suitableFamilies.end() ; i++)
                {
                    i->second.remove_if(removePredicat);
                }
            }
            if (requiredQueueToDelete)
            {
                suitableFamilies.erase(requiredQueueToDelete.value());
            }
        }

    #ifdef VKI_ENABLE_DEBUG_LOGS 
        if (logInfo)
            deleteMeImADebuggingFunction(suitableFamilies);
    #endif//VKI_ENABLE_DEBUG_LOGS 

        if (logInfo) // Log extra info about the final layout.
        {
            std::stringstream ss;
            ss<<"findQueueFamilyIndices final queue family layout :\n"
                "\t\t\t|-> "<<requiredQueues.size()<<" queues families are used.\n";
            size_t queueIndex=0;
            for (const QueueInfo& info : requiredQueues)
            {
                ss<<"\t\t\t|-> Queue index="<<queueIndex<<" is assigned to family index="<<info.familyIndex<<"\n";
                queueIndex++;
            }

            logInfoCB(ss.str().c_str());
        }

        return requiredQueues;
    }

    bool areRequiredFamilyQueueAvailable(const VkPhysicalDevice device, const VkSurfaceKHR surface)
    {
        bool success = true;
        try
        {
            std::vector<QueueInfo> queueInfos = findQueueFamilyIndices(
                                                    device,
                                                    physicalDeviceMinimalRequirement.queueInfos,
                                                    surface,
                                                    false
                                                );
        }
        catch(std::runtime_error &err)
        {
            success = false;
        }

        return success;
    }

    VkDeviceQueueCreateInfo populateDeviceQueueCreateInfo(const QueueInfo& info)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        /*
    #ifdef VK_VERSION_1_2 // TODO
        queueCreateInfo.flags = 0;// see : https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkDeviceQueueCreateInfo-flags-06449
                                  // tell that protected bit can be used if the familly suport it.
                                  // To set this bit, the device must support the feature
                                  // protectedMemory from the struct VkPhysicalDeviceVulkan12Properties.
                                  // Here is what could be done :

        //queueCreateInfo.flags = (VkPhysicalDeviceVulkan12Properties.protectedMemory) ?
        //                         info.operations & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;
    #else */
        queueCreateInfo.flags = 0;
    //#endif

        queueCreateInfo.queueFamilyIndex = info.familyIndex;
        queueCreateInfo.queueCount = info.count;
        queueCreateInfo.pQueuePriorities = info.priorities;

        return queueCreateInfo;
    }

    std::set<uint32_t> listQueueSharingTheSwapchain(const std::vector<QueueFamily>& families) noexcept
    {
        std::set<uint32_t> queueIndicesSharingTheSwapchain;
        for ( const QueueFamily & family : families)
        {
            if (family.info.isPresentable)
                queueIndicesSharingTheSwapchain.insert(family.info.familyIndex);
        }
        return queueIndicesSharingTheSwapchain;
    }

    std::string queueFlagBitsToString(VkQueueFlags flags) noexcept
    {
        std::stringstream ss;

        if (flags == 0)
            ss << "Null flag, ";

        if (flags & VK_QUEUE_GRAPHICS_BIT)
            ss << "graphics, ";
        if (flags & VK_QUEUE_TRANSFER_BIT)
            ss << "transfer, ";
        if (flags & VK_QUEUE_COMPUTE_BIT)
            ss << "compute, ";
        if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
            ss << "sparse binding, ";
    #ifdef VK_VERSION_1_1
        if (flags & VK_QUEUE_PROTECTED_BIT)
            ss << "protected bit";
    #endif
    #ifdef VK_KHR_video_decode_queue
        if (flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
            ss << "decode video, ";
    #endif
    #ifdef VK_KHR_video_encode_queue
        if (flags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
            ss << "encode video, ";
    #endif
    #ifdef VK_NV_optical_flow
        if (flags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
            ss << "nvidia optical flow,";
    #endif

        ss<<"("<<std::hex<<static_cast<uint32_t>(flags)<<").";

        std::string string = ss.str();
        return string;
    }

    void queueSubmit(const VkQueue queue, const std::vector<SubmitInfo>& submitInfos, const VkFence fence)
    {
        std::vector<std::vector<VkSemaphore>>           submitInfosWaitSemaphores(submitInfos.size());
        std::vector<std::vector<VkPipelineStageFlags>>  submitInfosStages(submitInfos.size());

        std::vector<VkSubmitInfo> vkSubmitInfos(submitInfos.size());
        for (size_t infoIndex(0) ; infoIndex<vkSubmitInfos.size() ; infoIndex++)
        {
            std::vector<VkSemaphore>          &waitSemaphores  = submitInfosWaitSemaphores[infoIndex];
            std::vector<VkPipelineStageFlags> &stages = submitInfosStages[infoIndex];
            waitSemaphores.resize(submitInfos[infoIndex].waitSemaphoresAtStages.size());
            stages.resize(        submitInfos[infoIndex].waitSemaphoresAtStages.size());

            for (size_t i(0) ; i<submitInfos[infoIndex].waitSemaphoresAtStages.size() ; i++)
            {
                waitSemaphores[i] = submitInfos[infoIndex].waitSemaphoresAtStages[i].first;
                stages[i]         = submitInfos[infoIndex].waitSemaphoresAtStages[i].second;
            }

            vkSubmitInfos[infoIndex].sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO; 
            vkSubmitInfos[infoIndex].pNext                = nullptr;
            vkSubmitInfos[infoIndex].waitSemaphoreCount   = waitSemaphores.size();
            vkSubmitInfos[infoIndex].pWaitSemaphores      = waitSemaphores.data();
            vkSubmitInfos[infoIndex].pWaitDstStageMask    = stages.data();
            vkSubmitInfos[infoIndex].commandBufferCount   = submitInfos[infoIndex].commandBuffers.size();
            vkSubmitInfos[infoIndex].pCommandBuffers      = submitInfos[infoIndex].commandBuffers.data();
            vkSubmitInfos[infoIndex].signalSemaphoreCount = submitInfos[infoIndex].signalSemaphores.size();
            vkSubmitInfos[infoIndex].pSignalSemaphores    = submitInfos[infoIndex].signalSemaphores.data();

        }

        VkResult error = vkQueueSubmit(queue, vkSubmitInfos.size(), vkSubmitInfos.data(), fence);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("submitToQueue successfully submited work to the queue.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("submitToQueue failed to submit work to the queue : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            case (VkResult::VK_ERROR_DEVICE_LOST):
            {
                logFatalErrorCB("submitToQueue failed to submit work to the queue : device lost.");
                throw std::runtime_error("GPU lost.");
            }
            default:
            {
                logFatalErrorCB("submitToQueue failed to submit work to the queue : unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void queuePresent(const VkQueue                     queue, 
                      const uint32_t                    imageIndex, 
                      const VkSwapchainKHR              swapchain,
                      const std::vector<VkSemaphore> &  semaphores,
                      SwapchainStatusFlags* const       pErrorFlags,
                      const SwapchainStatusFlags        supportedErrorFlags
                     )
    {
        VkResult swapchainError;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext               = nullptr;
        presentInfo.waitSemaphoreCount  = semaphores.size();
        presentInfo.pWaitSemaphores     = semaphores.data();
        presentInfo.swapchainCount      = 1;
        presentInfo.pSwapchains         = &swapchain;
        presentInfo.pImageIndices       = &imageIndex;
        presentInfo.pResults            = &swapchainError;

        VkResult error = vkQueuePresentKHR(queue, &presentInfo);

        switch(swapchainError)
        {
            case (VK_SUCCESS):
            {
                logInfoCB("queuePresent successfully presented a image to the swapchain.");
                break;
            }
            case (VK_SUBOPTIMAL_KHR):
            {
                logWarningCB("queuePresent successfully presented a image to the swapchain but the swapchain is subOptimal.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_SUBOPTIMAL_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_SUBOPTIMAL_BIT;
                break;
            }
            case (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT):
            {
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_FULL_SCREEN_EXCLUSIVE_MODE_LOST_INDEX) && pErrorFlags!=nullptr)
                {
                    logErrorCB("queuePresent failed to present a image to the swapchain : fullscreen exclusive mode lost.");
                    *pErrorFlags |= SWAPCHAIN_STATUS_FULL_SCREEN_EXCLUSIVE_MODE_LOST_BIT;
                }
                else
                {
                    logFatalErrorCB("queuePresent failed to present a image to the swapchain : fullscreen exclusive mode lost.");
                    throw std::runtime_error("Program doesn't survive the lost of full screen exclusive mode.");
                }
                break;
            }
            case (VK_ERROR_OUT_OF_DATE_KHR):
            {
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_OUT_OF_DATE_INDEX) && pErrorFlags!=nullptr)
                {
                    logErrorCB("queuePresent failed to present a image to the swapchain : out of date.");
                    *pErrorFlags |= SWAPCHAIN_STATUS_OUT_OF_DATE_BIT;
                }
                else
                {
                    logFatalErrorCB("queuePresent failed to present a image to the swapchain : out of date.");
                    throw std::runtime_error("out of date.");
                }
                break;
            }
            case (VK_ERROR_SURFACE_LOST_KHR):
            {
                logFatalErrorCB("queuePresent failed to present a image to the swapchain : surface lost.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_SURFACE_LOST_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_SURFACE_LOST_BIT;
                else
                    throw std::runtime_error("surface lost.");
                break;
            }
            case (VK_ERROR_DEVICE_LOST):
            {
                logFatalErrorCB("queuePresent failed to present a image to the swapchain : device lost.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_DEVICE_LOST_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_DEVICE_LOST_BIT;
                else
                    throw std::runtime_error("GPU lost.");
                break;
            }
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    bool waitQueueBecomeIdle(const VkQueue queue) noexcept
    {
        logInfoCB("Start waiting for the queue to be idle.");
        VkResult error = vkQueueWaitIdle(queue);
        switch (error)
        {
            case (VkResult::VK_SUCCESS):
                logInfoCB("waitQueueBecomeIdle successfully returned.");
                return true;
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                logErrorCB("waitQueueBecomeIdle falied : out of memory.");
                return false;
            case (VkResult::VK_ERROR_DEVICE_LOST):
                logErrorCB("waitQueueBecomeIdle failed : device lost -> this might be an use after free !");
                return false;
            default:
                logFatalErrorCB("waitQueueBecomeIdle failed : program out dated.");
                return false;
        }
    }

    /* Not supported.
    void queuePresent(const VkQueue queue, 
                      const std::vector<std::pair<VkSwapchainKHR, uint32_t>> &swapchainImagesIndices, 
                      const std::vector<VkSemaphore> &semaphores,
                      bool *pSuboptimal,
                      bool *pSurfaceLost,
                      bool *pOutOfDate,
                      bool *pDeviceLost,
                      bool *pFullScreenExclusiveModeLost
                     )
    {
        std::vector<uint32_t>       imgIndices(swapchainImagesIndices.size());
        std::vector<VkSwapchainKHR> swapchains (swapchainImagesIndices.size());

        for (size_t i(0) ; i<imgIndices.size() ; i++)
        {
            swapchains [i] = swapchainImagesIndices[i].first;
            imgIndices[i] = swapchainImagesIndices[i].second;
        }

        std::vector<VkResult> swapchainResults(swapchains.size());

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext               = nullptr;
        presentInfo.waitSemaphoreCount  = semaphores.size();
        presentInfo.pWaitSemaphores     = semaphores.data();
        presentInfo.swapchainCount      = swapchains.size();
        presentInfo.pSwapchains         = swapchains.data();
        presentInfo.pImageIndices       = imgIndices.data();
        presentInfo.pResults            = swapchainResults.data();

        VkResult error = vkQueuePresentKHR(queue, &presentInfo);

        bool suboptimal=false, surfaceLost=false, outOfDate=false, deviceLost=false, fullScreenModeLost=false;
        switch(error)
        {
            case (VK_SUCCESS):
            {
                logInfoCB("queuePresent successfully presented all images to the swapchains.");
                break;
            }
            case (VK_SUBOPTIMAL_KHR):
            {
                logInfoCB("queuePresent successfully presented all images to the swapchains but the swapchain is suboptimal.");
                suboptimal=true;
                break;
            }
            case (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : fullscreen exclusive mode lost.");
                fullScreenModeLost=true;
                if (pFullScreenExclusiveModeLost == nullptr)
                    throw std::runtime_error("Program doesn't survive the lost of full screen exclusive mode.");
                break;
            }
            case (VK_ERROR_OUT_OF_DATE_KHR):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : out of date.");
                outOfDate=true;
                if (pOutOfDate == nullptr)
                    throw std::runtime_error("Surface became out of date.");
                break;
            }
            case (VK_ERROR_SURFACE_LOST_KHR):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : device lost.");
                surfaceLost=true;
                if (pSurfaceLost == nullptr)
                    throw std::runtime_error("Surface lost.");
                break;
            }
            case (VK_ERROR_DEVICE_LOST):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : device lost.");
                deviceLost=true;
                if (pDeviceLost==nullptr)
                    throw std::runtime_error("GPU lost.");
                break;
            }
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("queuePresent failed to present all images to the swapchains : unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }

        if (pSuboptimal != nullptr)
            *pSuboptimal=suboptimal;
        if (pSurfaceLost != nullptr)
            *pSurfaceLost=surfaceLost;
        if (pOutOfDate != nullptr)
            *pOutOfDate=outOfDate;
        if (pDeviceLost != nullptr)
            *pDeviceLost=deviceLost;
        if (pFullScreenExclusiveModeLost != nullptr)
            *pFullScreenExclusiveModeLost=fullScreenModeLost;

    }
    */

    VkDevice createLogicalDevice(
            const VkPhysicalDevice physicalDevice, 
            const std::vector<QueueInfo> queueInfos,
            const std::vector<const char*> &deviceExtensionsNames
            )
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        if (not isPhysicalDeviceExtensionSupported(physicalDevice, deviceExtensionsNames))
            logWarningCB("CreateLogicDevice : a device extension is missing but won't stop the program since it doesn't check for layer specific device extensions.");
        else
            logVerboseCB("CreateLogicDevice : all device extension have been found.");

        for (QueueInfo queueInfo : queueInfos)
        {
            std::stringstream ss;
            ss<<"createLogicalDevice add a queue family with those capabilities : "<<queueFlagBitsToString(queueInfo.operations);
            ss<<" at index="<<queueInfo.familyIndex<<".";
            logInfoCB(ss.str().c_str());

            VkDeviceQueueCreateInfo queueCreateInfo = populateDeviceQueueCreateInfo(queueInfo);

            queueCreateInfos.push_back(queueCreateInfo);
        }
        queueCreateInfos.shrink_to_fit(); // kinda useless.

        VkDeviceCreateInfo logicDeviceCreateInfo{};
        logicDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        logicDeviceCreateInfo.pNext = nullptr;
        logicDeviceCreateInfo.flags = 0; // reserved by the spec.
        logicDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        logicDeviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
        logicDeviceCreateInfo.pEnabledFeatures = &physicalDeviceMinimalRequirement.features;

        // this might break backward compatibility with older implementation of vulkan
        // (logicDeviceCreateInfo required layername+layerCount back in the days but are deprecated today)
        logicDeviceCreateInfo.enabledLayerCount = 0;
        logicDeviceCreateInfo.ppEnabledLayerNames = nullptr;

        logicDeviceCreateInfo.enabledExtensionCount = deviceExtensionsNames.size();
        logicDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensionsNames.data();

        VkDevice logicalDevice;
        lastApiError = vkCreateDevice(physicalDevice, &logicDeviceCreateInfo, nullptr, &logicalDevice);

        if (lastApiError == VkResult::VK_SUCCESS)
        {
            logInfoCB("createLogicalDevice successfully created a new logic device");
        }
        else if (lastApiError == VkResult::VK_ERROR_INITIALIZATION_FAILED)
        {
            logFatalErrorCB("createLogicalDevice failed to initialize a VkDevice "
                            "(you should check that your vulkan driver is up to date)");
            throw std::runtime_error("VKI::createLogicalDevice failed to initialize a VkDevice "
                                     "(you should check that your vulkan driver is up to date)");
        }
        else if (lastApiError == VkResult::VK_ERROR_EXTENSION_NOT_PRESENT)
        {
            logFatalErrorCB("createLogicalDevice failed to create a VkDevice : "
                            "a logic device specific extensions is missing.");
            throw std::runtime_error("VKI::createLogicalDevice failed to create a VkDevice : "
                                     "a logic device specific extensions is missing.");
        }
        else if (lastApiError == VkResult::VK_ERROR_FEATURE_NOT_PRESENT)
        {
            logFatalErrorCB("createLogicalDevice failed to create a VkDevice : "
                            "a device feature is missing.");
            throw std::runtime_error("VKI::createLogicalDevice failed to create a VkDevice : "
                                     "a device feature is missing.");
        }
        else if (lastApiError == VkResult::VK_ERROR_TOO_MANY_OBJECTS)
        {
            logFatalErrorCB("createLogicalDevice failed to create a VkDevice : "
                            "apparently too many objects (logic device) have already been created.");
            throw std::runtime_error("VKI::createLogicalDevice failed to create a VkDevice : "
                                     "apparently too many objects (logic device) have already been created.");
        }
        else if (lastApiError == VkResult::VK_ERROR_DEVICE_LOST)
        {
            logFatalErrorCB("createLogicalDevice failed to create a VkDevice : "
                            "physical device lost ... did you just remove your GPU "
                            "while i was initializing :O ?!");
            throw std::runtime_error("VKI::createLogicalDevice failed to create a VkDevice : "
                                     "physical device lost ... did you just remove your GPU "
                                     "while i was initializing :O ?!");
        }
        else
        {
            logFatalErrorCB("createLogicalDevice failed to create a logic device -> out of memory");
            throw std::runtime_error("createLogicalDevice failed to create a logic device -> out of memory");
        }

        return logicalDevice;
    }

    SwapChainInfo getPhysicalDeviceSwapChainInfo(const VkPhysicalDevice device, const VkSurfaceKHR surface, bool logInfo)
    {
        SwapChainInfo sci;

        // Capabilities :

        VkResult apiError = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &sci.capabilities);
        if      (apiError == VkResult::VK_SUCCESS)
        {
            if (logInfo)
            {
                std::stringstream ss;
                ss<<"swap chain capabilities :";
                ss<<"\n\tminImageCount = "<<sci.capabilities.minImageCount;
                ss<<"\n\tmaxImageCount = "<<sci.capabilities.maxImageCount;
                ss<<"\n\tcurrentExtent.width = "<<sci.capabilities.currentExtent.width;
                ss<<"\n\tcurrentExtent.height = "<<sci.capabilities.currentExtent.height;
                ss<<"\n\tminImageExtent.width = "<<sci.capabilities.minImageExtent.width;
                ss<<"\n\tminImageExtent.height = "<<sci.capabilities.minImageExtent.height;
                ss<<"\n\tmaxImageExtent.width = "<<sci.capabilities.maxImageExtent.width;
                ss<<"\n\tmaxImageExtent.height = "<<sci.capabilities.maxImageExtent.height;
                ss<<"\n\tmaxImageArrayLayers = "<<sci.capabilities.maxImageArrayLayers;
                ss<<"\n\tsupportedTransforms = "<<std::hex<<sci.capabilities.supportedTransforms;
                ss<<"\n\tcurrentTransform = "<<std::hex<<sci.capabilities.currentTransform;
                ss<<"\n\tsupportedCompositeAlpha = "<<std::hex<<sci.capabilities.supportedCompositeAlpha;
                ss<<"\n\tsupportedUsageFlags = "<<std::hex<<sci.capabilities.supportedUsageFlags;

                logInfoCB(ss.str().c_str());
            }
        }
        else if (apiError == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo out of memory !");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo out of memory !");
        } 

        // Formats :

        uint32_t formatCount=0;
        apiError = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if      (apiError == VkResult::VK_SUCCESS)
        {
            if (logInfo)
            {
                std::stringstream ss;
                ss<<std::dec<<formatCount<<" formats are suported by the device swapchain.";
                logInfoCB(ss.str().c_str());
            }
        }
        else if (apiError == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo out of memory !");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo out of memory !");
        }// can't be VK_INCOMPLETE;
        else if (formatCount == 0)
        {
            logErrorCB("No format are surported by this swapchain.");
            sci.formats.clear();
            goto SKIP_FORMATS_FETCH;
        }

        sci.formats.clear();
        sci.formats.resize(formatCount);
        apiError = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, sci.formats.data());
        if      (apiError == VkResult::VK_SUCCESS)
        {
            if (logInfo)
            {
            LOGSWAPCHAINFORMATS:
                std::stringstream ss;
                ss<<"list of supported formats :";
                for (VkSurfaceFormatKHR format : sci.formats)
                {
                    ss<<"\n\tFormat = "<<format.format<<", colorSpace = "<<format.colorSpace;
                }
                logInfoCB(ss.str().c_str());
            }
        }
        else if (apiError == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
        }
        else if (apiError == VkResult::VK_INCOMPLETE)
        {
            logWarningCB("getPhysicalDeviceSwapChainInfo received a VK_INCOMPLETE while fetching the swap chain format.");
            goto LOGSWAPCHAINFORMATS;
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo out of memory !");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo out of memory !");
        }

    SKIP_FORMATS_FETCH:

        // Present modes :

        uint32_t presentModesCount = 0;
        apiError = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, nullptr);
        if      (apiError == VkResult::VK_SUCCESS)
        {
            if (logInfo)
            {
                std::stringstream ss;
                ss<<std::dec<<presentModesCount<<" presentation modes are suported by the device swapchain.";
                logInfoCB(ss.str().c_str());
            }
        }
        else if (apiError == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo out of memory !");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo out of memory !");
        }// can't be VK_INCOMPLETE;
        else if (formatCount == 0)
        {
            logErrorCB("No format are surported by this swapchain.");
            sci.presentModes.clear();
            goto SKIP_PRESENT_MODES_FETCH;
        }

        sci.presentModes.clear();
        sci.presentModes.resize(presentModesCount);
        apiError = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, sci.presentModes.data());
        if      (apiError == VkResult::VK_SUCCESS)
        {
            if (logInfo)
            {
            LOGSWAPCHAINPRESENTMODES:
                std::stringstream ss;
                ss<<"list of supported present modes :";
                for (VkPresentModeKHR presentMode : sci.presentModes)
                {
                    ss<<"\n\tPresent mode = ";
                    switch (presentMode)
                    {
                        case (VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR):
                            {
                                ss<<"VK_PRESENT_MODE_IMMEDIATE_KHR";
                                break;
                            }
                        case (VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR):
                            {
                                ss<<"VK_PRESENT_MODE_MAILBOX_KHR";
                                break;
                            }
                        case (VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR):
                            {
                                ss<<"VK_PRESENT_MODE_FIFO_KHR";
                                break;
                            }
                        case (VkPresentModeKHR::VK_PRESENT_MODE_FIFO_RELAXED_KHR):
                            {
                                ss<<"VK_PRESENT_MODE_FIFO_RELAXED_KHR";
                                break;
                            }
                        default:
                            {
                                ss<<"UNKOWN PRESENT MODE";
                            }
                    }
                }
                logInfoCB(ss.str().c_str());
            }
        }
        else if (apiError == VkResult::VK_ERROR_SURFACE_LOST_KHR)
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo receive a VK_ERROR_SURFACE_LOST.");
        }
        else if (apiError == VkResult::VK_INCOMPLETE)
        {
            logWarningCB("getPhysicalDeviceSwapChainInfo received a VK_INCOMPLETE while fetching the swap chain presents modes.");
            goto LOGSWAPCHAINPRESENTMODES;
        }
        else if ((apiError == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (apiError == VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY))
        {
            logFatalErrorCB("getPhysicalDeviceSwapChainInfo out of memory !");
            throw std::runtime_error("VKI::getPhysicalDeviceSwapChainInfo out of memory !");
        }

    SKIP_PRESENT_MODES_FETCH:

        return sci;
    }

    /**@brief compare two flags, return true if the lf is less than rf.
     */
    bool cmpVkSurfaceTransformFlagsKHR(const VkSurfaceTransformFlagsKHR lf, const VkSurfaceTransformFlagsKHR rf)
    {
        /* simple explanation :
         * for each bit value:
         *  if ((lf not require operation) || (rf support operation))
         */

        bool isLF_lessThan_RF = true;

        //isLF_lessThan_RF &= ((lf & $$$) == 0) || (rf & $$$);
        //isLF_lessThan_RF &= (((lf & $$$) == (rf & $$$)) || ((lf & $$$) == 0));
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR) == 0) || (rf & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR);

        return isLF_lessThan_RF;
    }

    /**@brief compare two flags, return true if the lf is less than rf.
     */
    bool cmpVkCompositeAlphaFlagsKHR(const VkCompositeAlphaFlagsKHR lf, const VkCompositeAlphaFlagsKHR rf)
    {
        /* simple explanation :
         * for each bit value:
         *  if ((lf not require operation) || (rf support operation))
         */

        bool isLF_lessThan_RF = true;

        //isLF_lessThan_RF &= ((lf & $$$) == 0) || (rf & $$$);
		isLF_lessThan_RF &= ((lf & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) || (rf & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) == 0) || (rf & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) == 0) || (rf & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR);
		isLF_lessThan_RF &= ((lf & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) == 0) || (rf & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR);

        return isLF_lessThan_RF;
    }

    /**@brief compare two flags, return true if the lf is less than rf.
     */
    bool cmpVkImageUsageFlags(const VkImageUsageFlags lf, const VkImageUsageFlags rf)
    {
        /* simple explanation :
         * for each bit value:
         *  if ((lf not require operation) || (rf support operation))
         */

        bool isLF_lessThan_RF = true;

        //isLF_lessThan_RF &= ((lf & $$$) == 0) || (rf & $$$);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) || (rf & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) || (rf & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) || (rf & VK_IMAGE_USAGE_SAMPLED_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_STORAGE_BIT) == 0) || (rf & VK_IMAGE_USAGE_STORAGE_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) || (rf & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) || (rf & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) == 0) || (rf & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
        isLF_lessThan_RF &= ((lf & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) == 0) || (rf & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        //VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT sound really cool.

        return isLF_lessThan_RF;
    }

    bool isSwapChainInfoMatchMinimalRequirement(const SwapChainInfo& sci)
    {
        const SwapChainInfo& rqdSci = VKI::physicalDeviceMinimalRequirement.swapChainInfo;
        
        // capabilities :
        if (not intersectMinMaxImageCount(sci, rqdSci, nullptr))
        {
            logWarningCB("isSwapchainInfoMatchMinimalRequirment the device min/max image count doesn't intersect with the required min/max images count.");
            goto TEST_FAILED;
        }
        /* // Old method : less permisive.
        if (sci.capabilities.minImageCount > rqdSci.capabilities.minImageCount)
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement minImageCount is less than the device minImageCount.");
            goto TEST_FAILED;
        }
        else if ((sci.capabilities.maxImageCount < rqdSci.capabilities.maxImageCount) && (rqdSci.capabilities.maxImageCount!=0))
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement maxImageCount is larger than the device maxImageCount.");
            goto TEST_FAILED;
        }
        */

        // Extent2D aren't relevant here since it's device defined.

        else if (sci.capabilities.maxImageArrayLayers < rqdSci.capabilities.maxImageArrayLayers)
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement maxImageArrayLayers is smaller than the device maxImageArrayLayers.");
            goto TEST_FAILED;
        }
        else if (not cmpVkSurfaceTransformFlagsKHR(rqdSci.capabilities.supportedTransforms, sci.capabilities.supportedTransforms))
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement the required surface transform flags aren't meet by the device.");
            goto TEST_FAILED;
        }
        // currentTransform isn't comparable.
        else if (not cmpVkCompositeAlphaFlagsKHR(rqdSci.capabilities.supportedCompositeAlpha, sci.capabilities.supportedCompositeAlpha))
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement the required composite aplha flags aren't meet by the device.");
            goto TEST_FAILED;
        }
        else if (not cmpVkImageUsageFlags(rqdSci.capabilities.supportedUsageFlags, sci.capabilities.supportedUsageFlags))
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement the required surface capabilities usages flags aren't meet by the device.");
            goto TEST_FAILED;
        }

        // format :
        // Only one format is necessary : rqdSci.formats are ordered in order of preferences.
        // So we pick the first from the rqdSci.formats.
        bool isAtLeastOneFormatIsFound;
        isAtLeastOneFormatIsFound = false;

        for (VkSurfaceFormatKHR rqdFormat : rqdSci.formats)
        {
            //isAtLeastOneFormatIsFound &= sci.formats.find(rqdFormat) != sci.formats.end();
            for (VkSurfaceFormatKHR sciFormat : sci.formats)
            {
                bool isFormatEq     = (rqdFormat.format == sciFormat.format);
                bool isColorSpaceEq = (rqdFormat.colorSpace == sciFormat.colorSpace);
                isAtLeastOneFormatIsFound |= (isFormatEq && isColorSpaceEq);
            }
            if (isAtLeastOneFormatIsFound)
                break;
        }
        if (not isAtLeastOneFormatIsFound)
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement none of the required formats were found for the device");
            if (rqdSci.formats.empty())
                logFatalErrorCB("isSwapChainInfoMatchMinimalRequirement : A field is missing -> swapchainInfo.formats.empty() == true;");
            goto TEST_FAILED;
        }

        // present mode :
        // Only one mode is necessary : rqdSci.presentModes are ordered in order of preferences.
        // So we pick the first from the rqdSci.presentModes.
        bool isAtLeastOnePresentModeIsFount;
        isAtLeastOnePresentModeIsFount = false;

        for (VkPresentModeKHR rqdPMode : rqdSci.presentModes)
        {
            for (VkPresentModeKHR  sciPMode : sci.presentModes)
            {
                isAtLeastOnePresentModeIsFount |= (rqdPMode == sciPMode);
            }
            if (isAtLeastOnePresentModeIsFount)
                break;
        }
        if (not isAtLeastOnePresentModeIsFount)
        {
            logWarningCB("isSwapChainInfoMatchMinimalRequirement none of the required formats were found for the device");
            if (rqdSci.presentModes.empty())
                logFatalErrorCB("isSwapChainInfoMatchMinimalRequirement : A field is missing -> swapchainInfo.presentModes.empty() == true;");
            goto TEST_FAILED;
        }

        return true;
    TEST_FAILED:
        // may add a logVerbose/Info ?
        return false;
    }


    /**@brief Wait until the device became idle i.e. no more task are running on the device.
     * @return true if the function doesn't abort before the device became idle.
     *              The errors that can cause this function to fail will make crash the whole program.
     **/
    bool waitDeviceBecomeIdle(const VkDevice device) noexcept
    {
        logInfoCB("Start waiting for the device to be idle.");
        VkResult error = vkDeviceWaitIdle(device);
        switch (error)
        {
            case (VkResult::VK_SUCCESS):
                logInfoCB("waitDeviceBecomeIdle successfully returned.");
                return true;
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                logErrorCB("waitDeviceBecomeIdle falied : out of memory.");
                return false;
            case (VkResult::VK_ERROR_DEVICE_LOST):
                logErrorCB("waitDeviceBecomeIdle failed : device lost -> this might be an use after free !");
                return false;
            default:
                logFatalErrorCB("waitDeviceBecomeIdle failed : program out dated.");
                return false;
        }
    }

    // clamp the currentExtent to the framebuffer of the windows.
    void updateSwapchainExtents(const VkPhysicalDevice phyDevice, 
                                const VkSurfaceKHR surface, 
                                const WindowContext &wContext,
                                SwapChainInfo& swapchainInfo
                               )
    {
        SwapChainInfo deviceSwapchainInfo = getPhysicalDeviceSwapChainInfo(phyDevice, surface, false);

        VkExtent2D windowExtent = getWindowFramebufferSize(wContext);
        VkExtent2D max          = deviceSwapchainInfo.capabilities.maxImageExtent;
        VkExtent2D min          = deviceSwapchainInfo.capabilities.minImageExtent;

        swapchainInfo.capabilities.currentExtent.width = std::clamp(windowExtent.width , min.width , max.width );
        swapchainInfo.capabilities.currentExtent.height= std::clamp(windowExtent.height, min.height, max.height);

        swapchainInfo.capabilities.maxImageExtent = max;
        swapchainInfo.capabilities.minImageExtent = min;
    }

    bool operator==(VkSurfaceFormatKHR lf, VkSurfaceFormatKHR rf)
    {
        return ((lf.format == rf.format) && (lf.colorSpace == rf.colorSpace));
    }

    void discardUnsuportedFormatsAndPresentModes(SwapChainInfo &swapchainInfo, 
                                                 const VkPhysicalDevice phDevice, 
                                                 const VkSurfaceKHR surface
                                                )
    {
        SwapChainInfo deviceSwapChainInfo = getPhysicalDeviceSwapChainInfo(phDevice, surface, false);
        
        std::vector<VkSurfaceFormatKHR> supportedFormats;
        supportedFormats.reserve(swapchainInfo.formats.size());

        for (VkSurfaceFormatKHR targetFormat : swapchainInfo.formats)
        {
            for (VkSurfaceFormatKHR deviceFormat : deviceSwapChainInfo.formats)
            {
                if (targetFormat == deviceFormat)
                    supportedFormats.push_back(targetFormat);
            }
        }
        if (supportedFormats.size() == 0)
            throw std::logic_error("MAJOR LOGIC ERROR FAILURE : An impossible situation happended : when adapting "
                                   "the swap chain to a device. It appear that the device don't suport any of the "
                                   "format required by the dev/user swapchain. Which mean that the device should "
                                   "have been discarded by isPhysicalDeviceMatchMinimalRequirement "
                                   "(More precisely isSwapChainInfoMatchMinimalRequirement)."
                                  );
        swapchainInfo.formats.clear();
        swapchainInfo.formats.assign(supportedFormats.begin(), supportedFormats.end());
        swapchainInfo.formats.shrink_to_fit();

        std::vector<VkPresentModeKHR> supportedPresentModes;
        supportedPresentModes.reserve(swapchainInfo.presentModes.size());

        for (VkPresentModeKHR targetMode : swapchainInfo.presentModes)
        {
            for (VkPresentModeKHR deviceMode : deviceSwapChainInfo.presentModes)
            {
                if (targetMode == deviceMode)
                    supportedPresentModes.push_back(targetMode);
            }
        }
        if (supportedPresentModes.size() == 0)
            throw std::logic_error("MAJOR LOGIC ERROR FAILURE : An impossible situation happended : when adapting "
                                   "the swap chain to a device. It appear that the device don't suport any of the "
                                   "present mode required by the dev/user swapchain. Which mean that the device should "
                                   "have been discarded by isPhysicalDeviceMatchMinimalRequirement. "
                                   "(More precisely isSwapChainInfoMatchMinimalRequirement)."
                                  );
        swapchainInfo.presentModes.clear();
        swapchainInfo.presentModes.assign(supportedPresentModes.begin(), supportedPresentModes.end());
        swapchainInfo.presentModes.shrink_to_fit();
    }

    //VkSwapchainKHR createSwapChain(const VulkanContext& context, VkSwapchainKHR oldSwapchain)
    VkSwapchainKHR createSwapChain(const VkDevice device, 
                                   const SwapChainInfo &swapchainInfo,
                                   const VkSurfaceKHR surface,
                                   VkSwapchainKHR oldSwapchain
                                  )
    {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        if (swapchainInfo.capabilities.maxImageArrayLayers != 1)
        {
            logWarningCB("swapchain creation with maxImageArrayLayers != 1, value ignored.");
        }

        std::vector<uint32_t> queueFamilyIndicesSharingTheSwapChain(
                                    swapchainInfo.queueFamilyIndicesSharingTheSwapChain.begin(),
                                    swapchainInfo.queueFamilyIndicesSharingTheSwapChain.end()
                                    );
                              
        { // log shared queue families:
            std::stringstream ss;
            ss<<queueFamilyIndicesSharingTheSwapChain.size()<<" Queue families are sharing the swapchain.";
            logInfoCB(ss.str().c_str());

            ss.str(std::string());
            ss<<"Queue family indices that share the swapchain : ";
            for (uint32_t i : queueFamilyIndicesSharingTheSwapChain)
                ss<<i<<", ";
            logInfoCB(ss.str().c_str());
        
        // log present mode:
            ss.str(std::string());

            ss<<"swapchain will use the present mode : ";
            switch(swapchainInfo.presentModes.front())
            {
                case (VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR):
                {
                    ss<<"IMMEDIATE.";
                    break;
                }
                case (VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR):
                {
                    ss<<"MAILBOX.";
                    break;
                }
                case (VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR):
                {
                    ss<<"FIFO.";
                    break;
                }
                case (VkPresentModeKHR::VK_PRESENT_MODE_FIFO_RELAXED_KHR):
                {
                    ss<<"FIFO_RELAXED.";
                    break;
                }
    #ifdef VK_KHR_shared_presentable_image
                case (VkPresentModeKHR::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR):
                {
                    ss<<"SHARED_DEMAND_REFRESH.";
                    break;
                }
                case (VkPresentModeKHR::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR):
                {
                    ss<<"SHARED_CONTINUOUS_REFRESH.";
                    break;
                }
    #endif
    #ifdef VK_EXT_present_mode_fifo_latest_ready
                case (VkPresentModeKHR::VK_PRESENT_MODE_FIFO_LATEST_READY_EXT):
                {
                    ss<<"FIFO_LATEST_READY.";
                    break;
                }
    #endif
                default:
                {
                    ss<<"UNKOWN !";
                    break;
                }
            }
            logInfoCB(ss.str().c_str());
        }

        clearNullMaxImageCount(physicalDeviceMinimalRequirement.swapChainInfo);
        std::pair<uint32_t, uint32_t> imageCountMinMax;
        intersectMinMaxImageCount(swapchainInfo, physicalDeviceMinimalRequirement.swapChainInfo, &imageCountMinMax);

        uint32_t requestedImageCount = imageCountMinMax.second; // Create as many image as allowed.

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.pNext                    = nullptr;
        createInfo.flags                    = 0;
        createInfo.surface                  = surface;
        //createInfo.minImageCount          = swapchainInfo.capabilities.maxImageCount;
        createInfo.minImageCount            = requestedImageCount;
        createInfo.imageFormat              = swapchainInfo.formats.at(0).format;
        createInfo.imageColorSpace          = swapchainInfo.formats.at(0).colorSpace;
        createInfo.imageExtent              = swapchainInfo.capabilities.currentExtent;
        createInfo.imageArrayLayers         = 1;
        createInfo.imageUsage               = swapchainInfo.imageUsage;
        createInfo.imageSharingMode         = swapchainInfo.sharingMode;
        createInfo.queueFamilyIndexCount    = queueFamilyIndicesSharingTheSwapChain.size();
        createInfo.pQueueFamilyIndices      = queueFamilyIndicesSharingTheSwapChain.data();
        createInfo.preTransform             = swapchainInfo.capabilities.currentTransform;
        createInfo.compositeAlpha           = swapchainInfo.currentCompositeAlpha;
        createInfo.presentMode              = swapchainInfo.presentModes.at(0);
        createInfo.clipped                  = swapchainInfo.enableClipping;
        createInfo.oldSwapchain             = oldSwapchain;

        VkResult error = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
        if      (error == VkResult::VK_SUCCESS)
            logInfoCB("createSwapChain successfully created a new swapchain.");
        else 
        {
            std::stringstream ss( "createSwapChain failed to create a new swapchain. Error received : " );
            switch (error)
            {
                case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                    ss<<"ERROR_OUT_OF_MEMORY.";
                    break;
                case (VkResult::VK_ERROR_DEVICE_LOST):
                    ss<<"ERROR_DEVICE_LOST.";
                    break;
                case (VkResult::VK_ERROR_SURFACE_LOST_KHR):
                    ss<<"ERROR_SURFACE_LOST.";
                    break;
                case (VkResult::VK_ERROR_NATIVE_WINDOW_IN_USE_KHR):
                    ss<<"ERROR_NATIVE_WINDOW_IN_USE.";
                    break;
                case (VkResult::VK_ERROR_INITIALIZATION_FAILED):
                    ss<<"ERROR_INITIALIZATION_FAILED.";
                    break;
                case (VkResult::VK_ERROR_COMPRESSION_EXHAUSTED_EXT): // Provided by VK_EXT_image_compression_control.
                    ss<<"ERROR_COMPRESSION_EXHAUSTED_EXT.";
                    break;
                default:
                {
                    ss<<"UNKOWN_ERROR";
                }  
            }
            logFatalErrorCB(ss.str().c_str());
            throw std::runtime_error(ss.str());
        }
        
        return swapchain;
    }

    // Retreive VkImages from the swapchain.
    std::vector<VkImage> fetchImagesFromSwapChain(const VkDevice &device, const VkSwapchainKHR &swapchain)
    {
        uint32_t imagesCount=0;

        VkResult error = vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
        if (error == VkResult::VK_SUCCESS)
            logInfoCB("fetchImagesFromSwapChain images count successfully retreived.");
        else
        {
            switch (error)
            {
                case (VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
                    logFatalErrorCB("fetchImagesFromSwapChain failed to fetch images count : out of memory.");
                    throw std::runtime_error("VKI::fetchImagesFromSwapChain failed to fetch images count : out of memory.");
                default:
                    logErrorCB("fetchImagesFromSwapChain failed to fetch images count : unkown error.");
                    throw std::runtime_error("VKI::fetchImagesFromSwapChain failed to fetch images count : unkown error.");
            }
        }

        std::vector<VkImage> images(imagesCount);

        error = vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, images.data());
        if (error == VkResult::VK_SUCCESS)
            logInfoCB("fetchImagesFromSwapChain images successfully retreived.");
        else
        {
            switch (error)
            {
                case (VK_INCOMPLETE):
                    logErrorCB("fetchImagesFromSwapChain failed to fetch all images : VK_INCOMPLETE.");
                    break;
                case (VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
                    logFatalErrorCB("fetchImagesFromSwapChain failed to fetch images : out of memory.");
                    throw std::runtime_error("VKI::fetchImagesFromSwapChain failed to fetch images : out of memory.");
                default:
                    logErrorCB("fetchImagesFromSwapChain failed to fetch images : unkown error.");
                    throw std::runtime_error("VKI::fetchImagesFromSwapChain failed to fetch images : unkown error.");
            }
        }

        return images;
    }

    std::vector<VkImageView> createImageViews(const VkDevice device,
                                              const std::vector<VkImage>& images, 
                                              const VkFormat format,
                                              const VkImageSubresourceRange subResourceRange,
                                              const VkImageViewType viewType)
    {
        std::vector<VkImageView> imageViews(images.size());

        VkImageViewCreateInfo createInfo{};
        createInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.pNext             = nullptr;
        createInfo.flags             = 0;
        createInfo.viewType          = viewType;
        createInfo.format            = format;
        createInfo.components.r      = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g      = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b      = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a      = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange  = subResourceRange;

        uint32_t i;
        VkResult error;
        for ( i=0 ; i<images.size() ; i++)
        {
            createInfo.image = images[i];

            error = vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]);
            if (error != VkResult::VK_SUCCESS)
                goto IMAGE_VIEW_CREATION_FAILED;
        }

        logInfoCB("createImageViews successfully created ImageView(s).");

        return imageViews;

    IMAGE_VIEW_CREATION_FAILED:
        std::stringstream ss ("createImageViews failed to create an image view : "); // Delete this : inefficient and lazy.

        switch (error)
        {
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
                ss<<"OUT_OF_MEMORY.";
                break;
            case (VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR):
                ss<<"VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR.";
                break;
            default:
                ss<<"UNKOWN CAUSES.";
        }
        logFatalErrorCB(ss.str().c_str());
        throw std::runtime_error("VKI::" + ss.str());
    }

    void destroyImageView(const VkDevice device, VkImageView &imageView) noexcept
    {
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, imageView, nullptr);
            logInfoCB("An VkImageView have been destroyed.");
            imageView = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroyImageView called on a VK_NULL_HANDLE.");
    }

    inline SwapChainInfo& clearNullMaxImageCount(SwapChainInfo& swapchainInfo)
    {
        if (swapchainInfo.capabilities.maxImageCount == 0)
            swapchainInfo.capabilities.maxImageCount =  0;

        return swapchainInfo;
    }

    /**@brief Clear maxImageCount=0 from a swapchainInfo.capabilities;
     */
    inline std::pair<uint32_t, uint32_t> getNonNullMaxImageCount(const SwapChainInfo& swapchainInfo)
    {
        std::pair<uint32_t, uint32_t> minmax = {swapchainInfo.capabilities.minImageCount,
                                                swapchainInfo.capabilities.maxImageCount};
        if (minmax.second == 0)
            minmax.second = VKI_IMAGE_COUNT_IF_UNLIMITED;

        return minmax;
    }

    /**@brief Compute the intersection of the device.minmaxImageCount and required.minmaxImageCount.
     * @return If the two image count ranges intersect.
     */
    inline bool intersectMinMaxImageCount(const SwapChainInfo& deviceSwapchainInfo,
                                          const SwapChainInfo& requiredSwapchainInfo,
                                          std::pair<uint32_t, uint32_t>* intersection
                                         )
    {
        std::pair<uint32_t, uint32_t> dev_minmax = getNonNullMaxImageCount(deviceSwapchainInfo);
        std::pair<uint32_t, uint32_t> rqd_minmax = getNonNullMaxImageCount(requiredSwapchainInfo);

        return intersectMinMaxImageCount(dev_minmax, rqd_minmax, intersection);
    }

    inline bool intersectMinMaxImageCount(const std::pair<uint32_t, uint32_t>& dev_minmax,
                                          const std::pair<uint32_t, uint32_t>& rqd_minmax,
                                          std::pair<uint32_t, uint32_t>* intersection
                                         )
    {
        uint32_t min = std::max(dev_minmax.first, rqd_minmax.first);
        uint32_t max = std::min(dev_minmax.second, rqd_minmax.second);

        if (intersection != nullptr)
        {
            intersection->first = std::max(dev_minmax.first, rqd_minmax.first);
            intersection->second = std::min(dev_minmax.second, rqd_minmax.second);
        }
        return min <= max;
    }

    uint32_t acquireNextImage(const VkDevice              device, 
                              const VkSwapchainKHR        swapchain, 
                              const VkSemaphore           semaphore, 
                              const VkFence               fence, 
                              uint64_t                    timeout,
                              SwapchainStatusFlags*       pErrorFlags,
                              const SwapchainStatusFlags  supportedErrorFlags
                             )
    {
        uint32_t imgIndex;
        VkResult error = vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, &imgIndex);

        switch (error)
        {
            case (VK_SUCCESS):
            {
                logInfoCB("acquireNextImage successfully retreived the next image.");
                break;
            }
            case (VK_SUBOPTIMAL_KHR):
            {
                logWarningCB("acquireNextImage retreived the next image but the swapchain is suboptimal.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_SUBOPTIMAL_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_SUBOPTIMAL_BIT;
                break;
            }
            case (VK_NOT_READY):
            {
                logWarningCB("acquireNextImage failed to retreive the next image : not ready.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_NOT_READY_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_NOT_READY_BIT;
                return -1;
            }
            case (VK_TIMEOUT):
            {
                logErrorCB("acquireNextImage failed to retreived the next image : timeout.");
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_TIMEOUT_INDEX) && pErrorFlags!=nullptr)
                    *pErrorFlags |= SWAPCHAIN_STATUS_TIMEOUT_BIT;
                return -1;
            }
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logFatalErrorCB("acquireNextImage failed to retreived the next image : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            case (VK_ERROR_DEVICE_LOST):
            {
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_DEVICE_LOST_INDEX) && pErrorFlags!=nullptr)
                {
                    logErrorCB("acquireNextImage failed to retreived the next image : Device lost.");
                    *pErrorFlags |= SWAPCHAIN_STATUS_DEVICE_LOST_BIT;
                }
                else
                {
                    logFatalErrorCB("acquireNextImage failed to retreived the next image : Device lost.");
                    throw std::runtime_error("GPU lost.");
                }
                break;
            }
            case (VK_ERROR_OUT_OF_DATE_KHR):
            {
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_OUT_OF_DATE_INDEX) && pErrorFlags!=nullptr)
                {
                    logErrorCB("acquireNextImage failed to retreived the next image : swapchain out of date.");
                    *pErrorFlags |= SWAPCHAIN_STATUS_OUT_OF_DATE_BIT;
                }
                else
                {
                    logFatalErrorCB("acquireNextImage failed to retreived the next image : swapchain out of date.");
                    throw std::runtime_error("Program failed to recover after a out of date swapchain.");
                }
                break;
            }
            case (VK_ERROR_SURFACE_LOST_KHR):
            {
                if (supportedErrorFlags.test(SWAPCHAIN_STATUS_SURFACE_LOST_INDEX) && pErrorFlags!=nullptr)
                {
                    logErrorCB("acquireNextImage failed to retreived the next image : surface lost.");
                    *pErrorFlags |= SWAPCHAIN_STATUS_SURFACE_LOST_BIT;
                }
                else
                {
                    logFatalErrorCB("acquireNextImage failed to retreived the next image : surface lost.");
                    throw std::runtime_error("Program failed to find a surface to draw.");
                }
                break;
            }
            default:
            {
                std::stringstream ss("acquireNextImage failed to retreived the next image : unkown error : ");
                ss<<error;
                logFatalErrorCB(ss.str().c_str());
                throw std::runtime_error("Program outdated.");
            }
        }

        return imgIndex;
    }


    /// Graphics Pipeline.

    VkPipeline createGraphicsPipeline(const VkDevice device, 
                                      const VkPipelineLayout pipelineLayout, 
                                      const VkRenderPass renderPass,
                                      const PipelineInfo &gPipelineInfo
                                      //const VkPipeline basePipeline
                                     )
    {
        if ((gPipelineInfo.vertexShader   == VK_NULL_HANDLE) ||
            (gPipelineInfo.fragmentShader == VK_NULL_HANDLE)
           )
        {
            logErrorCB("createPipeline failed to create a pipeline an shader (vert or frag) is missing.");
            throw std::invalid_argument("VKI::createPipeline failed to create a pipeline an shader (vert or frag) is missing.");
        }

        VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
        vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexShaderStageCreateInfo.pNext = nullptr;
        vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderStageCreateInfo.module= gPipelineInfo.vertexShader;
        vertexShaderStageCreateInfo.pName = gPipelineInfo.vertexShaderEntryPoint;
        vertexShaderStageCreateInfo.pSpecializationInfo = (gPipelineInfo.vertexSpecializationInfo.has_value())
                                                          ? &gPipelineInfo.vertexSpecializationInfo.value() 
                                                          : nullptr;

        VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
        fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentShaderStageCreateInfo.pNext = nullptr;
        fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentShaderStageCreateInfo.module= gPipelineInfo.fragmentShader;
        fragmentShaderStageCreateInfo.pName = gPipelineInfo.fragmentShaderEntryPoint;
        fragmentShaderStageCreateInfo.pSpecializationInfo = (gPipelineInfo.fragmentSpecializationInfo.has_value())
                                                            ? &gPipelineInfo.fragmentSpecializationInfo.value()
                                                            : nullptr;

        VkPipelineShaderStageCreateInfo geometryShaderStageCreateInfo{};
        geometryShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geometryShaderStageCreateInfo.pNext = nullptr;
        geometryShaderStageCreateInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geometryShaderStageCreateInfo.module= gPipelineInfo.geometryShader;
        geometryShaderStageCreateInfo.pName = gPipelineInfo.geometryShaderEntryPoint;
        geometryShaderStageCreateInfo.pSpecializationInfo = (gPipelineInfo.geometrySpecializationInfo.has_value())
                                                            ? &gPipelineInfo.geometrySpecializationInfo.value() 
                                                            : nullptr;

        VkPipelineShaderStageCreateInfo tessellationControlShaderStageCreateInfo{};
        tessellationControlShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        tessellationControlShaderStageCreateInfo.pNext = nullptr;
        tessellationControlShaderStageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        tessellationControlShaderStageCreateInfo.module= gPipelineInfo.tessellationControlShader;
        tessellationControlShaderStageCreateInfo.pName = gPipelineInfo.tessellationControlShaderEntryPoint;
        tessellationControlShaderStageCreateInfo.pSpecializationInfo = (gPipelineInfo.tessellationControlSpecializationInfo.has_value())
                                                                       ? &gPipelineInfo.tessellationControlSpecializationInfo.value()
                                                                       : nullptr;

        VkPipelineShaderStageCreateInfo tessellationEvaluationShaderStageCreateInfo{};
        tessellationEvaluationShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        tessellationEvaluationShaderStageCreateInfo.pNext = nullptr;
        tessellationEvaluationShaderStageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        tessellationEvaluationShaderStageCreateInfo.module= gPipelineInfo.tessellationEvaluationShader;
        tessellationEvaluationShaderStageCreateInfo.pName = gPipelineInfo.tessellationEvaluationShaderEntryPoint;
        tessellationEvaluationShaderStageCreateInfo.pSpecializationInfo = (gPipelineInfo.tessellationEvaluationSpecializationInfo.has_value())
                                                                       ? &gPipelineInfo.tessellationEvaluationSpecializationInfo.value()
                                                                       : nullptr;

        if ( (gPipelineInfo.tessellationControlShader    == VK_NULL_HANDLE) xor 
             (gPipelineInfo.tessellationEvaluationShader == VK_NULL_HANDLE)
           ) 
        {
            logWarningCB("createPipeline expected both tesselation control and evaluation shader or none.");
        }

        std::vector<VkPipelineShaderStageCreateInfo> shaders;
        shaders.reserve(5);

        shaders.push_back(vertexShaderStageCreateInfo);
        shaders.push_back(fragmentShaderStageCreateInfo);

        if (gPipelineInfo.geometryShader != VK_NULL_HANDLE)
            shaders.push_back(geometryShaderStageCreateInfo);
        if (gPipelineInfo.tessellationControlShader != VK_NULL_HANDLE) 
            shaders.push_back(tessellationControlShaderStageCreateInfo);
        if (gPipelineInfo.tessellationEvaluationShader != VK_NULL_HANDLE) 
            shaders.push_back(tessellationEvaluationShaderStageCreateInfo);

        // Dynamic states :
        VkPipelineDynamicStateCreateInfo dynamicStateCI{};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCI.pNext = nullptr;
        dynamicStateCI.flags = 0; // reserved.
        dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(gPipelineInfo.dynamicStates.size());
        dynamicStateCI.pDynamicStates = gPipelineInfo.dynamicStates.data();

        // viewport.
        VkPipelineViewportStateCreateInfo viewportStateCI{};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.pNext = nullptr;
        viewportStateCI.flags = 0; // reserved.
        viewportStateCI.viewportCount = gPipelineInfo.viewports.size();
        viewportStateCI.pViewports = gPipelineInfo.viewports.data();    // ignored if set as a dynamic state.
        viewportStateCI.scissorCount = gPipelineInfo.scissors.size();
        viewportStateCI.pScissors  = gPipelineInfo.scissors.data();     // ignored if set as a dynamic state.

        // vertex shader input :
        VkPipelineVertexInputStateCreateInfo vertexInputStateCI;
        vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCI.pNext = nullptr;
        vertexInputStateCI.flags = 0; // reserved.
        vertexInputStateCI.vertexBindingDescriptionCount = gPipelineInfo.vertexInputBindingDescriptions.size();
        vertexInputStateCI.pVertexBindingDescriptions = gPipelineInfo.vertexInputBindingDescriptions.data();
        vertexInputStateCI.vertexAttributeDescriptionCount = gPipelineInfo.vertexInputAttributeDescriptions.size();
        vertexInputStateCI.pVertexAttributeDescriptions = gPipelineInfo.vertexInputAttributeDescriptions.data();

        // Input Assembly :
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
        inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCI.pNext = nullptr;
        inputAssemblyStateCI.flags = 0; // reserved.
        inputAssemblyStateCI.topology = gPipelineInfo.topology;
        inputAssemblyStateCI.primitiveRestartEnable = boolToVkBool32(gPipelineInfo.enablePrimitiveRestart);

        // Rasterization.
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI;
        rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCI.pNext = nullptr;
        rasterizationStateCI.flags = 0; // reserved.
        rasterizationStateCI.depthClampEnable        = physicalDeviceMinimalRequirement.features.depthClamp; //boolToVkBool32(gPipelineInfo.depthClampEnable);
        rasterizationStateCI.rasterizerDiscardEnable = boolToVkBool32(gPipelineInfo.rasterizerDiscardEnable);
        rasterizationStateCI.polygonMode             = gPipelineInfo.polygonMode;
        rasterizationStateCI.cullMode                = gPipelineInfo.cullMode;
        rasterizationStateCI.frontFace               = gPipelineInfo.frontFace;
        rasterizationStateCI.depthBiasEnable         = gPipelineInfo.depthBiasEnable;
        rasterizationStateCI.depthBiasConstantFactor = gPipelineInfo.depthBiasConstantFactor;
        rasterizationStateCI.depthBiasClamp          = gPipelineInfo.depthBiasClamp;
        rasterizationStateCI.depthBiasSlopeFactor    = gPipelineInfo.depthBiasSlopeFactor;
        rasterizationStateCI.lineWidth               = gPipelineInfo.lineWidth;

        // Multisampling (currently disabled).
        VkPipelineMultisampleStateCreateInfo multisampleStateCI;
        multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCI.pNext = nullptr;
        multisampleStateCI.flags = 0; // reserved.
        multisampleStateCI.sampleShadingEnable  = VK_FALSE;
        multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleStateCI.minSampleShading     = 1.0f;
        multisampleStateCI.pSampleMask          = nullptr;
        multisampleStateCI.alphaToCoverageEnable= VK_FALSE;
        multisampleStateCI.alphaToOneEnable     = VK_FALSE;

        //Depth and stencil.
        //TODO later.

        // Color bending.
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI = {};
        colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCI.pNext = nullptr;
        colorBlendStateCI.flags = gPipelineInfo.colorBlendFlags;
        colorBlendStateCI.logicOpEnable = gPipelineInfo.colorBlendEnableLogicOp;
        colorBlendStateCI.logicOp = gPipelineInfo.colorBlendLogicOp;
        colorBlendStateCI.attachmentCount   = gPipelineInfo.colorBlendAttachmentStates.size();
        colorBlendStateCI.pAttachments      = gPipelineInfo.colorBlendAttachmentStates.data();
        colorBlendStateCI.blendConstants[0] = gPipelineInfo.colorBlendBlendConstants[0];
        colorBlendStateCI.blendConstants[1] = gPipelineInfo.colorBlendBlendConstants[1];
        colorBlendStateCI.blendConstants[2] = gPipelineInfo.colorBlendBlendConstants[2];
        colorBlendStateCI.blendConstants[3] = gPipelineInfo.colorBlendBlendConstants[3];

        // tessellation.
        if (gPipelineInfo.tessellatorPatchControlPoints <1)
        {
            logErrorCB("createPipeline : can't create a pipeline with the tessellatorPatchControlPoints <1 (set it to 1 to disable the tessellation).");
            throw std::invalid_argument("VKI::createPipeline : can't create a pipeline with the tessellatorPatchControlPoints <1");
        }
        VkPipelineTessellationStateCreateInfo tessellationStateCI={};
        tessellationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessellationStateCI.pNext = nullptr;
        tessellationStateCI.flags = 0 ; // reserved.
        tessellationStateCI.patchControlPoints = gPipelineInfo.tessellatorPatchControlPoints;

        // pipeline creation :
        VkPipeline pipeline = VK_NULL_HANDLE;

        VkGraphicsPipelineCreateInfo graphicsPipelineCI={};
        graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineCI.pNext = nullptr;
        graphicsPipelineCI.flags = gPipelineInfo.flags;
        graphicsPipelineCI.stageCount           = shaders.size();
        graphicsPipelineCI.pStages              = shaders.data();
        graphicsPipelineCI.pVertexInputState    = &vertexInputStateCI;
        graphicsPipelineCI.pInputAssemblyState  = &inputAssemblyStateCI;
        graphicsPipelineCI.pTessellationState   = &tessellationStateCI;
        graphicsPipelineCI.pViewportState       = &viewportStateCI;
        graphicsPipelineCI.pRasterizationState  = &rasterizationStateCI;
        graphicsPipelineCI.pMultisampleState    = &multisampleStateCI;
        graphicsPipelineCI.pDepthStencilState   = nullptr;//&depthStencilStateCI;
        graphicsPipelineCI.pColorBlendState     = &colorBlendStateCI;
        graphicsPipelineCI.pDynamicState        = &dynamicStateCI;
        graphicsPipelineCI.layout               = pipelineLayout;
        graphicsPipelineCI.renderPass           = renderPass;
        graphicsPipelineCI.subpass              = gPipelineInfo.subpassIndex;
        graphicsPipelineCI.basePipelineHandle   = gPipelineInfo.basePipeline;
        graphicsPipelineCI.basePipelineIndex    = -1;

        VkResult error = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCI, nullptr, &pipeline);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createGraphicsPipeline successfully created a new pipeline.");
                break;
            }
            case (VkResult::VK_PIPELINE_COMPILE_REQUIRED_EXT):
            {
                logErrorCB("createGraphicsPipeline failed to create a graphics pipeline : pipeline compile required ext.");
                throw std::runtime_error("VKI::createGraphicsPipeline failed to create a graphics pipeline : pipeline compile required ext.");
            }
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logErrorCB("createGraphicsPipeline failed to create a pipeline : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logErrorCB("createGraphicsPipeline failed for an unkown reason to create a new graphics pipeline");
                throw std::runtime_error("VKI::createGraphicsPipeline failed to create a new pipeline for an unkown reason.");
            }
        }


        return pipeline;
    }

    VkPipelineLayout createPipelineLayout(const VkDevice device, const PipelineInfo &pipelineInfo)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
        pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCI.pNext = nullptr;
        pipelineLayoutCI.flags = pipelineInfo.pipelineLayoutFlags;
        pipelineLayoutCI.setLayoutCount = pipelineInfo.pipelineDescriptorSetLayouts.size();
        pipelineLayoutCI.pSetLayouts    = pipelineInfo.pipelineDescriptorSetLayouts.data();
        pipelineLayoutCI.pushConstantRangeCount = pipelineInfo.pipelinePushConstantRanges.size();
        pipelineLayoutCI.pPushConstantRanges    = pipelineInfo.pipelinePushConstantRanges.data();


        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult error = vkCreatePipelineLayout(device, 
                                                &pipelineLayoutCI, 
                                                nullptr, 
                                                &pipelineLayout
                                               );
        switch (error)
        {
            case (VkResult::VK_SUCCESS):
                logInfoCB("createPipelineLayout successfully created a new pipeline layout.");
                break;
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                logErrorCB("createPipelineLayout failed to create a pipeline layout : out of memory.");
                throw std::runtime_error("Program out of memory.");
            default:
                logErrorCB("createPipelineLayout failed to create a pipeline layout : unkown error.");
                break;
        }

        return pipelineLayout;
    }

    VkSubpassDescription createVkSubpassDescription(const RenderSubPassInfo& subPassInfo)
    {
        // Even if a vector is empty, data() isn't guaranty to be nullptr;
        const VkAttachmentReference *resolveAttRefs = ((subPassInfo.resolveAttachmentReferences.empty())
                                                       ? nullptr : subPassInfo.resolveAttachmentReferences.data()
                                                      );

        if ((resolveAttRefs != nullptr) && (subPassInfo.resolveAttachmentReferences.size() !=
                                            subPassInfo.colorAttachmentReferences.size()
                                           ))
        {
            logErrorCB("createRenderPass refuse to create a (sub) render pass with the "
                       "referenceAttachmentReferences.size() != colorAttachmentReference.size().");
            throw std::invalid_argument("VKI::createRenderPass can't create a render pass due to invalid "
                                        "referenceAttachmentReferences.size().");
        }

        VkSubpassDescription subPassDesc = {};
        subPassDesc.flags                   = subPassInfo.flags;
        subPassDesc.pipelineBindPoint       = subPassInfo.pipelineBindPoint;
        subPassDesc.inputAttachmentCount    = subPassInfo.inputAttachmentReferences.size();
        subPassDesc.pInputAttachments       = subPassInfo.inputAttachmentReferences.data();
        subPassDesc.colorAttachmentCount    = subPassInfo.colorAttachmentReferences.size();
        subPassDesc.pColorAttachments       = subPassInfo.colorAttachmentReferences.data();
        subPassDesc.pResolveAttachments     = resolveAttRefs;
        subPassDesc.pDepthStencilAttachment = &subPassInfo.depthStencilAttachmentReference;
        subPassDesc.preserveAttachmentCount = subPassInfo.preserveAttachmentReferences.size();
        subPassDesc.pPreserveAttachments    = subPassInfo.preserveAttachmentReferences.data();

        return subPassDesc;
    }

    VkRenderPass createRenderPass(const VkDevice device, const RenderPassInfo &renderPassInfo)
    {
        std::vector<VkSubpassDescription> subPassDescs(renderPassInfo.subPassInfos.size());

        for (uint32_t i = 0; i<renderPassInfo.subPassInfos.size() ; i++)
        {
            subPassDescs[i] = createVkSubpassDescription(renderPassInfo.subPassInfos[i]);
        }

        VkRenderPassCreateInfo renderPassCI = {};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCI.pNext = nullptr;
        renderPassCI.flags = renderPassInfo.flags;
        renderPassCI.attachmentCount = renderPassInfo.attachmentDescs.size();
        renderPassCI.pAttachments    = renderPassInfo.attachmentDescs.data();
        renderPassCI.subpassCount    = subPassDescs.size();
        renderPassCI.pSubpasses      = subPassDescs.data();
        renderPassCI.dependencyCount = renderPassInfo.subPassDependencies.size();;
        renderPassCI.pDependencies   = renderPassInfo.subPassDependencies.data();

        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkResult error = vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createRenderPass successfully created a new render pass.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logErrorCB("createRenderPass failed to create a new render pass : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logErrorCB("createRenderPass : Unexpected error VKI is out of date or Vulkan API changed.");
                throw std::logic_error("VKI unexpected error happend in VKI::createRenderPass.");
            }
        }
        return renderPass;
    }

    void destroyRenderPass(const VkDevice device, VkRenderPass& renderPass) noexcept
    {
        if (renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device, renderPass, nullptr);
            logInfoCB("destroyRenderPass successfully destroyed a renderPass.");
            renderPass = VK_NULL_HANDLE;
        }
        else
            logInfoCB("destroyRenderPass called on a VK_NULL_HANDLE.");
    }

    void destroyPipelineLayout(const VkDevice device, VkPipelineLayout& layout) noexcept
    {
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
            logInfoCB("A pipelineLayout have been destroyed.");
        }
        else
            logWarningCB("destroyPipelineLayout called on a VK_NULL_HANDLE.");
    }

    void destroyPipeline(const VkDevice device, VkPipeline &pipeline) noexcept
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, pipeline, nullptr);
            logInfoCB("A pipeline have been destroyed.");

            pipeline = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroyPipeline called on VK_NULL_HANDLE.");
    }

    void destroyPipelineInfoShaderModules(const VkDevice device, PipelineInfo& pipeline) noexcept
    {
        logVerboseCB("Destroying a PipelineInfo.");

        destroyShaderModule(device, pipeline.vertexShader);
        destroyShaderModule(device, pipeline.fragmentShader);
        //destroyShaderModule(device, pipeline.computeShader);
        destroyShaderModule(device, pipeline.geometryShader);
        destroyShaderModule(device, pipeline.tessellationControlShader);
        destroyShaderModule(device, pipeline.tessellationEvaluationShader);
    }

    /*
    @brief Destroy a shader module if it is UNIQUE, dereference it otherwise.
    void destroyShaderModule(const VkDevice device, std::shared_ptr<VkShaderModule>& shader) noexcept
    {
        if (shader.use_count() == 1)
        {
            destroyShaderModule(device, *shader.get());
        }
        else if (shader.use_count() != 0)
            logInfoCB("destroyShaderModule called on a shadermodule but the shader isn't unique (use_count>1), the shared_ptr will be reset.");
        shader.reset();
    }
    */

    void destroyShaderModule(const VkDevice device, VkShaderModule& shader) noexcept
    {
        if (shader != VK_NULL_HANDLE)
        {
            logInfoCB("Destroying a shader module.");
            vkDestroyShaderModule(device, shader, nullptr);
            shader = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroyShaderModule have been called on a empty (VK_NULL_HANDLE) shaderModule.");
    }

    void destroyRenderPass(const VkDevice device, RenderPass& renderPass) noexcept
    {
        logInfoCB("destroyRenderPass have been called.");

        for (size_t i(0) ; i<renderPass.pipelines.size() ; i++)
        {
            destroyPipeline                 (device, renderPass.pipelines[i].pipeline);
            destroyPipelineLayout           (device, renderPass.pipelines[i].layout);
            destroyPipelineInfoShaderModules(device, renderPass.pipelines[i].info);
        }
        destroyRenderPass(device, renderPass.renderPass);

        logInfoCB("Successfully destroyed a full RenderPass.");
    }

    VkShaderModule createShaderModule(const VkDevice device, const std::vector<char>& spirvCode)
    {
        VkShaderModule shaderModule;

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.flags = 0; // Reserverd.
        createInfo.codeSize = spirvCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(spirvCode.data());

        VkResult error = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        if      (error == VkResult::VK_SUCCESS)
            logInfoCB("createShaderModule successfully created a new module.");
        else if ((error == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY) || (error == VkResult::VK_ERROR_OUT_OF_HOST_MEMORY))
        {
            logErrorCB("createShaderModule failed to create a new module : out of memory");
            throw std::runtime_error("VKI::createShaderModule failed to create a new module : out of memory");
        }
        else if (error == VkResult::VK_ERROR_INVALID_SHADER_NV)
        {
            logErrorCB("createShaderModule failed to create a new module : invalid shader nv, aka"
                      "One or more shader failed to compile or link.");
            throw std::runtime_error("VKI::createShaderModule failed to create a new module : invalid shader nv, aka"
                      "One or more shader failed to compile or link.");
        }

        return shaderModule;
    }

    /**@brief Create a descriptor layout from bindings.
     * @param device, the logical device used to create the instance. 
     * @param bindings, a set of layout bindings to create descriptor layout from.
     * @param flags, a bitmask of VkDescriptorSetLayoutCreateFlagBit specifying options for descriptor set layout creation.
     * @except std::runtime_error If the program run out of memory.
     */
    VkDescriptorSetLayout createPipelineDescriptorSetLayout(const VkDevice device, 
                                                                         std::vector<VkDescriptorSetLayoutBinding> bindings,
                                                                         VkDescriptorSetLayoutCreateFlags flags
                                                                        )
    {
        VkDescriptorSetLayout descriptor;

        VkDescriptorSetLayoutCreateInfo descSetLayoutCI = {};
        descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descSetLayoutCI.pNext = nullptr;
        descSetLayoutCI.flags = flags;
        descSetLayoutCI.bindingCount = bindings.size();
        descSetLayoutCI.pBindings    = bindings.data();

        VkResult error = vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &descriptor);

        switch (error)
        {
            case (VkResult::VK_SUCCESS):
                logInfoCB("createPipelineDescriptorSetLayout successfully create a new descriptor.");
                break;
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                logErrorCB("createPipelineDescriptorSetLayout failed to create a new descriptor : out of memory.");
                throw std::runtime_error("Program out of memory.");
            default:
                logErrorCB("createPipelineDescriptorSetLayout receive an unkown error. No case for it : the error will be ignored.");
                break;
        }

        return descriptor;
    }

    VkFramebuffer createFramebuffer(const VkDevice                  device,
                                    const uint32_t                  width,
                                    const uint32_t                  height,
                                    const VkRenderPass              renderPass,
                                    const std::vector<VkImageView> &attachments,
                                    const VkFramebufferCreateFlags  flags
                                   )
    {
        VkFramebuffer framebuffer;

        VkFramebufferCreateInfo framebufferCI = {};
        framebufferCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCI.pNext           = nullptr;
        framebufferCI.flags           = flags;
        framebufferCI.renderPass      = renderPass;
        framebufferCI.attachmentCount = attachments.size();
        framebufferCI.pAttachments    = attachments.data();
        framebufferCI.width           = width;
        framebufferCI.height          = height;
        framebufferCI.layers          = 1; // unsupported by VKI.

        VkResult error = vkCreateFramebuffer(device, &framebufferCI, nullptr, &framebuffer);

        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createFramebuffer successfully created a new framebuffer.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("createFramebuffer failed to create a new VkFramebuffer object : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("createFramebuffer failed to create a new VkFramebuffer object : unkown error.");
                throw std::runtime_error("createFrameBuffer failed with an unkown error.");
            }
        }

        return framebuffer;
    }

    void destroyFrameBuffer(const VkDevice device, VkFramebuffer &framebuffer) noexcept
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            logInfoCB("A framebuffer have been destroyed.");
            framebuffer = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroyFrameBuffer called on a VK_NULL_HANDLE.");
    }

    std::vector<Buffer> createBuffers(const VkDevice device, const std::vector<BufferInfo>& bufferInfos)
    {
        std::vector<Buffer> buffers;
        buffers.reserve(bufferInfos.size());

        for (size_t i(0) ; i<bufferInfos.size() ; i++)
       {
            Buffer buffer{VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, bufferInfos[i]};

            VkBufferCreateInfo bufferCI;
            bufferCI.sType      = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    #ifndef VK_VERSION_1_4
            bufferCI.pNext      = (buffer.info.useUsage2) ? buffer.info.usage2 : nullptr;
    #else
            bufferCI.pNext      = nullptr;
    #endif
            bufferCI.flags      = buffer.info.flags;
            bufferCI.size       = buffer.info.size;
            bufferCI.usage      = buffer.info.usage;
            bufferCI.sharingMode= (buffer.info.exclusiveMode) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
            bufferCI.queueFamilyIndexCount = buffer.info.queueFamilyIndicesSharingTheBuffer.size();
            bufferCI.pQueueFamilyIndices   = buffer.info.queueFamilyIndicesSharingTheBuffer.data();

            VkResult error = vkCreateBuffer(device, &bufferCI, nullptr, &buffer.buffer);
            switch(error)
            {
                case (VkResult::VK_SUCCESS):
                    break;
                case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                {
                    std::stringstream ss;
                    ss<<"createBuffer failed to create buffer "<<std::dec<<i<<" : out of memory.";
                    logErrorCB(ss.str().c_str());
                    throw std::runtime_error("Program out of memory.");
                }
                default:
                {
                    std::stringstream ss;
                    ss<<"createBuffer failed to create buffer "<<std::dec<<i<<" : unkown error happended.";
                    logFatalErrorCB(ss.str().c_str());
                    throw std::runtime_error("Program outdated.");
                }
            }
            buffers.push_back(buffer);
        }

        std::stringstream ss;
        ss<<"createBuffers successfully created "<<std::dec<<buffers.size()<<" buffers";
        logInfoCB(ss.str().c_str());

        return buffers;
    }

    BufferMirror createBufferMirror(const VkDevice      device,
                                    const BufferInfo&   info, 
                                    const bool          write,
                                    const bool          read,
                                    const std::optional<std::vector<uint32_t>> hostFamilyIndexAccessing,
                                    const std::optional<std::vector<uint32_t>> deviceFamilyIndexAccessing,
                                    const bool removeCoherenceOnDevice,
                                    const bool setupForHostToDeviceBond
                                    )
    {
        BufferInfo infoH=info,
                   infoD=info;

        if (setupForHostToDeviceBond)
        {
            infoH.memoryProperties &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // Remove device local on the host   buffer.
            infoH.memoryProperties |=  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // Add    host visible on the host   buffer.
            infoD.memoryProperties &= ~VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // Remove host visible on the device buffer.
            infoD.memoryProperties |=  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // Add    device local on the device buffer.
        }
        
        if (removeCoherenceOnDevice)
            infoD.memoryProperties &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (write)
        {
            infoH.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            infoD.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        if (read)
        {
            infoH.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            infoD.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        const std::vector<BufferInfo> bufferInfos = {infoH, infoD};
        const std::vector<Buffer> bufferList = createBuffers(device, bufferInfos);
        BufferMirror bm;
        bm.hostBuffer   = std::make_shared<Buffer>(bufferList.front());
        bm.deviceBuffer = std::make_shared<Buffer>(bufferList.back());

        return bm;
    }

    void destroyBuffer(const VkDevice device, Buffer &buffer, const bool free_memory) noexcept
    {
        destroyBuffer(device, buffer.buffer);
        buffer.buffer=VK_NULL_HANDLE;
        if (free_memory)
        {
            freeMemory   (device, buffer.memory);
            buffer.memory=VK_NULL_HANDLE;
        }
        logInfoCB("destroyBuffer (Buffer) successfully destroyed a Buffer struct.");
    }

    void destroyBuffer(const VkDevice device, VkBuffer& buffer) noexcept
    {
        if (buffer==VK_NULL_HANDLE) 
            logWarningCB("destroyBuffer (VkBuffer) called onto a VK_NULL_HANDLE.");
        else
        {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer=VK_NULL_HANDLE;
            logInfoCB("destroyBuffer (VkBuffer) successfully destroyed a buffer.");
        }
    }

    bool allocateBuffer(const VkDevice                          device, 
                              Buffer&                           buffer, 
                        const VkPhysicalDeviceMemoryProperties& phyMemoriesProperties, 
                        const bool                              bindMemoryToBuffer,
                        const bool                              logInfo
                       )
    {
        VkMemoryRequirements bufferRequirements{};
        vkGetBufferMemoryRequirements(device, buffer.buffer, &bufferRequirements);

        if (logInfo)
        {
            std::stringstream ss;
            ss<<"allocateBuffer called on buffer at ";
            ss<<std::hex<<reinterpret_cast<uint64_t>(&buffer);
            ss<<"\n\t\t with a size       (Bytes) : "<<std::dec<<static_cast<uint64_t>(bufferRequirements.size);
            ss<<"\n\t\t with an alignment (Bytes) : "<<std::dec<<static_cast<uint64_t>(bufferRequirements.alignment);
            ss<<"\n\t\t and memory type bits      : "<<std::hex<<static_cast<uint64_t>(bufferRequirements.memoryTypeBits);

            ss<<"\n\t\t\t |-> ";
            if (bufferRequirements.memoryTypeBits == 0)
                ss<<"None the property is null";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                ss<<"DEVICE_LOCAL, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                ss<<"HOST_VISIBLE, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                ss<<"HOST_COHERENT, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                ss<<"HOST_CACHED, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                ss<<"LAZILY_ALLOCATED, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_PROTECTED_BIT)
                ss<<"PROTECTED, ";
        #ifdef VK_AMD_device_coherent_memory
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
                ss<<"AMD_DEVICE_COHERENT, ";
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
                ss<<"AMD_DEVICE_UNCACHED, ";
        #endif
        #ifdef VK_NV_external_memory_rdma
            if (bufferRequirements.memoryTypeBits & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
                ss<<"NVIDIA_RDMA_CAPABLE, ";
        #endif
            
            logInfoCB(ss.str().c_str());
        }

        // Select the right heap to assign the buffer to.

        const uint32_t bufferTypesFilter = bufferRequirements.memoryTypeBits;
        uint32_t memoryTypeIndex = -1;
        for (uint32_t i(0) ; i<phyMemoriesProperties.memoryTypeCount ; i++)
        {
            if ((bufferTypesFilter & (1<<i)) &&
                (phyMemoriesProperties.memoryTypes[i].propertyFlags & buffer.info.memoryProperties)
               )
            {
                memoryTypeIndex=i;
                break;
            }
        }
        if (memoryTypeIndex == -1)
        {
            logErrorCB("allocateBuffer failed to find a suitable memory to allocate from.");
            //throw std::runtime_error("VKI::allocateBuffer failed to find a suitable memory to allocate from.");
            return false;
        }

        if (logInfo)
        {
            const uint32_t heapIndex = phyMemoriesProperties.memoryTypes[memoryTypeIndex].heapIndex;
            std::stringstream ss;
            ss<<"allocateBuffer will allocate memory from heap id=";
            ss<<std::dec<<heapIndex;
            logInfoCB(ss.str().c_str());
            
            logMemoryHeapInfo(phyMemoriesProperties, heapIndex);
        }

        buffer.memory = allocateMemory(device, bufferRequirements.size, memoryTypeIndex);

        if (bindMemoryToBuffer)
        {
            logVerboseCB("allocateBuffer directly call bindBufferMemory");
            bindBufferMemory(device, buffer.buffer, buffer.memory, buffer.info.offset);
        }

        return true;
    }

    VkDeviceMemory allocateMemory(const VkDevice      device,
                                  const VkDeviceSize  size,
                                  const uint32_t      memoryTypeIndex
                                 )
    {
        VkDeviceMemory deviceMemory;
        VkMemoryAllocateInfo memoryAllocateCI{};
        memoryAllocateCI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateCI.pNext           = nullptr;
        memoryAllocateCI.allocationSize  = size;
        memoryAllocateCI.memoryTypeIndex = memoryTypeIndex;

        VkResult error = vkAllocateMemory(device, &memoryAllocateCI, nullptr, &deviceMemory);
        switch (error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("allocateBuffer successfully allocated memory.");
                vkiGlobalMemoryAllocationCount++;
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("allocateBuffer failed to allocate memory : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("allocateBuffer encountered an unkown error while allocating some memory.");
                throw std::runtime_error("Program outdated.");
            }
        }

        return deviceMemory;
    }

    void freeMemory(const VkDevice device, VkDeviceMemory &memory) noexcept
    {
        if (memory == VK_NULL_HANDLE)
            logWarningCB("freeMemory called on a VK_NULL_HANDLE.");
        else
        {
            vkFreeMemory(device, memory, nullptr);
            memory=VK_NULL_HANDLE;
            vkiGlobalMemoryAllocationCount--;
            logInfoCB("freeMemory successfully freed memory.");
        }
    }

    void bindBufferMemory(const VkDevice       device, 
                                VkBuffer       buffer, 
                                VkDeviceMemory memory,
                          const VkDeviceSize   memoryOffset
                         )
    {
        if ((buffer==VK_NULL_HANDLE) || (memory==VK_NULL_HANDLE))
            logWarningCB("bindBufferMemory called onto VK_NULL_HANDLE.");
        else
        {
            VkResult error = vkBindBufferMemory(device, buffer, memory, memoryOffset);
            switch(error)
            {
                case (VkResult::VK_SUCCESS):
                {
                    logInfoCB("bindBufferMemory successfully binded a device memory to a buffer.");
                    break;
                }
                case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                {
                    logFatalErrorCB("bindBufferMemory failed to bind a device memory to a buffer : out of memory.");
                    throw std::runtime_error("Program out of memory.");
                }
                case (VkResult::VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR):
                {
                    logFatalErrorCB("bindBufferMemory failed to bind a device memory to a buffer : VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR.");
                    throw std::runtime_error("Program can't access memory.");
                }
                default:
                {
                    logFatalErrorCB("bindBufferMemory encountered an unkown error while binding the memory to the buffer.");
                    throw std::runtime_error("Program outdated.");
                }
            }
        }
    }

    size_t getGlobalMemoryAllocationCount() noexcept // Return how many allocations simultaneously exist.
    {
        return vkiGlobalMemoryAllocationCount;
    }

    void logMemoryInfo(const VkPhysicalDeviceMemoryProperties& deviceMemProperties)
    {
        std::stringstream ss;
        ss<<"----- logMemoryInfo called, logging info about all "<<deviceMemProperties.memoryHeapCount<<" available heaps -----";
        logInfoCB(ss.str().c_str());

        for (uint32_t heapIndex=0 ; heapIndex < deviceMemProperties.memoryHeapCount ; heapIndex++)
            logMemoryHeapInfo(deviceMemProperties, heapIndex);

        ss.str(std::string());
        ss<<"----- logMemoryInfo successfully logged info about all available heap on the system";
        logInfoCB(ss.str().c_str());
    }

    void logMemoryHeapInfo(const VkPhysicalDeviceMemoryProperties& deviceMemProperties, const uint32_t heapIndex)
    {
        std::vector<VkMemoryPropertyFlags> heapProperties;

        for (uint32_t i=0 ; i<deviceMemProperties.memoryTypeCount ; i++)
        {
            if (deviceMemProperties.memoryTypes[i].heapIndex == heapIndex)
            {
                heapProperties.push_back(deviceMemProperties.memoryTypes[i].propertyFlags);
            }
        }

        std::stringstream ss;
        ss<<"logging info about heap at index="<<std::dec<<heapIndex;
        ss<<"\n\t\tThis heap support "<<heapProperties.size()<<" properties :";
        uint32_t index=0;
        for (VkMemoryPropertyFlags flags : heapProperties)
        {
            ss<<"\n\t\t|---> property "<<index<<" : (value=0x"<<std::hex<<flags<<')';
            if (flags == 0)
                ss<<"\n\t\t|\t|-> No property ??";
            else if (flags & (  ~  (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | 
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|
                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
                                    VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT |
                                    #ifdef VK_VERSION_1_1
                                    VK_MEMORY_PROPERTY_PROTECTED_BIT |
                                    #endif
                                    #ifdef VK_AMD_device_coherent_memory
                                    VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
                                    VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD |
                                    #endif
                                    #ifdef VK_NV_external_memory_rdma
                                    VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV
                                    #endif
                                    )
                            )
                    ) // aka a property exist but isn't registered in the spec at the time.
                ss<<"\n\t\t|\t|-> unkown properties (VKI might be outdated) ???";
            if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                ss<<"\n\t\t|\t|-> DEVICE_LOCAL";
            if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                ss<<"\n\t\t|\t|-> HOST_COHERENT";
            if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                ss<<"\n\t\t|\t|-> HOST_CACHED";
            if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                ss<<"\n\t\t|\t|-> LAZILY_ALLOCATED";
    #ifdef VK_VERSION_1_1
            if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
                ss<<"\n\t\t|\t|-> PROTECTED_MEMORY";
    #endif
    #ifdef VK_AMD_device_coherent_memory
            if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
                ss<<"\n\t\t|\t|-> AMD_DEVICE_COHERENT";
            if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
                ss<<"\n\t\t|\t|-> AMD_DEVICE_UNCACHED";
    #endif
    #ifdef VK_NV_external_memory_rdma
            if (flags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
                ss<<"\n\t\t|\t|-> NVIDIA_RDMA_CAPABLE";
    #endif

            index++;
        }
        logInfoCB(ss.str().c_str());
    }

    void* mapMemory(const VkDevice           device,
                    const VkDeviceMemory     memory,
                    const VkDeviceSize       offset,
                    const VkDeviceSize       size
                   )
    {
        void* hostMemory = nullptr;

        VkResult error = vkMapMemory(device, memory, offset, size, 0, &hostMemory);
        bool logged=false;
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("mapMemory successfully mapped the device memory to the host.");
                break;
            }
            case (VkResult::VK_ERROR_MEMORY_MAP_FAILED):
            {
                logErrorCB("mapMemory failed to allocate memory on the host to map the device memory to.");
                logged=true;
                [[fallthrough]];
            }   
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                if (!logged) logErrorCB("mapMemory failed to map device memory to host : out of host memory.");
                logged=true;
                [[fallthrough]];
            }  
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                if(!logged) logFatalErrorCB("mapMemory failed to map device memory to host : out of device memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("mapMemory encountered an unkown error while mapping device memory to the host.");
                throw std::runtime_error("Program outdated.");
            }
        }

        return hostMemory;
    }
    void mapBuffer(const VkDevice device, Buffer& buffer)
    {
        if (buffer.mappedAddr!=nullptr)
            logErrorCB("mapBuffer called on what appear to be a already mapped buffer since buffer.mappedAddr!=nullptr !");
        else
        {
            buffer.mappedAddr = mapMemory(device, buffer.memory, buffer.info.offset, buffer.info.size);
            logInfoCB("mapBuffer successfully mapped a buffer.");
        }
    }

    void unmapBuffer(const VkDevice device, Buffer& buffer)
    {
        if (buffer.mappedAddr == nullptr)
            logWarningCB("unmappedBuffer called on buffer.mappedAddr=nullptr.");
        else
        {
            vkUnmapMemory(device, buffer.memory);
            buffer.mappedAddr = nullptr;
            logInfoCB("unmapBuffer successfully unmapped a buffer");
        }
    }

    void writeBuffer(const VkDevice device,
                           Buffer&  buffer,
                     const void*    data,
                     const size_t   maxSize
                    )
    {
        if (buffer.mappedAddr==nullptr)
            mapBuffer(device, buffer);

        const size_t size = (maxSize==VK_WHOLE_SIZE) ? buffer.info.size : std::min(buffer.info.size, maxSize);
        std::memcpy(buffer.mappedAddr, data, size);
    }

    void flushBuffers(const VkDevice device,
                      const std::vector<Buffer>& buffers,
                      const bool flushRead,
                      const bool flushWrite
                     )
    {
        std::vector<VkMappedMemoryRange> ranges;
        ranges.reserve(buffers.size());
        for (size_t i=0 ; i<buffers.size() ; i++)
        {
            VkMappedMemoryRange range{};
            ranges[i].sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            ranges[i].pNext  = nullptr;
            ranges[i].memory = buffers[i].memory;
            ranges[i].offset = buffers[i].info.offset;
            ranges[i].size   = buffers[i].info.size;
            ranges.push_back(range);
        }

        if (flushRead)
        {
            VkResult error = vkInvalidateMappedMemoryRanges(device, ranges.size(), ranges.data());
            switch(error)
            {
                case (VkResult::VK_SUCCESS):
                {
                    logInfoCB("flushBuffers successfully flushed all buffers.");
                    break;
                }
                case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                {
                    logFatalErrorCB("flushBuffers failed to invalidate (flush to read) memory ranges : out of memory.");
                    throw std::runtime_error("Program out of memory.");
                }
                default:
                {
                    logFatalErrorCB("flushBuffers encountered an unkown error while invalidating (flush to read) memory ranges.");
                    throw std::runtime_error("Program outdated.");
                }
            }
        }
        if (flushWrite)
        {
            VkResult error = vkFlushMappedMemoryRanges(device, ranges.size(), ranges.data());
            switch(error)
            {
                case (VkResult::VK_SUCCESS):
                {
                    logInfoCB("flushBuffers successfully flushed all buffers.");
                    break;
                }
                case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
                {
                    logFatalErrorCB("flushBuffers failed to flush (to write) memory ranges : out of memory.");
                    throw std::runtime_error("Program out of memory.");
                }
                default:
                {
                    logFatalErrorCB("flushBuffers encountered an unkown error while flushing (to write) memory ranges.");
                    throw std::runtime_error("Program outdated.");
                }
            }
        }
    }

    VkCommandPool createCommandPool(const VkDevice device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex)
    {
        VkCommandPool commandPool;

        VkCommandPoolCreateInfo commandPoolCI = {};
        commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCI.pNext = nullptr;
        commandPoolCI.flags = flags;
        commandPoolCI.queueFamilyIndex = queueFamilyIndex;

        VkResult error = vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createCommandPool successfully created a new command pool.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("createCommandPool failed to create a new command pool : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("createCommandPool encountered an unkown error while creating a new command pool.");
                throw std::runtime_error("Program outdated.");
            }
        }


        return commandPool;
    }

    void destroyCommandPool(const VkDevice device, VkCommandPool &commandPool) noexcept
    {
        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
            logInfoCB("A VkCommandPool have been destroyed.");
        }
        else
            logWarningCB("destroyCommandPool called on a VK_NULL_HANDLE.");
    }

    /*
    void reCreateCommandPool(const VkDevice device, 
                             const VkCommandPoolCreateFlags flags, 
                             const uint32_t queueFamilyIndex,
                             VkCommandPool &pool
                            )
    {
        logInfoCB("Re-creating a command pool.");
        destroyCommandPool(device, pool);
        pool = createCommandPool(device, flags, queueFamilyIndex);
    }
    */

    std::vector<VkCommandBuffer> createNCommandBuffer(const VkDevice device, 
                                                      const VkCommandPool pool, 
                                                      const uint32_t count, 
                                                      const bool isSecondary
                                                     )
    {
        if (count == 0)
            return {};

        std::vector<VkCommandBuffer> buffers(count);

        VkCommandBufferAllocateInfo commandBufferAI{};
        commandBufferAI.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAI.pNext       = nullptr;
        commandBufferAI.commandPool = pool;
        commandBufferAI.level       = (isSecondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY:VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAI.commandBufferCount = count;

        VkResult error = vkAllocateCommandBuffers(device, &commandBufferAI, buffers.data());
        switch (error)
        {
            case (VK_SUCCESS):
            {
                if (isSecondary)
                    logInfoCB("createNCommandBuffer successfully created a/some Secondary command buffer(s).");
                else
                    logInfoCB("createNCommandBuffer successfully created a/some Primary command buffers(s).");
                break;
            }
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                if (isSecondary)
                    logFatalErrorCB("createNCommandBuffer failed to create a/some Secondary command buffer(s).");
                else
                    logFatalErrorCB("createNCommandBuffer failed to create a/some Primary command buffers(s).");
                break;
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("createNCommandBuffer received an unkown error (aborting).");
                throw std::runtime_error("Program outdated.");
            }
        }

        return buffers;
    }

    void createCommandFromCommandInfo(const VkDevice device, 
                                      const uint32_t queueFamilyIndex, 
                                      const CommandInfo &cmdInfo, 
                                      Command &cmd
                                     )
    {
        cmd.pools.resize(cmdInfo.poolsCount);
        for (VkCommandPool &pool : cmd.pools)
            pool = createCommandPool(device, cmdInfo.poolsFlags, queueFamilyIndex);

        for (const CommandBufferInfo &cmdBufferInfo : cmdInfo.commandBufferInfos)
        {
            cmd.PBuffers = createNCommandBuffer(device,
                                                cmd.pools.at(cmdBufferInfo.poolIndex),
                                                cmdBufferInfo.primaryCount,
                                                false
                                               );
            cmd.SBuffers = createNCommandBuffer(device,
                                                cmd.pools.at(cmdBufferInfo.poolIndex),
                                                cmdBufferInfo.secondaryCount,
                                                true
                                               );
        }
    }

    void cmdBeginRecordCommandBuffer(VkCommandBuffer buffer, const VkCommandBufferUsageFlags &flags)
    {
        VkCommandBufferBeginInfo bufferBeginInfo{};
        bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bufferBeginInfo.pNext = nullptr;
        bufferBeginInfo.flags = flags;
        bufferBeginInfo.pInheritanceInfo = nullptr;

        VkResult error = vkBeginCommandBuffer(buffer, &bufferBeginInfo);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("CommandBuffer record begin.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logFatalErrorCB("beginRecordCommandBuffer failed to begin the record : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("beginRecordCommandBuffer received an unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void cmdBeginRecordCommandBuffer(VkCommandBuffer buffer, const VkCommandBufferUsageFlags &flags, 
                                  const VkCommandBufferInheritanceInfo &inheritanceInfo)
    {
        VkCommandBufferBeginInfo bufferBeginInfo{};
        bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bufferBeginInfo.pNext = nullptr;
        bufferBeginInfo.flags = flags;
        bufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

        VkResult error = vkBeginCommandBuffer(buffer, &bufferBeginInfo);
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("CommandBuffer record begin.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logFatalErrorCB("beginRecordCommandBuffer failed to begin the record : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("beginRecordCommandBuffer received an unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void cmdResetCommandBuffer(VkCommandBuffer cmdBuffer, bool willBeReuse)
    {
        VkResult error = vkResetCommandBuffer(cmdBuffer, ((willBeReuse) ? 0 : VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT ));
        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("cmdResetCommandBuffer successfully reset a command buffer.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("cmdResetCommandBuffer failed to reset the command buffer : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("cmdResetCommandBuffer received an unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void cmdEndRecordCommandBuffer(VkCommandBuffer buffer)
    {
        VkResult error = vkEndCommandBuffer(buffer);
        switch(error)
        {
            case(VkResult::VK_SUCCESS):
            {
                logInfoCB("CommandBuffer record end.");
                break;
            }
            case(VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            case(VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            {
                logFatalErrorCB("endRecordCommandBuffer failed to end the recording of a buffer : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
           #ifdef VK_KHR_video_encode_queue
            case(VkResult::VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR):
            {
                logFatalErrorCB("endRecordCommandBuffer failed to end the recording of a buffer : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
           #endif//VK_KHR_video_encode_queue
            default:
            {
                logFatalErrorCB("endRecordCommandBuffer received an unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void cmdBeginRenderPass(VkCommandBuffer                  cmdBuffer,
                            const VkRenderPass               renderPass, 
                            const VkFramebuffer              framebuffer, 
                            const VkRect2D                   renderArea, 
                            const std::vector<VkClearValue> &clearValues,
                            const VkSubpassContents          subPassContent
                           )
    {
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext           = nullptr;
        renderPassBeginInfo.renderPass      = renderPass;
        renderPassBeginInfo.framebuffer     = framebuffer;
        renderPassBeginInfo.renderArea      = renderArea;
        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues    = clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, subPassContent);
        logInfoCB("Command buffer begin render pass.");
    }

    void cmdBindPipeline(VkCommandBuffer cmdBuffer, const VkPipeline pipeline, const VkPipelineBindPoint bindPoint)
    {
        vkCmdBindPipeline(cmdBuffer, bindPoint, pipeline);

        switch (bindPoint)
        {
            case (VK_PIPELINE_BIND_POINT_GRAPHICS):
            {
                logInfoCB("Command buffer bind graphic pipeline");
                break;
            }
            case (VK_PIPELINE_BIND_POINT_COMPUTE):
            {
                logInfoCB("Command buffer bind compute pipeline");
                break;
            }
            default:
            {
                logWarningCB("Command buffer bind [UNKOWN_BIND_POINT] pipeline");
                break;
            }
        }
    }

    // host to device.
    void recordPushBufferMirror(const BufferMirror&       bufferMirror,
                                const VkCommandBuffer     cmdBuffer,
                                std::vector<VkBufferCopy> regions
                               )
    {
        for (VkBufferCopy &region : regions)
            region.size = (region.size==VK_WHOLE_SIZE) ? bufferMirror.hostBuffer->info.size : region.size;

        vkCmdCopyBuffer(cmdBuffer, bufferMirror.hostBuffer->buffer, bufferMirror.deviceBuffer->buffer, regions.size(), regions.data());
    }

    // device to host.
    void recordPullBufferMirror(const BufferMirror&       bufferMirror,
                                const VkCommandBuffer     cmdBuffer,
                                std::vector<VkBufferCopy> regions
                               )
    {
        for (VkBufferCopy &region : regions)
            region.size = (region.size==VK_WHOLE_SIZE) ? bufferMirror.deviceBuffer->info.size : region.size;

        vkCmdCopyBuffer(cmdBuffer, bufferMirror.deviceBuffer->buffer, bufferMirror.hostBuffer->buffer, regions.size(), regions.data());
    }
    

    void cmdDraw(VkCommandBuffer cmdBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertexIndex, uint32_t firstInstanceIndex)
    {
        vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertexIndex, firstInstanceIndex);
        logInfoCB("Command buffer draw call.");
    }

    void cmdEndRenderPass(VkCommandBuffer cmdBuffer)
    {
        vkCmdEndRenderPass(cmdBuffer);
        logInfoCB("Command buffer end render pass.");
    }

    VkSemaphore createSemaphore(const VkDevice device)
    {
        VkSemaphoreCreateInfo semaphoreCI{};
        semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCI.pNext = nullptr; // reserved.
        semaphoreCI.flags = 0;       // reserved.

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkResult error = vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore);

        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createSemaphore successfully created a new semaphore.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("createSemaphore failed to created a new semaphore : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("createSemaphore failed to created a new semaphore : unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }

        return semaphore;
    }

    VkFence createFence(const VkDevice device, bool startSignaled)
    {
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.pNext = nullptr;
        fenceCI.flags = (startSignaled) ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        VkFence fence = VK_NULL_HANDLE;
        VkResult error = vkCreateFence(device, &fenceCI, nullptr, &fence);

        switch(error)
        {
            case (VkResult::VK_SUCCESS):
            {
                logInfoCB("createFence successfully created a new fence.");
                break;
            }
            case (VkResult::VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VkResult::VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("createFence failed to created a new fence : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("createFence failed to created a new fence : unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }

        return fence;
    }

    void destroySemaphore(const VkDevice device, VkSemaphore &semaphore) noexcept
    {
        if (semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
            semaphore = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroySemaphore called on a VK_NULL_HANDLE.");
    }

    void destroyFence(const VkDevice device, VkFence &fence) noexcept
    {
        if (fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
        else
            logWarningCB("destroyFence called on a VK_NULL_HANDLE.");
    }

    bool isFenceSignaled(const VkDevice device, const VkFence fence)
    {
        VkResult error = vkGetFenceStatus(device, fence);
        if (error == VkResult::VK_ERROR_DEVICE_LOST)
            throw std::runtime_error("isFenceSignaled received VK_ERROR_DEVICE_LOST.");

        return error == VK_SUCCESS; // else VK_NOT_READY;
    }

    bool waitFences(const VkDevice device, const std::vector<VkFence> &fences, bool waitAll, uint64_t timeout, bool logInfo)
    {
        if (logInfo) logVerboseCB("waitFences called.");
        VkResult error = vkWaitForFences(device, fences.size(), fences.data(), boolToVkBool32(waitAll), timeout);
        if (logInfo) logVerboseCB("waitFences returned.");

        switch (error)
        {
            case (VK_SUCCESS):
            {
                //logVerboseCB("waitFence");
                return true;
            }
            case (VK_TIMEOUT):
            {
                logWarningCB("waitFences returned du to a timeout.");
                return false;
            }
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("waitFences failed : out of memory");
                throw std::runtime_error("Program out of memory");
            }
            default:
            {
                logFatalErrorCB("waitFence failed du to a unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    bool waitFence(const VkDevice device, const VkFence &fence, uint64_t timeout, bool logInfo)
    {
        if (logInfo) logVerboseCB("waitFence called.");
        VkResult error = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
        if (logInfo) logVerboseCB("waitFence returned.");

        switch (error)
        {
            case (VK_SUCCESS):
            {
                //logVerboseCB("waitFence");
                return true;
            }
            case (VK_TIMEOUT):
            {
                logWarningCB("waitFence returned du to a timeout.");
                return false;
            }
            case (VK_ERROR_OUT_OF_HOST_MEMORY):
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("waitFence failed : out of memory");
                throw std::runtime_error("Program out of memory");
            }
            default:
            {
                logFatalErrorCB("waitFence failed du to a unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void resetFence(const VkDevice device, const VkFence fence)
    {
        VkResult error = vkResetFences(device, 1, &fence);

        switch (error)
        {
            case (VK_SUCCESS):
            {
                logInfoCB("resetFence successfully reset a fence.");
                break;
            }
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("resetFence failed to reset a fence : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("resetFences receive a unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    void resetFence(const VkDevice device, const std::vector<VkFence> &fences)
    {
        VkResult error = vkResetFences(device, fences.size(), fences.data());

        switch (error)
        {
            case (VK_SUCCESS):
            {
                logInfoCB("resetFence successfully reset fences.");
                break;
            }
            case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
            {
                logFatalErrorCB("resetFence failed to reset fences : out of memory.");
                throw std::runtime_error("Program out of memory.");
            }
            default:
            {
                logFatalErrorCB("resetFence receive a unkown error.");
                throw std::runtime_error("Program outdated.");
            }
        }
    }

    /// Others.

  /*constexpr*/ bool operator==(const QueueInfo& A,const QueueInfo& B)
    {
        bool same = true;
        same &= A.familyIndex   == B.familyIndex;
        same &= A.count         == B.count;
        for (size_t i=0 ; i<VKI_MAX_QUEUE_COUNT_PER_FAMILY ; i++)
            same &= A.priorities[i] == B.priorities[i];
        same &= A.operations    == B.operations;
        same &= A.isPresentable == B.isPresentable;

        if (A.cmdPoolInfos.size() == B.cmdPoolInfos.size())
        {
            for (size_t i=0 ; i<A.cmdPoolInfos.size() ; i++)
            {
                same &= A.cmdPoolInfos[i].poolsFlags == B.cmdPoolInfos[i].poolsFlags;
                same &= A.cmdPoolInfos[i].poolsCount == B.cmdPoolInfos[i].poolsCount;
                if (A.cmdPoolInfos[i].commandBufferInfos.size() == B.cmdPoolInfos[i].commandBufferInfos.size())
                {
                    for (size_t ii=0 ; ii<A.cmdPoolInfos[i].commandBufferInfos.size() ; ii++)
                    {
                        same &= A.cmdPoolInfos[i].commandBufferInfos[ii].primaryCount   == B.cmdPoolInfos[i].commandBufferInfos[ii].primaryCount ;
                        same &= A.cmdPoolInfos[i].commandBufferInfos[ii].secondaryCount == B.cmdPoolInfos[i].commandBufferInfos[ii].secondaryCount ;
                        same &= A.cmdPoolInfos[i].commandBufferInfos[ii].poolIndex      == B.cmdPoolInfos[i].commandBufferInfos[ii].poolIndex;
                    }
                }
                else
                {
                    same = false;
                    break;
                }
            }
        }
        else
            same = false;

        return same;
    }

    /*constexpr*/ void resetPhysicalDeviceMinimalRequirement(PhysicalDeviceMinimalRequirement& minRqd)
    {
		minRqd.features.robustBufferAccess				    	= VK_FALSE ;
		minRqd.features.fullDrawIndexUint32					    = VK_FALSE ;
		minRqd.features.imageCubeArray							= VK_FALSE ;
		minRqd.features.independentBlend						= VK_FALSE ;
		minRqd.features.geometryShader							= VK_FALSE ;
		minRqd.features.tessellationShader						= VK_FALSE ;
		minRqd.features.sampleRateShading						= VK_FALSE ;
		minRqd.features.dualSrcBlend							= VK_FALSE ;
		minRqd.features.logicOp					    		    = VK_FALSE ;
		minRqd.features.multiDrawIndirect						= VK_FALSE ;
		minRqd.features.drawIndirectFirstInstance				= VK_FALSE ; 
        minRqd.features.depthClamp					    		= VK_FALSE ;
		minRqd.features.depthBiasClamp							= VK_FALSE ;
		minRqd.features.fillModeNonSolid						= VK_FALSE ;
		minRqd.features.depthBounds							    = VK_FALSE ;
		minRqd.features.wideLines					    		= VK_FALSE ;
		minRqd.features.largePoints				   			    = VK_FALSE ;
		minRqd.features.alphaToOne					    		= VK_FALSE ;
		minRqd.features.multiViewport							= VK_FALSE ;
		minRqd.features.samplerAnisotropy			    		= VK_FALSE ;
		minRqd.features.textureCompressionETC2					= VK_FALSE ;
		minRqd.features.textureCompressionASTC_LDR				= VK_FALSE ;
		minRqd.features.textureCompressionBC					= VK_FALSE ;
		minRqd.features.occlusionQueryPrecise					= VK_FALSE ;
		minRqd.features.pipelineStatisticsQuery				    = VK_FALSE ;
		minRqd.features.vertexPipelineStoresAndAtomics			= VK_FALSE ;
		minRqd.features.fragmentStoresAndAtomics				= VK_FALSE ;
		minRqd.features.shaderTessellationAndGeometryPointSize	= VK_FALSE ;
		minRqd.features.shaderImageGatherExtended				= VK_FALSE ;
		minRqd.features.shaderStorageImageExtendedFormats		= VK_FALSE ;
		minRqd.features.shaderStorageImageMultisample			= VK_FALSE ;
		minRqd.features.shaderStorageImageReadWithoutFormat	    = VK_FALSE ;
		minRqd.features.shaderStorageImageWriteWithoutFormat	= VK_FALSE ;
		minRqd.features.shaderUniformBufferArrayDynamicIndexing = VK_FALSE ;
		minRqd.features.shaderSampledImageArrayDynamicIndexing	= VK_FALSE ;
		minRqd.features.shaderStorageBufferArrayDynamicIndexing = VK_FALSE ;
		minRqd.features.shaderStorageImageArrayDynamicIndexing	= VK_FALSE ;
		minRqd.features.shaderClipDistance						= VK_FALSE ;
		minRqd.features.shaderCullDistance						= VK_FALSE ;
		minRqd.features.shaderFloat64							= VK_FALSE ;
		minRqd.features.shaderInt64							    = VK_FALSE ;
		minRqd.features.shaderInt16							    = VK_FALSE ;
		minRqd.features.shaderResourceResidency				    = VK_FALSE ;
		minRqd.features.shaderResourceMinLod			    	= VK_FALSE ;
		minRqd.features.sparseBinding							= VK_FALSE ;
		minRqd.features.sparseResidencyBuffer					= VK_FALSE ;
		minRqd.features.sparseResidencyImage2D					= VK_FALSE ;
		minRqd.features.sparseResidencyImage3D					= VK_FALSE ;
		minRqd.features.sparseResidency2Samples				    = VK_FALSE ;
		minRqd.features.sparseResidency4Samples				    = VK_FALSE ;
		minRqd.features.sparseResidency8Samples				    = VK_FALSE ;
		minRqd.features.sparseResidency16Samples				= VK_FALSE ;
		minRqd.features.sparseResidencyAliased					= VK_FALSE ;
		minRqd.features.variableMultisampleRate			        = VK_FALSE ;
		minRqd.features.inheritedQueries						= VK_FALSE ;

        // (if you use [n]vim, you should type "set cursorline").

    /*uint32_t*/			minRqd.limits.maxImageDimension1D			    				= 0;
    /*uint32_t*/			minRqd.limits.maxImageDimension2D				    			= 0;
    /*uint32_t*/			minRqd.limits.maxImageDimension3D					    		= 0;
    /*uint32_t*/			minRqd.limits.maxImageDimensionCube					    		= 0;
    /*uint32_t*/			minRqd.limits.maxImageArrayLayers					    		= 0;
    /*uint32_t*/			minRqd.limits.maxTexelBufferElements							= 0;
    /*uint32_t*/			minRqd.limits.maxUniformBufferRange					    		= 0;
    /*uint32_t*/			minRqd.limits.maxStorageBufferRange					    		= 0;
    /*uint32_t*/			minRqd.limits.maxPushConstantsSize					    		= 0;
    /*uint32_t*/			minRqd.limits.maxMemoryAllocationCount							= 0;
    /*uint32_t*/			minRqd.limits.maxSamplerAllocationCount							= 0;
    /*VkDeviceSize*/		minRqd.limits.bufferImageGranularity							= 0;
    /*VkDeviceSize*/		minRqd.limits.sparseAddressSpaceSize							= 0;
    /*uint32_t*/			minRqd.limits.maxBoundDescriptorSets							= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorSamplers						= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorUniformBuffers				= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorStorageBuffers				= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorSampledImages				= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorStorageImages				= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageDescriptorInputAttachments				= 0;
    /*uint32_t*/			minRqd.limits.maxPerStageResources					    		= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetSamplers							= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetUniformBuffers					= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetUniformBuffersDynamic				= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetStorageBuffers					= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetStorageBuffersDynamic				= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetSampledImages						= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetStorageImages						= 0;
    /*uint32_t*/			minRqd.limits.maxDescriptorSetInputAttachments					= 0;
    /*uint32_t*/			minRqd.limits.maxVertexInputAttributes							= 0;
    /*uint32_t*/			minRqd.limits.maxVertexInputBindings							= 0;
    /*uint32_t*/			minRqd.limits.maxVertexInputAttributeOffset						= 0;
    /*uint32_t*/			minRqd.limits.maxVertexInputBindingStride						= 0;
    /*uint32_t*/			minRqd.limits.maxVertexOutputComponents							= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationGenerationLevel					= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationPatchSize							= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationControlPerVertexInputComponents	= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationControlPerVertexOutputComponents	= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationControlPerPatchOutputComponents	= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationControlTotalOutputComponents		= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationEvaluationInputComponents			= 0;
    /*uint32_t*/			minRqd.limits.maxTessellationEvaluationOutputComponents			= 0;
    /*uint32_t*/			minRqd.limits.maxGeometryShaderInvocations						= 0;
    /*uint32_t*/			minRqd.limits.maxGeometryInputComponents						= 0;
    /*uint32_t*/			minRqd.limits.maxGeometryOutputComponents						= 0;
    /*uint32_t*/			minRqd.limits.maxGeometryOutputVertices							= 0;
    /*uint32_t*/			minRqd.limits.maxGeometryTotalOutputComponents					= 0;
    /*uint32_t*/			minRqd.limits.maxFragmentInputComponents						= 0;
    /*uint32_t*/			minRqd.limits.maxFragmentOutputAttachments						= 0;
    /*uint32_t*/			minRqd.limits.maxFragmentDualSrcAttachments						= 0;
    /*uint32_t*/			minRqd.limits.maxFragmentCombinedOutputResources				= 0;
    /*uint32_t*/			minRqd.limits.maxComputeSharedMemorySize						= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupCount[0]	   					= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupCount[1]	   					= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupCount[2]	   					= 0;
    /*uint32_t*/			minRqd.limits.maxComputeWorkGroupInvocations					= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupSize[0]						= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupSize[1]						= 0;
    /*uint32_t[3]*/			minRqd.limits.maxComputeWorkGroupSize[2]						= 0;
    /*uint32_t*/			minRqd.limits.subPixelPrecisionBits 							= 0;
    /*uint32_t*/			minRqd.limits.subTexelPrecisionBits			    				= 0;
    /*uint32_t*/			minRqd.limits.mipmapPrecisionBits			    				= 0;
    /*uint32_t*/			minRqd.limits.maxDrawIndexedIndexValue		   					= 0;
    /*uint32_t*/			minRqd.limits.maxDrawIndirectCount			    				= 0;
    /*float*/				minRqd.limits.maxSamplerLodBias				        			= 0.0;
    /*float*/				minRqd.limits.maxSamplerAnisotropy				    			= 0.0;
    /*uint32_t*/			minRqd.limits.maxViewports						            	= 0;
    /*uint32_t[2]*/			minRqd.limits.maxViewportDimensions[0]							= 0;
    /*uint32_t[2]*/			minRqd.limits.maxViewportDimensions[1]							= 0;
    /*float[2]*/			minRqd.limits.viewportBoundsRange[0]							= 0.0;
    /*float[2]*/			minRqd.limits.viewportBoundsRange[1]							= 0.0;
    /*uint32_t*/			minRqd.limits.viewportSubPixelBits						    	= 0;
    /*size_t*/				minRqd.limits.minMemoryMapAlignment						    	= 0;
    /*VkDeviceSize*/		minRqd.limits.minTexelBufferOffsetAlignment						= 0;
    /*VkDeviceSize*/		minRqd.limits.minUniformBufferOffsetAlignment					= 0;
    /*VkDeviceSize*/		minRqd.limits.minStorageBufferOffsetAlignment					= 0;
    /*int32_t*/				minRqd.limits.minTexelOffset    			    				= 0;
    /*uint32_t*/			minRqd.limits.maxTexelOffset    			    				= 0;
    /*int32_t*/				minRqd.limits.minTexelGatherOffset			    				= 0;
    /*uint32_t*/			minRqd.limits.maxTexelGatherOffset			    				= 0;
    /*float*/				minRqd.limits.minInterpolationOffset							= 0.0;
    /*float*/				minRqd.limits.maxInterpolationOffset							= 0.0;
    /*uint32_t*/			minRqd.limits.subPixelInterpolationOffsetBits  					= 0;
    /*uint32_t*/			minRqd.limits.maxFramebufferWidth			    				= 0;
    /*uint32_t*/			minRqd.limits.maxFramebufferHeight			    				= 0;
    /*uint32_t*/			minRqd.limits.maxFramebufferLayers			    				= 0;
    /*VkSampleCountFlags*/	minRqd.limits.framebufferColorSampleCounts						= 0;
    /*VkSampleCountFlags*/	minRqd.limits.framebufferDepthSampleCounts						= 0;
    /*VkSampleCountFlags*/	minRqd.limits.framebufferStencilSampleCounts					= 0;
    /*VkSampleCountFlags*/	minRqd.limits.framebufferNoAttachmentsSampleCounts				= 0;
    /*uint32_t*/		    minRqd.limits.maxColorAttachments				    			= 0;
    /*VkSampleCountFlags*/	minRqd.limits.sampledImageColorSampleCounts						= 0;
    /*VkSampleCountFlags*/	minRqd.limits.sampledImageIntegerSampleCounts					= 0;
    /*VkSampleCountFlags*/	minRqd.limits.sampledImageDepthSampleCounts						= 0;
    /*VkSampleCountFlags*/	minRqd.limits.sampledImageStencilSampleCounts					= 0;
    /*VkSampleCountFlags*/	minRqd.limits.storageImageSampleCounts							= 0;
    /*uint32_t*/			minRqd.limits.maxSampleMaskWords				    			= 0;
    /*VkBool32*/			minRqd.limits.timestampComputeAndGraphics						= 0;
    /*float*/				minRqd.limits.timestampPeriod					        		= 0.0;
    /*uint32_t*/			minRqd.limits.maxClipDistances						        	= 0;
    /*uint32_t*/			minRqd.limits.maxCullDistances				        			= 0;
    /*uint32_t*/			minRqd.limits.maxCombinedClipAndCullDistances					= 0;
    /*uint32_t*/	    	minRqd.limits.discreteQueuePriorities							= 0;
    /*float[2]*/			minRqd.limits.pointSizeRange[0]					        		= 0.0;
    /*float[2]*/			minRqd.limits.pointSizeRange[1]					        		= 0.0;
    /*float[2]*/			minRqd.limits.lineWidthRange[0]						        	= 0.0;
    /*float[2]*/			minRqd.limits.lineWidthRange[1]						        	= 0.0;
    /*float*/				minRqd.limits.pointSizeGranularity			    				= 0.0;
    /*float*/				minRqd.limits.lineWidthGranularity			    				= 0.0;
    /*VkBool32*/			minRqd.limits.strictLines					            		= false;
    /*VkBool32*/			minRqd.limits.standardSampleLocations							= false;
    /*VkDeviceSize*/		minRqd.limits.optimalBufferCopyOffsetAlignment					= 0;
    /*VkDeviceSize*/		minRqd.limits.optimalBufferCopyRowPitchAlignment				= 0;
    /*VkDeviceSize*/		minRqd.limits.nonCoherentAtomSize					    		= 0;

        //minRqd.swapChainInfo.capabilities.
    /*uint32_t*/                         minRqd.swapChainInfo.capabilities.minImageCount            = 1;
    /*uint32_t*/                         minRqd.swapChainInfo.capabilities.maxImageCount            = 0;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.currentExtent.width      = -1;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.currentExtent.height     = -1;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.minImageExtent.width     = -1;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.minImageExtent.height    = -1;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.maxImageExtent.width     = -1;
    /*VkExtent2D*/                       minRqd.swapChainInfo.capabilities.maxImageExtent.height    = -1;
    /*uint32_t*/                         minRqd.swapChainInfo.capabilities.maxImageArrayLayers      = 1;
    /*VkSurfaceTransformFlagsKHR*/       minRqd.swapChainInfo.capabilities.supportedTransforms      = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    /*VkSurfaceTransformFlagBitsKHR*/    minRqd.swapChainInfo.capabilities.currentTransform         = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    /*VkCompositeAlphaFlagsKHR*/         minRqd.swapChainInfo.capabilities.supportedCompositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    /*VkImageUsageFlags*/                minRqd.swapChainInfo.capabilities.supportedUsageFlags      = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                                                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        minRqd.swapChainInfo.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        minRqd.swapChainInfo.sharingMode            = VK_SHARING_MODE_EXCLUSIVE;
        minRqd.swapChainInfo.currentCompositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        minRqd.swapChainInfo.enableClipping         = VK_FALSE;

        minRqd.swapChainInfo.subresourceRange = { 
            /*aspectMask*/     VK_IMAGE_ASPECT_COLOR_BIT,
            /*baseMipLevel*/   0,
            /*levelCount*/     1,
            /*baseArrayLayer*/ 0,
            /*layerCount*/     1
        };

        //return minRqd;
    }


    ///////////////////////////////////////////////////////////////////////////
    //////////   MANUALLY LOADED FUNCTION FROM VULKAN EXTENSIONS   ////////////
    ///////////////////////////////////////////////////////////////////////////

    /**@brief Manually load the ckCreateDebugUtilsMessengerEXT and wrap it into a local function.
     */
    VkResult createInstanceDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
            const VkAllocationCallbacks* pAllocator, 
            VkDebugUtilsMessengerEXT* pDebugMessenger
            )
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        else
            return VkResult::VK_ERROR_EXTENSION_NOT_PRESENT;
    }

   #ifdef VKI_ENABLE_VULKAN_VALIDATION_LAYERS
    /**@brief Manually load the vkCreateDebugUtilsMessengerEXT and wrap it into a local function.
     */
    void destroyInstanceDebugUtilsMessengerEXT (
            VkInstance instance, 
            VkDebugUtilsMessengerEXT debugMessenger, 
            const VkAllocationCallbacks* pAllocator
            ) 
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if (func != nullptr) 
        {
            func(instance, debugMessenger, pAllocator);
            logInfoCB("destroyInstanceDebugUtilsMessenger called successfully.");
        }
        else
            logErrorCB("destroyInstanceDebugUtilsMessenger failed to fetch the vkDestroyDebugUtilsMessengerEXT which is really weird since \
                    If this function has been called then the debug messenger has been created : which mean that the required \
                    extensions is available.");
            // Yes this log is horrible but it will never be displayed since,
            // has it said : if you need it then it have to be available.

        return ;
    }
   #endif//VKI_ENABLE_VULKAN_VALIDATION_LAYERS


}//namespace VKI;



