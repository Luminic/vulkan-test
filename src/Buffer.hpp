#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <QVulkanInstance>
#include "VulkanFunctions.hpp"

class Buffer {
public:
    class CreateData {
    public:
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags memory_properties;

        VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        VkBufferCreateFlags create_flags = 0;
    };

    VkResult create(VulkanData vkd, const CreateData& bcd);
    void destroy();

    // Creates a host-mappable staging buffer, maps the data into the staging buffer, then copys the data from the staging buffer into the buffer
    // Returns the created staging buffer. It Should be destroyed after the queue is submitted (it will not be automatically destroyed)
    // No synchronization is done
    static Buffer copy_data_to_buffer(VulkanData vkd, VkBuffer buffer, const void* data, VkDeviceSize size, VkCommandBuffer command_buffer);
    Buffer copy_data_to_buffer(const void* data, VkDeviceSize size, VkCommandBuffer command_buffer);


    // Getters

    VkBuffer get_vk_buffer() {return buffer;}
    VkDeviceMemory get_vk_buffer_memory() {return buffer_memory;}
    CreateData get_create_data() {return bcd;}

private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;

    VulkanData vkd{};
    CreateData bcd{};
};

#endif