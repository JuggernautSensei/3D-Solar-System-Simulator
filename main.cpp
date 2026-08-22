#include <GL/glut.h>
#include <GL/glu.h>
#include <SOIL.h>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cassert>
#include <locale>
#include <codecvt>
#include <string>
#define DAY_UPDATE_TIME_PER_SEC 60
using namespace std;
namespace fs = std::filesystem;

// 리소스
#pragma region

//리소스 경로
wstring resPath = L"";

//태양계 열거
enum SOLAR_SYSTEM
{
	SUN,
	MERCURY,
	VENUS,
	EARTH,
	MARS,
	JUPITER,
	SATURN,
	URANUS,
	NEPTUNE,

	SOLAR_SYSTEM_COUNT
};

// 지구 Planet
const float earthRadius = 0.1;
const float earthDist = 8;
const float earthOrbitSpd = 1;

struct Planet
{
	wstring path;            //텍스처 경로
	GLuint  texture;         //텍스처

	float planetRadius;      //행성 반지름

	float planetDist;        //태양으로 부터의 거리, 즉 공전 반지름
	float rotSpd;            //공전속도 (지구 = 1/365)
	float rotRot;            //공전축

	float orbitSpd;          //자전속도
	float orbitRot;          //자전축
};

/* 정보 출처: https://astro.kasi.re.kr/learning/pageView/5116 */
Planet planets[SOLAR_SYSTEM_COUNT] =
{
	/* 텍스터 경로, 텍스처,     행성 반지름,           거리,             공전속도,      공전축,  자전속도,        자전축*/
	{ L"Sun.bmp",     0, 30 * earthRadius  ,0                , 0            ,        0   ,  360 / 25.38f     ,7.25}  , //원래 태양의 사이즈는 109이지만, 너무 큰 관계로 30으로 줄임
	{ L"Mercury.bmp", 0, 0.4 * earthRadius  ,0.4 * earthDist  ,  360 / 87.97f,        7   ,  360 / 58.6462f ,0.01}  ,
	{ L"Venus.bmp",   0, 0.9 * earthRadius  ,0.7 * earthDist  ,  360 / 224.7f ,       3   ,  -360 / 243.0185f ,2.64}  ,
	{ L"Earth.bmp",   0, 1 * earthRadius    ,1 * earthDist    ,  360 / 365.26,        0   ,  360 / 0.99726963  ,23.44} ,
	{ L"Mars.bmp",    0, 0.5 * earthRadius  ,1.5 * earthDist  ,  360 / 686.9f ,       1.8 ,  360 / 1.02595676  ,25.19} ,
	{ L"Jupiter.bmp", 0, 11.2 * earthRadius ,5.2 * earthDist  ,  360 / 4333.29f ,        1.3 ,  360 / 12.6 ,3.12}  ,
	{ L"Saturn.bmp",  0, 9.4 * earthRadius  ,9.5 * earthDist  ,  360 / 10756.2f ,        2.4 ,  360 / 9.87  ,26.73} ,
	{ L"Uranos.bmp",  0, 4 * earthRadius    ,19.2 * earthDist ,  360 / 30707.49f   ,        0.7 ,  360 / 2.59 ,82.23} ,
	{ L"Neptune.bmp", 0, 3.9 * earthRadius  ,30 * earthDist   ,  360 / 60223.35f   ,        1.7 ,  360 / 2.68  ,23.33}
};

#pragma endregion

// 전역 변수
#pragma region 전역변수

GLfloat xrot = 0.9f, yrot = 0.8f, zrot = 1.0f;
constexpr float M_PI = 3.14159265358979f;

// 카메라 위치 및 회전 변수
GLfloat cameraX = 0.0f, cameraY = 0.0f, cameraZ = 5.0f;
GLfloat cameraYaw = 0.0f, cameraPitch = 0.0f;

static LARGE_INTEGER	prevTime;        //이전시간
static LARGE_INTEGER	freq;            //진동수
static float			deltaTime;       //단위시간

int		windowWidth = 1280;
int		windowHeight = 720;
int		centerX, centerY;
int		lastMouseX, lastMouseY;
bool	firstMouse = true;
bool	destroyWindow = false;
float	cameraMoveSpeed = 5;

float	days = 0;

float	timeSum = 0.f;
float	daySum = 0.05f;

#pragma endregion

// 함수 선언
void MyReshape(int w, int h);
void MyDisplay();
int LoadGLTextures(SOLAR_SYSTEM idx);
void ProgramInit(int* argc, char* argv[]);
void MyTimerUpdate();
void MyUpdate();
void InfoRender();
void MyKeyboardEvent();
void MyMouseEvent(int x, int y);
void UpdateCamera();
void CenterMouse();
void ResourceInit();
void ResourceLoader();

void InfoRender()
{
	string buffer =
		"Camera move speed: " + to_string(cameraMoveSpeed) +
		"\tTime flow per time: " + to_string(daySum * DAY_UPDATE_TIME_PER_SEC) + " day/sec" +
		"\tCur Time: " + to_string((int)days) + " day";

	glutSetWindowTitle(buffer.c_str());
}

void MyReshape(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(40.0, (GLfloat)w / (GLfloat)h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	windowWidth = w;
	windowHeight = h;
	centerX = windowWidth / 2;
	centerY = windowHeight / 2;
}
void MyDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	UpdateCamera();

	for (int i = 0; i < SOLAR_SYSTEM_COUNT; ++i)
	{
		// 공전 궤도
		GLUquadric* orbitQuadric = gluNewQuadric();
		glPushMatrix();
		glColor3f(1.0f, 1.0f, 1.0f); // White
		glRotatef(planets[i].rotRot, 0, 1, 0);
		gluQuadricDrawStyle(orbitQuadric, GLU_FILL);
		gluDisk(orbitQuadric, planets[i].planetDist - 0.005f, planets[i].planetDist + 0.005f, 100, 10);
		glPopMatrix();
		gluDeleteQuadric(orbitQuadric);

		// 행성 그리기
		glBindTexture(GL_TEXTURE_2D, planets[i].texture);
		GLUquadric* quadric = gluNewQuadric();
		gluQuadricTexture(quadric, GL_TRUE);
		glPushMatrix();

		// 공전축
		glRotatef(planets[i].rotRot, 0, 1, 0);

		// 공전
		glRotatef(planets[i].rotSpd * days, 0, 0, 1);

		// 공전 궤도에 배치
		glTranslatef(planets[i].planetDist, 0.0f, 0.0f);

		// 자전축
		glRotatef(planets[i].orbitRot, 0, 1, 0);

		// 자전
		glRotatef(planets[i].orbitSpd * days, 0, 0, 1);

		// 자전축
		glColor3f(1.0f, 1.0f, 1.0f);
		glPushMatrix();
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.0f, planets[i].planetRadius + planets[i].planetRadius * 0.3);
		glVertex3f(0.0f, 0.0f, -planets[i].planetRadius - planets[i].planetRadius * 0.3);
		glEnd();
		glPopMatrix();

		// 행성
		gluSphere(quadric, planets[i].planetRadius, 100, 100);

		glPopMatrix();
		gluDeleteQuadric(quadric);
	}

	glutSwapBuffers();
}

void MyTimerUpdate() {
	LARGE_INTEGER curTime;
	::QueryPerformanceCounter(&curTime);
	deltaTime = (float)(curTime.QuadPart - prevTime.QuadPart) / (float)freq.QuadPart;
	prevTime = curTime;

	/* 초당 대력 60번 연산 */
	timeSum += deltaTime;
	
	if (timeSum >= 1/DAY_UPDATE_TIME_PER_SEC)
	{
		timeSum = timeSum - 1/DAY_UPDATE_TIME_PER_SEC;
		days += daySum;
	}
}

void MyUpdate()
{
	MyTimerUpdate();
	MyKeyboardEvent();
	glutPostRedisplay();
	CenterMouse();
	InfoRender();
}

void MyKeyboardEvent() {
	const float moveSpeed = cameraMoveSpeed * deltaTime; // 이동 속도

	// 이동
	if (GetAsyncKeyState('A')) {
		cameraX -= moveSpeed * cos(cameraYaw * M_PI / 180.0);
		cameraZ -= moveSpeed * sin(cameraYaw * M_PI / 180.0);
	}
	if (GetAsyncKeyState('D')) {
		cameraX += moveSpeed * cos(cameraYaw * M_PI / 180.0);
		cameraZ += moveSpeed * sin(cameraYaw * M_PI / 180.0);
	}
	if (GetAsyncKeyState(VK_SPACE)) {
		cameraY += moveSpeed;
	}
	if (GetAsyncKeyState(VK_LSHIFT)) {
		cameraY -= moveSpeed;
	}

	// 이동
	if (GetAsyncKeyState('W')) {
		cameraX += moveSpeed * sin(cameraYaw * M_PI / 180.0);
		cameraZ -= moveSpeed * cos(cameraYaw * M_PI / 180.0);
	}
	if (GetAsyncKeyState('S')) {
		cameraX -= moveSpeed * sin(cameraYaw * M_PI / 180.0);
		cameraZ += moveSpeed * cos(cameraYaw * M_PI / 180.0);
	}

	// 속도 조절
	if (GetAsyncKeyState('P'))
	{
		cameraMoveSpeed = min(50.f, cameraMoveSpeed + 10 * deltaTime);
	}
	if (GetAsyncKeyState('O'))
	{
		cameraMoveSpeed = max(0.f, cameraMoveSpeed - 10 * deltaTime);
	}

	// 제한 각도
	cameraPitch = clamp(cameraPitch, -89.0f, 89.0f);

	// 날짜가 지나는 정도 조절
	if (GetAsyncKeyState('L'))
		daySum = max(0.f, daySum - 0.5f * deltaTime);
	if (GetAsyncKeyState('K'))
		daySum = max(0.f, daySum + 0.5f * deltaTime);

	// 프로그램 종료
	if (GetAsyncKeyState(VK_ESCAPE))
	{
		glutDestroyWindow(glutGetWindow());
		exit(0);
	}
}

void MyMouseEvent(int x, int y) {
	if (firstMouse) {
		lastMouseX = centerX;
		lastMouseY = centerY;
		firstMouse = false;
	}

	float xoffset = x - centerX;
	float yoffset = centerY - y;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	cameraYaw += xoffset;
	cameraPitch += yoffset;

	cameraPitch = clamp(cameraPitch, -89.0f, 89.0f);
}

void UpdateCamera() {
	GLfloat dirX = sin(cameraYaw * M_PI / 180.0) * cos(cameraPitch * M_PI / 180.0);
	GLfloat dirY = sin(cameraPitch * M_PI / 180.0);
	GLfloat dirZ = -cos(cameraYaw * M_PI / 180.0) * cos(cameraPitch * M_PI / 180.0);

	gluLookAt(cameraX, cameraY, cameraZ,
		cameraX + dirX, cameraY + dirY, cameraZ + dirZ,
		0.0f, 1.0f, 0.0f);
}

void CenterMouse() {
	glutWarpPointer(centerX, centerY);
}

void ResourceInit()
{
	//리소스 상대경로
	fs::path path = fs::current_path();
	path /= "resources\\";

	//경로 결정
	resPath = path.c_str();
}

void ResourceLoader()
{
	for (int i = 0; i < SOLAR_SYSTEM_COUNT; i++)
		LoadGLTextures((SOLAR_SYSTEM)i);
}

int LoadGLTextures(SOLAR_SYSTEM idx)
{
	Planet* p = &planets[idx];
	wstring path = resPath + p->path;

	string strpath(path.begin(), path.end());

	glBindTexture(GL_TEXTURE_2D, 0);

	// 텍스처 로딩
	p->texture = SOIL_load_OGL_texture(
		strpath.c_str(),
		SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID,
		SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y
	);

	// 텍스처 로딩 오류 처리
	if (p->texture == 0) {
		cerr << "Error loading texture: " << SOIL_last_result() << endl;
		return FALSE;
	}

	// 텍스처 매핑 설정
	glBindTexture(GL_TEXTURE_2D, p->texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	return TRUE;
}

void ProgramInit(int* argc, char* argv[])
{
	glutInit(argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(windowWidth, windowHeight);
	glutCreateWindow("202012007 Ahnjiwoo");
	glutSetCursor(GLUT_CURSOR_NONE);

	::QueryPerformanceCounter(&prevTime);
	::QueryPerformanceFrequency(&freq);

	ResourceInit();
	ResourceLoader();
}

int main(int argc, char* argv[]) {
	ProgramInit(&argc, argv);

	glutDisplayFunc(MyDisplay);
	glutReshapeFunc(MyReshape);
	glutIdleFunc(MyUpdate);
	glutPassiveMotionFunc(MyMouseEvent);

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glClearDepth(1.0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glutMainLoop();

	return 0;
}