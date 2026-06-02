#include "Model.h"
#include "Log.h"

// Use tinygltf implementation with embedded stb_image
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <algorithm>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define MAX_TEXTURE_UNITS 16

// Use stb_image functions from tinygltf's stb_image.h
#define STB_IMAGE_STATIC
#include "stb_image.h"

namespace Apple
{
    Model::Model() = default;

    Model::~Model()
    {
        Unload();
    }

    bool Model::LoadFromFile(const std::string& filepath)
    {
        if (filepath.empty())
        {
            m_LastError = "File path is empty";
            return false;
        }

        m_FilePath = filepath;

        // Determine file type by extension
        if (filepath.size() > 4)
        {
            std::string ext = filepath.substr(filepath.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".glb")
            {
                return LoadFromBinary(filepath);
            }
        }

        return LoadFromText(filepath);
    }

    bool Model::LoadFromText(const std::string& filepath)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err;
        std::string warn;

        bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

        if (!warn.empty())
        {
            APPLE_CORE_WARN("glTF warning: {}", warn);
        }

        if (!err.empty())
        {
            m_LastError = err;
            APPLE_CORE_ERROR("glTF error: {}", err);
        }

        if (!res)
        {
            m_LastError = "Failed to load glTF file";
            APPLE_CORE_ERROR("Failed to load glTF: {}", filepath);
            return false;
        }

        return ProcessDocument(model);
    }

    bool Model::LoadFromBinary(const std::string& filepath)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err;
        std::string warn;

        bool res = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);

        if (!warn.empty())
        {
            APPLE_CORE_WARN("glTF binary warning: {}", warn);
        }

        if (!err.empty())
        {
            m_LastError = err;
            APPLE_CORE_ERROR("glTF binary error: {}", err);
        }

        if (!res)
        {
            m_LastError = "Failed to load glTF binary file";
            APPLE_CORE_ERROR("Failed to load glTF binary: {}", filepath);
            return false;
        }

        return ProcessDocument(model);
    }

    void Model::Unload()
    {
        // Delete OpenGL resources
        for (auto& mesh : m_Meshes)
        {
            for (auto& prim : mesh.Primitives)
            {
                if (prim.VAO)
                    glDeleteVertexArrays(1, &prim.VAO);
                if (prim.VBO)
                    glDeleteBuffers(1, &prim.VBO);
                if (prim.EBO)
                    glDeleteBuffers(1, &prim.EBO);
            }
        }

        for (auto& tex : m_Textures)
        {
            if (tex.TextureID)
                glDeleteTextures(1, &tex.TextureID);
        }

        m_Meshes.clear();
        m_Materials.clear();
        m_Textures.clear();
        m_Nodes.clear();
        m_Loaded = false;
        m_FilePath.clear();
    }

    bool Model::ProcessDocument(const tinygltf::Model& model)
    {
        Unload();

        // Get base directory for external resources
        size_t lastSlash = m_FilePath.find_last_of("/\\");
        std::string baseDir = (lastSlash != std::string::npos) ?
            m_FilePath.substr(0, lastSlash + 1) : "";

        m_DefaultScene = model.defaultScene;

        // Load materials first
        if (!LoadMaterials(model))
        {
            APPLE_CORE_WARN("Failed to load materials");
        }

        // Load textures
        if (!LoadTextures(model, baseDir))
        {
            APPLE_CORE_WARN("Failed to load textures");
        }

        // Load meshes
        if (!LoadMeshes(model))
        {
            m_LastError = "Failed to load meshes";
            return false;
        }

        // Load scene nodes
        if (!LoadNodes(model))
        {
            APPLE_CORE_WARN("Failed to load nodes");
        }

        m_Loaded = true;
        APPLE_CORE_INFO("Successfully loaded glTF model: {} meshes, {} materials, {} textures",
            m_Meshes.size(), m_Materials.size(), m_Textures.size());

        return true;
    }

    bool Model::LoadMaterials(const tinygltf::Model& model)
    {
        m_Materials.reserve(model.materials.size());

        for (const auto& gltfMat : model.materials)
        {
            MaterialData mat{};

            // PBR Metallic-Roughness
            auto baseColorIt = gltfMat.values.find("baseColorFactor");
            if (baseColorIt != gltfMat.values.end())
            {
                const auto& color = baseColorIt->second.ColorFactor();
                mat.Albedo = glm::vec4(color[0], color[1], color[2], color[3]);
            }
            else
            {
                mat.Albedo = glm::vec4(1.0f);
            }

            auto metallicIt = gltfMat.values.find("metallicFactor");
            if (metallicIt != gltfMat.values.end())
            {
                mat.Metallic = static_cast<float>(metallicIt->second.Factor());
            }
            else
            {
                mat.Metallic = 1.0f;
            }

            auto roughnessIt = gltfMat.values.find("roughnessFactor");
            if (roughnessIt != gltfMat.values.end())
            {
                mat.Roughness = static_cast<float>(roughnessIt->second.Factor());
            }
            else
            {
                mat.Roughness = 1.0f;
            }

            auto baseColorTexIt = gltfMat.values.find("baseColorTexture");
            if (baseColorTexIt != gltfMat.values.end())
            {
                mat.AlbedoTexture = baseColorTexIt->second.TextureIndex();
            }

            auto metallicRoughnessTexIt = gltfMat.values.find("metallicRoughnessTexture");
            if (metallicRoughnessTexIt != gltfMat.values.end())
            {
                mat.MetallicRoughnessTexture = metallicRoughnessTexIt->second.TextureIndex();
            }

            // Normal texture
            auto normalTexIt = gltfMat.additionalValues.find("normalTexture");
            if (normalTexIt != gltfMat.additionalValues.end())
            {
                mat.NormalTexture = normalTexIt->second.TextureIndex();
            }

            // Occlusion texture
            auto occlusionTexIt = gltfMat.additionalValues.find("occlusionTexture");
            if (occlusionTexIt != gltfMat.additionalValues.end())
            {
                mat.OcclusionTexture = occlusionTexIt->second.TextureIndex();
            }

            // Emissive texture
            auto emissiveTexIt = gltfMat.additionalValues.find("emissiveTexture");
            if (emissiveTexIt != gltfMat.additionalValues.end())
            {
                mat.EmissiveTexture = emissiveTexIt->second.TextureIndex();
            }

            // Alpha mode
            auto alphaModeIt = gltfMat.additionalValues.find("alphaMode");
            if (alphaModeIt != gltfMat.additionalValues.end())
            {
                std::string alphaMode = alphaModeIt->second.string_value;
                if (alphaMode == "OPAQUE")
                    mat.AlphaMode = 0;
                else if (alphaMode == "MASK")
                    mat.AlphaMode = 1;
                else if (alphaMode == "BLEND")
                    mat.AlphaMode = 2;
            }

            auto alphaCutoffIt = gltfMat.additionalValues.find("alphaCutoff");
            if (alphaCutoffIt != gltfMat.additionalValues.end())
            {
                mat.AlphaCutoff = static_cast<float>(alphaCutoffIt->second.Factor());
            }

            auto doubleSidedIt = gltfMat.additionalValues.find("doubleSided");
            if (doubleSidedIt != gltfMat.additionalValues.end())
            {
                mat.DoubleSided = doubleSidedIt->second.bool_value;
            }

            m_Materials.push_back(std::move(mat));
        }

        // Add default material if none exist
        if (m_Materials.empty())
        {
            m_Materials.emplace_back();
        }

        return true;
    }

    bool Model::LoadTextures(const tinygltf::Model& model, const std::string& baseDir)
    {
        m_Textures.reserve(model.textures.size());
        APPLE_CORE_INFO("Loading textures from base directory: {}", baseDir);

        for (const auto& gltfTex : model.textures)
        {
            TextureData tex{};

            tex.SamplerIndex = gltfTex.sampler;

            if (gltfTex.source >= 0 && gltfTex.source < static_cast<int32_t>(model.images.size()))
            {
                const auto& image = model.images[gltfTex.source];
                tex.TextureID = LoadTextureImage(model, image, baseDir);
            }

            m_Textures.push_back(std::move(tex));
        }

        return true;
    }

    bool Model::LoadMeshes(const tinygltf::Model& model)
    {
        m_Meshes.reserve(model.meshes.size());

        for (const auto& gltfMesh : model.meshes)
        {
            MeshData meshData{};
            meshData.Name = gltfMesh.name;

            for (const auto& gltfPrim : gltfMesh.primitives)
            {
                MeshPrimitive prim{};

                if (!CreatePrimitive(model, gltfPrim, prim))
                {
                    APPLE_CORE_ERROR("Failed to create primitive for mesh: {}", gltfMesh.name);
                    continue;
                }

                prim.MaterialIndex = gltfPrim.material >= 0 ? gltfPrim.material : 0;
                meshData.Primitives.push_back(std::move(prim));
            }

            m_Meshes.push_back(std::move(meshData));
        }

        return !m_Meshes.empty();
    }

    bool Model::LoadNodes(const tinygltf::Model& model)
    {
        m_Nodes.reserve(model.nodes.size());

        // First pass: create all nodes
        for (const auto& gltfNode : model.nodes)
        {
            Node node{};

            node.Name = gltfNode.name;
            node.MeshIndex = gltfNode.mesh;
            node.Children = gltfNode.children;

            // Get transform
            node.LocalTransform = GetNodeTransform(gltfNode);

            // Extract TRS from matrix for easier manipulation
            if (gltfNode.matrix.empty())
            {
                // Use individual TRS components
                if (gltfNode.translation.size() >= 3)
                {
                    node.Translation = glm::vec3(
                        static_cast<float>(gltfNode.translation[0]),
                        static_cast<float>(gltfNode.translation[1]),
                        static_cast<float>(gltfNode.translation[2])
                    );
                }

                if (gltfNode.rotation.size() >= 4)
                {
                    node.Rotation = glm::quat(
                        static_cast<float>(gltfNode.rotation[3]),
                        static_cast<float>(gltfNode.rotation[0]),
                        static_cast<float>(gltfNode.rotation[1]),
                        static_cast<float>(gltfNode.rotation[2])
                    );
                }

                if (gltfNode.scale.size() >= 3)
                {
                    node.Scale = glm::vec3(
                        static_cast<float>(gltfNode.scale[0]),
                        static_cast<float>(gltfNode.scale[1]),
                        static_cast<float>(gltfNode.scale[2])
                    );
                }
            }
            else
            {
                // Decompose matrix to get TRS
                glm::mat4 mat = node.LocalTransform;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(mat, node.Scale, node.Rotation, node.Translation, skew, perspective);
            }

            m_Nodes.push_back(std::move(node));
        }

        return true;
    }

    bool Model::CreatePrimitive(const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        MeshPrimitive& outPrimitive)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        if (!ExtractVertices(model, primitive, vertices))
        {
            m_LastError = "Failed to extract vertices";
            return false;
        }

        if (!ExtractIndices(model, primitive, indices))
        {
            m_LastError = "Failed to extract indices";
            return false;
        }

        outPrimitive.IndexCount = static_cast<uint32_t>(indices.size());

        // Create OpenGL buffers
        glGenVertexArrays(1, &outPrimitive.VAO);
        glGenBuffers(1, &outPrimitive.VBO);
        glGenBuffers(1, &outPrimitive.EBO);

        glBindVertexArray(outPrimitive.VAO);

        // Upload vertex data
        glBindBuffer(GL_ARRAY_BUFFER, outPrimitive.VBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(),
            GL_STATIC_DRAW);

        // Upload index data
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outPrimitive.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(uint32_t),
            indices.data(),
            GL_STATIC_DRAW);

        // Set vertex attributes
        // Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, Position));

        // Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        // TexCoord
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, TexCoord));

        // Color
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, Color));

        // Tangent
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

        glBindVertexArray(0);

        return true;
    }

    bool Model::ExtractVertices(const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<Vertex>& outVertices)
    {
        // Determine vertex count from position accessor
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end())
        {
            m_LastError = "Primitive has no POSITION attribute";
            return false;
        }

        int32_t posAccessorIdx = posIt->second;
        if (posAccessorIdx < 0 || posAccessorIdx >= static_cast<int32_t>(model.accessors.size()))
        {
            m_LastError = "Invalid POSITION accessor index";
            return false;
        }

        const auto& posAccessor = model.accessors[posAccessorIdx];
        size_t vertexCount = posAccessor.count;
        outVertices.resize(vertexCount);

        // Extract positions
        std::vector<float> positions;
        if (GetAccessorData(model, posAccessorIdx, positions))
        {
            for (size_t i = 0; i < vertexCount; ++i)
            {
                if (i * 3 + 2 < positions.size())
                {
                    outVertices[i].Position = glm::vec3(
                        positions[i * 3],
                        positions[i * 3 + 1],
                        positions[i * 3 + 2]
                    );
                }
            }
        }

        // Extract normals
        auto normalIt = primitive.attributes.find("NORMAL");
        if (normalIt != primitive.attributes.end())
        {
            std::vector<float> normals;
            if (GetAccessorData(model, normalIt->second, normals))
            {
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    if (i * 3 + 2 < normals.size())
                    {
                        outVertices[i].Normal = glm::vec3(
                            normals[i * 3],
                            normals[i * 3 + 1],
                            normals[i * 3 + 2]
                        );
                    }
                }
            }
        }

        // Extract tex coords
        auto texCoordIt = primitive.attributes.find("TEXCOORD_0");
        if (texCoordIt != primitive.attributes.end())
        {
            std::vector<float> texCoords;
            if (GetAccessorData(model, texCoordIt->second, texCoords))
            {
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    if (i * 2 + 1 < texCoords.size())
                    {
                        outVertices[i].TexCoord = glm::vec2(
                            texCoords[i * 2],
                            texCoords[i * 2 + 1]
                        );
                    }
                }
            }
        }

        // Extract colors
        auto colorIt = primitive.attributes.find("COLOR_0");
        if (colorIt != primitive.attributes.end())
        {
            // Try vec4 first
            std::vector<float> colors;
            if (GetAccessorData(model, colorIt->second, colors))
            {
                const auto& colorAccessor = model.accessors[colorIt->second];
                bool isVec4 = (colorAccessor.type == TINYGLTF_TYPE_VEC4);

                for (size_t i = 0; i < vertexCount; ++i)
                {
                    if (isVec4 && i * 4 + 3 < colors.size())
                    {
                        outVertices[i].Color = glm::vec4(
                            colors[i * 4],
                            colors[i * 4 + 1],
                            colors[i * 4 + 2],
                            colors[i * 4 + 3]
                        );
                    }
                    else if (i * 3 + 2 < colors.size())
                    {
                        outVertices[i].Color = glm::vec4(
                            colors[i * 3],
                            colors[i * 3 + 1],
                            colors[i * 3 + 2],
                            1.0f
                        );
                    }
                }
            }
        }

        // Extract tangents
        auto tangentIt = primitive.attributes.find("TANGENT");
        if (tangentIt != primitive.attributes.end())
        {
            std::vector<float> tangents;
            if (GetAccessorData(model, tangentIt->second, tangents))
            {
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    if (i * 4 + 3 < tangents.size())
                    {
                        outVertices[i].Tangent = glm::vec4(
                            tangents[i * 4],
                            tangents[i * 4 + 1],
                            tangents[i * 4 + 2],
                            tangents[i * 4 + 3]
                        );
                    }
                }
            }
        }

        return true;
    }

    bool Model::ExtractIndices(const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<uint32_t>& outIndices)
    {
        if (primitive.indices < 0)
        {
            // No indices, generate sequential indices
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt != primitive.attributes.end())
            {
                const auto& posAccessor = model.accessors[posIt->second];
                outIndices.resize(posAccessor.count);
                for (size_t i = 0; i < posAccessor.count; ++i)
                {
                    outIndices[i] = static_cast<uint32_t>(i);
                }
                return true;
            }
            return false;
        }

        // Try different index types
        std::vector<uint8_t> indices8;
        std::vector<uint16_t> indices16;
        std::vector<uint32_t> indices32;

        const auto& accessor = model.accessors[primitive.indices];

        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            if (GetAccessorData(model, primitive.indices, indices8))
            {
                outIndices.assign(indices8.begin(), indices8.end());
                return true;
            }
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            if (GetAccessorData(model, primitive.indices, indices16))
            {
                outIndices.assign(indices16.begin(), indices16.end());
                return true;
            }
            break;

        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            if (GetAccessorData(model, primitive.indices, indices32))
            {
                outIndices = indices32;
                return true;
            }
            break;

        default:
            m_LastError = "Unsupported index component type";
            return false;
        }

        return false;
    }

    template<typename T>
    bool Model::GetAccessorData(const tinygltf::Model& model,
        int32_t accessorIndex,
        std::vector<T>& outData)
    {
        if (accessorIndex < 0 || accessorIndex >= static_cast<int32_t>(model.accessors.size()))
        {
            return false;
        }

        const auto& accessor = model.accessors[accessorIndex];

        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int32_t>(model.bufferViews.size()))
        {
            return false;
        }

        const auto& bufferView = model.bufferViews[accessor.bufferView];

        if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int32_t>(model.buffers.size()))
        {
            return false;
        }

        const auto& buffer = model.buffers[bufferView.buffer];
        const uint8_t* data = buffer.data.data();

        size_t offset = bufferView.byteOffset + accessor.byteOffset;
        size_t count = accessor.count;
        size_t stride = bufferView.byteStride;

        // Determine element size from accessor type
        size_t elementSize;
        switch (accessor.type)
        {
        case TINYGLTF_TYPE_SCALAR:
            elementSize = 1;
            break;
        case TINYGLTF_TYPE_VEC2:
            elementSize = 2;
            break;
        case TINYGLTF_TYPE_VEC3:
            elementSize = 3;
            break;
        case TINYGLTF_TYPE_VEC4:
            elementSize = 4;
            break;
        case TINYGLTF_TYPE_MAT2:
            elementSize = 4;
            break;
        case TINYGLTF_TYPE_MAT3:
            elementSize = 9;
            break;
        case TINYGLTF_TYPE_MAT4:
            elementSize = 16;
            break;
        default:
            return false;
        }

        // Determine component size
        size_t componentSize = 0;
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            componentSize = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            componentSize = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            componentSize = 4;
            break;
        default:
            return false;
        }

        size_t totalElements = count * elementSize;
        outData.resize(totalElements);

        if (stride == 0)
        {
            stride = elementSize * componentSize;
        }

        // Copy data
        const uint8_t* src = data + offset;
        for (size_t i = 0; i < count; ++i)
        {
            std::memcpy(&outData[i * elementSize], src + i * stride, elementSize * componentSize);
        }

        return true;
    }

    glm::mat4 Model::GetNodeTransform(const tinygltf::Node& node) const
    {
        if (!node.matrix.empty())
        {
            glm::mat4 mat(1.0f);
            for (int i = 0; i < 16; ++i)
            {
                glm::value_ptr(mat)[i] = static_cast<float>(node.matrix[i]);
            }
            return mat;
        }

        glm::vec3 translation(0.0f);
        if (!node.translation.empty())
        {
            translation = glm::vec3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            );
        }

        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        if (!node.rotation.empty())
        {
            rotation = glm::quat(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2])
            );
        }

        glm::vec3 scale(1.0f);
        if (!node.scale.empty())
        {
            scale = glm::vec3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            );
        }

        glm::mat4 mat = glm::mat4(1.0f);
        mat = glm::translate(mat, translation);
        mat = mat * glm::mat4_cast(rotation);
        mat = glm::scale(mat, scale);

        return mat;
    }

    glm::mat4 Model::ComputeNodeWorldTransform(uint32_t nodeIndex) const
    {
        if (nodeIndex >= m_Nodes.size())
        {
            return glm::mat4(1.0f);
        }

        // Simple implementation - just return local transform
        // For full hierarchy support, would need to track parent relationships
        return m_Nodes[nodeIndex].LocalTransform;
    }

    GLuint Model::LoadTextureImage(const tinygltf::Model& model,
        const tinygltf::Image& image,
        const std::string& baseDir)
    {
        int width, height, channels;
        unsigned char* pixels = nullptr;
        std::string textureName = image.name.empty() ? "(unnamed)" : image.name;

        APPLE_CORE_INFO("Loading texture: {}", textureName);

        // Case 1: Image data already loaded by tinygltf
        if (!image.image.empty())
        {
            width = image.width;
            height = image.height;
            channels = image.component;
            pixels = const_cast<unsigned char*>(image.image.data());

            APPLE_CORE_INFO("  -> Pre-loaded image: {}x{}, {} channels", width, height, channels);

            if (pixels && width > 0 && height > 0)
            {
                // Create OpenGL texture from pre-loaded data
                GLuint textureID;
                glGenTextures(1, &textureID);
                glBindTexture(GL_TEXTURE_2D, textureID);

                GLenum format = GL_RGB;
                if (channels == 1)
                    format = GL_RED;
                else if (channels == 2)
                    format = GL_RG;
                else if (channels == 4)
                    format = GL_RGBA;

                GLenum type = GL_UNSIGNED_BYTE;
                if (image.bits == 16)
                    type = GL_UNSIGNED_SHORT;

                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, pixels);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                APPLE_CORE_INFO("  -> Created GL texture ID: {}", textureID);
                return textureID;
            }
        }

        // Case 2: External file
        if (!image.uri.empty() && image.bufferView < 0)
        {
            APPLE_CORE_INFO("  -> External file: {}", image.uri);

            // Extract filename from uri
            size_t lastSlash = image.uri.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                image.uri.substr(lastSlash + 1) : image.uri;

            // Get base name and extension
            size_t dotPos = filename.find_last_of('.');
            std::string baseName = (dotPos != std::string::npos) ?
                filename.substr(0, dotPos) : filename;
            std::string ext = (dotPos != std::string::npos) ?
                filename.substr(dotPos) : "";

            // Convert extension to lowercase
            std::string extLower = ext;
            std::transform(extLower.begin(), extLower.end(),
                extLower.begin(), ::tolower);

            // Use baseDir (glTF file directory) as texture path
            std::string textureBasePath = baseDir;

            // Try different formats
            bool loaded = false;
            std::vector<std::string> extensionsToTry;

            if (extLower == ".dds")
            {
                // Skip DDS, try jpg/png
                extensionsToTry = { ".jpg", ".jpeg", ".png" };
            }
            else
            {
                // Try original first, then alternatives
                extensionsToTry = { ext, ".jpg", ".jpeg", ".png" };
            }

            for (const auto& extToTry : extensionsToTry)
            {
                std::string imagePath = textureBasePath + baseName + extToTry;
                APPLE_CORE_INFO("  -> Trying: {}", imagePath);

                pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
                if (pixels)
                {
                    APPLE_CORE_INFO("  -> Loaded successfully");
                    loaded = true;
                    break;
                }
            }

            if (!loaded || !pixels)
            {
                APPLE_CORE_ERROR("  -> Failed to load from all attempted paths");
                return 0;
            }

            // Create OpenGL texture
            GLuint textureID;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            GLenum format = GL_RGB;
            if (channels == 1)
                format = GL_RED;
            else if (channels == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(pixels);

            APPLE_CORE_INFO("  -> Created GL texture ID: {}", textureID);
            return textureID;
        }

        APPLE_CORE_WARN("  -> No image data available");
        return 0;
    }

    void Model::Render() const
    {
        for (size_t i = 0; i < m_Meshes.size(); ++i)
        {
            RenderMesh(static_cast<uint32_t>(i));
        }
    }

    void Model::RenderMesh(uint32_t meshIndex) const
    {
        if (meshIndex >= m_Meshes.size())
            return;

        const auto& mesh = m_Meshes[meshIndex];
        for (size_t i = 0; i < mesh.Primitives.size(); ++i)
        {
            RenderPrimitive(meshIndex, static_cast<uint32_t>(i));
        }
    }

    void Model::RenderPrimitive(uint32_t meshIndex, uint32_t primitiveIndex) const
    {
        if (meshIndex >= m_Meshes.size())
            return;

        const auto& mesh = m_Meshes[meshIndex];
        if (primitiveIndex >= mesh.Primitives.size())
            return;

        const auto& prim = mesh.Primitives[primitiveIndex];

        if (prim.VAO == 0)
            return;

        glBindVertexArray(prim.VAO);
        glDrawElements(GL_TRIANGLES, prim.IndexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

} // namespace Apple
