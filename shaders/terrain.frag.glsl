#version 450

layout (location = 0) in VS_OUT {
    vec3 position;
    vec3 normal;
} fs_in;

layout (location = 0) out vec4 outColor;

void main()
{
    vec3 baseColor = vec3(0.5, 1.0, 0.5);
    vec3 ambient = 0.05 * baseColor;

    vec3 lightDir = vec3(0.0, 1.0, 0.0);
    vec3 normal = normalize(fs_in.normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * ambient;

    outColor = vec4(ambient + diffuse, 1.0f);
}