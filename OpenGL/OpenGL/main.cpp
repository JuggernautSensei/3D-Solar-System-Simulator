#include <GL/glut.h>
#include <SOIL.h>
#include <string>
using namespace std;

// �ؽ�ó ID
GLuint moonTexture;
GLuint faceTexture;

// ȸ�� ����
float angle = 0.0f;

void init() {
    // GL �ʱ�ȭ
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("202012007 Ahnjiwoo");

    // ���� ���� + ���� 2�� + ���� Ȱ��ȭ
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_TEXTURE_2D);

    // Omni Light ���� 
    GLfloat lightPos0[] = { 0.0f, 5.0f, 5.0f, 1.0f };  // ��ġ
    GLfloat lightColor0[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // ����
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor0);

    // Spot Light ����
    GLfloat lightPos1[] = { 5.0f, 5.0f, 5.0f, 1.0f }; // ��ġ
    GLfloat lightDir1[] = { -1.0f, -1.0f, -1.0f };  // ����
    GLfloat lightColor1[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // ����
    glLightfv(GL_LIGHT1, GL_POSITION, lightPos1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightColor1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lightColor1);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, lightDir1);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 45.0f);  // ����
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 2.0f);

    moonTexture = SOIL_load_OGL_texture(
        "moon.jpg",
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
    );

    faceTexture = SOIL_load_OGL_texture(
        "face.jpg",
        SOIL_LOAD_AUTO,
        SOIL_CREATE_NEW_ID,
        SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_TEXTURE_REPEATS
    );

    if (moonTexture == 0 || faceTexture == 0) {
        printf("SOIL loading error: '%s'\n", SOIL_last_result());
        exit(1);
    }

    // �ؽ�ó ���� ����
    glBindTexture(GL_TEXTURE_2D, moonTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, faceTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void reshape(int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (GLfloat)w / (GLfloat)h, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // ī�޶� ����
    gluLookAt(0.0, 0.0, 10.0,
        0.0, 0.0, 0.0,
        0.0, 1.0, 0.0);

    // ������ ȸ��
    glPushMatrix();
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    glTranslatef(-3.f, 0.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, moonTexture);
    glutSolidTeapot(1.0);
    glPopMatrix();

    // �� ȸ��
    glPushMatrix();
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    glTranslatef(3.f, 0.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, faceTexture);
    glColor3f(1.0f, 1.0f, 1.0f);
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluSphere(quad, 1.0, 20, 20);
    gluDeleteQuadric(quad);
    glPopMatrix();

    glutSwapBuffers();
}

void timer(int value) {
    angle += 1.f;
    if (angle > 360.0f) {
        angle -= 360.0f;
    }
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);

    init();

    glutDisplayFunc(display);
    glutTimerFunc(16, timer, 0);
    glutReshapeFunc(reshape);

    glutMainLoop();

    return 0;
}
