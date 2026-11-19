// windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS   // stb_image_write가 sprintf 등을 사용합니다.
#include <Windows.h>
#include <hidusage.h>

// graphics
#include <GL/glew.h>
#include <GL/wglew.h>

// stb
#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <stb_easy_font.h>
#pragma warning(pop)

// std
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr float kDayUpdateInterval = 1.f / 60.f;
constexpr float kPi                = 3.14159265358979f;

// ===========================================
//  Window
// ===========================================

constexpr int     kWindowWidth       = 1920;
constexpr int     kWindowHeight      = 1080;
constexpr int     kMultisampleCount  = 4;
constexpr wchar_t kWindowClassName[] = L"Solar System Simulator";

#ifdef _DEBUG
constexpr int kContextFlags = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
constexpr int kContextFlags = 0;
#endif

HWND  g_hWnd  = nullptr;
HDC   g_hDC   = nullptr;
HGLRC g_hGLRC = nullptr;

int g_viewportWidth  = kWindowWidth;
int g_viewportHeight = kWindowHeight;

// ===========================================
//  Math
// ===========================================

struct Vec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

Vec3 operator+(
    const Vec3& _lhs,
    const Vec3& _rhs)
{
    return { _lhs.x + _rhs.x, _lhs.y + _rhs.y, _lhs.z + _rhs.z };
}

Vec3 operator-(
    const Vec3& _lhs,
    const Vec3& _rhs)
{
    return { _lhs.x - _rhs.x, _lhs.y - _rhs.y, _lhs.z - _rhs.z };
}

Vec3 operator*(
    const Vec3& _v,
    const float _scale)
{
    return { _v.x * _scale, _v.y * _scale, _v.z * _scale };
}

float Dot(
    const Vec3& _lhs,
    const Vec3& _rhs)
{
    return _lhs.x * _rhs.x + _lhs.y * _rhs.y + _lhs.z * _rhs.z;
}

Vec3 Cross(
    const Vec3& _lhs,
    const Vec3& _rhs)
{
    return { _lhs.y * _rhs.z - _lhs.z * _rhs.y,
             _lhs.z * _rhs.x - _lhs.x * _rhs.z,
             _lhs.x * _rhs.y - _lhs.y * _rhs.x };
}

Vec3 Normalize(const Vec3& _v)
{
    const float length = std::sqrt(Dot(_v, _v));
    return { _v.x / length, _v.y / length, _v.z / length };
}

float ToRadians(const float _degrees)
{
    return _degrees * kPi / 180.f;
}

struct Mat4
{
    // clang-format off
    std::array<float, 16> m = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    // clang-format on
};

Mat4 operator*(
    const Mat4& _lhs,
    const Mat4& _rhs)
{
    Mat4 result;
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.f;
            for (int k = 0; k < 4; ++k)
            {
                sum += _lhs.m[k * 4 + row] * _rhs.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

Mat4 Translate(const Vec3& _offset)
{
    Mat4 result;
    result.m[12] = _offset.x;
    result.m[13] = _offset.y;
    result.m[14] = _offset.z;
    return result;
}

Mat4 Scale(const float _scale)
{
    Mat4 result;
    result.m[0]  = _scale;
    result.m[5]  = _scale;
    result.m[10] = _scale;
    return result;
}

Mat4 RotateY(const float _degrees)
{
    const float c = std::cos(ToRadians(_degrees));
    const float s = std::sin(ToRadians(_degrees));

    Mat4 result;
    result.m[0]  = c;
    result.m[2]  = -s;
    result.m[8]  = s;
    result.m[10] = c;
    return result;
}

Mat4 RotateZ(const float _degrees)
{
    const float c = std::cos(ToRadians(_degrees));
    const float s = std::sin(ToRadians(_degrees));

    Mat4 result;
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

Mat4 Perspective(
    const float _fovY,
    const float _aspect,
    const float _near,
    const float _far)
{
    const float f = 1.f / std::tan(ToRadians(_fovY) * 0.5f);

    Mat4 result;
    result.m[0]  = f / _aspect;
    result.m[5]  = f;
    result.m[10] = (_far + _near) / (_near - _far);
    result.m[11] = -1.f;
    result.m[14] = 2.f * _far * _near / (_near - _far);
    result.m[15] = 0.f;
    return result;
}

Mat4 LookAt(
    const Vec3& _eye,
    const Vec3& _target,
    const Vec3& _up)
{
    const Vec3 forward = Normalize(_target - _eye);
    const Vec3 right   = Normalize(Cross(forward, _up));
    const Vec3 up      = Cross(right, forward);

    Mat4 result;
    result.m[0]  = right.x;
    result.m[4]  = right.y;
    result.m[8]  = right.z;
    result.m[1]  = up.x;
    result.m[5]  = up.y;
    result.m[9]  = up.z;
    result.m[2]  = -forward.x;
    result.m[6]  = -forward.y;
    result.m[10] = -forward.z;
    result.m[12] = -Dot(right, _eye);
    result.m[13] = -Dot(up, _eye);
    result.m[14] = Dot(forward, _eye);
    return result;
}

// ===========================================
//  Planet
// ===========================================

struct ePlanet
{
    enum Enum
    {
        Sun,
        Mercury,
        Venus,
        Earth,
        Mars,
        Jupiter,
        Saturn,
        Uranus,
        Neptune,
        Count
    };
};

struct Planet
{
    const char* path            = nullptr;
    GLuint      textureID       = 0;
    float       radius          = 0.f;
    float       distance        = 0.f;
    float       revolutionSpeed = 0.f;
    float       revolutionTilt  = 0.f;
    float       rotationSpeed   = 0.f;
    float       rotationTilt    = 0.f;
};

constexpr float kEarthRadius   = 0.1f;
constexpr float kEarthDistance = 8.f;

std::array<Planet, ePlanet::Count> g_planets = {};

float ToDisplayDistance(const float _distance)
{
    return _distance > 0.f ? 5.f + std::log2(1.f + _distance) * 4.f : 0.f;
}

void InitPlanets()
{
    g_planets[ePlanet::Sun] = {
        .path          = "Sun.bmp",
        .radius        = 30.f * kEarthRadius,
        .rotationSpeed = 360.f / 25.38f,
        .rotationTilt  = 7.25f,
    };

    g_planets[ePlanet::Mercury] = {
        .path            = "Mercury.bmp",
        .radius          = 0.4f * kEarthRadius,
        .distance        = 0.4f * kEarthDistance,
        .revolutionSpeed = 360.f / 87.97f,
        .revolutionTilt  = 7.f,
        .rotationSpeed   = 360.f / 58.6462f,
        .rotationTilt    = 0.01f,
    };

    g_planets[ePlanet::Venus] = {
        .path            = "Venus.bmp",
        .radius          = 0.9f * kEarthRadius,
        .distance        = 0.7f * kEarthDistance,
        .revolutionSpeed = 360.f / 224.7f,
        .revolutionTilt  = 3.f,
        .rotationSpeed   = -360.f / 243.0185f,
        .rotationTilt    = 2.64f,
    };

    g_planets[ePlanet::Earth] = {
        .path            = "Earth.bmp",
        .radius          = 1.f * kEarthRadius,
        .distance        = 1.f * kEarthDistance,
        .revolutionSpeed = 360.f / 365.26f,
        .revolutionTilt  = 0.f,
        .rotationSpeed   = 360.f / 0.99726963f,
        .rotationTilt    = 23.44f,
    };

    g_planets[ePlanet::Mars] = {
        .path            = "Mars.bmp",
        .radius          = 0.5f * kEarthRadius,
        .distance        = 1.5f * kEarthDistance,
        .revolutionSpeed = 360.f / 686.9f,
        .revolutionTilt  = 1.8f,
        .rotationSpeed   = 360.f / 1.02595676f,
        .rotationTilt    = 25.19f,
    };

    g_planets[ePlanet::Jupiter] = {
        .path            = "Jupiter.bmp",
        .radius          = 11.2f * kEarthRadius,
        .distance        = 5.2f * kEarthDistance,
        .revolutionSpeed = 360.f / 4333.29f,
        .revolutionTilt  = 1.3f,
        .rotationSpeed   = 360.f / 12.6f,
        .rotationTilt    = 3.12f,
    };

    g_planets[ePlanet::Saturn] = {
        .path            = "Saturn.bmp",
        .radius          = 9.4f * kEarthRadius,
        .distance        = 9.5f * kEarthDistance,
        .revolutionSpeed = 360.f / 10756.2f,
        .revolutionTilt  = 2.4f,
        .rotationSpeed   = 360.f / 9.87f,
        .rotationTilt    = 26.73f,
    };

    g_planets[ePlanet::Uranus] = {
        .path            = "Uranos.bmp",
        .radius          = 4.f * kEarthRadius,
        .distance        = 19.2f * kEarthDistance,
        .revolutionSpeed = 360.f / 30707.49f,
        .revolutionTilt  = 0.7f,
        .rotationSpeed   = 360.f / 2.59f,
        .rotationTilt    = 82.23f,
    };

    g_planets[ePlanet::Neptune] = {
        .path            = "Neptune.bmp",
        .radius          = 3.9f * kEarthRadius,
        .distance        = 30.f * kEarthDistance,
        .revolutionSpeed = 360.f / 60223.35f,
        .revolutionTilt  = 1.7f,
        .rotationSpeed   = 360.f / 2.68f,
        .rotationTilt    = 23.33f,
    };
}

// ===========================================
//  Star
// ===========================================

constexpr int kStarCount = 900;

// ===========================================
//  Camera
// ===========================================

Vec3  g_cameraPosition  = { 0.f, 68.f, 73.f };
float g_cameraYaw       = 0.f;
float g_cameraPitch     = -43.f;
float g_cameraMoveSpeed = 15.f;
Mat4  g_projection      = {};

// ===========================================
//  Time
// ===========================================

LARGE_INTEGER g_prevTime  = {};
LARGE_INTEGER g_frequency = {};
float         g_deltaTime = 0.f;

constexpr float kTimeBoostScale = 3.f;

int   g_centerX       = kWindowWidth / 2;
int   g_centerY       = kWindowHeight / 2;
float g_days          = 1600.f;
float g_timeSum       = 0.f;
float g_dayStep       = 0.05f / kTimeBoostScale;
bool  g_isTimeBoosted = false;
int   g_frameCount    = 0;
float g_frameTimeSum  = 0.f;
float g_fps           = 0.f;

// HUD의 프레임 시간 그래프에 쓰는 최근 프레임 기록입니다.
struct FrameSample
{
    float milliseconds = 0.f;
    bool  instanced    = false;
};

constexpr int                              kFrameHistorySize  = 240;
std::array<FrameSample, kFrameHistorySize> g_frameHistory     = {};
int                                        g_frameHistoryNext = 0;

float GetDayStep()
{
    return g_isTimeBoosted ? g_dayStep * kTimeBoostScale : g_dayStep;
}

// ===========================================
//  Shader
// ===========================================

constexpr const char* kPlanetVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) uniform mat4 uModel;
layout(location = 1) uniform mat4 uViewProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition     = worldPosition.xyz;
    vNormal            = mat3(uModel) * aPosition;
    vTexCoord          = aTexCoord;
    gl_Position        = uViewProjection * worldPosition;
}
)";

constexpr const char* kPlanetFragmentShader = R"(
#version 460 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;

layout(location = 2) uniform bool uLit;
layout(binding = 0) uniform sampler2D uTexture;

out vec4 oColor;

const vec3 kAmbient  = vec3(0.12, 0.14, 0.2) * 0.2;
const vec3 kSunlight = vec3(1.0, 0.94, 0.84) * 0.8;

void main()
{
    vec4 albedo = texture(uTexture, vTexCoord);
    if (!uLit)
    {
        oColor = albedo;
        return;
    }

    float diffuse = max(dot(normalize(vNormal), normalize(-vWorldPosition)), 0.0);
    oColor        = vec4(albedo.rgb * (kAmbient + kSunlight * diffuse), albedo.a);
}
)";

constexpr const char* kColorVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 aPosition;

layout(location = 0) uniform mat4 uModelViewProjection;

void main()
{
    gl_Position = uModelViewProjection * vec4(aPosition, 1.0);
}
)";

constexpr const char* kColorFragmentShader = R"(
#version 460 core

layout(location = 1) uniform vec4 uColor;

out vec4 oColor;

void main()
{
    oColor = uColor;
}
)";

constexpr const char* kStarVertexShader = R"(
#version 460 core

layout(location = 0) in vec4 aStar;

layout(location = 0) uniform mat4 uViewProjection;
layout(location = 1) uniform vec3 uCameraPosition;

out vec3 vColor;

void main()
{
    vColor      = aStar.w * vec3(0.8, 0.88, 1.0);
    gl_Position = uViewProjection * vec4(uCameraPosition + aStar.xyz, 1.0);
}
)";

constexpr const char* kStarFragmentShader = R"(
#version 460 core

in vec3 vColor;

out vec4 oColor;

void main()
{
    oColor = vec4(vColor, 1.0);
}
)";

constexpr const char* kGlowVertexShader = R"(
#version 460 core

layout(location = 0) in vec2 aCorner;

layout(location = 0) uniform mat4 uViewProjection;
layout(location = 1) uniform vec3 uCameraRight;
layout(location = 2) uniform vec3 uCameraUp;
layout(location = 3) uniform float uRadius;

out vec2 vCorner;

void main()
{
    vCorner     = aCorner;
    gl_Position = uViewProjection * vec4((uCameraRight * aCorner.x + uCameraUp * aCorner.y) * uRadius, 1.0);
}
)";

constexpr const char* kGlowFragmentShader = R"(
#version 460 core

in vec2 vCorner;

out vec4 oColor;

void main()
{
    float falloff = length(vCorner);
    if (falloff > 1.0)
    {
        discard;
    }

    oColor = mix(vec4(1.0, 0.45, 0.08, 0.65), vec4(1.0, 0.2, 0.02, 0.0), falloff);
}
)";

constexpr const char* kAsteroidVertexShader = R"(
#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in vec4 aOrbit;
layout(location = 5) in vec4 aSpin;
layout(location = 6) in vec4 aScale;

layout(location = 0) uniform mat4 uViewProjection;
layout(location = 1) uniform float uDays;

out vec3 vWorldPosition;
out vec2 vTexCoord;
out mat3 vTangentToWorld;

mat3 AxisAngle(vec3 axis, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;
    return mat3(t * axis.x * axis.x + c,          t * axis.x * axis.y + s * axis.z, t * axis.x * axis.z - s * axis.y,
                t * axis.x * axis.y - s * axis.z, t * axis.y * axis.y + c,          t * axis.y * axis.z + s * axis.x,
                t * axis.x * axis.z + s * axis.y, t * axis.y * axis.z - s * axis.x, t * axis.z * axis.z + c);
}

void main()
{
    mat3  rotation = AxisAngle(aSpin.xyz, aScale.w + aSpin.w * uDays);
    float angle    = aOrbit.z + aOrbit.w * uDays;
    vec3  center   = vec3(aOrbit.x * cos(angle), aOrbit.y, -aOrbit.x * sin(angle));

    vec3 normal    = normalize(rotation * (aNormal / aScale.xyz));
    vec3 tangent   = normalize(rotation * (aTangent.xyz * aScale.xyz));
    tangent        = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = cross(normal, tangent) * aTangent.w;

    vWorldPosition  = center + rotation * (aPosition * aScale.xyz);
    vTexCoord       = aTexCoord;
    vTangentToWorld = mat3(tangent, bitangent, normal);
    gl_Position     = uViewProjection * vec4(vWorldPosition, 1.0);
}
)";

constexpr const char* kAsteroidFragmentShader = R"(
#version 460 core

in vec3 vWorldPosition;
in vec2 vTexCoord;
in mat3 vTangentToWorld;

layout(binding = 0) uniform sampler2D uDiffuse;
layout(binding = 1) uniform sampler2D uNormal;

out vec4 oColor;

const vec3 kAmbient  = vec3(0.12, 0.14, 0.2) * 0.2;
const vec3 kSunlight = vec3(1.0, 0.94, 0.84) * 0.8;

void main()
{
    vec3  normal  = normalize(vTangentToWorld * (texture(uNormal, vTexCoord).xyz * 2.0 - 1.0));
    float diffuse = max(dot(normal, normalize(-vWorldPosition)), 0.0);
    vec3  albedo  = texture(uDiffuse, vTexCoord).rgb;
    oColor        = vec4(albedo * (kAmbient + kSunlight * diffuse), 1.0);
}
)";

// 화면 왼쪽 위의 성능 표시(HUD)를 그리는 셰이더입니다. 정점은 픽셀 좌표로 들어옵니다.
constexpr const char* kHudVertexShader = R"(
#version 460 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

layout(location = 0) uniform vec2 uViewportSize;

out vec4 vColor;

void main()
{
    vColor      = aColor;
    gl_Position = vec4(aPosition.x / uViewportSize.x * 2.0 - 1.0, 1.0 - aPosition.y / uViewportSize.y * 2.0, 0.0, 1.0);
}
)";

constexpr const char* kHudFragmentShader = R"(
#version 460 core

in vec4 vColor;

out vec4 oColor;

void main()
{
    oColor = vColor;
}
)";

GLuint g_planetProgram   = 0;
GLuint g_colorProgram    = 0;
GLuint g_starProgram     = 0;
GLuint g_glowProgram     = 0;
GLuint g_asteroidProgram = 0;
GLuint g_hudProgram      = 0;

GLuint CompileShader(
    const GLenum      _type,
    const char* const _source)
{
    const GLuint shader = glCreateShader(_type);
    glShaderSource(shader, 1, &_source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        std::array<char, 1024> log = {};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << "Failed to compile shader: " << log.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateProgram(
    const char* const _vertexSource,
    const char* const _fragmentSource)
{
    const GLuint vertexShader   = CompileShader(GL_VERTEX_SHADER, _vertexSource);
    const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, _fragmentSource);
    if (!vertexShader || !fragmentShader)
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        std::array<char, 1024> log = {};
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << "Failed to link program: " << log.data() << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// ===========================================
//  Mesh
// ===========================================

struct Mesh
{
    GLuint  vao   = 0;
    GLuint  vbo   = 0;
    GLuint  ebo   = 0;
    GLenum  mode  = GL_TRIANGLES;
    GLsizei count = 0;
};

Mesh g_sphereMesh   = {};
Mesh g_ringMesh     = {};
Mesh g_orbitMesh    = {};
Mesh g_axisMesh     = {};
Mesh g_starMesh     = {};
Mesh g_glowMesh     = {};

Mesh CreateMesh(
    const GLenum                       _mode,
    const std::span<const float>       _vertices,
    const std::initializer_list<GLint> _attributeSizes,
    const std::span<const GLuint>      _indices = {})
{
    Mesh mesh = { .mode = _mode };

    GLint stride = 0;
    for (const GLint size: _attributeSizes)
    {
        stride += size;
    }

    glCreateBuffers(1, &mesh.vbo);
    glNamedBufferStorage(mesh.vbo, static_cast<GLsizeiptr>(_vertices.size_bytes()), _vertices.data(), 0);

    glCreateVertexArrays(1, &mesh.vao);
    glVertexArrayVertexBuffer(mesh.vao, 0, mesh.vbo, 0, stride * static_cast<GLsizei>(sizeof(float)));

    GLuint location = 0;
    GLuint offset   = 0;
    for (const GLint size: _attributeSizes)
    {
        glEnableVertexArrayAttrib(mesh.vao, location);
        glVertexArrayAttribFormat(mesh.vao, location, size, GL_FLOAT, GL_FALSE, offset);
        glVertexArrayAttribBinding(mesh.vao, location, 0);
        ++location;
        offset += static_cast<GLuint>(size * sizeof(float));
    }

    if (_indices.empty())
    {
        mesh.count = static_cast<GLsizei>(_vertices.size() / static_cast<size_t>(stride));
        return mesh;
    }

    glCreateBuffers(1, &mesh.ebo);
    glNamedBufferStorage(mesh.ebo, static_cast<GLsizeiptr>(_indices.size_bytes()), _indices.data(), 0);
    glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
    mesh.count = static_cast<GLsizei>(_indices.size());
    return mesh;
}

void DestroyMesh(Mesh& _mesh)
{
    glDeleteVertexArrays(1, &_mesh.vao);
    glDeleteBuffers(1, &_mesh.vbo);
    glDeleteBuffers(1, &_mesh.ebo);
    _mesh = {};
}

void DrawMesh(const Mesh& _mesh)
{
    glBindVertexArray(_mesh.vao);
    if (_mesh.ebo)
    {
        glDrawElements(_mesh.mode, _mesh.count, GL_UNSIGNED_INT, nullptr);
    }
    else
    {
        glDrawArrays(_mesh.mode, 0, _mesh.count);
    }
}

Mesh CreateSphereMesh(
    const int _slices,
    const int _stacks)
{
    std::vector<float> vertices;
    for (int stack = 0; stack <= _stacks; ++stack)
    {
        const float v   = static_cast<float>(stack) / static_cast<float>(_stacks);
        const float rho = kPi * v;
        for (int slice = 0; slice <= _slices; ++slice)
        {
            const float u     = static_cast<float>(slice) / static_cast<float>(_slices);
            const float theta = 2.f * kPi * u;
            vertices.insert(vertices.end(), { std::sin(theta) * std::sin(rho), std::cos(rho), -std::cos(theta) * std::sin(rho), 1.f - u, 1.f - v });
        }
    }

    std::vector<GLuint> indices;
    const auto          rowLength = static_cast<GLuint>(_slices + 1);
    for (int stack = 0; stack < _stacks; ++stack)
    {
        for (int slice = 0; slice < _slices; ++slice)
        {
            const GLuint top    = static_cast<GLuint>(stack) * rowLength + static_cast<GLuint>(slice);
            const GLuint bottom = top + rowLength;
            indices.insert(indices.end(), { top, bottom, top + 1, top + 1, bottom, bottom + 1 });
        }
    }

    return CreateMesh(GL_TRIANGLES, vertices, { 3, 2 }, indices);
}

Mesh CreateRingMesh(
    const float _innerRadius,
    const float _outerRadius,
    const int   _slices)
{
    std::vector<float> vertices;
    for (int slice = 0; slice <= _slices; ++slice)
    {
        const float angle = 2.f * kPi * static_cast<float>(slice) / static_cast<float>(_slices);
        const float c     = std::cos(angle);
        const float s     = std::sin(angle);
        vertices.insert(vertices.end(), { _innerRadius * c, 0.f, -_innerRadius * s, _outerRadius * c, 0.f, -_outerRadius * s });
    }
    return CreateMesh(GL_TRIANGLE_STRIP, vertices, { 3 });
}

Mesh CreateCircleMesh(const int _segments)
{
    std::vector<float> vertices;
    for (int segment = 0; segment < _segments; ++segment)
    {
        const float angle = 2.f * kPi * static_cast<float>(segment) / static_cast<float>(_segments);
        vertices.insert(vertices.end(), { std::cos(angle), 0.f, -std::sin(angle) });
    }
    return CreateMesh(GL_LINE_LOOP, vertices, { 3 });
}

Mesh CreateAxisMesh()
{
    constexpr std::array vertices = { 0.f, 1.f, 0.f, 0.f, -1.f, 0.f };
    return CreateMesh(GL_LINES, vertices, { 3 });
}

Mesh CreateStarMesh(const int _count)
{
    std::mt19937                          random { 7 };
    std::uniform_real_distribution<float> position { -500.f, 500.f };
    std::uniform_real_distribution<float> brightness { 0.2f, 0.75f };

    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(_count) * 4);
    for (int i = 0; i < _count; ++i)
    {
        vertices.insert(vertices.end(), { position(random), position(random), position(random), brightness(random) });
    }
    return CreateMesh(GL_POINTS, vertices, { 4 });
}

Mesh CreateQuadMesh()
{
    constexpr std::array vertices = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    return CreateMesh(GL_TRIANGLE_STRIP, vertices, { 2 });
}

// ===========================================
//  Asteroid
// ===========================================

// 숫자 키 1~4로 고르는 소행성 수입니다. 인스턴스 버퍼는 최대 개수만큼 한 번 만들고 앞에서부터 사용합니다.
// 기본 장면은 4,000개이고, 그보다 많으면 원거리 LOD 수준의 저폴리 바위로 그립니다.
constexpr std::array kAsteroidCountPresets = { 4'000, 10'000, 50'000, 100'000 };
constexpr int        kMaxAsteroidCount     = kAsteroidCountPresets.back();
constexpr int        kDetailedRockMaxCount = kAsteroidCountPresets.front();
constexpr int        kAsteroidStride       = 12;   // 개체당 float 12개 (aOrbit, aSpin, aScale)

int  g_asteroidCount = kAsteroidCountPresets.front();
bool g_useInstancing = true;   // I 키: 인스턴싱 켜기/끄기

Mesh   g_lowPolyRockMesh         = {};
Mesh   g_detailedRockMesh        = {};
GLuint g_asteroidInstanceBuffer  = 0;
std::vector<float> g_asteroidInstances;   // 인스턴싱을 끈 경우 CPU가 소행성마다 읽어 보내는 같은 데이터

GLuint g_rockDiffuseTexture = 0;
GLuint g_rockNormalTexture  = 0;

struct ObjVertex
{
    Vec3  position   = {};
    Vec3  normal     = {};
    float u          = 0.f;
    float v          = 0.f;
    Vec3  tangent    = {};
    float handedness = 1.f;
};

static_assert(sizeof(ObjVertex) == sizeof(float) * 12);

bool ParseObjFaceToken(
    const std::string&  _token,
    std::array<int, 3>& _indices)
{
    size_t start = 0;
    for (int& index: _indices)
    {
        if (start > _token.size())
        {
            return false;
        }

        const size_t end = std::min(_token.find('/', start), _token.size());
        if (std::from_chars(_token.data() + start, _token.data() + end, index).ec != std::errc {})
        {
            return false;
        }
        start = end + 1;
    }
    return true;
}

bool LoadObjMesh(
    const std::string& _path,
    Mesh&              _mesh)
{
    std::ifstream file(_path);
    if (!file)
    {
        std::cerr << "Error loading model: " << _path << '\n';
        return false;
    }

    std::vector<Vec3>                       positions;
    std::vector<Vec3>                       normals;
    std::vector<std::array<float, 2>>       texCoords;
    std::vector<ObjVertex>                  vertices;
    std::vector<GLuint>                     indices;
    std::unordered_map<std::string, GLuint> vertexCache;

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string        type;
        stream >> type;

        if (type == "v")
        {
            Vec3& position = positions.emplace_back();
            stream >> position.x >> position.y >> position.z;
        }
        else if (type == "vn")
        {
            Vec3& normal = normals.emplace_back();
            stream >> normal.x >> normal.y >> normal.z;
        }
        else if (type == "vt")
        {
            std::array<float, 2>& texCoord = texCoords.emplace_back();
            stream >> texCoord[0] >> texCoord[1];
        }
        else if (type == "f")
        {
            std::vector<GLuint> face;
            std::string         token;
            while (stream >> token)
            {
                const auto [it, inserted] = vertexCache.try_emplace(token, static_cast<GLuint>(vertices.size()));
                if (inserted)
                {
                    std::array<int, 3> objIndices = {};
                    if (!ParseObjFaceToken(token, objIndices)
                        || objIndices[0] < 1 || objIndices[0] > static_cast<int>(positions.size())
                        || objIndices[1] < 1 || objIndices[1] > static_cast<int>(texCoords.size())
                        || objIndices[2] < 1 || objIndices[2] > static_cast<int>(normals.size()))
                    {
                        std::cerr << "Unsupported face in model: " << _path << ": " << token << '\n';
                        return false;
                    }

                    const std::array<float, 2>& texCoord = texCoords[objIndices[1] - 1];
                    vertices.push_back({ .position = positions[objIndices[0] - 1],
                                         .normal   = normals[objIndices[2] - 1],
                                         .u        = texCoord[0],
                                         .v        = texCoord[1] });
                }
                face.push_back(it->second);
            }

            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                indices.insert(indices.end(), { face[0], face[i], face[i + 1] });
            }
        }
    }

    if (indices.empty())
    {
        std::cerr << "Empty model: " << _path << '\n';
        return false;
    }

    float boundingRadius = 0.f;
    for (const ObjVertex& vertex: vertices)
    {
        boundingRadius = std::max(boundingRadius, std::sqrt(Dot(vertex.position, vertex.position)));
    }

    std::vector<Vec3> bitangents(vertices.size());
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        ObjVertex& v0 = vertices[indices[i]];
        ObjVertex& v1 = vertices[indices[i + 1]];
        ObjVertex& v2 = vertices[indices[i + 2]];

        const Vec3  edge1 = v1.position - v0.position;
        const Vec3  edge2 = v2.position - v0.position;
        const float du1   = v1.u - v0.u;
        const float dv1   = v1.v - v0.v;
        const float du2   = v2.u - v0.u;
        const float dv2   = v2.v - v0.v;
        const float area  = du1 * dv2 - du2 * dv1;
        if (std::abs(area) < 1e-12f)
        {
            continue;
        }

        const Vec3 tangent   = (edge1 * dv2 - edge2 * dv1) * (1.f / area);
        const Vec3 bitangent = (edge2 * du1 - edge1 * du2) * (1.f / area);
        for (int k = 0; k < 3; ++k)
        {
            const GLuint index      = indices[i + k];
            vertices[index].tangent = vertices[index].tangent + tangent;
            bitangents[index]       = bitangents[index] + bitangent;
        }
    }

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        ObjVertex& vertex = vertices[i];
        vertex.position   = vertex.position * (1.f / boundingRadius);
        vertex.normal     = Normalize(vertex.normal);

        Vec3 tangent = vertex.tangent - vertex.normal * Dot(vertex.normal, vertex.tangent);
        if (Dot(tangent, tangent) < 1e-12f)
        {
            tangent = Cross(vertex.normal, std::abs(vertex.normal.y) < 0.99f ? Vec3 { 0.f, 1.f, 0.f } : Vec3 { 1.f, 0.f, 0.f });
        }
        vertex.tangent    = Normalize(tangent);
        vertex.handedness = Dot(Cross(vertex.normal, vertex.tangent), bitangents[i]) < 0.f ? -1.f : 1.f;
    }

    const std::span<const float> vertexData(reinterpret_cast<const float*>(vertices.data()), vertices.size() * 12);
    _mesh = CreateMesh(GL_TRIANGLES, vertexData, { 3, 3, 2, 4 }, indices);
    return true;
}

// 10만 개를 그릴 때 사용하는 저폴리 바위(원거리 LOD 수준).
// 정이십면체를 한 번 분할(정점 42개, 삼각형 80개)하고 정점마다 반지름을 흔들어 바위 모양을 만듭니다.
Mesh CreateLowPolyRockMesh()
{
    const float       t         = (1.f + std::sqrt(5.f)) * 0.5f;
    std::vector<Vec3> positions = {
        { -1.f, t, 0.f }, { 1.f, t, 0.f }, { -1.f, -t, 0.f }, { 1.f, -t, 0.f },
        { 0.f, -1.f, t }, { 0.f, 1.f, t }, { 0.f, -1.f, -t }, { 0.f, 1.f, -t },
        { t, 0.f, -1.f }, { t, 0.f, 1.f }, { -t, 0.f, -1.f }, { -t, 0.f, 1.f }
    };
    constexpr std::array<GLuint, 60> faces = {
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
        3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
    };
    for (Vec3& position: positions)
    {
        position = Normalize(position);
    }

    std::unordered_map<unsigned long long, GLuint> midpoints;
    const auto midpoint = [&](const GLuint _a, const GLuint _b)
    {
        const unsigned long long key = (static_cast<unsigned long long>(std::min(_a, _b)) << 32) | std::max(_a, _b);
        const auto [it, inserted]    = midpoints.try_emplace(key, static_cast<GLuint>(positions.size()));
        if (inserted)
        {
            positions.push_back(Normalize((positions[_a] + positions[_b]) * 0.5f));
        }
        return it->second;
    };

    std::vector<GLuint> indices;
    for (size_t i = 0; i < faces.size(); i += 3)
    {
        const GLuint a = faces[i], b = faces[i + 1], c = faces[i + 2];
        const GLuint ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
        indices.insert(indices.end(), { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
    }

    std::mt19937                          random { 7 };
    std::uniform_real_distribution<float> jitter { 0.8f, 1.f };
    std::vector<ObjVertex>                vertices(positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
    {
        const Vec3 direction = positions[i];
        vertices[i].position = direction * jitter(random);
        vertices[i].u        = 0.5f + std::atan2(direction.z, direction.x) / (2.f * kPi);
        vertices[i].v        = std::acos(std::clamp(direction.y, -1.f, 1.f)) / kPi;
    }

    std::vector<Vec3> bitangents(vertices.size());
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        const ObjVertex& v0         = vertices[indices[i]];
        const ObjVertex& v1         = vertices[indices[i + 1]];
        const ObjVertex& v2         = vertices[indices[i + 2]];
        const Vec3       edge1      = v1.position - v0.position;
        const Vec3       edge2      = v2.position - v0.position;
        const Vec3       faceNormal = Cross(edge1, edge2);
        const float      du1 = v1.u - v0.u, dv1 = v1.v - v0.v, du2 = v2.u - v0.u, dv2 = v2.v - v0.v;
        const float      area      = du1 * dv2 - du2 * dv1;
        const bool       isFlat    = std::abs(area) < 1e-12f;
        const Vec3       tangent   = isFlat ? Vec3 {} : (edge1 * dv2 - edge2 * dv1) * (1.f / area);
        const Vec3       bitangent = isFlat ? Vec3 {} : (edge2 * du1 - edge1 * du2) * (1.f / area);
        for (int k = 0; k < 3; ++k)
        {
            ObjVertex& vertex           = vertices[indices[i + k]];
            vertex.normal               = vertex.normal + faceNormal;
            vertex.tangent              = vertex.tangent + tangent;
            bitangents[indices[i + k]] = bitangents[indices[i + k]] + bitangent;
        }
    }
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        ObjVertex& vertex = vertices[i];
        vertex.normal     = Normalize(vertex.normal);
        Vec3 tangent      = vertex.tangent - vertex.normal * Dot(vertex.normal, vertex.tangent);
        if (Dot(tangent, tangent) < 1e-12f)
        {
            tangent = Cross(vertex.normal, std::abs(vertex.normal.y) < 0.99f ? Vec3 { 0.f, 1.f, 0.f } : Vec3 { 1.f, 0.f, 0.f });
        }
        vertex.tangent    = Normalize(tangent);
        vertex.handedness = Dot(Cross(vertex.normal, vertex.tangent), bitangents[i]) < 0.f ? -1.f : 1.f;
    }

    const std::span<const float> vertexData(reinterpret_cast<const float*>(vertices.data()), vertices.size() * 12);
    return CreateMesh(GL_TRIANGLES, vertexData, { 3, 3, 2, 4 }, indices);
}

void CreateAsteroidInstances(const int _count)
{
    std::mt19937                          random { 42 };
    std::normal_distribution<float>       semiMajorAxis { 2.7f, 0.3f };
    std::normal_distribution<float>       height { 0.f, 0.35f };
    std::uniform_real_distribution<float> unit { 0.f, 1.f };
    std::uniform_real_distribution<float> signedUnit { -1.f, 1.f };

    std::vector<float>& instances = g_asteroidInstances;
    instances.clear();
    instances.reserve(static_cast<size_t>(_count) * kAsteroidStride);
    for (int i = 0; i < _count; ++i)
    {
        const float au           = std::clamp(semiMajorAxis(random), 2.1f, 3.3f);
        const float radius       = ToDisplayDistance(au * kEarthDistance);
        const float angularSpeed = 2.f * kPi / (365.26f * au * std::sqrt(au));
        const float phase        = 2.f * kPi * unit(random);
        const Vec3  spinAxis     = Normalize({ signedUnit(random), signedUnit(random), signedUnit(random) + 1e-4f });
        const float spinSpeed    = ToRadians(20.f + 100.f * unit(random)) * (unit(random) < 0.5f ? -1.f : 1.f);
        const float spinPhase    = 2.f * kPi * unit(random);
        const float size         = 0.06f + 0.2f * std::pow(unit(random), 3.f);

        instances.insert(instances.end(), { radius, height(random), phase, angularSpeed,
                                            spinAxis.x, spinAxis.y, spinAxis.z, spinSpeed,
                                            size * (0.75f + 0.5f * unit(random)),
                                            size * (0.75f + 0.5f * unit(random)),
                                            size * (0.75f + 0.5f * unit(random)),
                                            spinPhase });
    }

    glCreateBuffers(1, &g_asteroidInstanceBuffer);
    glNamedBufferStorage(g_asteroidInstanceBuffer, static_cast<GLsizeiptr>(instances.size() * sizeof(float)), instances.data(), 0);
}

// 두 바위 메시가 같은 인스턴스 버퍼를 바인딩 슬롯 1에서 읽도록 연결합니다.
void AttachAsteroidInstances(const Mesh& _mesh)
{
    glVertexArrayVertexBuffer(_mesh.vao, 1, g_asteroidInstanceBuffer, 0, kAsteroidStride * static_cast<GLsizei>(sizeof(float)));
    glVertexArrayBindingDivisor(_mesh.vao, 1, 1);
    for (GLuint i = 0; i < 3; ++i)
    {
        const GLuint location = 4 + i;
        glEnableVertexArrayAttrib(_mesh.vao, location);
        glVertexArrayAttribFormat(_mesh.vao, location, 4, GL_FLOAT, GL_FALSE, i * 4 * static_cast<GLuint>(sizeof(float)));
        glVertexArrayAttribBinding(_mesh.vao, location, 1);
    }
}

const Mesh& GetAsteroidMesh()
{
    return g_asteroidCount <= kDetailedRockMaxCount ? g_detailedRockMesh : g_lowPolyRockMesh;
}

int GetAsteroidDrawCalls()
{
    return g_useInstancing ? 1 : g_asteroidCount;
}

void DrawAsteroids()
{
    const Mesh& mesh = GetAsteroidMesh();
    glBindVertexArray(mesh.vao);

    if (g_useInstancing)
    {
        // 인스턴싱: 드로우 명령 한 번으로 모든 소행성을 그립니다.
        glDrawElementsInstanced(mesh.mode, mesh.count, GL_UNSIGNED_INT, nullptr, g_asteroidCount);
        return;
    }

    // 인스턴싱 없이: 인스턴스 속성 배열을 끄고, 소행성마다 같은 값을 상수 속성으로 설정한 뒤 따로 그립니다.
    // 셰이더와 데이터는 그대로이고 드로우 명령의 수만 소행성 수만큼 늘어납니다.
    for (GLuint location = 4; location <= 6; ++location)
    {
        glDisableVertexArrayAttrib(mesh.vao, location);
    }
    for (int i = 0; i < g_asteroidCount; ++i)
    {
        const float* const data = g_asteroidInstances.data() + static_cast<size_t>(i) * kAsteroidStride;
        glVertexAttrib4fv(4, data);
        glVertexAttrib4fv(5, data + 4);
        glVertexAttrib4fv(6, data + 8);
        glDrawElements(mesh.mode, mesh.count, GL_UNSIGNED_INT, nullptr);
    }
    for (GLuint location = 4; location <= 6; ++location)
    {
        glEnableVertexArrayAttrib(mesh.vao, location);
    }
}

// ===========================================
//  Render
// ===========================================

constexpr std::array kOrbitColor = { 0.42f, 0.58f, 0.72f, 0.45f };
constexpr std::array kAxisColor  = { 0.5f, 0.65f, 0.8f, 0.3f };
constexpr std::array kRingColor  = { 0.7f, 0.62f, 0.46f, 0.7f };

std::string WithCommas(const long long _value)
{
    std::string text = std::to_string(_value);
    for (int i = static_cast<int>(text.size()) - 3; i > 0; i -= 3)
    {
        text.insert(static_cast<size_t>(i), ",");
    }
    return text;
}

void UpdateWindowTitle()
{
    const std::string title = std::format("Instancing: {} | Asteroids: {} | Draw calls: {} | FPS: {:.0f} | Camera move speed: {:.2f} | Time flow: {:.2f} day/sec{} | Cur Time: {} day",
                                          g_useInstancing ? "ON" : "OFF",
                                          WithCommas(g_asteroidCount),
                                          WithCommas(GetAsteroidDrawCalls()),
                                          g_fps,
                                          g_cameraMoveSpeed,
                                          GetDayStep() / kDayUpdateInterval,
                                          g_isTimeBoosted ? " (x3)" : "",
                                          static_cast<int>(g_days));

    ::SetWindowTextA(g_hWnd, title.c_str());
}

void Reshape(
    const int _width,
    const int _height)
{
    glViewport(0, 0, _width, _height);
    if (_width == 0 || _height == 0)
    {
        return;
    }

    g_viewportWidth  = _width;
    g_viewportHeight = _height;

    g_projection = Perspective(40.f, static_cast<float>(_width) / static_cast<float>(_height), 0.1f, 1000.f);
    g_centerX    = _width / 2;
    g_centerY    = _height / 2;
}

Vec3 GetCameraForward()
{
    const float yaw   = ToRadians(g_cameraYaw);
    const float pitch = ToRadians(g_cameraPitch);
    return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
}

Vec3 GetCameraRight()
{
    const float yaw = ToRadians(g_cameraYaw);
    return { std::cos(yaw), 0.f, std::sin(yaw) };
}

Mat4 CreateViewMatrix()
{
    return LookAt(g_cameraPosition, g_cameraPosition + GetCameraForward(), { 0.f, 1.f, 0.f });
}

void DrawColorMesh(
    const Mesh&                 _mesh,
    const Mat4&                 _modelViewProjection,
    const std::array<float, 4>& _color)
{
    glUseProgram(g_colorProgram);
    glProgramUniformMatrix4fv(g_colorProgram, 0, 1, GL_FALSE, _modelViewProjection.m.data());
    glProgramUniform4fv(g_colorProgram, 1, 1, _color.data());
    DrawMesh(_mesh);
}

// ===========================================
//  HUD
// ===========================================

struct HudVertex
{
    float x = 0.f;
    float y = 0.f;
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    float a = 0.f;
};

struct HudColor
{
    float r = 1.f;
    float g = 1.f;
    float b = 1.f;
    float a = 1.f;
};

constexpr HudColor kHudOnColor    = { 0.36f, 0.92f, 0.5f, 1.f };
constexpr HudColor kHudOffColor   = { 1.f, 0.42f, 0.36f, 1.f };
constexpr HudColor kHudTextColor  = { 0.93f, 0.95f, 1.f, 1.f };
constexpr HudColor kHudDimColor   = { 0.62f, 0.68f, 0.78f, 1.f };
constexpr HudColor kHudFpsColor   = { 1.f, 0.86f, 0.35f, 1.f };
constexpr HudColor kHudPanelColor = { 0.f, 0.f, 0.f, 0.62f };

GLuint                 g_hudVao  = 0;
GLuint                 g_hudVbo  = 0;
bool                   g_showHud = true;   // H 키
std::vector<HudVertex> g_hudVertices;

void CreateHud()
{
    glCreateBuffers(1, &g_hudVbo);
    glCreateVertexArrays(1, &g_hudVao);
    glVertexArrayVertexBuffer(g_hudVao, 0, g_hudVbo, 0, sizeof(HudVertex));
    glEnableVertexArrayAttrib(g_hudVao, 0);
    glVertexArrayAttribFormat(g_hudVao, 0, 2, GL_FLOAT, GL_FALSE, offsetof(HudVertex, x));
    glVertexArrayAttribBinding(g_hudVao, 0, 0);
    glEnableVertexArrayAttrib(g_hudVao, 1);
    glVertexArrayAttribFormat(g_hudVao, 1, 4, GL_FLOAT, GL_FALSE, offsetof(HudVertex, r));
    glVertexArrayAttribBinding(g_hudVao, 1, 0);
}

void DestroyHud()
{
    glDeleteVertexArrays(1, &g_hudVao);
    glDeleteBuffers(1, &g_hudVbo);
    g_hudVao = 0;
    g_hudVbo = 0;
}

void AddHudRect(
    const float     _x,
    const float     _y,
    const float     _width,
    const float     _height,
    const HudColor& _color)
{
    const HudVertex v0 = { _x, _y, _color.r, _color.g, _color.b, _color.a };
    const HudVertex v1 = { _x + _width, _y, _color.r, _color.g, _color.b, _color.a };
    const HudVertex v2 = { _x + _width, _y + _height, _color.r, _color.g, _color.b, _color.a };
    const HudVertex v3 = { _x, _y + _height, _color.r, _color.g, _color.b, _color.a };
    g_hudVertices.insert(g_hudVertices.end(), { v0, v1, v2, v0, v2, v3 });
}

// stb_easy_font는 글자를 사각형 목록으로 만들어 줍니다. 각 사각형을 삼각형 두 개로 바꿔 쌓습니다.
float AddHudText(
    const float        _x,
    const float        _y,
    const float        _scale,
    const std::string& _text,
    const HudColor&    _color)
{
    struct EasyFontVertex
    {
        float         x, y, z;
        unsigned char color[4];
    };

    static std::array<EasyFontVertex, 4 * 1024> buffer;
    std::string                                 text  = _text;
    unsigned char                               white[4] = { 255, 255, 255, 255 };
    const int quadCount = stb_easy_font_print(0.f, 0.f, text.data(), white, buffer.data(), static_cast<int>(sizeof(buffer)));
    for (int quad = 0; quad < quadCount; ++quad)
    {
        const EasyFontVertex* const q = &buffer[static_cast<size_t>(quad) * 4];
        AddHudRect(_x + q[0].x * _scale, _y + q[0].y * _scale, (q[2].x - q[0].x) * _scale, (q[2].y - q[0].y) * _scale, _color);
    }
    return static_cast<float>(stb_easy_font_width(text.data())) * _scale;
}

void DrawHud()
{
    if (!g_showHud)
    {
        return;
    }

    g_hudVertices.clear();

    const float     unit      = static_cast<float>(g_viewportHeight) / 1080.f;
    const float     left      = 28.f * unit;
    const float     top       = 28.f * unit;
    const float     padding   = 22.f * unit;
    const float     panelW    = 760.f * unit;
    const float     graphH    = 120.f * unit;
    const float     panelH    = 520.f * unit;
    const HudColor& modeColor = g_useInstancing ? kHudOnColor : kHudOffColor;
    const float     frameMs   = g_fps > 0.f ? 1000.f / g_fps : 0.f;
    const int       drawCalls = GetAsteroidDrawCalls();

    AddHudRect(left, top, panelW, panelH, kHudPanelColor);

    float y = top + padding;
    float x = left + padding;
    x += AddHudText(x, y, 5.f * unit, "INSTANCING ", kHudTextColor);
    AddHudText(x, y, 5.f * unit, g_useInstancing ? "ON" : "OFF", modeColor);
    y += 62.f * unit;

    AddHudText(left + padding, y, 6.f * unit, std::format("{:.0f} FPS", g_fps), kHudFpsColor);
    y += 72.f * unit;
    AddHudText(left + padding, y, 3.f * unit, std::format("{:.2f} ms / frame", frameMs), kHudTextColor);
    y += 46.f * unit;

    AddHudText(left + padding, y, 3.f * unit, std::format("{} asteroids", WithCommas(g_asteroidCount)), kHudTextColor);
    y += 38.f * unit;
    x = left + padding;
    x += AddHudText(x, y, 3.f * unit, WithCommas(drawCalls), modeColor);
    AddHudText(x, y, 3.f * unit, drawCalls == 1 ? " draw call" : " draw calls", kHudTextColor);
    y += 46.f * unit;

    // 최근 프레임 시간 그래프 (0~50ms). 막대 색은 그 프레임의 인스턴싱 상태입니다.
    const float graphX  = left + padding;
    const float graphW  = panelW - padding * 2.f;
    const float graphY  = y;
    const float barW    = graphW / static_cast<float>(kFrameHistorySize);
    const float maxMs   = 50.f;
    AddHudRect(graphX, graphY, graphW, graphH, { 1.f, 1.f, 1.f, 0.06f });
    for (int i = 0; i < kFrameHistorySize; ++i)
    {
        const FrameSample& sample = g_frameHistory[static_cast<size_t>((g_frameHistoryNext + i) % kFrameHistorySize)];
        if (sample.milliseconds <= 0.f)
        {
            continue;
        }
        const float height = std::min(sample.milliseconds, maxMs) / maxMs * graphH;
        AddHudRect(graphX + barW * static_cast<float>(i), graphY + graphH - height, std::max(barW - unit, unit), height,
                   sample.instanced ? kHudOnColor : kHudOffColor);
    }
    const float line60 = graphY + graphH - (1000.f / 60.f) / maxMs * graphH;
    AddHudRect(graphX, line60, graphW, std::max(unit * 1.5f, 1.f), { 1.f, 1.f, 1.f, 0.55f });
    std::string line60Label = "16.7 ms (60 FPS)";
    const float line60Width = static_cast<float>(stb_easy_font_width(line60Label.data())) * 2.f * unit;
    AddHudText(graphX + graphW - line60Width - 6.f * unit, line60 - 20.f * unit, 2.f * unit, line60Label, kHudTextColor);
    AddHudText(graphX, graphY + graphH + 8.f * unit, 2.f * unit, "frame time, last 240 frames (0-50 ms)", kHudDimColor);
    y = graphY + graphH + 40.f * unit;

    AddHudText(left + padding, y, 2.f * unit,
               g_asteroidCount <= kDetailedRockMaxCount ? std::format("rock mesh: detailed ({} triangles)", WithCommas(GetAsteroidMesh().count / 3))
                                 : std::format("rock mesh: low-poly ({} triangles)", GetAsteroidMesh().count / 3),
               kHudDimColor);
    y += 26.f * unit;
    AddHudText(left + padding, y, 2.f * unit, "[I] instancing  [1-4] 4K/10K/50K/100K  [H] hide", kHudDimColor);

    glNamedBufferData(g_hudVbo, static_cast<GLsizeiptr>(g_hudVertices.size() * sizeof(HudVertex)), g_hudVertices.data(), GL_STREAM_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(g_hudProgram);
    glProgramUniform2f(g_hudProgram, 0, static_cast<float>(g_viewportWidth), static_cast<float>(g_viewportHeight));
    glBindVertexArray(g_hudVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_hudVertices.size()));
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Display()
{
    const Mat4 view           = CreateViewMatrix();
    const Mat4 viewProjection = g_projection * view;

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    glUseProgram(g_starProgram);
    glProgramUniformMatrix4fv(g_starProgram, 0, 1, GL_FALSE, viewProjection.m.data());
    glProgramUniform3f(g_starProgram, 1, g_cameraPosition.x, g_cameraPosition.y, g_cameraPosition.z);
    DrawMesh(g_starMesh);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glUseProgram(g_glowProgram);
    glProgramUniformMatrix4fv(g_glowProgram, 0, 1, GL_FALSE, viewProjection.m.data());
    glProgramUniform3f(g_glowProgram, 1, view.m[0], view.m[4], view.m[8]);
    glProgramUniform3f(g_glowProgram, 2, view.m[1], view.m[5], view.m[9]);
    glProgramUniform1f(g_glowProgram, 3, g_planets[ePlanet::Sun].radius * 2.5f);
    DrawMesh(g_glowMesh);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    glUseProgram(g_asteroidProgram);
    glProgramUniformMatrix4fv(g_asteroidProgram, 0, 1, GL_FALSE, viewProjection.m.data());
    glProgramUniform1f(g_asteroidProgram, 1, g_days);
    glBindTextureUnit(0, g_rockDiffuseTexture);
    glBindTextureUnit(1, g_rockNormalTexture);
    DrawAsteroids();

    glProgramUniformMatrix4fv(g_planetProgram, 1, 1, GL_FALSE, viewProjection.m.data());

    for (int i = 0; i < ePlanet::Count; ++i)
    {
        const Planet& planet    = g_planets[i];
        const bool    isOrbiter = planet.distance > 0.f;
        const float   radius    = isOrbiter ? std::max(planet.radius * 1.6f, 0.55f) : planet.radius;
        const float   distance  = ToDisplayDistance(planet.distance);

        if (isOrbiter)
        {
            glDepthMask(GL_FALSE);
            DrawColorMesh(g_orbitMesh, viewProjection * RotateZ(-planet.revolutionTilt) * Scale(distance), kOrbitColor);
            glDepthMask(GL_TRUE);
        }

        const Mat4 model = RotateZ(-planet.revolutionTilt)
                         * RotateY(planet.revolutionSpeed * g_days)
                         * Translate({ distance, 0.f, 0.f })
                         * RotateZ(-planet.rotationTilt)
                         * RotateY(planet.rotationSpeed * g_days);

        DrawColorMesh(g_axisMesh, viewProjection * model * Scale(radius * 1.3f), kAxisColor);

        glUseProgram(g_planetProgram);
        glProgramUniformMatrix4fv(g_planetProgram, 0, 1, GL_FALSE, (model * Scale(radius)).m.data());
        glProgramUniform1i(g_planetProgram, 2, isOrbiter);
        glBindTextureUnit(0, planet.textureID);
        DrawMesh(g_sphereMesh);

        if (i == ePlanet::Saturn)
        {
            DrawColorMesh(g_ringMesh, viewProjection * model * Scale(radius), kRingColor);
        }
    }

    DrawHud();
}

// ===========================================
//  Update
// ===========================================

void UpdateTime()
{
    LARGE_INTEGER currentTime;
    ::QueryPerformanceCounter(&currentTime);
    g_deltaTime = static_cast<float>(currentTime.QuadPart - g_prevTime.QuadPart)
                / static_cast<float>(g_frequency.QuadPart);
    g_prevTime  = currentTime;

    g_timeSum += g_deltaTime;
    while (g_timeSum >= kDayUpdateInterval)
    {
        g_timeSum -= kDayUpdateInterval;
        g_days += GetDayStep();
    }

    g_frameHistory[static_cast<size_t>(g_frameHistoryNext)] = { g_deltaTime * 1000.f, g_useInstancing };
    g_frameHistoryNext                                      = (g_frameHistoryNext + 1) % kFrameHistorySize;

    ++g_frameCount;
    g_frameTimeSum += g_deltaTime;
    if (g_frameTimeSum >= 0.5f)
    {
        g_fps          = static_cast<float>(g_frameCount) / g_frameTimeSum;
        g_frameCount   = 0;
        g_frameTimeSum = 0.f;
    }
}

void ProcessKeyboard()
{
    const Vec3 forward = GetCameraForward();
    const Vec3 right   = GetCameraRight();
    const Vec3 up      = { 0.f, 1.f, 0.f };

    Vec3 direction = {};
    if (::GetAsyncKeyState('W') & 0x8000)
    {
        direction = direction + forward;
    }
    if (::GetAsyncKeyState('S') & 0x8000)
    {
        direction = direction - forward;
    }
    if (::GetAsyncKeyState('D') & 0x8000)
    {
        direction = direction + right;
    }
    if (::GetAsyncKeyState('A') & 0x8000)
    {
        direction = direction - right;
    }
    if (::GetAsyncKeyState('E') & 0x8000)
    {
        direction = direction + up;
    }
    if (::GetAsyncKeyState('Q') & 0x8000)
    {
        direction = direction - up;
    }

    if (Dot(direction, direction) > 0.f)
    {
        g_cameraPosition = g_cameraPosition + Normalize(direction) * (g_cameraMoveSpeed * g_deltaTime);
    }

    if (::GetAsyncKeyState('P') & 0x8000)
    {
        g_cameraMoveSpeed = std::min(50.f, g_cameraMoveSpeed + 10.f * g_deltaTime);
    }
    if (::GetAsyncKeyState('O') & 0x8000)
    {
        g_cameraMoveSpeed = std::max(0.f, g_cameraMoveSpeed - 10.f * g_deltaTime);
    }

    g_isTimeBoosted = (::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    if (::GetAsyncKeyState('L') & 0x8000)
    {
        g_dayStep = std::max(0.f, g_dayStep - 0.5f / kTimeBoostScale * g_deltaTime);
    }
    if (::GetAsyncKeyState('K') & 0x8000)
    {
        g_dayStep += 0.5f / kTimeBoostScale * g_deltaTime;
    }

    if (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
    }
}

void ProcessMouse(
    const LONG _deltaX,
    const LONG _deltaY)
{
    constexpr float kSensitivity = 0.1f;
    g_cameraYaw += static_cast<float>(_deltaX) * kSensitivity;
    g_cameraPitch -= static_cast<float>(_deltaY) * kSensitivity;
    g_cameraPitch = std::clamp(g_cameraPitch, -89.f, 89.f);
}

void ResetFpsWindow()
{
    g_frameCount   = 0;
    g_frameTimeSum = 0.f;
}

// 한 번 눌렀을 때만 반응해야 하는 키는 WM_KEYDOWN에서 처리합니다.
bool IsDemoRunning();

void OnKeyPressed(const WPARAM _key)
{
    if (_key == VK_ESCAPE)
    {
        ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
        return;
    }
    if (IsDemoRunning())
    {
        return;
    }

    switch (_key)
    {
        case 'I':
            g_useInstancing = !g_useInstancing;
            ResetFpsWindow();
            break;
        case 'H':
            g_showHud = !g_showHud;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
            g_asteroidCount = kAsteroidCountPresets[_key - '1'];
            ResetFpsWindow();
            break;
        default:
            break;
    }
}

// ===========================================
//  Demo recording (--demo <dir>)
// ===========================================
//  같은 카메라 경로와 시뮬레이션 시간을 인스턴싱 OFF/ON으로 한 번씩 재생하며
//  화면을 30fps 간격으로 JPEG로 압축해 구간별 .mjpeg 파일에 이어 붙이고, 구간별 프레임 시간을 기록합니다.
//  화면 읽기는 PBO로 비동기 처리하고 압축과 저장은 별도 스레드에서 하므로 두 구간에 같은 부담만 더해집니다.

struct DemoPhase
{
    const char* name;
    bool        instancing;
    float       duration;
    bool        record;
};

constexpr std::array kDemoPhases = {
    DemoPhase { "warmup", true, 2.f, false },
    DemoPhase { "individual", false, 8.f, true },
    DemoPhase { "instanced", true, 8.f, true },
};

constexpr float kDemoCaptureRate   = 30.f;
constexpr float kDemoSettleSeconds = 0.5f;   // 구간 시작 직후는 통계에서 제외합니다.
constexpr int   kDemoPboCount      = 4;

struct DemoFrame
{
    size_t                     phase  = 0;
    int                        slot   = 0;
    int                        width  = 0;
    int                        height = 0;
    std::vector<unsigned char> rgba;
};

struct DemoPendingRead
{
    GLuint pbo   = 0;
    GLsync fence = nullptr;
    size_t phase = 0;
    int    slot  = 0;
};

struct DemoState
{
    bool                  active     = false;
    std::filesystem::path outputDirectory;
    size_t                phase      = 0;
    float                 phaseTime  = 0.f;
    int                   lastSlot   = -1;
    std::vector<float>    frameTimes;   // 현재 구간의 통계용 프레임 시간(ms)
    std::ofstream         frameLog;
    std::ofstream         summary;
    bool                  firstSummary = true;

    std::array<GLuint, kDemoPboCount> pbos     = {};
    int                               nextPbo  = 0;
    std::deque<DemoPendingRead>       pending;

    std::thread             writer;
    std::mutex              mutex;
    std::condition_variable condition;
    std::deque<DemoFrame>   queue;
    bool                    stopWriter = false;
};

DemoState g_demo;

bool IsDemoRunning()
{
    return g_demo.active;
}

void DemoWriterLoop()
{
    std::array<std::ofstream, kDemoPhases.size()> videos;
    std::ofstream                                 index(g_demo.outputDirectory / "captured-frames.csv");
    index << "phase,slot\n";

    for (;;)
    {
        DemoFrame frame;
        {
            std::unique_lock lock(g_demo.mutex);
            g_demo.condition.wait(lock, [] { return g_demo.stopWriter || !g_demo.queue.empty(); });
            if (g_demo.queue.empty())
            {
                return;
            }
            frame = std::move(g_demo.queue.front());
            g_demo.queue.pop_front();
        }

        // 아래에서 위로 읽힌 RGBA를 위에서 아래 순서의 절반 해상도 RGB로 줄여 저장합니다.
        const int                  outWidth  = frame.width / 2;
        const int                  outHeight = frame.height / 2;
        std::vector<unsigned char> rgb(static_cast<size_t>(outWidth) * outHeight * 3);
        for (int y = 0; y < outHeight; ++y)
        {
            const int sourceY0 = frame.height - 1 - y * 2;
            const int sourceY1 = sourceY0 - 1;
            for (int x = 0; x < outWidth; ++x)
            {
                for (int c = 0; c < 3; ++c)
                {
                    const auto at = [&](const int _x, const int _y)
                    {
                        return static_cast<int>(frame.rgba[(static_cast<size_t>(_y) * frame.width + _x) * 4 + c]);
                    };
                    const int sum = at(x * 2, sourceY0) + at(x * 2 + 1, sourceY0) + at(x * 2, sourceY1) + at(x * 2 + 1, sourceY1);
                    rgb[(static_cast<size_t>(y) * outWidth + x) * 3 + c] = static_cast<unsigned char>((sum + 2) / 4);
                }
            }
        }
        std::ofstream& video = videos[frame.phase];
        if (!video.is_open())
        {
            video.open(g_demo.outputDirectory / std::format("{}.mjpeg", kDemoPhases[frame.phase].name), std::ios::binary);
        }
        stbi_write_jpg_to_func([](void* _context, void* _data, const int _size)
                               { static_cast<std::ofstream*>(_context)->write(static_cast<const char*>(_data), _size); },
                               &video, outWidth, outHeight, 3, rgb.data(), 92);
        index << kDemoPhases[frame.phase].name << ',' << frame.slot << '\n';
    }
}

void ApplyDemoCamera(const float _time)
{
    // 소행성대 바깥에서 태양 쪽을 바라보며 천천히 돌아 들어가는 경로입니다.
    const float angle  = -0.3f + 0.05f * _time;
    const float radius = 36.f - 0.5f * _time;
    const float height = 7.f - 0.25f * _time;
    g_cameraPosition   = { radius * std::sin(angle), height, radius * std::cos(angle) };
    g_cameraYaw        = -angle * 180.f / kPi;
    g_cameraPitch      = -std::atan2(height - 1.f, radius) * 180.f / kPi;
    g_days             = 1600.f + _time;   // 앱 기본값과 같은 1일/초
}

void FinishDemoRead(const DemoPendingRead& _read)
{
    glClientWaitSync(_read.fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glDeleteSync(_read.fence);

    DemoFrame frame = { .phase = _read.phase, .slot = _read.slot, .width = g_viewportWidth, .height = g_viewportHeight, .rgba = {} };
    frame.rgba.resize(static_cast<size_t>(frame.width) * frame.height * 4);
    glGetNamedBufferSubData(_read.pbo, 0, static_cast<GLsizeiptr>(frame.rgba.size()), frame.rgba.data());
    {
        std::lock_guard lock(g_demo.mutex);
        g_demo.queue.push_back(std::move(frame));
    }
    g_demo.condition.notify_one();
}

void FinishReadyDemoReads(const bool _wait)
{
    while (!g_demo.pending.empty())
    {
        const DemoPendingRead& read = g_demo.pending.front();
        if (!_wait && glClientWaitSync(read.fence, 0, 0) == GL_TIMEOUT_EXPIRED)
        {
            break;
        }
        FinishDemoRead(read);
        g_demo.pending.pop_front();
    }
}

bool StartDemo(const std::filesystem::path& _directory)
{
    std::error_code error;
    std::filesystem::remove_all(_directory, error);
    std::filesystem::create_directories(_directory);
    g_demo.outputDirectory = _directory;
    g_demo.frameLog.open(_directory / "frame-times.csv");
    g_demo.summary.open(_directory / "demo-summary.json");
    if (!g_demo.frameLog || !g_demo.summary)
    {
        std::cerr << "Could not open demo output files in " << _directory << '\n';
        return false;
    }
    g_demo.frameLog << "phase,time_s,frame_ms\n";

    const GLsizeiptr size = static_cast<GLsizeiptr>(g_viewportWidth) * g_viewportHeight * 4;
    glCreateBuffers(kDemoPboCount, g_demo.pbos.data());
    for (const GLuint pbo: g_demo.pbos)
    {
        glNamedBufferStorage(pbo, size, nullptr, GL_CLIENT_STORAGE_BIT);
    }

    const GLubyte* const renderer = glGetString(GL_RENDERER);
    g_demo.summary << "{\n  \"renderer\": \"" << (renderer ? reinterpret_cast<const char*>(renderer) : "") << "\",\n"
                   << "  \"viewport\": [" << g_viewportWidth << ", " << g_viewportHeight << "],\n"
                   << "  \"asteroids\": " << kMaxAsteroidCount << ",\n"
                   << "  \"low_poly_triangles\": " << g_lowPolyRockMesh.count / 3 << ",\n"
                   << "  \"capture_rate\": " << kDemoCaptureRate << ",\n"
                   << "  \"phases\": [";

    g_demo.writer   = std::thread(DemoWriterLoop);
    g_demo.active   = true;
    g_asteroidCount = kMaxAsteroidCount;
    g_showHud       = true;
    return true;
}

void EnterDemoPhase(const size_t _phase)
{
    g_demo.phase     = _phase;
    g_demo.phaseTime = 0.f;
    g_demo.lastSlot  = -1;
    g_demo.frameTimes.clear();
    g_useInstancing   = kDemoPhases[_phase].instancing;
    g_frameHistory    = {};
    ResetFpsWindow();
    std::cout << "DEMO phase " << kDemoPhases[_phase].name << std::endl;
}

void FinishDemoPhase()
{
    const DemoPhase& phase = kDemoPhases[g_demo.phase];
    std::vector<float> sorted = g_demo.frameTimes;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.;
    for (const float ms: sorted)
    {
        sum += ms;
    }
    const auto percentile = [&](const double _p)
    {
        return sorted.empty() ? 0.f : sorted[std::min(sorted.size() - 1, static_cast<size_t>(_p * static_cast<double>(sorted.size())))];
    };
    const double fps = sum > 0. ? static_cast<double>(sorted.size()) * 1000. / sum : 0.;

    g_demo.summary << (g_demo.firstSummary ? "\n" : ",\n")
                   << std::format("    {{ \"name\": \"{}\", \"instancing\": {}, \"frames\": {}, \"fps\": {:.2f}, \"mean_ms\": {:.3f}, \"p50_ms\": {:.3f}, \"p95_ms\": {:.3f}, \"draw_calls\": {} }}",
                                  phase.name, phase.instancing, sorted.size(), fps,
                                  sorted.empty() ? 0. : sum / static_cast<double>(sorted.size()), percentile(0.5), percentile(0.95),
                                  phase.instancing ? 1 : kMaxAsteroidCount);
    g_demo.firstSummary = false;
    std::cout << std::format("DEMO done {} fps={:.1f}", phase.name, fps) << std::endl;
}

void UpdateDemo()
{
    g_demo.phaseTime += g_deltaTime;
    if (g_demo.phaseTime >= kDemoPhases[g_demo.phase].duration)
    {
        FinishDemoPhase();
        if (g_demo.phase + 1 >= kDemoPhases.size())
        {
            ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
            g_demo.active = false;
            return;
        }
        EnterDemoPhase(g_demo.phase + 1);
    }
    else if (g_demo.phaseTime >= kDemoSettleSeconds)
    {
        g_demo.frameTimes.push_back(g_deltaTime * 1000.f);
        g_demo.frameLog << std::format("{},{:.4f},{:.4f}\n", kDemoPhases[g_demo.phase].name, g_demo.phaseTime, g_deltaTime * 1000.f);
    }

    ApplyDemoCamera(g_demo.phaseTime);
}

// Display() 후, 화면을 표시하기 전에 호출합니다.
void CaptureDemoFrame()
{
    FinishReadyDemoReads(false);
    if (!g_demo.active || !kDemoPhases[g_demo.phase].record)
    {
        return;
    }

    const int slot = static_cast<int>(g_demo.phaseTime * kDemoCaptureRate);
    if (slot <= g_demo.lastSlot)
    {
        return;
    }
    g_demo.lastSlot = slot;

    if (g_demo.pending.size() >= kDemoPboCount)
    {
        FinishDemoRead(g_demo.pending.front());
        g_demo.pending.pop_front();
    }

    DemoPendingRead read;
    read.pbo   = g_demo.pbos[static_cast<size_t>(g_demo.nextPbo)];
    read.phase = g_demo.phase;
    read.slot  = slot;
    g_demo.nextPbo = (g_demo.nextPbo + 1) % kDemoPboCount;

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, read.pbo);
    glReadPixels(0, 0, g_viewportWidth, g_viewportHeight, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    read.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_demo.pending.push_back(std::move(read));
}

void StopDemo()
{
    FinishReadyDemoReads(true);
    {
        std::lock_guard lock(g_demo.mutex);
        g_demo.stopWriter = true;
    }
    g_demo.condition.notify_one();
    if (g_demo.writer.joinable())
    {
        g_demo.writer.join();
    }
    if (g_demo.summary.is_open())
    {
        g_demo.summary << "\n  ]\n}\n";
        g_demo.summary.close();
    }
    glDeleteBuffers(kDemoPboCount, g_demo.pbos.data());
    g_demo.pbos = {};
}

void Update()
{
    UpdateTime();
    if (g_demo.active)
    {
        UpdateDemo();
    }
    else if (::GetForegroundWindow() == g_hWnd)
    {
        ProcessKeyboard();

        POINT center = { g_centerX, g_centerY };
        ::ClientToScreen(g_hWnd, &center);
        ::SetCursorPos(center.x, center.y);
    }
    else
    {
        g_isTimeBoosted = false;
    }

    if (g_frameCount == 0)   // FPS가 갱신된 직후에만 제목을 바꿉니다.
    {
        UpdateWindowTitle();
    }
}

// ===========================================
//  Resource
// ===========================================

GLuint LoadTexture(const std::string& _path)
{
    int            width    = 0;
    int            height   = 0;
    int            channels = 0;
    stbi_uc* const pixels   = stbi_load(_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        std::cerr << "Error loading texture: " << _path << ": " << stbi_failure_reason() << '\n';
        return 0;
    }

    GLuint     texture = 0;
    const auto levels  = static_cast<GLsizei>(std::bit_width(static_cast<unsigned>(std::max(width, height))));
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, levels, GL_RGBA8, width, height);
    glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateTextureMipmap(texture);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(pixels);
    return texture;
}

bool LoadTextures()
{
    stbi_set_flip_vertically_on_load(true);

    for (Planet& planet: g_planets)
    {
        planet.textureID = LoadTexture(std::string { "Resources/" } + planet.path);
        if (!planet.textureID)
        {
            return false;
        }
    }

    g_rockDiffuseTexture = LoadTexture("Resources/tex_low_diffuse_2048.jpg");
    g_rockNormalTexture  = LoadTexture("Resources/tex_low_normal_2048.png");
    return g_rockDiffuseTexture && g_rockNormalTexture;
}

bool CreateResources()
{
    InitPlanets();
    if (!LoadTextures())
    {
        return false;
    }

    g_planetProgram   = CreateProgram(kPlanetVertexShader, kPlanetFragmentShader);
    g_colorProgram    = CreateProgram(kColorVertexShader, kColorFragmentShader);
    g_starProgram     = CreateProgram(kStarVertexShader, kStarFragmentShader);
    g_glowProgram     = CreateProgram(kGlowVertexShader, kGlowFragmentShader);
    g_asteroidProgram = CreateProgram(kAsteroidVertexShader, kAsteroidFragmentShader);
    g_hudProgram      = CreateProgram(kHudVertexShader, kHudFragmentShader);
    if (!g_planetProgram || !g_colorProgram || !g_starProgram || !g_glowProgram || !g_asteroidProgram || !g_hudProgram)
    {
        return false;
    }

    if (!LoadObjMesh("Resources/Rock1.obj", g_detailedRockMesh))
    {
        return false;
    }
    g_lowPolyRockMesh = CreateLowPolyRockMesh();
    CreateAsteroidInstances(kMaxAsteroidCount);
    AttachAsteroidInstances(g_detailedRockMesh);
    AttachAsteroidInstances(g_lowPolyRockMesh);
    CreateHud();

    g_sphereMesh = CreateSphereMesh(100, 100);
    g_ringMesh   = CreateRingMesh(1.35f, 2.2f, 100);
    g_orbitMesh  = CreateCircleMesh(256);
    g_axisMesh   = CreateAxisMesh();
    g_starMesh   = CreateStarMesh(kStarCount);
    g_glowMesh   = CreateQuadMesh();
    return true;
}

void DestroyResources()
{
    for (Planet& planet: g_planets)
    {
        glDeleteTextures(1, &planet.textureID);
        planet.textureID = 0;
    }

    for (GLuint* const texture: { &g_rockDiffuseTexture, &g_rockNormalTexture })
    {
        glDeleteTextures(1, texture);
        *texture = 0;
    }

    for (Mesh* const mesh: { &g_sphereMesh, &g_ringMesh, &g_orbitMesh, &g_axisMesh, &g_starMesh, &g_glowMesh, &g_lowPolyRockMesh, &g_detailedRockMesh })
    {
        DestroyMesh(*mesh);
    }
    glDeleteBuffers(1, &g_asteroidInstanceBuffer);
    g_asteroidInstanceBuffer = 0;
    DestroyHud();

    for (GLuint* const program: { &g_planetProgram, &g_colorProgram, &g_starProgram, &g_glowProgram, &g_asteroidProgram, &g_hudProgram })
    {
        glDeleteProgram(*program);
        *program = 0;
    }
}

// ===========================================
//  Platform
// ===========================================

void GLAPIENTRY OnDebugMessage(
    GLenum,
    GLenum,
    GLuint,
    const GLenum _severity,
    GLsizei,
    const GLchar* const _message,
    const void*)
{
    if (_severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        std::cerr << "[OpenGL] " << _message << '\n';
    }
}

LRESULT CALLBACK WindowProc(
    const HWND   _hWnd,
    const UINT   _message,
    const WPARAM _wParam,
    const LPARAM _lParam)
{
    if (_hWnd != g_hWnd)
    {
        return ::DefWindowProcW(_hWnd, _message, _wParam, _lParam);
    }

    switch (_message)
    {
        case WM_SIZE:
            if (g_hGLRC)
            {
                Reshape(LOWORD(_lParam), HIWORD(_lParam));
            }
            return 0;

        case WM_INPUT:
        {
            RAWINPUT input = {};
            UINT     size  = sizeof(input);
            if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(_lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)
                && input.header.dwType == RIM_TYPEMOUSE
                && !(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
            {
                ProcessMouse(input.data.mouse.lLastX, input.data.mouse.lLastY);
            }
            break;
        }

        case WM_SETCURSOR:
            if (LOWORD(_lParam) == HTCLIENT)
            {
                ::SetCursor(nullptr);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (!(_lParam & (1 << 30)))   // 키를 누르고 있을 때 반복되는 메시지는 무시합니다.
            {
                OnKeyPressed(_wParam);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(_hWnd, _message, _wParam, _lParam);
}

bool LoadWglExtensions(const HINSTANCE _hInstance)
{
    const HWND hWnd = ::CreateWindowExW(0, kWindowClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, _hInstance, nullptr);
    if (!hWnd)
    {
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize                 = sizeof(pfd);
    pfd.nVersion              = 1;
    pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType            = PFD_TYPE_RGBA;
    pfd.cColorBits            = 32;
    pfd.cDepthBits            = 24;
    pfd.iLayerType            = PFD_MAIN_PLANE;

    const HDC hDC    = ::GetDC(hWnd);
    bool      loaded = false;
    if (::SetPixelFormat(hDC, ::ChoosePixelFormat(hDC, &pfd), &pfd))
    {
        const HGLRC hGLRC = ::wglCreateContext(hDC);
        if (hGLRC && ::wglMakeCurrent(hDC, hGLRC))
        {
            loaded = glewInit() == GLEW_OK
                  && WGLEW_ARB_pixel_format
                  && WGLEW_ARB_create_context
                  && WGLEW_ARB_create_context_profile;
            ::wglMakeCurrent(nullptr, nullptr);
        }
        if (hGLRC)
        {
            ::wglDeleteContext(hGLRC);
        }
    }

    ::ReleaseDC(hWnd, hDC);
    ::DestroyWindow(hWnd);
    return loaded;
}

int ChoosePixelFormatARB(
    const HDC _hDC,
    const int _samples)
{
    const std::array attributes = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE, WGL_SUPPORT_OPENGL_ARB, GL_TRUE, WGL_DOUBLE_BUFFER_ARB, GL_TRUE, WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB, WGL_COLOR_BITS_ARB, 32, WGL_DEPTH_BITS_ARB, 24, WGL_SAMPLE_BUFFERS_ARB, _samples > 0 ? GL_TRUE : GL_FALSE, WGL_SAMPLES_ARB, _samples, 0
    };

    int  format = 0;
    UINT count  = 0;
    if (!wglChoosePixelFormatARB(_hDC, attributes.data(), nullptr, 1, &format, &count) || count == 0)
    {
        return 0;
    }
    return format;
}

bool CreateMainWindow(const HINSTANCE _hInstance)
{
    WNDCLASSEXW windowClass   = {};
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = WindowProc;
    windowClass.hInstance     = _hInstance;
    windowClass.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;
    if (!::RegisterClassExW(&windowClass))
    {
        std::cerr << "Failed to register window class.\n";
        return false;
    }

    if (!LoadWglExtensions(_hInstance))
    {
        std::cerr << "Failed to load WGL extensions.\n";
        return false;
    }

    RECT rect = { 0, 0, kWindowWidth, kWindowHeight };
    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = ::CreateWindowExW(0, kWindowClassName, L"Solar System", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, _hInstance, nullptr);
    if (!g_hWnd)
    {
        std::cerr << "Failed to create window.\n";
        return false;
    }
    g_hDC = ::GetDC(g_hWnd);

    int format = ChoosePixelFormatARB(g_hDC, kMultisampleCount);
    if (format == 0)
    {
        format = ChoosePixelFormatARB(g_hDC, 0);
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    if (format == 0 || !::DescribePixelFormat(g_hDC, format, sizeof(pfd), &pfd) || !::SetPixelFormat(g_hDC, format, &pfd))
    {
        std::cerr << "Failed to set pixel format.\n";
        return false;
    }

    const std::array contextAttributes = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4, WGL_CONTEXT_MINOR_VERSION_ARB, 6, WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, WGL_CONTEXT_FLAGS_ARB, kContextFlags, 0
    };

    g_hGLRC = wglCreateContextAttribsARB(g_hDC, nullptr, contextAttributes.data());
    if (!g_hGLRC || !::wglMakeCurrent(g_hDC, g_hGLRC))
    {
        std::cerr << "Failed to create OpenGL 4.6 core context.\n";
        return false;
    }

    glewExperimental = GL_TRUE;
    if (const GLenum result = glewInit(); result != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(result) << '\n';
        return false;
    }
    glGetError();

    if (!GLEW_VERSION_4_6)
    {
        std::cerr << "OpenGL 4.6 is required.\n";
        return false;
    }

    // 드로우 방식에 따른 처리량 차이가 보이도록 수직 동기화를 끕니다.
    if (WGLEW_EXT_swap_control)
    {
        wglSwapIntervalEXT(0);
    }

    if constexpr (kContextFlags & WGL_CONTEXT_DEBUG_BIT_ARB)   // NOLINT
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OnDebugMessage, nullptr);
    }

    const RAWINPUTDEVICE mouse = { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, 0, g_hWnd };
    if (!::RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
    {
        std::cerr << "Failed to register raw mouse input.\n";
        return false;
    }

    ::ShowWindow(g_hWnd, SW_SHOW);

    RECT clientRect = {};
    ::GetClientRect(g_hWnd, &clientRect);
    Reshape(clientRect.right, clientRect.bottom);
    return true;
}

void DestroyMainWindow()
{
    if (g_hGLRC)
    {
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(g_hGLRC);
        g_hGLRC = nullptr;
    }
    if (g_hDC)
    {
        ::ReleaseDC(g_hWnd, g_hDC);
        g_hDC = nullptr;
    }
    if (g_hWnd)
    {
        ::DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }
}

int main(
    const int         _argc,
    const char* const _argv[])
{
    std::filesystem::path demoDirectory;
    if (_argc == 3 && std::string_view(_argv[1]) == "--demo")
    {
        demoDirectory = _argv[2];
    }

    if (!CreateMainWindow(::GetModuleHandleW(nullptr)))
    {
        DestroyMainWindow();
        return EXIT_FAILURE;
    }

    if (!CreateResources())
    {
        DestroyResources();
        DestroyMainWindow();
        return EXIT_FAILURE;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.004f, 0.007f, 0.016f, 1.f);
    glPointSize(1.5f);

    if (!demoDirectory.empty())
    {
        if (!StartDemo(demoDirectory))
        {
            DestroyResources();
            DestroyMainWindow();
            return EXIT_FAILURE;
        }
        EnterDemoPhase(0);
    }

    ::QueryPerformanceCounter(&g_prevTime);
    ::QueryPerformanceFrequency(&g_frequency);

    bool isRunning = true;
    while (isRunning)
    {
        MSG message = {};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                isRunning = false;
                break;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }

        if (isRunning)
        {
            Update();
            Display();
            CaptureDemoFrame();
            ::SwapBuffers(g_hDC);
        }
    }

    if (!demoDirectory.empty())
    {
        StopDemo();
        std::cout << "DEMO_COMPLETE" << std::endl;
    }
    DestroyResources();
    DestroyMainWindow();
    return EXIT_SUCCESS;
}
