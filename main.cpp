#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/GLU.h>
#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma warning(pop)
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

constexpr float kDayUpdateInterval = 1.f / 60.f;
constexpr float kPi                = 3.14159265358979f;

// 창
constexpr int     kWindowWidth      = 1280;
constexpr int     kWindowHeight     = 720;
constexpr int     kMultisampleCount = 4;
constexpr wchar_t kWindowClassName[] = L"SolarSystemWindow";

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

// Win32 창 및 OpenGL 컨텍스트
HWND  g_hWnd  = nullptr;
HDC   g_hDC   = nullptr;
HGLRC g_hGLRC = nullptr;

void UpdateWindowTitle()
{
    const std::string title =
        "Camera move speed: " + std::to_string(g_cameraMoveSpeed)
        + " | Time flow: " + std::to_string(g_dayStep / kDayUpdateInterval) + " day/sec"
        + " | Cur Time: " + std::to_string(static_cast<int>(g_days)) + " day";

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

    ::SwapBuffers(g_hDC);
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
        ::PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
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

        // 마우스 커서를 창 중앙으로 고정
        POINT center = { g_centerX, g_centerY };
        ::ClientToScreen(g_hWnd, &center);
        ::SetCursorPos(center.x, center.y);
    }
    UpdateWindowTitle();
}

bool LoadTextures()
{
    // OpenGL 텍스처 좌표계에 맞게 상하 반전
    stbi_set_flip_vertically_on_load(true);

    for (Planet& planet: g_planets)
    {
        // 리소스 상대경로
        const std::string path = std::string { "Resources/" } + planet.path;

        // 이미지 로딩 (RGBA 4채널 고정)
        int            width    = 0;
        int            height   = 0;
        int            channels = 0;
        stbi_uc* const pixels   = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        // 이미지 로딩 오류 처리
        if (!pixels)
        {
            std::cerr << "Error loading texture: " << path << ": " << stbi_failure_reason() << '\n';
            return false;
        }

        // 텍스처 생성 및 업로드
        glGenTextures(1, &planet.texture);
        glBindTexture(GL_TEXTURE_2D, planet.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);

        // 밉맵 생성 (OpenGL 3.0, GLEW로 로드)
        glGenerateMipmap(GL_TEXTURE_2D);

        // 텍스처 매핑 설정
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    return true;
}

LRESULT CALLBACK WindowProc(
    const HWND   _hWnd,
    const UINT   _message,
    const WPARAM _wParam,
    const LPARAM _lParam)
{
    // 픽셀 포맷 탐색용 임시 창은 기본 처리
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

    case WM_MOUSEMOVE:
        ProcessMouse(GET_X_LPARAM(_lParam), GET_Y_LPARAM(_lParam));
        return 0;

    case WM_SETCURSOR:
        // 클라이언트 영역에서 커서 숨김
        if (LOWORD(_lParam) == HTCLIENT)
        {
            ::SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(g_hGLRC);
        ::ReleaseDC(_hWnd, g_hDC);
        g_hGLRC = nullptr;
        g_hDC   = nullptr;
        g_hWnd  = nullptr;
        ::PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return ::DefWindowProcW(_hWnd, _message, _wParam, _lParam);
}

PIXELFORMATDESCRIPTOR MakePixelFormatDescriptor()
{
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize                 = sizeof(pfd);
    pfd.nVersion              = 1;
    pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType            = PFD_TYPE_RGBA;
    pfd.cColorBits            = 32;
    pfd.cDepthBits            = 24;
    pfd.iLayerType            = PFD_MAIN_PLANE;
    return pfd;
}

// 임시 창과 컨텍스트로 GLEW(WGL 확장)를 초기화해 멀티샘플 픽셀 포맷을 찾는다. 실패 시 0
int FindMultisamplePixelFormat(const HINSTANCE _hInstance)
{
    const HWND hWnd = ::CreateWindowExW(0, kWindowClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, _hInstance, nullptr);
    if (!hWnd)
    {
        return 0;
    }

    const HDC                   hDC    = ::GetDC(hWnd);
    const PIXELFORMATDESCRIPTOR pfd    = MakePixelFormatDescriptor();
    int                         format = 0;
    if (::SetPixelFormat(hDC, ::ChoosePixelFormat(hDC, &pfd), &pfd))
    {
        const HGLRC hGLRC = ::wglCreateContext(hDC);
        if (hGLRC && ::wglMakeCurrent(hDC, hGLRC))
        {
            if (glewInit() == GLEW_OK && WGLEW_ARB_pixel_format && WGLEW_ARB_multisample)
            {
                const std::array attributes = {
                    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                    WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
                    WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
                    WGL_COLOR_BITS_ARB,     32,
                    WGL_DEPTH_BITS_ARB,     24,
                    WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
                    WGL_SAMPLES_ARB,        kMultisampleCount,
                    0
                };

                UINT count = 0;
                if (!wglChoosePixelFormatARB(hDC, attributes.data(), nullptr, 1, &format, &count) || count == 0)
                {
                    format = 0;
                }
            }
            ::wglMakeCurrent(nullptr, nullptr);
        }
        if (hGLRC)
        {
            ::wglDeleteContext(hGLRC);
        }
    }

    ::ReleaseDC(hWnd, hDC);
    ::DestroyWindow(hWnd);
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

    // 멀티샘플 포맷을 못 찾으면 기본 포맷 사용
    const int multisampleFormat = FindMultisamplePixelFormat(_hInstance);

    // 클라이언트 영역이 1280x720이 되도록 창 크기 보정
    RECT rect = { 0, 0, kWindowWidth, kWindowHeight };
    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = ::CreateWindowExW(0, kWindowClassName, L"Solar System", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, _hInstance, nullptr);
    if (!g_hWnd)
    {
        std::cerr << "Failed to create window.\n";
        return false;
    }
    g_hDC = ::GetDC(g_hWnd);

    // 픽셀 포맷 설정
    PIXELFORMATDESCRIPTOR pfd    = MakePixelFormatDescriptor();
    const int             format = multisampleFormat != 0 ? multisampleFormat : ::ChoosePixelFormat(g_hDC, &pfd);
    ::DescribePixelFormat(g_hDC, format, sizeof(pfd), &pfd);
    if (format == 0 || !::SetPixelFormat(g_hDC, format, &pfd))
    {
        std::cerr << "Failed to set pixel format.\n";
        return false;
    }

    // OpenGL 컨텍스트 생성
    const HGLRC hGLRC = ::wglCreateContext(g_hDC);
    if (!hGLRC || !::wglMakeCurrent(g_hDC, hGLRC))
    {
        std::cerr << "Failed to create OpenGL context.\n";
        return false;
    }
    g_hGLRC = hGLRC;

    // GLEW 초기화
    if (const GLenum result = glewInit(); result != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(result) << '\n';
        return false;
    }
    if (!glGenerateMipmap)
    {
        std::cerr << "OpenGL 3.0 or later is required.\n";
        return false;
    }

    ::ShowWindow(g_hWnd, SW_SHOW);

    RECT clientRect = {};
    ::GetClientRect(g_hWnd, &clientRect);
    Reshape(clientRect.right, clientRect.bottom);
    return true;
}

int main()
{
    if (!CreateMainWindow(::GetModuleHandleW(nullptr)))
    {
        ::DestroyWindow(g_hWnd);
        return EXIT_FAILURE;
    }

    if (!LoadTextures())
    {
        ::DestroyWindow(g_hWnd);
        return EXIT_FAILURE;
    }

    g_pQuadric = gluNewQuadric();
    if (!g_pQuadric)
    {
        std::cerr << "Failed to create GLU quadric.\n";
        ::DestroyWindow(g_hWnd);
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

    // 메시지 루프: 대기 메시지를 처리한 뒤 매 프레임 갱신 및 렌더링
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
        }
    }

    gluDeleteQuadric(g_pQuadric);
    return EXIT_SUCCESS;
}
