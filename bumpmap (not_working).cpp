#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
unsigned int loadCubemap(std::vector<std::string> faces);
unsigned int loadTexture(const char* path, bool gammaCorrection = false);
std::string readShaderFile(const char* path);
unsigned int compileShader(GLenum type, const char* source);
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath);
bool loadOBJWithMTL(const char* objPath, 
                    std::vector<std::vector<float>>& verticesArray,
                    std::vector<std::vector<unsigned int>>& indicesArray,
                    std::vector<unsigned int>& textureIDs,
                    std::vector<std::string>& texturePaths);
void calculateTangents(std::vector<float>& vertices, const std::vector<unsigned int>& indices);

// Chair Mesh structure
struct ChairMesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int diffuseMapID;
    unsigned int normalMapID;
    unsigned int displacementMapID;
    unsigned int aoMapID;
    unsigned int specularMapID;
    std::string materialName;
    
    // OpenGL objects
    unsigned int VAO, VBO, EBO;
};

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Camera
glm::vec3 cameraPos = glm::vec3(0.0f, 1.0f, 5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Skybox vertices
float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// Store chair meshes (one for each material)
std::vector<ChairMesh> chairMeshes;

// Displacement mapping settings
float displacementScale = 0.1f;
bool useDisplacement = true;
bool useNormalMapping = true;
bool useParallaxMapping = true;

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Indoor Room with Chair - WASD: Move | Mouse: Look", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // ==================== COMPILE SHADERS ====================
    unsigned int skyboxShader = createShaderProgram("shaders/skybox.vs", "shaders/skybox.fs");
    unsigned int chairShader = createShaderProgram("shaders/chair.vs", "shaders/chair.fs");

    // ==================== SKYBOX ====================

    // Skybox VAO, VBO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Load cubemap textures with your naming convention
    std::vector<std::string> faces = {
        "cubemap/px.png",   // +X (right wall)
        "cubemap/nx.png",   // -X (left wall)
        "cubemap/py.png",   // +Y (ceiling)
        "cubemap/ny.png",   // -Y (floor)
        "cubemap/pz.png",   // +Z (back wall)
        "cubemap/nz.png"    // -Z (entrance wall)
    };
    
    unsigned int cubemapTexture = loadCubemap(faces);

    // ==================== CHAIR ====================

    std::cout << "\n=== Loading Chair OBJ File ===" << std::endl;
    
    // Load the actual OBJ file with proper parsing
    std::vector<std::vector<float>> verticesArray;
    std::vector<std::vector<unsigned int>> indicesArray;
    std::vector<unsigned int> textureIDs;
    std::vector<std::string> texturePaths;
    
    if (loadOBJWithMTL("data/chair.obj", verticesArray, indicesArray, textureIDs, texturePaths)) {
        std::cout << "Successfully loaded OBJ with " << verticesArray.size() << " material groups" << std::endl;
        
        // Create a mesh for each material group
        for (size_t i = 0; i < verticesArray.size(); i++) {
            ChairMesh mesh;
            mesh.vertices = verticesArray[i];
            mesh.indices = indicesArray[i];
            
            // Calculate tangents and bitangents for normal/displacement mapping
            calculateTangents(mesh.vertices, mesh.indices);
            
            if (i < texturePaths.size() && !texturePaths[i].empty()) {
                mesh.materialName = texturePaths[i];
                
                // Extract base texture name without extension
                std::string basePath = texturePaths[i];
                size_t dotPos = basePath.find_last_of('.');
                if (dotPos != std::string::npos) {
                    basePath = basePath.substr(0, dotPos);
                }
                
                // Try to load all texture maps
                std::string dataDir = "data/";
                
                // 1. Diffuse/Albedo map
                std::vector<std::string> diffusePaths = {
                    texturePaths[i],
                    dataDir + texturePaths[i],
                    basePath + ".png"
                };
                
                for (const auto& path : diffusePaths) {
                    mesh.diffuseMapID = loadTexture(path.c_str(), true); // Gamma correct diffuse
                    if (mesh.diffuseMapID > 0) {
                        std::cout << "Loaded diffuse map: " << path << " for mesh " << i << std::endl;
                        break;
                    }
                }
                
                // 2. Normal map
                std::vector<std::string> normalPaths = {
                    basePath + "_normal.png",
                };
                
                for (const auto& path : normalPaths) {
                    mesh.normalMapID = loadTexture(path.c_str(), false);
                    if (mesh.normalMapID > 0) {
                        std::cout << "Loaded normal map: " << path << " for mesh " << i << std::endl;
                        break;
                    }
                }
                
                // 3. Displacement/Height map
                std::vector<std::string> displacementPaths = {
                    basePath + "_displacement.png",
                };
                
                for (const auto& path : displacementPaths) {
                    mesh.displacementMapID = loadTexture(path.c_str(), false);
                    if (mesh.displacementMapID > 0) {
                        std::cout << "Loaded displacement map: " << path << " for mesh " << i << std::endl;
                        break;
                    }
                }
                
                // 4. Ambient Occlusion map
                std::vector<std::string> aoPaths = {
                    basePath + "_ambient.png",
                };
                
                for (const auto& path : aoPaths) {
                    mesh.aoMapID = loadTexture(path.c_str(), false);
                    if (mesh.aoMapID > 0) {
                        std::cout << "Loaded AO map: " << path << " for mesh " << i << std::endl;
                        break;
                    }
                }
                
                // 5. Specular/Roughness map
                std::vector<std::string> specularPaths = {
                    basePath + "_specular.png",
                };
                
                for (const auto& path : specularPaths) {
                    mesh.specularMapID = loadTexture(path.c_str(), false);
                    if (mesh.specularMapID > 0) {
                        std::cout << "Loaded specular map: " << path << " for mesh " << i << std::endl;
                        break;
                    }
                }
            } else {
                mesh.materialName = "default";
                mesh.diffuseMapID = 0;
                mesh.normalMapID = 0;
                mesh.displacementMapID = 0;
                mesh.aoMapID = 0;
                mesh.specularMapID = 0;
            }
            
            // Setup OpenGL buffers
            glGenVertexArrays(1, &mesh.VAO);
            glGenBuffers(1, &mesh.VBO);
            glGenBuffers(1, &mesh.EBO);
            
            glBindVertexArray(mesh.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);
            
            // Vertex attributes (position, color, normal, texcoord, tangent, bitangent)
            int stride = 17; // 3 + 3 + 3 + 2 + 3 + 3
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(9 * sizeof(float)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(11 * sizeof(float)));
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(14 * sizeof(float)));
            
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            chairMeshes.push_back(mesh);
            std::cout << "Created mesh " << i << " with " << mesh.vertices.size() / stride << " vertices and " 
                      << mesh.indices.size() / 3 << " triangles" << std::endl;
        }
    }

    // Lighting
    glm::vec3 lightPos(2.0f, 3.0f, 2.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "WASD: Move camera" << std::endl;
    std::cout << "Mouse: Look around" << std::endl;
    std::cout << "Space: Move up" << std::endl;
    std::cout << "Shift: Move down" << std::endl;
    std::cout << "C: Reset camera" << std::endl;
    std::cout << "ESC: Exit" << std::endl;
    std::cout << "P: Toggle parallax mapping" << std::endl;
    std::cout << "N: Toggle normal mapping" << std::endl;
    std::cout << "D: Toggle displacement mapping" << std::endl;
    std::cout << "UP/DOWN: Adjust displacement scale" << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process input
        processInput(window);

        // Clear buffers
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Create view matrix based on camera position and orientation
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT, 
                                                0.1f, 100.0f);

        glUseProgram(chairShader);
        
        // Model matrix
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::translate(model, glm::vec3(0.0f, -0.5f, 3.0f));
        model = glm::scale(model, glm::vec3(0.1f, 0.1f, 0.1f));
        
        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(chairShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(chairShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(chairShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(chairShader, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        
        // Light properties
        glUniform3f(glGetUniformLocation(chairShader, "light.position"), lightPos.x, lightPos.y, lightPos.z);
        glUniform3f(glGetUniformLocation(chairShader, "light.ambient"), 0.2f, 0.2f, 0.2f);
        glUniform3f(glGetUniformLocation(chairShader, "light.diffuse"), 0.8f, 0.8f, 0.8f);
        glUniform3f(glGetUniformLocation(chairShader, "light.specular"), 1.0f, 1.0f, 1.0f);
        
        // Material properties
        glUniform1f(glGetUniformLocation(chairShader, "material.shininess"), 32.0f);
        glUniform1f(glGetUniformLocation(chairShader, "displacementScale"), displacementScale);
        glUniform1i(glGetUniformLocation(chairShader, "useDisplacement"), useDisplacement);
        glUniform1i(glGetUniformLocation(chairShader, "useNormalMapping"), useNormalMapping);
        glUniform1i(glGetUniformLocation(chairShader, "useParallaxMapping"), useParallaxMapping);
        
        for (const auto& mesh : chairMeshes) {
            // Texture unit
            if (mesh.diffuseMapID > 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.diffuseMapID);
                glUniform1i(glGetUniformLocation(chairShader, "material.diffuse"), 0);
            }
            
            // Normal map
            if (mesh.normalMapID > 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mesh.normalMapID);
                glUniform1i(glGetUniformLocation(chairShader, "material.normal"), 1);
            } 
            
            //  Displacement/Height map
            if (mesh.displacementMapID > 0) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, mesh.displacementMapID);
                glUniform1i(glGetUniformLocation(chairShader, "material.displacement"), 2);
            }
            
            // Ambient map
            if (mesh.aoMapID > 0) {
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, mesh.aoMapID);
                glUniform1i(glGetUniformLocation(chairShader, "material.ao"), 3);
            } 
            
            // Specular map
            if (mesh.specularMapID > 0) {
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, mesh.specularMapID);
                glUniform1i(glGetUniformLocation(chairShader, "material.specular"), 4);
            } 
            // Draw mesh
            glBindVertexArray(mesh.VAO);
            if (mesh.indices.empty()) {
                glDrawArrays(GL_TRIANGLES, 0, mesh.vertices.size() / 17);
            } else {
                glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
            }
            
            // Unbind textures
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Change depth function 
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShader);
        
        // Remove translation from view matrix for skybox
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
        
        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        
        // Bind skybox texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
        
        // Draw skybox
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        
        // Reset depth function
        glDepthFunc(GL_LESS);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &skyboxVAO);
    for (const auto& mesh : chairMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
        if (mesh.diffuseMapID > 0) glDeleteTextures(1, &mesh.diffuseMapID);
        if (mesh.normalMapID > 0) glDeleteTextures(1, &mesh.normalMapID);
        if (mesh.displacementMapID > 0) glDeleteTextures(1, &mesh.displacementMapID);
        if (mesh.aoMapID > 0) glDeleteTextures(1, &mesh.aoMapID);
        if (mesh.specularMapID > 0) glDeleteTextures(1, &mesh.specularMapID);
    }
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(skyboxShader);
    glDeleteProgram(chairShader);
    glDeleteTextures(1, &cubemapTexture);

    glfwTerminate();
    return 0;
}

// Helper Function

unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        std::cout << "Loading cubemap face: " << faces[i] << std::endl;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            std::cout << "Loaded cubemap texture: " << faces[i] << " (" << width << "x" << height << ")" << std::endl;
        } else {
            std::cout << "Failed to load cubemap texture: " << faces[i] << std::endl;
            // Try alternative paths
            std::vector<std::string> altPaths = {
                "cubemap/" + faces[i],
                "room_skybox/" + faces[i],
                faces[i]
            };
            
            bool loaded = false;
            for (const auto& path : altPaths) {
                data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
                if (data) {
                    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                                 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                    stbi_image_free(data);
                    std::cout << "Loaded cubemap texture from: " << path << std::endl;
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded) {
                // Create a fallback colored texture
                unsigned char fallback[] = {128, 128, 128}; // Gray
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                             0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fallback);
                std::cout << "Created fallback texture for: " << faces[i] << std::endl;
            }
        }
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int loadTexture(const char* path, bool gammaCorrection) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    // Check if file exists
    std::ifstream testFile(path);
    if (!testFile.good()) {
        return 0;
    }
    testFile.close();
    
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true); // Flip texture vertically
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum internalFormat;
        GLenum dataFormat;
        
        if (nrComponents == 1) {
            internalFormat = dataFormat = GL_RED;
        } else if (nrComponents == 3) {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        } else if (nrComponents == 4) {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        std::cout << "Successfully loaded texture: " << path << " (" << width << "x" << height << ")" << std::endl;
        return textureID;
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera movement speed
    float cameraSpeed = 2.5f * deltaTime;
    
    // CAMERA MOVEMENT ONLY (WASD + Space/Shift)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
        
    // Reset camera position
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        cameraPos = glm::vec3(0.0f, 1.0f, 5.0f);
    
    // Toggle mapping techniques
    static bool pKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !pKeyPressed) {
        useParallaxMapping = !useParallaxMapping;
        std::cout << "Parallax mapping: " << (useParallaxMapping ? "ON" : "OFF") << std::endl;
        pKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) {
        pKeyPressed = false;
    }
    
    static bool nKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !nKeyPressed) {
        useNormalMapping = !useNormalMapping;
        std::cout << "Normal mapping: " << (useNormalMapping ? "ON" : "OFF") << std::endl;
        nKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE) {
        nKeyPressed = false;
    }
    
    static bool dKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !dKeyPressed) {
        useDisplacement = !useDisplacement;
        std::cout << "Displacement mapping: " << (useDisplacement ? "ON" : "OFF") << std::endl;
        dKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) {
        dKeyPressed = false;
    }
    
    // Adjust displacement scale
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        displacementScale += 0.01f;
        if (displacementScale > 0.3f) displacementScale = 0.3f;
        std::cout << "Displacement scale: " << displacementScale << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        displacementScale -= 0.01f;
        if (displacementScale < 0.0f) displacementScale = 0.0f;
        std::cout << "Displacement scale: " << displacementScale << std::endl;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

std::string readShaderFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "ERROR: Cannot open shader file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int compileShader(GLenum type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    
    return shader;
}

unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath) {
    // Read shader files
    std::string vertexCode = readShaderFile(vertexPath);
    std::string fragmentCode = readShaderFile(fragmentPath);
    
    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "ERROR: Failed to read shader files" << std::endl;
        return 0;
    }
    
    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();
    
    // Compile shaders
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    // Create shader program
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // Check linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    // Clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return shaderProgram;
}

bool loadOBJWithMTL(const char* objPath, 
                    std::vector<std::vector<float>>& verticesArray,
                    std::vector<std::vector<unsigned int>>& indicesArray,
                    std::vector<unsigned int>& textureIDs,
                    std::vector<std::string>& texturePaths) {
    
    std::cout << "Loading OBJ file: " << objPath << std::endl;
    
    std::ifstream objFile(objPath);
    if (!objFile.is_open()) {
        std::cerr << "ERROR: Cannot open OBJ file: " << objPath << std::endl;
        return false;
    }
    
    // Temporary storage for the entire OBJ file
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    
    std::string line;
    std::string currentMtl = "default";
    std::map<std::string, std::vector<float>> mtlVertices;
    std::map<std::string, std::vector<unsigned int>> mtlIndices;
    std::map<std::string, std::string> mtlTextures;
    std::map<std::string, std::map<std::string, unsigned int>> mtlVertexMap;
    
    while (std::getline(objFile, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (prefix == "vn") {
            glm::vec3 norm;
            iss >> norm.x >> norm.y >> norm.z;
            normals.push_back(norm);
        }
        else if (prefix == "vt") {
            glm::vec2 tex;
            iss >> tex.x >> tex.y;
            tex.y = 1.0f - tex.y; // Flip for OpenGL
            texcoords.push_back(tex);
        }
    }
    
    objFile.clear();
    objFile.seekg(0);
    
    while (std::getline(objFile, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "usemtl") {
            iss >> currentMtl;
            std::cout << "Switching to material: " << currentMtl << std::endl;
        }
        else if (prefix == "f") {
            std::vector<std::string> faceVertices;
            std::string vertex;
            
            while (iss >> vertex) {
                faceVertices.push_back(vertex);
            }
            
            if (faceVertices.size() < 3) {
                continue;
            }
            
            int triangles = (faceVertices.size() == 3) ? 1 : 2;
            
            for (int t = 0; t < triangles; t++) {
                // Triangle indices for this sub-triangle
                int triIndices[3];
                if (triangles == 1) {
                    // Single triangle
                    triIndices[0] = 0;
                    triIndices[1] = 1;
                    triIndices[2] = 2;
                } else {
                    // Quad split into two triangles
                    if (t == 0) {
                        triIndices[0] = 0;
                        triIndices[1] = 1;
                        triIndices[2] = 2;
                    } else {
                        triIndices[0] = 0;
                        triIndices[1] = 2;
                        triIndices[2] = 3;
                    }
                }
                
                // Process each vertex in the triangle
                for (int i = 0; i < 3; i++) {
                    const std::string& face = faceVertices[triIndices[i]];
                    std::stringstream ss(face);
                    std::string token;
                    std::vector<int> indices;
                    
                    while (std::getline(ss, token, '/')) {
                        if (!token.empty()) {
                            indices.push_back(std::stoi(token) - 1); // OBJ is 1-indexed
                        } else {
                            indices.push_back(-1);
                        }
                    }
                    
                    // Create a unique key for this vertex combination
                    std::string vertexKey = face;
                    
                    // Check if we've already processed this vertex for the current material
                    unsigned int vertexIndex;
                    auto& vertexMap = mtlVertexMap[currentMtl];
                    
                    if (vertexMap.find(vertexKey) != vertexMap.end()) {
                        // Reuse existing vertex
                        vertexIndex = vertexMap[vertexKey];
                    } else {
                        // Create new vertex
                        vertexIndex = mtlVertices[currentMtl].size() / 11;
                        vertexMap[vertexKey] = vertexIndex;
                        
                        // Ensure we have at least position index
                        if (indices.size() > 0 && indices[0] < positions.size()) {
                            // Position
                            glm::vec3 pos = positions[indices[0]];
                            mtlVertices[currentMtl].push_back(pos.x);
                            mtlVertices[currentMtl].push_back(pos.y);
                            mtlVertices[currentMtl].push_back(pos.z);
                            
                            // Color (based on material)
                            if (currentMtl.find("leather") != std::string::npos) {
                                mtlVertices[currentMtl].push_back(0.6f); // R
                                mtlVertices[currentMtl].push_back(0.4f); // G
                                mtlVertices[currentMtl].push_back(0.2f); // B
                            } else if (currentMtl.find("wood") != std::string::npos) {
                                mtlVertices[currentMtl].push_back(0.5f); // R
                                mtlVertices[currentMtl].push_back(0.3f); // G
                                mtlVertices[currentMtl].push_back(0.1f); // B
                            } else {
                                mtlVertices[currentMtl].push_back(0.8f); // R
                                mtlVertices[currentMtl].push_back(0.8f); // G
                                mtlVertices[currentMtl].push_back(0.8f); // B
                            }
                            
                            // Normal
                            if (indices.size() > 2 && indices[2] < normals.size()) {
                                glm::vec3 norm = normals[indices[2]];
                                mtlVertices[currentMtl].push_back(norm.x);
                                mtlVertices[currentMtl].push_back(norm.y);
                                mtlVertices[currentMtl].push_back(norm.z);
                            } else if (positions.size() > 0) {
                                // Calculate face normal if not provided
                                mtlVertices[currentMtl].push_back(0.0f);
                                mtlVertices[currentMtl].push_back(1.0f);
                                mtlVertices[currentMtl].push_back(0.0f);
                            } else {
                                mtlVertices[currentMtl].push_back(0.0f);
                                mtlVertices[currentMtl].push_back(1.0f);
                                mtlVertices[currentMtl].push_back(0.0f);
                            }
                            
                            // Texture coordinate
                            if (indices.size() > 1 && indices[1] < texcoords.size()) {
                                glm::vec2 tex = texcoords[indices[1]];
                                mtlVertices[currentMtl].push_back(tex.x);
                                mtlVertices[currentMtl].push_back(tex.y);
                            } else {
                                mtlVertices[currentMtl].push_back(0.0f);
                                mtlVertices[currentMtl].push_back(0.0f);
                            }
                        }
                    }
                    
                    // Add index to triangle
                    mtlIndices[currentMtl].push_back(vertexIndex);
                }
            }
        }
        else if (prefix == "mtllib") {
            std::string mtlFile;
            iss >> mtlFile;
            
            // Load MTL file to get texture paths
            std::ifstream mtlFileStream(mtlFile);
            if (!mtlFileStream.is_open()) {
                // Try with path relative to OBJ
                std::string objDir = fs::path(objPath).parent_path().string();
                if (objDir.empty()) objDir = ".";
                mtlFileStream.open(objDir + "/" + mtlFile);
            }
            
            if (mtlFileStream.is_open()) {
                std::string mtlLine;
                std::string currentMaterial;
                
                while (std::getline(mtlFileStream, mtlLine)) {
                    std::istringstream mtlIss(mtlLine);
                    std::string mtlPrefix;
                    mtlIss >> mtlPrefix;
                    
                    if (mtlPrefix == "newmtl") {
                        mtlIss >> currentMaterial;
                    }
                    else if (mtlPrefix == "map_Kd") {
                        std::string texturePath;
                        mtlIss >> texturePath;
                        mtlTextures[currentMaterial] = texturePath;
                        std::cout << "Material " << currentMaterial << " uses texture: " << texturePath << std::endl;
                    }
                }
                mtlFileStream.close();
            }
        }
    }
    
    objFile.close();
    
    // Convert maps to arrays
    for (const auto& pair : mtlVertices) {
        verticesArray.push_back(pair.second);
        indicesArray.push_back(mtlIndices[pair.first]);
        texturePaths.push_back(mtlTextures[pair.first]);
        
        std::cout << "Material group: " << pair.first 
                  << ", Vertices: " << pair.second.size() / 11
                  << ", Triangles: " << mtlIndices[pair.first].size() / 3 << std::endl;
    }
    
    if (verticesArray.empty()) {
        std::cerr << "ERROR: No geometry found in OBJ file" << std::endl;
        return false;
    }
    
    std::cout << "Total material groups: " << verticesArray.size() << std::endl;
    return true;
}

void calculateTangents(std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    
    // Extract data from vertices array
    for (size_t i = 0; i < vertices.size(); i += 11) {
        positions.push_back(glm::vec3(vertices[i], vertices[i+1], vertices[i+2]));
        texCoords.push_back(glm::vec2(vertices[i+9], vertices[i+10]));
        normals.push_back(glm::vec3(vertices[i+6], vertices[i+7], vertices[i+8]));
    }
    
    std::vector<glm::vec3> tangents(positions.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(positions.size(), glm::vec3(0.0f));
    
    // Calculate tangents and bitangents for each triangle
    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int i1 = indices[i];
        unsigned int i2 = indices[i+1];
        unsigned int i3 = indices[i+2];
        
        glm::vec3 edge1 = positions[i2] - positions[i1];
        glm::vec3 edge2 = positions[i3] - positions[i1];
        glm::vec2 deltaUV1 = texCoords[i2] - texCoords[i1];
        glm::vec2 deltaUV2 = texCoords[i3] - texCoords[i1];
        
        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        
        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent = glm::normalize(tangent);
        
        glm::vec3 bitangent;
        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
        bitangent = glm::normalize(bitangent);
        
        tangents[i1] += tangent;
        tangents[i2] += tangent;
        tangents[i3] += tangent;
        
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
        bitangents[i3] += bitangent;
    }
    
    // Reconstruct vertices array with tangents and bitangents
    std::vector<float> newVertices;
    for (size_t i = 0; i < positions.size(); i++) {
        // Original data (11 floats)
        for (int j = 0; j < 11; j++) {
            newVertices.push_back(vertices[i*11 + j]);
        }
        
        // Tangent (orthonormalize with respect to normal)
        glm::vec3 normal = glm::normalize(normals[i]);
        glm::vec3 tangent = glm::normalize(tangents[i]);
        tangent = glm::normalize(tangent - glm::dot(tangent, normal) * normal);
        newVertices.push_back(tangent.x);
        newVertices.push_back(tangent.y);
        newVertices.push_back(tangent.z);
        
        // Bitangent (can be calculated or stored)
        glm::vec3 bitangent = glm::normalize(bitangents[i]);
        bitangent = glm::normalize(bitangent - glm::dot(bitangent, normal) * normal);
        newVertices.push_back(bitangent.x);
        newVertices.push_back(bitangent.y);
        newVertices.push_back(bitangent.z);
    }
    
    vertices = newVertices;
}