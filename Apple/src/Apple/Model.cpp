#include "Model.h"
#include "Log.h"
#include "stb_image.h"
#include <algorithm>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define MAX_TEXTURE_UNITS 16

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
        try
        {
            fx::gltf::ReadQuotas quotas{};
            quotas.MaxBufferCount = 16;
            quotas.MaxBufferByteLength = 128 * 1024 * 1024; // 128MB
            quotas.MaxFileSize = 128 * 1024 * 1024;

            fx::gltf::Document doc = fx::gltf::LoadFromText(filepath, quotas);
            return ProcessDocument(doc);
        }
        catch (const fx::gltf::invalid_gltf_document& ex)
        {
            m_LastError = std::string("Invalid glTF document: ") + ex.what();
            APPLE_CORE_ERROR("Failed to load glTF: {}", m_LastError);
            return false;
        }
        catch (const std::exception& ex)
        {
            m_LastError = std::string("Exception: ") + ex.what();
            APPLE_CORE_ERROR("Failed to load glTF: {}", m_LastError);
            return false;
        }
    }

    bool Model::LoadFromBinary(const std::string& filepath)
    {
        try
        {
            fx::gltf::ReadQuotas quotas{};
            quotas.MaxBufferCount = 16;
            quotas.MaxBufferByteLength = 128 * 1024 * 1024;
            quotas.MaxFileSize = 128 * 1024 * 1024;

            fx::gltf::Document doc = fx::gltf::LoadFromBinary(filepath, quotas);
            return ProcessDocument(doc);
        }
        catch (const fx::gltf::invalid_gltf_document& ex)
        {
            m_LastError = std::string("Invalid glTF document: ") + ex.what();
            APPLE_CORE_ERROR("Failed to load glTF binary: {}", m_LastError);
            return false;
        }
        catch (const std::exception& ex)
        {
            m_LastError = std::string("Exception: ") + ex.what();
            APPLE_CORE_ERROR("Failed to load glTF binary: {}", m_LastError);
            return false;
        }
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

    bool Model::ProcessDocument(const fx::gltf::Document& doc)
    {
        Unload();

        // Get base directory for external resources
        size_t lastSlash = m_FilePath.find_last_of("/\\");
        std::string baseDir = (lastSlash != std::string::npos) ?
            m_FilePath.substr(0, lastSlash + 1) : "";

        m_DefaultScene = doc.scene;

        // Load materials first
        if (!LoadMaterials(doc))
        {
            APPLE_CORE_WARN("Failed to load materials");
        }

        // Load textures
        if (!LoadTextures(doc, baseDir))
        {
            APPLE_CORE_WARN("Failed to load textures");
        }

        // Load meshes
        if (!LoadMeshes(doc))
        {
            m_LastError = "Failed to load meshes";
            return false;
        }

        // Load scene nodes
        if (!LoadNodes(doc))
        {
            APPLE_CORE_WARN("Failed to load nodes");
        }

        m_Loaded = true;
        APPLE_CORE_INFO("Successfully loaded glTF model: {} meshes, {} materials, {} textures",
            m_Meshes.size(), m_Materials.size(), m_Textures.size());

        return true;
    }

    bool Model::LoadMaterials(const fx::gltf::Document& doc)
    {
        m_Materials.reserve(doc.materials.size());

        for (const auto& gltfMat : doc.materials)
        {
            MaterialData mat{};

            // PBR Metallic-Roughness
            if (!gltfMat.pbrMetallicRoughness.empty())
            {
                const auto& pbr = gltfMat.pbrMetallicRoughness;

                if (pbr.baseColorFactor.size() >= 4)
                {
                    mat.Albedo = glm::vec4(
                        pbr.baseColorFactor[0],
                        pbr.baseColorFactor[1],
                        pbr.baseColorFactor[2],
                        pbr.baseColorFactor[3]
                    );
                }

                mat.Metallic = pbr.metallicFactor;
                mat.Roughness = pbr.roughnessFactor;

                if (!pbr.baseColorTexture.empty())
                    mat.AlbedoTexture = pbr.baseColorTexture.index;

                if (!pbr.metallicRoughnessTexture.empty())
                    mat.MetallicRoughnessTexture = pbr.metallicRoughnessTexture.index;
            }

            // Normal texture
            if (!gltfMat.normalTexture.empty())
                mat.NormalTexture = gltfMat.normalTexture.index;

            // Occlusion texture
            if (!gltfMat.occlusionTexture.empty())
                mat.OcclusionTexture = gltfMat.occlusionTexture.index;

            // Emissive
            if (!gltfMat.emissiveTexture.empty())
                mat.EmissiveTexture = gltfMat.emissiveTexture.index;

            // Alpha mode
            mat.AlphaMode = static_cast<int>(gltfMat.alphaMode);
            mat.AlphaCutoff = gltfMat.alphaCutoff;
            mat.DoubleSided = gltfMat.doubleSided;

            m_Materials.push_back(std::move(mat));
        }

        // Add default material if none exist
        if (m_Materials.empty())
        {
            m_Materials.emplace_back();
        }

        return true;
    }

    bool Model::LoadTextures(const fx::gltf::Document& doc, const std::string& baseDir)
    {
        m_Textures.reserve(doc.textures.size());
        APPLE_CORE_INFO("Loading textures from base directory: {}", baseDir);

        for (const auto& gltfTex : doc.textures)
        {
            TextureData tex{};

            tex.SamplerIndex = gltfTex.sampler;

            if (gltfTex.source >= 0 && gltfTex.source < static_cast<int32_t>(doc.images.size()))
            {
                const auto& image = doc.images[gltfTex.source];
                tex.TextureID = LoadTextureImage(doc, image, baseDir);
            }

            m_Textures.push_back(std::move(tex));
        }

        return true;
    }

    bool Model::LoadMeshes(const fx::gltf::Document& doc)
    {
        m_Meshes.reserve(doc.meshes.size());

        for (const auto& gltfMesh : doc.meshes)
        {
            MeshData meshData{};
            meshData.Name = gltfMesh.name;

            for (const auto& gltfPrim : gltfMesh.primitives)
            {
                MeshPrimitive prim{};

                if (!CreatePrimitive(doc, gltfPrim, prim))
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

    bool Model::LoadNodes(const fx::gltf::Document& doc)
    {
        m_Nodes.reserve(doc.nodes.size());

        // First pass: create all nodes
        for (const auto& gltfNode : doc.nodes)
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
                        gltfNode.translation[0],
                        gltfNode.translation[1],
                        gltfNode.translation[2]
                    );
                }

                if (gltfNode.rotation.size() >= 4)
                {
                    node.Rotation = glm::quat(
                        gltfNode.rotation[3],
                        gltfNode.rotation[0],
                        gltfNode.rotation[1],
                        gltfNode.rotation[2]
                    );
                }

                if (gltfNode.scale.size() >= 3)
                {
                    node.Scale = glm::vec3(
                        gltfNode.scale[0],
                        gltfNode.scale[1],
                        gltfNode.scale[2]
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

    bool Model::CreatePrimitive(const fx::gltf::Document& doc,
        const fx::gltf::Primitive& primitive,
        MeshPrimitive& outPrimitive)
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        if (!ExtractVertices(doc, primitive, vertices))
        {
            m_LastError = "Failed to extract vertices";
            return false;
        }

        if (!ExtractIndices(doc, primitive, indices))
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

    bool Model::ExtractVertices(const fx::gltf::Document& doc,
        const fx::gltf::Primitive& primitive,
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
        if (posAccessorIdx < 0 || posAccessorIdx >= static_cast<int32_t>(doc.accessors.size()))
        {
            m_LastError = "Invalid POSITION accessor index";
            return false;
        }

        const auto& posAccessor = doc.accessors[posAccessorIdx];
        size_t vertexCount = posAccessor.count;
        outVertices.resize(vertexCount);

        // Extract positions
        std::vector<float> positions;
        if (GetAccessorData(doc, posAccessorIdx, positions))
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
            if (GetAccessorData(doc, normalIt->second, normals))
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
            if (GetAccessorData(doc, texCoordIt->second, texCoords))
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
            if (GetAccessorData(doc, colorIt->second, colors))
            {
                const auto& colorAccessor = doc.accessors[colorIt->second];
                bool isVec4 = (colorAccessor.type == fx::gltf::Accessor::Type::Vec4);

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
            if (GetAccessorData(doc, tangentIt->second, tangents))
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

    bool Model::ExtractIndices(const fx::gltf::Document& doc,
        const fx::gltf::Primitive& primitive,
        std::vector<uint32_t>& outIndices)
    {
        if (primitive.indices < 0)
        {
            // No indices, generate sequential indices
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt != primitive.attributes.end())
            {
                const auto& posAccessor = doc.accessors[posIt->second];
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

        const auto& accessor = doc.accessors[primitive.indices];

        switch (accessor.componentType)
        {
        case fx::gltf::Accessor::ComponentType::UnsignedByte:
            if (GetAccessorData(doc, primitive.indices, indices8))
            {
                outIndices.assign(indices8.begin(), indices8.end());
                return true;
            }
            break;

        case fx::gltf::Accessor::ComponentType::UnsignedShort:
            if (GetAccessorData(doc, primitive.indices, indices16))
            {
                outIndices.assign(indices16.begin(), indices16.end());
                return true;
            }
            break;

        case fx::gltf::Accessor::ComponentType::UnsignedInt:
            if (GetAccessorData(doc, primitive.indices, indices32))
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
    bool Model::GetAccessorData(const fx::gltf::Document& doc,
        int32_t accessorIndex,
        std::vector<T>& outData)
    {
        if (accessorIndex < 0 || accessorIndex >= static_cast<int32_t>(doc.accessors.size()))
        {
            return false;
        }

        const auto& accessor = doc.accessors[accessorIndex];

        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int32_t>(doc.bufferViews.size()))
        {
            return false;
        }

        const auto& bufferView = doc.bufferViews[accessor.bufferView];

        if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int32_t>(doc.buffers.size()))
        {
            return false;
        }

        const auto& buffer = doc.buffers[bufferView.buffer];
        const uint8_t* data = buffer.data.data();

        size_t offset = bufferView.byteOffset + accessor.byteOffset;
        size_t count = accessor.count;
        size_t stride = bufferView.byteStride;

        // Determine element size from accessor type
        size_t elementSize;
        switch (accessor.type)
        {
        case fx::gltf::Accessor::Type::Scalar:
            elementSize = 1;
            break;
        case fx::gltf::Accessor::Type::Vec2:
            elementSize = 2;
            break;
        case fx::gltf::Accessor::Type::Vec3:
            elementSize = 3;
            break;
        case fx::gltf::Accessor::Type::Vec4:
        case fx::gltf::Accessor::Type::Mat2:
            elementSize = 4;
            break;
        case fx::gltf::Accessor::Type::Mat3:
            elementSize = 9;
            break;
        case fx::gltf::Accessor::Type::Mat4:
            elementSize = 16;
            break;
        default:
            return false;
        }

        // Determine component size
        size_t componentSize = 0;
        switch (accessor.componentType)
        {
        case fx::gltf::Accessor::ComponentType::Byte:
        case fx::gltf::Accessor::ComponentType::UnsignedByte:
            componentSize = 1;
            break;
        case fx::gltf::Accessor::ComponentType::Short:
        case fx::gltf::Accessor::ComponentType::UnsignedShort:
            componentSize = 2;
            break;
        case fx::gltf::Accessor::ComponentType::UnsignedInt:
        case fx::gltf::Accessor::ComponentType::Float:
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

    glm::mat4 Model::GetNodeTransform(const fx::gltf::Node& node) const
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

    GLuint Model::LoadTextureImage(const fx::gltf::Document& doc,
        const fx::gltf::Image& image,
        const std::string& baseDir)
    {
        int width, height, channels;
        unsigned char* pixels = nullptr;
        std::string textureName = image.name.empty() ? "(unnamed)" : image.name;

        APPLE_CORE_INFO("Loading texture: {}", textureName);

        // Case 1: Embedded in .glb buffer (bufferView >= 0)
        if (image.bufferView >= 0)
        {
            const auto& bufferView = doc.bufferViews[image.bufferView];
            const auto& buffer = doc.buffers[bufferView.buffer];

            const uint8_t* data = buffer.data.data() + bufferView.byteOffset;
            int dataSize = static_cast<int>(bufferView.byteLength);

            APPLE_CORE_INFO("  -> Embedded in GLB buffer, size: {} bytes", dataSize);

            // Check for DDS magic bytes
            if (dataSize >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')
            {
                APPLE_CORE_ERROR("  -> DDS format not supported (stb_image limitation)");
                return 0;
            }

            // Try to load
            pixels = stbi_load_from_memory(data, dataSize, &width, &height, &channels, 0);
            if (pixels)
            {
                APPLE_CORE_INFO("  -> Loaded successfully: {}x{}, {} channels", width, height, channels);
            }
            else
            {
                APPLE_CORE_ERROR("  -> Failed to decode embedded image");
                return 0;
            }
        }
        // Case 2: External or base64 embedded
        else if (!image.uri.empty())
        {
            // Case 2a: Base64 data URI
            if (image.IsEmbeddedResource())
            {
                APPLE_CORE_INFO("  -> Base64 embedded data");

                std::vector<uint8_t> imageData;
                try
                {
                    image.MaterializeData(imageData);
                    APPLE_CORE_INFO("  -> Base64 decoded, size: {} bytes", imageData.size());

                    // Check for DDS magic bytes
                    if (imageData.size() >= 4 &&
                        imageData[0] == 'D' && imageData[1] == 'D' &&
                        imageData[2] == 'S' && imageData[3] == ' ')
                    {
                        APPLE_CORE_ERROR("  -> Base64 data is DDS format (not supported)");
                        return 0;
                    }

                    pixels = stbi_load_from_memory(imageData.data(),
                        static_cast<int>(imageData.size()), &width, &height, &channels, 0);

                    if (!pixels)
                    {
                        APPLE_CORE_ERROR("  -> stb_image failed to decode base64 image (may be unsupported format like DDS/KTX/)");
                        return 0;
                    }

                    APPLE_CORE_INFO("  -> Base64 image loaded: {}x{}, {} channels", width, height, channels);
                }
                catch (const std::exception& e)
                {
                    APPLE_CORE_ERROR("  -> Exception during base64 decode: {}", e.what());
                    return 0;
                }
            }
            // Case 2b: External file
            else
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

                if (!loaded)
                {
                    APPLE_CORE_ERROR("  -> Failed to load from all attempted paths");
                    return 0;
                }
            }
        }
        else
        {
            APPLE_CORE_WARN("  -> No image data (empty bufferView and uri)");
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
