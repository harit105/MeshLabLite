#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Define structures for vertices, faces, and normals
struct Vertex {
    float x, y, z;
};

struct Face {
    int v1, v2, v3;
};

struct Normal {
    float x, y, z;
};

// Camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.05f;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 400, lastY = 300;
bool firstMouse = true;

// Mesh color
glm::vec3 meshColor(0.5f, 0.5f, 0.5f);

// Function to load an OBJ file
bool loadOBJ(const std::string& filename, std::vector<Vertex>& vertices, std::vector<Face>& faces) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "v") {
            // Parse vertex data
            Vertex vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            // Increase the size by 20%
            vertex.x *= 2.2f;
            vertex.y *= 2.2f;
            vertex.z *= 2.2f;
            vertices.push_back(vertex);
        }
        else if (type == "f") {
            // Parse face data
            Face face;
            iss >> face.v1 >> face.v2 >> face.v3;
            // OBJ file indices start at 1, so we subtract 1 to convert to 0-based indexing
            face.v1--; face.v2--; face.v3--;
            faces.push_back(face);
        }
        // Ignore other types (vn, vt, etc.)
    }

    file.close();
    return true;
}

// Function to calculate face normal
Normal calculateFaceNormal(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    // Calculate two vectors on the face
    float ux = v2.x - v1.x;
    float uy = v2.y - v1.y;
    float uz = v2.z - v1.z;
    
    float vx = v3.x - v1.x;
    float vy = v3.y - v1.y;
    float vz = v3.z - v1.z;
    
    // Calculate cross product to get normal
    Normal normal;
    normal.x = uy * vz - uz * vy;
    normal.y = uz * vx - ux * vz;
    normal.z = ux * vy - uy * vx;
    
    // Normalize the normal vector
    float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length != 0) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }
    
    return normal;
}

// Function to add noise to vertices along their normals
void addNoiseToVertices(std::vector<Vertex>& vertices, const std::vector<Normal>& vertexNormals, float noiseStrength) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    for (size_t i = 0; i < vertices.size(); ++i) {
        float noise = noiseStrength * dis(gen);
        vertices[i].x += vertexNormals[i].x * noise;
        vertices[i].y += vertexNormals[i].y * noise;
        vertices[i].z += vertexNormals[i].z * noise;
    }
}

// Function to perform Laplacian smoothing (mesh denoising)
void laplacianSmoothing(std::vector<Vertex>& vertices, const std::vector<Face>& faces, float smoothingFactor) {
    std::vector<Vertex> newVertices = vertices;

    for (size_t i = 0; i < vertices.size(); ++i) {
        Vertex sum = {0, 0, 0};
        int count = 0;

        // Find all neighboring vertices
        for (const auto& face : faces) {
            if (face.v1 == i || face.v2 == i || face.v3 == i) {
                if (face.v1 != i) {
                    sum.x += vertices[face.v1].x;
                    sum.y += vertices[face.v1].y;
                    sum.z += vertices[face.v1].z;
                    count++;
                }
                if (face.v2 != i) {
                    sum.x += vertices[face.v2].x;
                    sum.y += vertices[face.v2].y;
                    sum.z += vertices[face.v2].z;
                    count++;
                }
                if (face.v3 != i) {
                    sum.x += vertices[face.v3].x;
                    sum.y += vertices[face.v3].y;
                    sum.z += vertices[face.v3].z;
                    count++;
                }
            }
        }

        // Calculate the average position of neighbors
        if (count > 0) {
            sum.x /= count;
            sum.y /= count;
            sum.z /= count;

            // Move the vertex towards the average position
            newVertices[i].x += (sum.x - vertices[i].x) * smoothingFactor;
            newVertices[i].y += (sum.y - vertices[i].y) * smoothingFactor;
            newVertices[i].z += (sum.z - vertices[i].z) * smoothingFactor;
        }
    }

    vertices = newVertices;
}

// Vertex shader source code
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    
    out vec3 FragPos;
    out vec3 Normal;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main()
    {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;  
        
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    in vec3 FragPos;
    in vec3 Normal;
    
    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform vec3 lightColor;
    uniform vec3 objectColor;
    uniform bool usePhongShading;
    uniform bool useWireframe;
    
    void main()
    {
        if (useWireframe) {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0); // White color for wireframe
        } else {
            // ambient lighting
            float ambientStrength = 0.1;
            vec3 ambient = ambientStrength * lightColor;
            
            // diffuse lighting
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * lightColor;
            
            // specular lighting
            float specularStrength = 0.5;
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);  
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
            vec3 specular = specularStrength * spec * lightColor;
            
            // Final color calculation
            vec3 result;
            if (usePhongShading) {
                result = (ambient + diffuse + specular) * objectColor;
            } else {
                result = (ambient + diffuse) * objectColor;
            }
            FragColor = vec4(result, 1.0);
        }
    }
)";

// Function to initialize GLFW window
GLFWwindow* initializeWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return nullptr;
    }

    // Set OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Mesh Viewer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return nullptr;
    }

    return window;
}

// Function to create and compile shader program
GLuint createShaderProgram() {
    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Create shader program and link shaders
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Delete individual shaders as they're now part of the program
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Mouse callback function
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Reversed since y-coordinates range from bottom to top
    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

// Scroll callback function
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraPos += cameraSpeed * cameraFront * static_cast<float>(yoffset);
}

int main() {
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    
    // Load OBJ file
    if (loadOBJ("/Users/haritshah/Desktop/Assignment296/bunny.obj", vertices, faces)) {
        std::cout << "Loaded " << vertices.size() << " vertices and " << faces.size() << " faces." << std::endl;
        
        // Calculate flat face normals
        std::vector<Normal> faceNormals;
        for (const auto& face : faces) {
            Normal normal = calculateFaceNormal(vertices[face.v1], vertices[face.v2], vertices[face.v3]);
            faceNormals.push_back(normal);
        }
        
        // Calculate smoothed vertex normals
        std::vector<Normal> vertexNormals(vertices.size(), {0, 0, 0});
        std::vector<int> vertexFaceCount(vertices.size(), 0);
        
        for (size_t i = 0; i < faces.size(); ++i) {
            const auto& face = faces[i];
            const auto& normal = faceNormals[i];
            
            // Add face normal to each vertex of the face
            vertexNormals[face.v1].x += normal.x;
            vertexNormals[face.v1].y += normal.y;
            vertexNormals[face.v1].z += normal.z;
            vertexFaceCount[face.v1]++;
            
            vertexNormals[face.v2].x += normal.x;
            vertexNormals[face.v2].y += normal.y;
            vertexNormals[face.v2].z += normal.z;
            vertexFaceCount[face.v2]++;
            
            vertexNormals[face.v3].x += normal.x;
            vertexNormals[face.v3].y += normal.y;
            vertexNormals[face.v3].z += normal.z;
            vertexFaceCount[face.v3]++;
        }
        
        // Normalize vertex normals
        for (size_t i = 0; i < vertexNormals.size(); ++i) {
            if (vertexFaceCount[i] > 0) {
                // Average the normal
                vertexNormals[i].x /= vertexFaceCount[i];
                vertexNormals[i].y /= vertexFaceCount[i];
                vertexNormals[i].z /= vertexFaceCount[i];
                
                // Normalize the normal vector
                float length = std::sqrt(vertexNormals[i].x * vertexNormals[i].x + 
                                         vertexNormals[i].y * vertexNormals[i].y + 
                                         vertexNormals[i].z * vertexNormals[i].z);
                if (length != 0) {
                    vertexNormals[i].x /= length;
                    vertexNormals[i].y /= length;
                    vertexNormals[i].z /= length;
                }
            }
        }
        
        std::cout << "Calculated " << faceNormals.size() << " face normals and " 
                  << vertexNormals.size() << " vertex normals." << std::endl;
        
        // Initialize GLFW and create window
        GLFWwindow* window = initializeWindow();
        if (!window) {
            return -1;
        }

        // Set up mouse callback
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // Capture the mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Create and use shader program
        GLuint shaderProgram = createShaderProgram();
        glUseProgram(shaderProgram);

        // Create Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Prepare data for VBO
        std::vector<float> meshData;
        for (const auto& face : faces) {
            // For each vertex in the face, add position and normal data
            // Vertex 1
            meshData.push_back(vertices[face.v1].x);
            meshData.push_back(vertices[face.v1].y);
            meshData.push_back(vertices[face.v1].z);
            meshData.push_back(vertexNormals[face.v1].x);
            meshData.push_back(vertexNormals[face.v1].y);
            meshData.push_back(vertexNormals[face.v1].z);

            // Vertex 2
            meshData.push_back(vertices[face.v2].x);
            meshData.push_back(vertices[face.v2].y);
            meshData.push_back(vertices[face.v2].z);
            meshData.push_back(vertexNormals[face.v2].x);
            meshData.push_back(vertexNormals[face.v2].y);
            meshData.push_back(vertexNormals[face.v2].z);

            // Vertex 3
            meshData.push_back(vertices[face.v3].x);
            meshData.push_back(vertices[face.v3].y);
            meshData.push_back(vertices[face.v3].z);
            meshData.push_back(vertexNormals[face.v3].x);
            meshData.push_back(vertexNormals[face.v3].y);
            meshData.push_back(vertexNormals[face.v3].z);
        }

        // Send data to GPU
        glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(float), meshData.data(), GL_STATIC_DRAW);

        // Set vertex attribute pointers
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Set up uniforms
        glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
        glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);

        bool usePhongShading = true;
        bool useWireframe = false;
        bool noiseAdded = false;
        float noiseStrength = 0.01f;
        float smoothingFactor = 0.5f;
        std::vector<Vertex> originalVertices = vertices;
        bool keyDPressed = false;
        int denoiseLevel = 0;

        // Predefined color options
        std::vector<glm::vec3> colorOptions = {
            glm::vec3(1.0f, 0.0f, 0.0f), // Red
            glm::vec3(0.0f, 1.0f, 0.0f), // Green
            glm::vec3(0.0f, 0.0f, 1.0f), // Blue
            glm::vec3(1.0f, 1.0f, 0.0f), // Yellow
            glm::vec3(1.0f, 0.0f, 1.0f), // Magenta
            glm::vec3(0.0f, 1.0f, 1.0f)  // Cyan
        };
        int currentColorIndex = 0;

        // Main render loop
        while (!glfwWindowShouldClose(window)) {
            // Process input
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                usePhongShading = !usePhongShading;

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                useWireframe = !useWireframe;

            if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
                addNoiseToVertices(vertices, vertexNormals, noiseStrength);
                noiseAdded = true;
            }

            if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
                if (!keyDPressed) {
                    keyDPressed = true;
                    denoiseLevel++;
                    if (denoiseLevel > 3) {
                        denoiseLevel = 0;
                        vertices = originalVertices;
                    } else {
                        laplacianSmoothing(vertices, faces, smoothingFactor);
                    }
                }
            } else {
                keyDPressed = false;
            }

            // Color change
            if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
                currentColorIndex = (currentColorIndex + 1) % colorOptions.size();
                meshColor = colorOptions[currentColorIndex];
            }

            // Camera movement
            float currentFrame = glfwGetTime();
            static float lastFrame = 0.0f;
            float deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            float cameraSpeed = 2.5f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                cameraPos += cameraSpeed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                cameraPos -= cameraSpeed * cameraFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

            // Update mesh data if noise was added or mesh was denoised
            if (noiseAdded || keyDPressed) {
                meshData.clear();
                for (const auto& face : faces) {
                    // Vertex 1
                    meshData.push_back(vertices[face.v1].x);
                    meshData.push_back(vertices[face.v1].y);
                    meshData.push_back(vertices[face.v1].z);
                    meshData.push_back(vertexNormals[face.v1].x);
                    meshData.push_back(vertexNormals[face.v1].y);
                    meshData.push_back(vertexNormals[face.v1].z);

                    // Vertex 2
                    meshData.push_back(vertices[face.v2].x);
                    meshData.push_back(vertices[face.v2].y);
                    meshData.push_back(vertices[face.v2].z);
                    meshData.push_back(vertexNormals[face.v2].x);
                    meshData.push_back(vertexNormals[face.v2].y);
                    meshData.push_back(vertexNormals[face.v2].z);

                    // Vertex 3
                    meshData.push_back(vertices[face.v3].x);
                    meshData.push_back(vertices[face.v3].y);
                    meshData.push_back(vertices[face.v3].z);
                    meshData.push_back(vertexNormals[face.v3].x);
                    meshData.push_back(vertexNormals[face.v3].y);
                    meshData.push_back(vertexNormals[face.v3].z);
                }

                // Update VBO data
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(float), meshData.data(), GL_STATIC_DRAW);
                noiseAdded = false;
            }

            // Clear the screen
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Set up view and projection matrices
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

            // Set uniforms
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
            glUniform1i(glGetUniformLocation(shaderProgram, "usePhongShading"), usePhongShading);
            glUniform1i(glGetUniformLocation(shaderProgram, "useWireframe"), useWireframe);
            glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(meshColor));

            // Draw the mesh
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glBindVertexArray(VAO);
            if (useWireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            glDrawArrays(GL_TRIANGLES, 0, faces.size() * 3);

            // Swap buffers and poll events
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // Clean up
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);

        glfwTerminate();
    } else {
        std::cerr << "Failed to load OBJ file" << std::endl;
    }

    return 0;
}
