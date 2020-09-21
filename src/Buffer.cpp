#include "Buffer.hpp"

VkResult Buffer::create(VulkanData vkd, const CreateData& bcd) {
    this->vkd = vkd;
    this->bcd = bcd;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.flags = bcd.create_flags;
    buffer_info.size = bcd.size;
    buffer_info.usage = bcd.usage;
    buffer_info.sharingMode = bcd.sharing_mode;

    VkResult res = vkd.vkdf->vkCreateBuffer(vkd.device, &buffer_info, nullptr, &buffer);
    if (res != VK_SUCCESS)
        return res;

    VkMemoryRequirements memory_requirements;
    vkd.vkdf->vkGetBufferMemoryRequirements(vkd.device, buffer, &memory_requirements);

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = find_memory_type(vkd, memory_requirements.memoryTypeBits, bcd.memory_properties);
    if (allocation_info.memoryTypeIndex == uint32_t(-1)) {
        qWarning("Failed to find suitable memory type index.");
        return VK_ERROR_UNKNOWN;
    }

    res = vkd.vkdf->vkAllocateMemory(vkd.device, &allocation_info, nullptr, &buffer_memory);
    if (res != VK_SUCCESS)
        return res;

    vkd.vkdf->vkBindBufferMemory(vkd.device, buffer, buffer_memory, 0);
    return VK_SUCCESS;
}

void Buffer::destroy() {
    vkd.vkdf->vkDestroyBuffer(vkd.device, buffer, nullptr);
    buffer = VK_NULL_HANDLE;
    vkd.vkdf->vkFreeMemory(vkd.device, buffer_memory, nullptr);
    buffer_memory = VK_NULL_HANDLE;

    vkd = VulkanData{};
    bcd = CreateData{};
}

Buffer Buffer::copy_data_to_buffer(VulkanData vkd, VkBuffer buffer, const void* data, VkDeviceSize size, VkCommandBuffer command_buffer) {
    Buffer staging_buffer{};
    staging_buffer.create(vkd, CreateData{size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

    void* buffer_data;
    vkd.vkdf->vkMapMemory(vkd.device, staging_buffer.get_vk_buffer_memory(), 0, size, 0, &buffer_data);
    memcpy(buffer_data, data, size);
    vkd.vkdf->vkUnmapMemory(vkd.device, staging_buffer.get_vk_buffer_memory());

    VkBufferCopy copy_region{0, 0, size};
    vkd.vkdf->vkCmdCopyBuffer(command_buffer, staging_buffer.get_vk_buffer(), buffer, 1, &copy_region);

    return staging_buffer;
}

Buffer Buffer::copy_data_to_buffer(const void* data, VkDeviceSize size, VkCommandBuffer command_buffer) {
    return Buffer::copy_data_to_buffer(vkd, buffer, data, size, command_buffer);
}