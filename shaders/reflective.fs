#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 cameraPos;
uniform samplerCube skybox;
uniform sampler2D materialTexture;
uniform float reflectivity;
uniform int hasTexture;

void main() {
    // Calculate reflection vector
    vec3 incident = normalize(FragPos - cameraPos);
    vec3 normal = normalize(Normal);
    vec3 reflection = reflect(incident, normal);
    
    // Sample from cubemap
    vec4 envColor = texture(skybox, reflection);
    
    if (hasTexture == 1) {
        // For textured objects, blend with material texture
        // In a real implementation, you'd need texture coordinates
        // For now, use the environment color with reflectivity factor
        FragColor = mix(vec4(0.7, 0.5, 0.3, 1.0), envColor, reflectivity);
    } else {
        // For non-textured objects, use environment color directly
        FragColor = mix(vec4(0.7, 0.5, 0.3, 1.0), envColor, reflectivity);
    }
    
    // Add some ambient light
    FragColor.rgb += 0.1;
}