// Example usage of the Model class
// This file demonstrates how to load and render a glTF model

#include "Apple.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"
using namespace Apple;

// Example shader source code (simplified PBR shader)
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vColor;

void main()
{
    vec4 worldPos = uModelMatrix * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModelMatrix) * aNormal;
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uProjectionMatrix * uViewMatrix * worldPos;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vColor;

uniform vec4 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform sampler2D uAlbedoTexture;
uniform int uUseAlbedoTexture;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 albedo = uAlbedo.rgb;

    if (uUseAlbedoTexture > 0)
    {
        albedo = texture(uAlbedoTexture, vTexCoord).rgb;
    }

    // Simple directional light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(N, lightDir), 0.0);

    vec3 color = albedo * (0.2 + 0.8 * NdotL);
    FragColor = vec4(color, uAlbedo.a);
}
)";

class ModelViewerApp : public Application
{
public:
    ModelViewerApp()
        : m_Model(nullptr)
        , m_ShaderProgram(0)
        , m_RotationX(0.0f)
        , m_RotationY(0.0f)
        , m_Zoom(-5.0f)
        , m_PanX(0.0f)
        , m_PanY(0.0f)
        , m_IsLeftDragging(false)
        , m_IsRightDragging(false)
        , m_IsMiddleDragging(false)
    {
    }

    ~ModelViewerApp() override
    {
        Cleanup();
    }

    bool Initialize()
    {
        // Create shader program
        m_ShaderProgram = CreateShaderProgram();
        if (m_ShaderProgram == 0)
        {
            APPLE_CORE_ERROR("Failed to create shader program");
            return false;
        }

        return true;
    }

    void LoadModel(const std::string& filepath)
    {
        m_Model = std::make_unique<Apple::Model>();

        if (!m_Model->LoadFromFile(filepath))
        {
            APPLE_CORE_ERROR("Failed to load model: {}", m_Model->GetLastError());
            m_Model.reset();
            return;
        }

        APPLE_CORE_INFO("Loaded model: {} meshes, {} materials",
            m_Model->GetMeshes().size(),
            m_Model->GetMaterials().size());
    }

    void RenderModel(const glm::mat4& view, const glm::mat4& projection)
    {
        if (!m_Model || !m_Model->IsLoaded())
            return;

        glUseProgram(m_ShaderProgram);

        glm::mat4 model = glm::mat4(1.0f);

        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "uModelMatrix"), 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "uViewMatrix"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "uProjectionMatrix"), 1, GL_FALSE, &projection[0][0]);

        // Render each mesh with its material
        for (size_t meshIdx = 0; meshIdx < m_Model->GetMeshes().size(); ++meshIdx)
        {
            const auto& mesh = m_Model->GetMeshes()[meshIdx];

            for (size_t primIdx = 0; primIdx < mesh.Primitives.size(); ++primIdx)
            {
                const auto& prim = mesh.Primitives[primIdx];

                if (prim.MaterialIndex >= 0 && prim.MaterialIndex < static_cast<int32_t>(m_Model->GetMaterials().size()))
                {
                    const auto& material = m_Model->GetMaterials()[prim.MaterialIndex];

                    glUniform4fv(glGetUniformLocation(m_ShaderProgram, "uAlbedo"), 1, &material.Albedo[0]);
                    glUniform1f(glGetUniformLocation(m_ShaderProgram, "uMetallic"), material.Metallic);
                    glUniform1f(glGetUniformLocation(m_ShaderProgram, "uRoughness"), material.Roughness);

                    // Bind texture if available
                    if (material.AlbedoTexture >= 0 && material.AlbedoTexture < static_cast<int32_t>(m_Model->GetTextures().size()))
                    {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, m_Model->GetTextures()[material.AlbedoTexture].TextureID);
                        glUniform1i(glGetUniformLocation(m_ShaderProgram, "uAlbedoTexture"), 0);
                        glUniform1i(glGetUniformLocation(m_ShaderProgram, "uUseAlbedoTexture"), 1);
                    }
                    else
                    {
                        glUniform1i(glGetUniformLocation(m_ShaderProgram, "uUseAlbedoTexture"), 0);
                    }
                }

                m_Model->RenderPrimitive(static_cast<uint32_t>(meshIdx), static_cast<uint32_t>(primIdx));
            }
        }

        // Restore GL state for ImGui
        glUseProgram(0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

protected:
    void OnRender() override
    {
        // Create view and projection matrices with mouse-controlled rotation and pan
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, m_Zoom));
        view = glm::rotate(view, m_RotationX, glm::vec3(1.0f, 0.0f, 0.0f));
        view = glm::rotate(view, m_RotationY, glm::vec3(0.0f, 1.0f, 0.0f));
        view = glm::translate(view, glm::vec3(m_PanX, m_PanY, 0.0f));

        // Use current window size for aspect ratio
        float aspect = (GetWindowHeight() > 0) ?
            static_cast<float>(GetWindowWidth()) / GetWindowHeight() : 16.0f / 9.0f;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        RenderModel(view, projection);
    }

    void OnMouseButton(const MouseButtonArgs& args) override
    {
        // Don't handle mouse if ImGui is using it
        if (IsImGuiCapturingMouse())
            return;

        if (args.Button == GLFW_MOUSE_BUTTON_LEFT)
        {
            m_IsLeftDragging = (args.Action == GLFW_PRESS);
        }
        if (args.Button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            m_IsRightDragging = (args.Action == GLFW_PRESS);
        }
        if (args.Button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            m_IsMiddleDragging = (args.Action == GLFW_PRESS);
        }
    }

    void OnMouseMove(const MouseMoveArgs& args) override
    {
        // Don't handle mouse if ImGui is using it
        if (IsImGuiCapturingMouse())
            return;

        if (m_IsLeftDragging)
        {
            m_RotationY += static_cast<float>(args.DeltaX) * 0.01f;
            m_RotationX += static_cast<float>(args.DeltaY) * 0.01f;
        }
        if (m_IsRightDragging)
        {
            // Move along Z axis (forward/backward)
            m_Zoom += static_cast<float>(args.DeltaY) * 0.05f;
            if (m_Zoom < -50.0f) m_Zoom = -50.0f;
            if (m_Zoom > -1.0f) m_Zoom = -1.0f;
        }
        if (m_IsMiddleDragging)
        {
            // Pan in XY plane
            float panSpeed = -m_Zoom * 0.002f;
            m_PanX += static_cast<float>(args.DeltaX) * panSpeed;
            m_PanY -= static_cast<float>(args.DeltaY) * panSpeed;
        }
    }

    void OnScroll(const ScrollArgs& args) override
    {
        // Don't handle mouse if ImGui is using it
        if (IsImGuiCapturingMouse())
            return;

        m_Zoom += static_cast<float>(args.YOffset) * 0.5f;
        if (m_Zoom < -50.0f) m_Zoom = -50.0f;
        if (m_Zoom > -1.0f) m_Zoom = -1.0f;
    }

    void OnImGuiRender() override
    {
        // Call parent to show demo window
        Application::OnImGuiRender();

        ImGui::Begin("Model Viewer");

        // Camera controls info
        ImGui::Text("Camera Controls:");
        ImGui::BulletText("Left Drag: Rotate");
        ImGui::BulletText("Middle Drag: Pan");
        ImGui::BulletText("Right Drag: Move Z");
        ImGui::BulletText("Scroll: Zoom");
        ImGui::Text("Window: %dx%d", GetWindowWidth(), GetWindowHeight());
        ImGui::Text("Rotation: X=%.2f, Y=%.2f", m_RotationX, m_RotationY);
        ImGui::Text("Pan: X=%.2f, Y=%.2f", m_PanX, m_PanY);
        ImGui::Text("Zoom: %.2f", m_Zoom);
        ImGui::Separator();

        if (m_Model && m_Model->IsLoaded())
        {
            ImGui::Text("Model: %s", m_Model->GetFilePath().c_str());
            ImGui::Text("Meshes: %zu", m_Model->GetMeshes().size());
            ImGui::Text("Materials: %zu", m_Model->GetMaterials().size());
            ImGui::Text("Textures: %zu", m_Model->GetTextures().size());

            if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
            {
                for (size_t i = 0; i < m_Model->GetMeshes().size(); ++i)
                {
                    const auto& mesh = m_Model->GetMeshes()[i];
                    ImGui::BulletText("Mesh %zu: %s (%zu primitives)",
                        i, mesh.Name.c_str(), mesh.Primitives.size());
                }
            }
        }
        else
        {
            ImGui::Text("No model loaded");
        }

        ImGui::End();
    }

private:
    void Cleanup()
    {
        if (m_ShaderProgram)
        {
            glDeleteProgram(m_ShaderProgram);
            m_ShaderProgram = 0;
        }
        m_Model.reset();
    }

    GLuint CreateShaderProgram()
    {
        // Compile vertex shader
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        // Check for compile errors
        GLint success;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
            APPLE_CORE_ERROR("Vertex shader compilation failed: {}", infoLog);
            return 0;
        }

        // Compile fragment shader
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
            APPLE_CORE_ERROR("Fragment shader compilation failed: {}", infoLog);
            glDeleteShader(vertexShader);
            return 0;
        }

        // Link shaders
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            APPLE_CORE_ERROR("Shader program linking failed: {}", infoLog);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }

private:
    std::unique_ptr<Apple::Model> m_Model;
    GLuint m_ShaderProgram;

    // Camera control
    float m_RotationX;
    float m_RotationY;
    float m_Zoom;
    float m_PanX;
    float m_PanY;
    bool m_IsLeftDragging;
    bool m_IsRightDragging;
    bool m_IsMiddleDragging;
};

// Entry point
Apple::Application* Apple::CreateApplication()
{
	auto* app = new ModelViewerApp();
	app->Initialize();
	app->LoadModel("E:\\AI_coding\\1.7.1\\RTXPT\\Assets\\Models\\glTF-Sample-Models\\2.0\\DragonAttenuation\\glTF-Binary\\DragonAttenuation.glb"); // Specify your model path here
	return app;
}
