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
unsigned int loadTexture(const char* path);
std::string readShaderFile(const char* path);
unsigned int compileShader(GLenum type, const char* source);
unsigned int createShaderProgram(const char* vertexPath, const char* fragmentPath);
bool loadOBJ(const char* path, 
             std::vector<float>& vertices, 
             std::vector<unsigned int>& indices,
             std::vector<std::string>& texturePaths);
void processFaceVertex(const std::string& faceStr,
                       const std::vector<glm::vec3>& temp_positions,
                       const std::vector<glm::vec3>& temp_normals,
                       const std::vector<glm::vec2>& temp_texcoords,
                       std::vector<float>& vertices,
                       std::vector<unsigned int>& indices);

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

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool environmentMappingEnabled = true;
float reflectivity = 0.8f;

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

// Chair Mesh structure
struct ChairMesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int textureID;
    std::string textureName;
    
    unsigned int VAO, VBO, EBO;
};

std::vector<ChairMesh> chairMeshes;

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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, 
        "Indoor Room with Chair - WASD: Move | Mouse: Look | E: Toggle Env Mapping | +/-: Adjust Reflectivity", 
        NULL, NULL);
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

    unsigned int skyboxShader = createShaderProgram("shaders/skybox.vs", "shaders/skybox.fs");
    unsigned int reflectiveShader = createShaderProgram("shaders/reflective.vs", "shaders/reflective.fs");

    // Skybox VAO, VBO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Load cubemap textures
    std::vector<std::string> faces = {
        "cubemap/px.png",   // +X (right wall)
        "cubemap/nx.png",   // -X (left wall)
        "cubemap/py.png",   // +Y (ceiling)
        "cubemap/ny.png",   // -Y (floor)
        "cubemap/pz.png",   // +Z (back wall)
        "cubemap/nz.png"    // -Z (entrance wall)
    };
    
    unsigned int cubemapTexture = loadCubemap(faces);

    std::cout << "\n=== Loading Chair ===" << std::endl;
    
    // Load chair OBJ with textures
    std::vector<float> chairVertices;
    std::vector<unsigned int> chairIndices;
    std::vector<std::string> chairTextures;
    
    if (loadOBJ("data/chair.obj", chairVertices, chairIndices, chairTextures)) {
        std::cout << "OBJ loaded successfully!" << std::endl;
        std::cout << "Vertices: " << chairVertices.size() / 11 << std::endl;
        std::cout << "Indices: " << chairIndices.size() << std::endl;
        std::cout << "Triangles: " << chairIndices.size() / 3 << std::endl;
        
        // Load textures if available
        unsigned int leatherTexture = 0;
        unsigned int woodTexture = 0;
        
        for (const auto& texPath : chairTextures) {
            if (texPath.find("leather") != std::string::npos) {
                leatherTexture = loadTexture(texPath.c_str());
                if (leatherTexture == 0) {
                    // Try alternative paths
                    leatherTexture = loadTexture("data/leather.jpg");
                }
            } else if (texPath.find("wood") != std::string::npos) {
                woodTexture = loadTexture(texPath.c_str());
                if (woodTexture == 0) {
                    woodTexture = loadTexture("data/wood.jpg");
                }
            }
        }
        
        ChairMesh chairMesh;
        chairMesh.vertices = chairVertices;
        chairMesh.indices = chairIndices;
        chairMesh.textureName = "chair";
        chairMesh.textureID = (leatherTexture > 0) ? leatherTexture : woodTexture;
        
        // Setup mesh
        glGenVertexArrays(1, &chairMesh.VAO);
        glGenBuffers(1, &chairMesh.VBO);
        glGenBuffers(1, &chairMesh.EBO);
        
        glBindVertexArray(chairMesh.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, chairMesh.VBO);
        glBufferData(GL_ARRAY_BUFFER, chairMesh.vertices.size() * sizeof(float), chairMesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chairMesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, chairMesh.indices.size() * sizeof(unsigned int), chairMesh.indices.data(), GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(9 * sizeof(float)));
        
        chairMeshes.push_back(chairMesh);
        std::cout << "Created chair mesh with " << chairMesh.vertices.size() / 11 << " vertices" << std::endl;
        
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
    std::cout << "E: Toggle environment mapping" << std::endl;
    std::cout << "+/-: Adjust reflectivity" << std::endl;
    std::cout << "ESC: Exit" << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                                (float)SCR_WIDTH / (float)SCR_HEIGHT, 
                                                0.1f, 100.0f);

        glUseProgram(reflectiveShader);
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::translate(model, glm::vec3(3.0f, 3.0f, 3.0f)); // Move toward positive Z
        model = glm::scale(model, glm::vec3(0.1f, 0.1f, 0.1f));
        
        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(reflectiveShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(reflectiveShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(reflectiveShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        
        // Camera position for reflection calculation
        glUniform3f(glGetUniformLocation(reflectiveShader, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        
        // Reflectivity control
        glUniform1f(glGetUniformLocation(reflectiveShader, "reflectivity"), reflectivity);
        
        // Bind cubemap texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glUniform1i(glGetUniformLocation(reflectiveShader, "skybox"), 0);
        
        // Bind diffuse texture
        for (const auto& mesh : chairMeshes) {
            if (mesh.textureID > 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glUniform1i(glGetUniformLocation(reflectiveShader, "materialTexture"), 1);
                glUniform1i(glGetUniformLocation(reflectiveShader, "hasTexture"), 1);
            } else {
                glUniform1i(glGetUniformLocation(reflectiveShader, "hasTexture"), 0);
            }
            
            // Draw mesh
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }

        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxShader);

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
        

        glDepthFunc(GL_LESS);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &skyboxVAO);
    for (const auto& mesh : chairMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
        if (mesh.textureID > 0) glDeleteTextures(1, &mesh.textureID);
    }
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(skyboxShader);
    glDeleteProgram(reflectiveShader);
    glDeleteTextures(1, &cubemapTexture);

    glfwTerminate();
    return 0;
}

// Helper Functions
void processFaceVertex(const std::string& faceStr,
                       const std::vector<glm::vec3>& temp_positions,
                       const std::vector<glm::vec3>& temp_normals,
                       const std::vector<glm::vec2>& temp_texcoords,
                       std::vector<float>& vertices,
                       std::vector<unsigned int>& indices) {
    
    std::string face = faceStr;
    std::replace(face.begin(), face.end(), '/', ' ');
    std::istringstream faceStream(face);
    
    unsigned int posIndex = 0, texIndex = 0, normIndex = 0;
    faceStream >> posIndex;
    posIndex--; // OBJ indices are 1-based
    
    if (faceStream.peek() != EOF) {
        faceStream >> texIndex;
        texIndex--;
    }
    
    if (faceStream.peek() != EOF) {
        faceStream >> normIndex;
        normIndex--;
    }
    
    // Add position
    if (posIndex < temp_positions.size()) {
        vertices.push_back(temp_positions[posIndex].x);
        vertices.push_back(temp_positions[posIndex].y);
        vertices.push_back(temp_positions[posIndex].z);
    } else {
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
    }
    
    // Add color 
    if (posIndex < temp_positions.size()) {
        float r = (temp_positions[posIndex].x + 1.0f) / 2.0f;
        float g = (temp_positions[posIndex].y + 1.0f) / 2.0f;
        float b = (temp_positions[posIndex].z + 1.0f) / 2.0f;
        vertices.push_back(r);
        vertices.push_back(g);
        vertices.push_back(b);
    } else {
        vertices.push_back(0.7f);
        vertices.push_back(0.5f);
        vertices.push_back(0.3f);
    }
    
    // Add normal
    if (normIndex < temp_normals.size()) {
        vertices.push_back(temp_normals[normIndex].x);
        vertices.push_back(temp_normals[normIndex].y);
        vertices.push_back(temp_normals[normIndex].z);
    } else {
        // Calculate default normal (up)
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
    }
    
    // Add texture coordinate
    if (texIndex < temp_texcoords.size()) {
        vertices.push_back(temp_texcoords[texIndex].x);
        vertices.push_back(temp_texcoords[texIndex].y);
    } else {
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
    }
    
    // Add index
    indices.push_back(vertices.size() / 11 - 1);
}

bool loadOBJ(const char* path, 
             std::vector<float>& vertices, 
             std::vector<unsigned int>& indices,
             std::vector<std::string>& texturePaths) {
    
    std::cout << "Loading OBJ file: " << path << std::endl;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open OBJ file: " << path << std::endl;
        
        // Try alternative paths
        std::vector<std::string> possiblePaths = {
            path,
            std::string("data/") + path,
            std::string("../") + path
        };
        
        for (const auto& altPath : possiblePaths) {
            file.open(altPath);
            if (file.is_open()) {
                std::cout << "Found OBJ file at: " << altPath << std::endl;
                break;
            }
        }
        
        if (!file.is_open()) {
            std::cerr << "ERROR: Could not find OBJ file in any location" << std::endl;
            return false;
        }
    }
    
    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_texcoords;
    std::string currentMtl;
    std::string mtlFile;
    
    std::string line;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string lineHeader;
        iss >> lineHeader;
        
        if (lineHeader == "v") {
            glm::vec3 position;
            iss >> position.x >> position.y >> position.z;
            temp_positions.push_back(position);
        }
        else if (lineHeader == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        }
        else if (lineHeader == "vt") {
            glm::vec2 texcoord;
            iss >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y; // Flip V coordinate for OpenGL
            temp_texcoords.push_back(texcoord);
        }
        else if (lineHeader == "f") {
            std::vector<std::string> faceVertices;
            std::string vertex;
            
            while (iss >> vertex) {
                faceVertices.push_back(vertex);
            }
            
            // If it's a quad (4 vertices), convert to 2 triangles
            if (faceVertices.size() == 4) {
                std::vector<std::string> triangle1 = {faceVertices[0], faceVertices[1], faceVertices[2]};
                std::vector<std::string> triangle2 = {faceVertices[0], faceVertices[2], faceVertices[3]};
                
                for (const auto& face : triangle1) {
                    processFaceVertex(face, temp_positions, temp_normals, temp_texcoords, vertices, indices);
                }
                
                for (const auto& face : triangle2) {
                    processFaceVertex(face, temp_positions, temp_normals, temp_texcoords, vertices, indices);
                }
            } 
            else if (faceVertices.size() == 3) {
                for (const auto& face : faceVertices) {
                    processFaceVertex(face, temp_positions, temp_normals, temp_texcoords, vertices, indices);
                }
            }
            else if (faceVertices.size() > 4) {
                std::cout << "WARNING: N-gon with " << faceVertices.size() << " vertices detected. Triangulating..." << std::endl;
                for (size_t i = 1; i < faceVertices.size() - 1; i++) {
                    std::vector<std::string> triangle = {faceVertices[0], faceVertices[i], faceVertices[i+1]};
                    for (const auto& face : triangle) {
                        processFaceVertex(face, temp_positions, temp_normals, temp_texcoords, vertices, indices);
                    }
                }
            } else {
                std::cerr << "WARNING: Face with " << faceVertices.size() << " vertices not supported" << std::endl;
            }
        }
        else if (lineHeader == "mtllib") {
            iss >> mtlFile;
            std::cout << "Found MTL file reference: " << mtlFile << std::endl;
            
            // Load MTL file
            std::string objDir = fs::path(path).parent_path().string();
            if (objDir.empty()) objDir = ".";
            
            std::string mtlPath = objDir + "/" + mtlFile;
            std::ifstream mtlFileStream(mtlPath);
            if (mtlFileStream.is_open()) {
                std::cout << "Loading MTL file: " << mtlPath << std::endl;
                
                std::string mtlLine;
                while (std::getline(mtlFileStream, mtlLine)) {
                    std::istringstream mtlIss(mtlLine);
                    std::string mtlHeader;
                    mtlIss >> mtlHeader;
                    
                    if (mtlHeader == "newmtl") {
                        mtlIss >> currentMtl;
                        std::cout << "Found material: " << currentMtl << std::endl;
                    }
                    else if (mtlHeader == "map_Kd") {
                        std::string texturePath;
                        mtlIss >> texturePath;
                        
                        if (!texturePath.empty()) {
                            // Try to find texture
                            std::vector<std::string> possibleTexPaths = {
                                texturePath,
                                objDir + "/" + texturePath,
                                "data/" + texturePath,
                                texturePath
                            };
                            
                            for (const auto& texPath : possibleTexPaths) {
                                std::ifstream texTest(texPath);
                                if (texTest.good()) {
                                    texturePaths.push_back(texPath);
                                    std::cout << "Found texture for material '" << currentMtl << "': " << texPath << std::endl;
                                    break;
                                }
                            }
                        }
                    }
                }
                mtlFileStream.close();
            } else {
                std::cout << "WARNING: Could not load MTL file: " << mtlPath << std::endl;
            }
        }
        else if (lineHeader == "usemtl") {
            iss >> currentMtl;
            std::cout << "Using material: " << currentMtl << std::endl;
        }
    }
    
    file.close();
    
    if (vertices.empty()) {
        std::cerr << "ERROR: No vertex data found in OBJ file" << std::endl;
        return false;
    }
    
    std::cout << "Successfully loaded OBJ file" << std::endl;
    std::cout << "Total vertices: " << vertices.size() / 11 << std::endl;
    std::cout << "Total indices: " << indices.size() << std::endl;
    std::cout << "Total triangles: " << indices.size() / 3 << std::endl;
    std::cout << "Textures found: " << texturePaths.size() << std::endl;
    
    return true;
}

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
                unsigned char fallback[] = {128, 128, 128}; 
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

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    // Check if file exists
    std::ifstream testFile(path);
    if (!testFile.good()) {
        std::cout << "Texture file not found at: " << path << std::endl;
        return 0;
    }
    testFile.close();
    
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true); // Flip texture vertically
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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
    static bool cPressed = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cPressed) {
        cameraPos = glm::vec3(0.0f, 1.0f, 5.0f);
        cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        yaw = -90.0f;
        pitch = 0.0f;
        cPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) {
        cPressed = false;
    }
    
    // Toggle environment mapping
    static bool ePressed = false;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !ePressed) {
        environmentMappingEnabled = !environmentMappingEnabled;
        std::cout << "Environment mapping: " << (environmentMappingEnabled ? "ENABLED" : "DISABLED") << std::endl;
        ePressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE) {
        ePressed = false;
    }
    
    // Adjust reflectivity
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) { // + key
        reflectivity += 0.05f;
        if (reflectivity > 1.0f) reflectivity = 1.0f;
        std::cout << "Reflectivity: " << reflectivity << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) { // - key
        reflectivity -= 0.05f;
        if (reflectivity < 0.0f) reflectivity = 0.0f;
        std::cout << "Reflectivity: " << reflectivity << std::endl;
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
    
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
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