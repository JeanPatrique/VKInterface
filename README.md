Disclaimer :
    This project is my vulkan-tutorial code base i.e. I know very little things about the API.

# VKInterface 

VKInterface (VulKan Interface, or VKI) is a bundle of functions and structures to ease the use of the Vulkan API.
The goal of this small lib is to reduce de verbosity of the API without removing the low-level aspect and performance.
This mean that this lib is intended to be used as an optional layer on top of the API, rather than as a full-featured renderer.

## Compilation

### Linux :
#### Dependencies :
    glfw, vulkan, dl, pthread, X11, Xxf86vm, Xrandr, Xi.
##### Debian :
    `sudo apt install libvulkan-dev vulkan-validationlayers-dev spirv-tools`
##### Arch :
    `sudo pacman -S vulkan-devel`

[//]: # (### Windows : // TODO later)

#### Build :
    `mkdir Build
    cmake -S . -B Build -D[CMAKE_BUILD_TYPE=Release][VKI_BUILD_EXEMPLE_EXECUTABLES][...]
    cmake --build Build`
    Here is a list of flag that can be set :
    `VKI_BUILD_EXEMPLE_EXECUTABLES` : Build exemples.
    `VKI_BUILD_EXEMPLE_EXECUTABLES` : Build exemples.
    `VKI_ENABLE_DEBUG_LOGS` : If defined, this include more logs calls to inspect some functions behavior (like findQueueFamilyIndices).
[//]: # (`VKI_DISABLE_LOGS_CALL` : If defined, all call to the logs functions are remove. // TODO)
    


