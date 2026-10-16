#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <GL/freeglut.h>
#include <SOIL/SOIL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

constexpr float kDayUpdateInterval = 1.f / 60.f;
constexpr float kPi                = 3.14159265358979f;

// 지구 Planet
constexpr float kEarthRadius   = 0.1f;
constexpr float kEarthDistance = 8.f;

struct Planet
{
    const char* path;              // 텍스처 경로
    GLuint      texture;           // 텍스처
    float       radius;            // 행성 반지름
    float       distance;          // 태양으로 부터의 거리, 즉 공전 반지름
    float       revolutionSpeed;   // 공전속도 (지구 = 1/365)
    float       revolutionTilt;    // 공전축
    float       rotationSpeed;     // 자전속도
    float       rotationTilt;      // 자전축
};

/* 정보 출처: https://astro.kasi.re.kr/learning/pageView/5116 */
/* 텍스터 경로, 텍스처,     행성 반지름,           거리,             공전속도,      공전축,  자전속도,        자전축*/
// 원래 태양의 사이즈는 109이지만, 너무 큰 관계로 30으로 줄임
std::array g_planets = {
    Planet {     "Sun.bmp", 0,  30.f * kEarthRadius,                    0.f,               0.f,  0.f,      360.f / 25.38f,  7.25f },
    Planet { "Mercury.bmp", 0,  0.4f * kEarthRadius,  0.4f * kEarthDistance,    360.f / 87.97f,  7.f,    360.f / 58.6462f,  0.01f },
    Planet {   "Venus.bmp", 0,  0.9f * kEarthRadius,  0.7f * kEarthDistance,    360.f / 224.7f,  3.f,  -360.f / 243.0185f,  2.64f },
    Planet {   "Earth.bmp", 0,   1.f * kEarthRadius,   1.f * kEarthDistance,   360.f / 365.26f,  0.f, 360.f / 0.99726963f, 23.44f },
    Planet {    "Mars.bmp", 0,  0.5f * kEarthRadius,  1.5f * kEarthDistance,    360.f / 686.9f, 1.8f, 360.f / 1.02595676f, 25.19f },
    Planet { "Jupiter.bmp", 0, 11.2f * kEarthRadius,  5.2f * kEarthDistance,  360.f / 4333.29f, 1.3f,       360.f / 12.6f,  3.12f },
    Planet {  "Saturn.bmp", 0,  9.4f * kEarthRadius,  9.5f * kEarthDistance,  360.f / 10756.2f, 2.4f,       360.f / 9.87f, 26.73f },
    Planet {  "Uranos.bmp", 0,   4.f * kEarthRadius, 19.2f * kEarthDistance, 360.f / 30707.49f, 0.7f,       360.f / 2.59f, 82.23f },
    Planet { "Neptune.bmp", 0,  3.9f * kEarthRadius,  30.f * kEarthDistance, 360.f / 60223.35f, 1.7f,       360.f / 2.68f, 23.33f }
};

struct Star
{
    float x;
    float y;
    float z;
    float brightness;
};

std::array<Star, 900> g_stars = {};

// 전역 변수
// 카메라 위치 및 회전 변수
float g_cameraX         = 0.f;
float g_cameraY         = -80.f;
float g_cameraZ         = 72.f;
float g_cameraYaw       = 0.f;
float g_cameraPitch     = 48.01f;
float g_cameraMoveSpeed = 5.f;

LARGE_INTEGER g_prevTime  = {};    // 이전시간
LARGE_INTEGER g_frequency = {};    // 진동수
float         g_deltaTime = 0.f;   // 단위시간

int         g_centerX  = 640;
int         g_centerY  = 360;
float       g_days     = 1600.f;
float       g_timeSum  = 0.f;
float       g_dayStep  = 0.05f;
GLUquadric* g_pQuadric = nullptr;

void UpdateWindowTitle()
{
    const std::string title =
        "Camera move speed: " + std::to_string(g_cameraMoveSpeed)
        + " | Time flow: " + std::to_string(g_dayStep / kDayUpdateInterval) + " day/sec"
        + " | Cur Time: " + std::to_string(static_cast<int>(g_days)) + " day";

    glutSetWindowTitle(title.c_str());
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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40.0, static_cast<double>(_width) / _height, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    g_centerX = _width / 2;
    g_centerY = _height / 2;
}

void ApplyCamera()
{
    const float yaw   = g_cameraYaw * kPi / 180.f;
    const float pitch = g_cameraPitch * kPi / 180.f;
    const float dirX  = std::sin(yaw) * std::cos(pitch);
    const float dirY  = std::sin(pitch);
    const float dirZ  = -std::cos(yaw) * std::cos(pitch);

    gluLookAt(g_cameraX, g_cameraY, g_cameraZ, g_cameraX + dirX, g_cameraY + dirY, g_cameraZ + dirZ, 0.0, 1.0, 0.0);
}

void Display()
{
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    ApplyCamera();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for (const Star& star: g_stars)
    {
        glColor3f(star.brightness * 0.8f, star.brightness * 0.88f, star.brightness);
        glVertex3f(g_cameraX + star.x, g_cameraY + star.y, g_cameraZ + star.z);
    }
    glEnd();

    const float yaw        = g_cameraYaw * kPi / 180.f;
    const float pitch      = g_cameraPitch * kPi / 180.f;
    const float glowRadius = g_planets.front().radius * 2.5f;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.f, 0.45f, 0.08f, 0.65f);
    glVertex3f(0.f, 0.f, 0.f);
    glColor4f(1.f, 0.2f, 0.02f, 0.f);
    for (int i = 0; i <= 64; ++i)
    {
        const float angle = 2.f * kPi * static_cast<float>(i) / 64.f;
        const float x     = std::cos(angle) * glowRadius;
        const float y     = std::sin(angle) * glowRadius;
        glVertex3f(x * std::cos(yaw) - y * std::sin(yaw) * std::sin(pitch),
                   y * std::cos(pitch),
                   x * std::sin(yaw) + y * std::cos(yaw) * std::sin(pitch));
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    const std::array<GLfloat, 4> lightPosition = { 0.f, 0.f, 0.f, 1.f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition.data());

    for (const Planet& planet: g_planets)
    {
        const float radius   = planet.distance > 0.f ? std::max(planet.radius * 1.6f, 0.55f) : planet.radius;
        const float distance = planet.distance > 0.f ? 5.f + std::log2(1.f + planet.distance) * 4.f : 0.f;

        // 공전 궤도
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        if (planet.distance > 0.f)
        {
            glDepthMask(GL_FALSE);
            glPushMatrix();
            glColor4f(0.42f, 0.58f, 0.72f, 0.45f);
            glRotatef(planet.revolutionTilt, 0.f, 1.f, 0.f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 256; ++i)
            {
                const float angle = 2.f * kPi * static_cast<float>(i) / 256.f;
                glVertex3f(distance * std::cos(angle), distance * std::sin(angle), 0.f);
            }
            glEnd();
            glPopMatrix();
            glDepthMask(GL_TRUE);
        }

        // 행성 그리기
        glBindTexture(GL_TEXTURE_2D, planet.texture);
        glPushMatrix();

        // 공전축
        glRotatef(planet.revolutionTilt, 0.f, 1.f, 0.f);

        // 공전
        glRotatef(planet.revolutionSpeed * g_days, 0.f, 0.f, 1.f);

        // 공전 궤도에 배치
        glTranslatef(distance, 0.f, 0.f);

        // 자전축
        glRotatef(planet.rotationTilt, 0.f, 1.f, 0.f);

        // 자전
        glRotatef(planet.rotationSpeed * g_days, 0.f, 0.f, 1.f);

        // 자전축
        glColor4f(0.5f, 0.65f, 0.8f, 0.3f);
        glBegin(GL_LINES);
        glVertex3f(0.f, 0.f, radius * 1.3f);
        glVertex3f(0.f, 0.f, -radius * 1.3f);
        glEnd();

        // 행성
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glEnable(GL_TEXTURE_2D);
        if (planet.distance > 0.f)
        {
            glEnable(GL_LIGHTING);
        }
        gluSphere(g_pQuadric, radius, 100, 100);
        if (&planet == &g_planets[6])
        {
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.7f, 0.62f, 0.46f, 0.7f);
            gluDisk(g_pQuadric, radius * 1.35f, radius * 2.2f, 100, 1);
        }
        glPopMatrix();
    }

    glutSwapBuffers();
}

void UpdateTime()
{
    LARGE_INTEGER currentTime;
    ::QueryPerformanceCounter(&currentTime);
    g_deltaTime = static_cast<float>(currentTime.QuadPart - g_prevTime.QuadPart)
                / static_cast<float>(g_frequency.QuadPart);
    g_prevTime  = currentTime;

    /* 초당 대력 60번 연산 */
    g_timeSum += g_deltaTime;
    while (g_timeSum >= kDayUpdateInterval)
    {
        g_timeSum -= kDayUpdateInterval;
        g_days += g_dayStep;
    }
}

void ProcessKeyboard()
{
    const float moveSpeed = g_cameraMoveSpeed * g_deltaTime;   // 이동 속도
    const float yaw       = g_cameraYaw * kPi / 180.f;

    // 이동
    if (::GetAsyncKeyState('A') & 0x8000)
    {
        g_cameraX -= moveSpeed * std::cos(yaw);
        g_cameraZ -= moveSpeed * std::sin(yaw);
    }
    if (::GetAsyncKeyState('D') & 0x8000)
    {
        g_cameraX += moveSpeed * std::cos(yaw);
        g_cameraZ += moveSpeed * std::sin(yaw);
    }
    if (::GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        g_cameraY += moveSpeed;
    }
    if (::GetAsyncKeyState(VK_LSHIFT) & 0x8000)
    {
        g_cameraY -= moveSpeed;
    }

    // 이동
    if (::GetAsyncKeyState('W') & 0x8000)
    {
        g_cameraX += moveSpeed * std::sin(yaw);
        g_cameraZ -= moveSpeed * std::cos(yaw);
    }
    if (::GetAsyncKeyState('S') & 0x8000)
    {
        g_cameraX -= moveSpeed * std::sin(yaw);
        g_cameraZ += moveSpeed * std::cos(yaw);
    }

    // 속도 조절
    if (::GetAsyncKeyState('P') & 0x8000)
    {
        g_cameraMoveSpeed = std::min(50.f, g_cameraMoveSpeed + 10.f * g_deltaTime);
    }
    if (::GetAsyncKeyState('O') & 0x8000)
    {
        g_cameraMoveSpeed = std::max(0.f, g_cameraMoveSpeed - 10.f * g_deltaTime);
    }

    // 날짜가 지나는 정도 조절
    if (::GetAsyncKeyState('L') & 0x8000)
    {
        g_dayStep = std::max(0.f, g_dayStep - 0.5f * g_deltaTime);
    }
    if (::GetAsyncKeyState('K') & 0x8000)
    {
        g_dayStep += 0.5f * g_deltaTime;
    }

    // 프로그램 종료
    if (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        glutLeaveMainLoop();
    }
}

void ProcessMouse(
    const int _x,
    const int _y)
{
    constexpr float kSensitivity = 0.1f;
    g_cameraYaw += static_cast<float>(_x - g_centerX) * kSensitivity;
    g_cameraPitch += static_cast<float>(g_centerY - _y) * kSensitivity;

    // 제한 각도
    g_cameraPitch = std::clamp(g_cameraPitch, -89.f, 89.f);
}

void Update()
{
    UpdateTime();
    if (::GetActiveWindow() == ::GetForegroundWindow())
    {
        ProcessKeyboard();
        glutWarpPointer(g_centerX, g_centerY);
    }
    glutPostRedisplay();
    UpdateWindowTitle();
}

bool LoadTextures()
{
    for (Planet& planet: g_planets)
    {
        // 리소스 상대경로
        const std::string path = std::string { "Resources/" } + planet.path;

        // 텍스처 로딩
        planet.texture = SOIL_load_OGL_texture(
            path.c_str(),
            SOIL_LOAD_AUTO,
            SOIL_CREATE_NEW_ID,
            SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);

        // 텍스처 로딩 오류 처리
        if (planet.texture == 0)
        {
            std::cerr << "Error loading texture: " << path << ": " << SOIL_last_result() << '\n';
            return false;
        }

        // 텍스처 매핑 설정
        glBindTexture(GL_TEXTURE_2D, planet.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    return true;
}

int main(
    int   _argc,
    char* _argv[])
{
    glutInit(&_argc, _argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Solar System");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutSetCursor(GLUT_CURSOR_NONE);

    if (!LoadTextures())
    {
        glutDestroyWindow(glutGetWindow());
        return EXIT_FAILURE;
    }

    g_pQuadric = gluNewQuadric();
    if (!g_pQuadric)
    {
        std::cerr << "Failed to create GLU quadric.\n";
        glutDestroyWindow(glutGetWindow());
        return EXIT_FAILURE;
    }
    gluQuadricTexture(g_pQuadric, GL_TRUE);

    std::mt19937                          random { 7 };
    std::uniform_real_distribution<float> position { -500.f, 500.f };
    std::uniform_real_distribution<float> brightness { 0.2f, 0.75f };
    for (Star& star: g_stars)
    {
        star = { position(random), position(random), position(random), brightness(random) };
    }

    ::QueryPerformanceCounter(&g_prevTime);
    ::QueryPerformanceFrequency(&g_frequency);

    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutIdleFunc(Update);
    glutPassiveMotionFunc(ProcessMouse);

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.004f, 0.007f, 0.016f, 1.f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LIGHT0);
    const std::array<GLfloat, 4> ambient  = { 0.12f, 0.14f, 0.2f, 1.f };
    const std::array<GLfloat, 4> sunlight = { 1.f, 0.94f, 0.84f, 1.f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient.data());
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunlight.data());

    glutMainLoop();
    gluDeleteQuadric(g_pQuadric);
    return EXIT_SUCCESS;
}
