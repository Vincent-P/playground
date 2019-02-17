#include "tiny_gltf.h"
#include <string>
#include <vulkan/vulkan.hpp>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#include <vk_mem_alloc.h>

namespace my_app
{
    static tinygltf::TinyGLTF loader;

    struct Vertex
    {
        glm::vec3 pos;

        static std::array<vk::VertexInputBindingDescription, 1> getBindingDescriptions()
        {
            std::array<vk::VertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Vertex);
            bindings[0].inputRate = vk::VertexInputRate::eVertex;
            return bindings;
        }

        static std::array<vk::VertexInputAttributeDescription, 1> getAttributeDescriptions()
        {
            std::array<vk::VertexInputAttributeDescription, 1> descs;
            descs[0].binding = 0;
            descs[0].location = 0;
            descs[0].format = vk::Format::eR32G32B32Sfloat;
            descs[0].offset = offsetof(Vertex, pos);
            return descs;
        }
    };

    struct Primitive
    {
        std::uint32_t first_vertex;
        std::uint32_t first_index;
        std::uint32_t index_count;
    };

    class Mesh
    {
        public:
        void draw(vk::CommandBuffer& cmd) const;

        std::vector<Primitive> primitives_;
    };

    class Model
    {
        public:
        Model(std::string);
        ~Model() = default;

        void draw(vk::CommandBuffer& cmd) const;
        void LoadMeshesBuffers();

        tinygltf::Model model_;

        std::vector<Mesh> meshes_;
        std::vector<Vertex> vertices_;
        std::vector<uint32_t> indices_;
    };
}    // namespace my_app
