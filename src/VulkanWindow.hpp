#ifndef VULKAN_WINDOW_HPP
#define VULKAN_WINDOW_HPP

#include <QWindow>
#include <QVulkanInstance>

#include <optional>
#include <vector>
#include <array>

#include "VulkanFunctions.hpp"
#include "Image.hpp"

class VulkanWindow;

class AbstractVulkanRenderer {
public:
    virtual ~AbstractVulkanRenderer() {};

    virtual void pre_init_resources(VulkanWindow*) {};
    virtual void init_resources() {};
    virtual void init_swap_chain_resources() {};
    virtual void release_swap_chain_resources() {};
    virtual void release_resources() {};

    virtual void start_next_frame() = 0;
};

class VulkanWindow : public QWindow {
    Q_OBJECT;

public:
    VulkanWindow(AbstractVulkanRenderer* vulkan_renderer, QWindow* parent=nullptr);
    ~VulkanWindow();

    constexpr uint32_t get_nr_concurrent_frames() {return nr_frames_in_flight;}


    // Pre-init functions (only valid before `init_resources`. Usually used in `pre_init_resources`)
    //========================================================

    typedef std::function<int (VkPhysicalDevice device, int window_rating)> PhysicalDeviceRater;
    // Rate physical device
    // Physical devices not suitable for the window will be discarded before reaching `get_physical_device_rating`
    // The user provided rating will be the final rating (the user chooses how much the initial `window_rating` matters)
    // It is recommended to only add to the score (or return 0 if the device is not suitable).
    // The highest rated device will be chosen as the VkPhysicalDevice. It is recommended to check and make sure this device has the requested features
    void set_physical_device_rater(PhysicalDeviceRater pdr) {physical_device_rater=pdr;}

    // Request the given VkPhysicalDeviceFeatures
    // If a feature isn't supported it will be ignored
    // If you want to weigh devices supporting the feature more highly you must use `set_physical_device_rater` to rate it more highly yourself
    void request_physical_device_features(const VkPhysicalDeviceFeatures& pdf) {requested_physical_device_features=pdf;}


    // Resource functions (only valid from `init_resources` to `release_resources`)
    //=============================================================================

    VulkanData get_vulkan_data() {return vkd;}

    VkPhysicalDevice get_physical_device() {return vkd.physical_device;}
    VkDevice get_device() {return vkd.device;}

    VkPhysicalDeviceFeatures get_enabled_physical_device_features() {return physical_device_features;}
    
    VkFormat get_color_format() {return swap_chain_surface_format.format;}
    VkColorSpaceKHR get_color_space() {return swap_chain_surface_format.colorSpace;}

    VkRenderPass get_default_render_pass() {return default_render_pass;}

    VkQueue get_graphics_queue() {return graphics_queue;}
    uint32_t get_graphics_queue_family_index() {return queue_families.graphics_family.value();}
    VkCommandPool get_graphics_command_pool() {return command_pool;}


    // Swap chain functions (only valid from `init_swap_chain_resources` to `release_swap_chain_resources`)
    //=====================================================================================================

    // See: `get_nr_concurrent_frames()`
    uint32_t get_nr_concurrent_images() {return image_count;}

    // Get image size
    VkExtent2D get_image_extent() {return swap_chain_extent;}


    // Frame functions (only valid after `start_next_frame` and before `frame_ready` is called)
    //=========================================================================================

    uint32_t get_current_image_index() {return image_index;}
    uint32_t get_current_frame_index() {return frame_index;}

    VkImage get_current_image() {return image_resources[image_index].image;}
    VkImageView get_current_image_view() {return image_resources[image_index].image_view;}

    VkCommandBuffer get_current_command_buffer() {return image_resources[image_index].command_buffer;}
    VkFramebuffer get_current_frame_buffer() {return image_resources[image_index].framebuffer;}

    // Should be called every time `start_next_frame` is called after finishing frame commands
    void frame_ready() {end_frame();}
    

protected:
    void exposeEvent(QExposeEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool event(QEvent* event) override;


private:
    void init_resources();
    void init_swap_chain_resources();
    void release_swap_chain_resources();
    void release_resources();

    void begin_frame();
    void end_frame();

    // Vulkan Extension Functions
    void resolve_instance_extension_functions();
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;

    void resolve_device_extension_functions();
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;


    enum class Status {
        Uninitialized = 0,
        Device_Ready = 1,
        Ready = 2
    };
    Status status = Status::Uninitialized;

    AbstractVulkanRenderer* vulkan_renderer;

    // Pre-init Resources (only valid before `init_resources`. In practice: set from `pre_init_resources`)
    //====================================================================================================

    PhysicalDeviceRater physical_device_rater = nullptr;
    VkPhysicalDeviceFeatures requested_physical_device_features{};


    // Resource Initialization (only valid from `init_resources` to `release_resources`)
    //==================================================================================

    VulkanData vkd{};

    void create_surface();
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };
    SwapChainSupportDetails query_swap_chain_support_details(VkPhysicalDevice device);

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete() {
            return graphics_family.has_value() && present_family.has_value();
        }
    };
    QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
    QueueFamilyIndices queue_families{};

    std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool check_device_extension_support(VkPhysicalDevice device);
    int rate_device_suitability(VkPhysicalDevice device);
    void pick_physical_device();

    void create_logical_device();
    VkPhysicalDeviceFeatures physical_device_features{};

    void create_queues();
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    void create_command_pool();
    VkCommandPool command_pool = VK_NULL_HANDLE;

    SwapChainSupportDetails swap_chain_support_details{}; // Valid from resource initialization but needs to be updated on swapchain creation

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkSurfaceFormatKHR swap_chain_surface_format;

    // Returns the index of the first supported format
    // Will return -1 if a supported format cannot be found
    int find_supported_format(const VkFormat* formats, int nr_candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat find_depth_format();
    // Only checks the format candidates in `find_depth_format`
    // DO NOT USE THIS FUNCTION FOR CHECKING IF A RANDOM FORMAT HAS A STENCIL COMPONENT
    bool has_stencil_component(VkFormat format);
    // Might not have stencil part if unsupported
    VkFormat depth_stencil_format;
    bool has_stencil;

    void create_default_render_pass();
    VkRenderPass default_render_pass = VK_NULL_HANDLE;


    // Swap Chain Initialization (only valid from `init_swap_chain_resources` to `release_swap_chain_resources`)
    //==========================================================================================================

    static constexpr uint32_t nr_frames_in_flight = 2;
    uint32_t image_count = 3;

    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> available_present_modes);
    VkExtent2D get_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
    void create_swap_chain();
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    VkExtent2D swap_chain_extent;

    void create_depth_image();
    Image depth_image{};

    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView image_view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    void get_swap_chain_images();
    void create_frame_buffers();
    std::vector<ImageResources> image_resources;

    struct FrameResources {
        VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
        VkSemaphore render_finished_semaphore = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    std::array<FrameResources, nr_frames_in_flight> frame_resources{};

    void create_sync_objects();


    // Frame Initialization (only valid from `begin_frame` to `end_frame`)
    //====================================================================

    uint32_t image_index;
    uint32_t frame_index = 0;
    void create_command_buffer(VkCommandBuffer& command_buffer);
};

#endif