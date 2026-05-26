#pragma once
#include "Core.h"
#include <fx/gltf.h>
#include <glad/gl.h>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace Apple
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
        glm::vec4 Color{ 1.0f };
        glm::vec4 Tangent{ 0.0f };
    };

    struct MeshPrimitive
    {
        GLuint VAO{ 0 };
        GLuint VBO{ 0 };
        GLuint EBO{ 0 };
        uint32_t IndexCount{ 0 };
        uint32_t MaterialIndex{ 0 };
    };

    struct MeshData
    {
        std::string Name;
        std::vector<MeshPrimitive> Primitives;
    };

    struct TextureData
    {
        GLuint TextureID{ 0 };
        int32_t SamplerIndex{ -1 };
    };

    struct MaterialData
    {
        glm::vec4 Albedo{ 1.0f };
        float Metallic{ 1.0f };
        float Roughness{ 1.0f };
        float AlphaCutoff{ 0.5f };
        int AlphaMode{ 0 }; // 0: Opaque, 1: Mask, 2: Blend
        bool DoubleSided{ false };

        int32_t AlbedoTexture{ -1 };
        int32_t NormalTexture{ -1 };
        int32_t MetallicRoughnessTexture{ -1 };
        int32_t OcclusionTexture{ -1 };
        int32_t EmissiveTexture{ -1 };
    };

    struct Node
    {
        std::string Name;
        glm::vec3 Translation{ 0.0f };
        glm::quat Rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale{ 1.0f };
        glm::mat4 LocalTransform{ 1.0f };
        int32_t MeshIndex{ -1 };
        std::vector<int32_t> Children;
    };

    class APPLE_API Model
    {
    public:
        Model();
        ~Model();

        bool LoadFromFile(const std::string& filepath);
        bool LoadFromText(const std::string& filepath);
        bool LoadFromBinary(const std::string& filepath);
        void Unload();

        bool IsLoaded() const { return m_Loaded; }
        const std::string& GetFilePath() const { return m_FilePath; }

        const std::vector<MeshData>& GetMeshes() const { return m_Meshes; }
        const std::vector<MaterialData>& GetMaterials() const { return m_Materials; }
        const std::vector<TextureData>& GetTextures() const { return m_Textures; }
        const std::vector<Node>& GetNodes() const { return m_Nodes; }
        int32_t GetDefaultScene() const { return m_DefaultScene; }

        const std::string& GetLastError() const { return m_LastError; }

        void Render() const;
        void RenderMesh(uint32_t meshIndex) const;
        void RenderPrimitive(uint32_t meshIndex, uint32_t primitiveIndex) const;

    private:
        bool ProcessDocument(const fx::gltf::Document& doc);
        bool LoadMaterials(const fx::gltf::Document& doc);
        bool LoadTextures(const fx::gltf::Document& doc, const std::string& baseDir);
        bool LoadMeshes(const fx::gltf::Document& doc);
        bool LoadNodes(const fx::gltf::Document& doc);

        bool CreatePrimitive(const fx::gltf::Document& doc,
            const fx::gltf::Primitive& primitive,
            MeshPrimitive& outPrimitive);

        bool ExtractVertices(const fx::gltf::Document& doc,
            const fx::gltf::Primitive& primitive,
            std::vector<Vertex>& outVertices);

        bool ExtractIndices(const fx::gltf::Document& doc,
            const fx::gltf::Primitive& primitive,
            std::vector<uint32_t>& outIndices);

        template<typename T>
        bool GetAccessorData(const fx::gltf::Document& doc,
            int32_t accessorIndex,
            std::vector<T>& outData);

        glm::mat4 GetNodeTransform(const fx::gltf::Node& node) const;
        glm::mat4 ComputeNodeWorldTransform(uint32_t nodeIndex) const;

        GLuint LoadTextureImage(const fx::gltf::Document& doc,
            const fx::gltf::Image& image,
            const std::string& baseDir);

    private:
        bool m_Loaded{ false };
        std::string m_FilePath;
        std::string m_LastError;

        std::vector<MeshData> m_Meshes;
        std::vector<MaterialData> m_Materials;
        std::vector<TextureData> m_Textures;
        std::vector<Node> m_Nodes;
        int32_t m_DefaultScene{ -1 };
    };

} // namespace Apple
