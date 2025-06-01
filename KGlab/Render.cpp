#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "Texture.h"

#include "cmath"

#include "ObjLoader.h"

#include "debout.h"

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") 

struct point
{
	double x;
	double y;
	double z;
};

void PlayBalalaMusic()
{
	PlaySound((LPCTSTR)L"music/Balala.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
}

point CalculateNormal(point A, point B, point C)
{
	//вычисляем стороны-вектора
	point BA{ A.x - B.x, A.y - B.y, A.z - B.z };
	point BC{ C.x - B.x, C.y - B.y, C.z - B.z };

	//считаем векторное произведение BA x BC
	point N{ BA.y * BC.z - BA.z * BC.y, -BA.x * BC.z + BA.z * BC.x, BA.x * BC.y - BA.y * BC.x };

	//Нормализуем нормаль
	double l = sqrt(N.x * N.x + N.y * N.y + N.z * N.z);
	N.x /= l;
	N.y /= l;
	N.z /= l;

	return N;
}


void DrawQuad(point a, point b, point c, point d)
{
	point N;
	N = CalculateNormal(a, b, c);
	glNormal3dv((double*)&N);

	glTexCoord2d(0, 0);
	glVertex3dv((double*)&a);
	glTexCoord2d(0, 1);
	glVertex3dv((double*)&b);
	glTexCoord2d(0, 1);
	glVertex3dv((double*)&c);
	glTexCoord2d(0, 1);
	glVertex3dv((double*)&d);
}

void DrawCover(point a, point b, point c, point d)
{
	point N;
	N = CalculateNormal(a, b, c);
	glNormal3dv((double*)&N);

	glTexCoord2d(0, 1);
	glVertex3dv((double*)&a);
	glTexCoord2d(1, 1);
	glVertex3dv((double*)&b);
	glTexCoord2d(1, 0);
	glVertex3dv((double*)&c);
	glTexCoord2d(0, 0);
	glVertex3dv((double*)&d);
}

point BezierCurve(point P0, point P1, point P2, point P3, double t)
{
	double x = pow(1 - t, 3) * P0.x + 3 * t * pow(1 - t, 2) * P1.x + 3 * pow(t, 2) * (1 - t) * P2.x + pow(t, 3) * P3.x;
	double y = pow(1 - t, 3) * P0.y + 3 * t * pow(1 - t, 2) * P1.y + 3 * pow(t, 2) * (1 - t) * P2.y + pow(t, 3) * P3.y;
	double z = pow(1 - t, 3) * P0.z + 3 * t * pow(1 - t, 2) * P1.z + 3 * pow(t, 2) * (1 - t) * P2.z + pow(t, 3) * P3.z;
	point C = { x, y, z };
	return C;
}

//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = true;


int randnum[4];
int randspeed[4];
int randdeckbacks;

void Shuffle()
{
	int start = 0;
	int end = 144;
	srand(time(0));
	randnum[0] = rand() % (end - start + 1) + start;
	randnum[1] = rand() % (end - start + 1) + start;
	randnum[2] = rand() % (end - start + 1) + start;
	randnum[3] = rand() % (end - start + 1) + start;
}

void RandomiseSinSpeed()
{
	int start = 40;
	int end = 50;
	srand(time(0));
	randspeed[0] = rand() % (end - start + 1) + start;
	randspeed[1] = rand() % (end - start + 1) + start;
	randspeed[2] = rand() % (end - start + 1) + start;
	randspeed[3] = rand() % (end - start + 1) + start;
}

void RandomiseDeckBacks()
{
	int start = 0;
	int end = 17;
	srand(time(0));
	randdeckbacks = rand() % (end - start + 1) + start;

}

bool open = false;
bool pull = false;
bool change = false;
bool shuffle = false;
bool rotate = false;

int k = 1;
double rotated = 0;

//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL *sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;

	case 'S':
		Shuffle();
		shuffle = !shuffle;
		rotated = 0;
		break;
	case 'P':
		pull = !pull;
		k = 1;
		break;
	case 'C':
		change = !change;
		break;
	case 'R':
		rotate = !rotate;
		break;
	}
}

//умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template<typename T, int M1, int N1, int M2, int N2>
void MatrixMultiply(const T* a, const T* b, T* c)
{
	for (int i = 0; i < M1; ++i)
	{
		for (int j = 0; j < N2; ++j)
		{
			c[i * N2 + j] = T(0);
			for (int k = 0; k < N1; ++k)
			{
				c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
			}
		}
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером

ObjModel f;


Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;


Texture Jockers[145];

Texture Booster;

Texture DeckBack[18];

void LoadJockersTextures()
{
	for(int i = 0; i < 155; i++)
	{
		if ( i != 83 and i != 84 and i != 85 and i != 86 and i != 87 and i != 90 and i != 91 and i != 92 and i != 93 and i != 94)
		{
			std::string Number = std::to_string(i + 1);
			Jockers[i].LoadTexture("textures/Jockers/Jokers" + Number + ".png");
		}
	}
}

void LoadDeckBacksTextures()
{
	for (int i = 0; i < 18; i++)
	{
		std::string Number = std::to_string(i + 1);
		DeckBack[i].LoadTexture("textures/DeckBacks/DeckBacks" + Number + ".png");
	}
}

void initRender()
{
	cassini_sh.VshaderFileName = "shaders/v.vert";
	cassini_sh.FshaderFileName = "shaders/cassini.frag";
	cassini_sh.LoadShaderFromFile();
	cassini_sh.Compile();

	phong_sh.VshaderFileName = "shaders/v.vert";
	phong_sh.FshaderFileName = "shaders/light.frag";
	phong_sh.LoadShaderFromFile();
	phong_sh.Compile();

	vb_sh.VshaderFileName = "shaders/v.vert";
	vb_sh.FshaderFileName = "shaders/vb.frag";
	vb_sh.LoadShaderFromFile();
	vb_sh.Compile();

	simple_texture_sh.VshaderFileName = "shaders/v.vert";
	simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
	simple_texture_sh.LoadShaderFromFile();
	simple_texture_sh.Compile();

	LoadJockersTextures();
	LoadDeckBacksTextures();
	Booster.LoadTexture("textures/Booster/booster.png");

	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	

	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 180);
	//========================================================
	   

	camera.setPosition(2, 1.5, 1.5);

	Shuffle();

	RandomiseSinSpeed();

	RandomiseDeckBacks();

	PlayBalalaMusic();
}

void DrawBooster()
{
	point O = { 0, 0, 0 };

	double length = 8.5;
	double height = 0.12;
	double width = 7;

	point CoverLeftUpFront = { O.x, O.y - width / 2.0, O.z + length / 2.0 };
	point CoverRightUpFront = { O.x, O.y + width / 2.0, O.z + length / 2.0 };
	point CoverLeftDownFront = { O.x, O.y - width / 2.0, O.z - length / 2.0 };
	point CoverRightDownFront = { O.x, O.y + width / 2.0, O.z - length / 2.0 };

	point CoverLeftUpBack = { O.x - height, O.y - width / 2.0, O.z + length / 2.0 };
	point CoverRightUpBack = { O.x - height, O.y + width / 2.0, O.z + length / 2.0 };
	point CoverLeftDownBack = { O.x - height, O.y - width / 2.0, O.z - length / 2.0 };
	point CoverRightDownBack = { O.x - height, O.y + width / 2.0, O.z - length / 2.0 };

	Booster.Bind();

	glBegin(GL_QUADS);
	glColor3d(0.5, 0.5, 0.5);

	DrawCover(CoverLeftUpFront, CoverRightUpFront, CoverRightDownFront, CoverLeftDownFront);

	DrawQuad(CoverLeftUpFront, CoverLeftUpBack, CoverRightUpBack, CoverRightUpFront);
	DrawQuad(CoverLeftUpBack, CoverLeftUpFront, CoverLeftDownFront, CoverLeftDownBack);
	DrawQuad(CoverRightUpFront, CoverRightUpBack, CoverRightDownBack, CoverRightDownFront);
	DrawQuad(CoverLeftDownBack, CoverLeftDownFront, CoverRightDownFront, CoverRightDownBack);

	DrawCover(CoverRightUpBack, CoverLeftUpBack, CoverLeftDownBack, CoverRightDownBack);

	glEnd();
}

void DrawCard(int num)
{
	point center = { 0, 0, 0 };

	double length = 8.5;
	double height = 0.015;
	double width = 5.5;

	point CoverLeftUpFront = { center.x, center.y - width / 2.0, center.z + length / 2.0 };
	point CoverRightUpFront = { center.x, center.y + width / 2.0, center.z + length / 2.0 };
	point CoverLeftDownFront = { center.x, center.y - width / 2.0, center.z - length / 2.0 };
	point CoverRightDownFront = { center.x, center.y + width / 2.0, center.z - length / 2.0 };

	point CoverLeftUpBack = { center.x - height, center.y - width / 2.0, center.z + length / 2.0 };
	point CoverRightUpBack = { center.x - height, center.y + width / 2.0, center.z + length / 2.0 };
	point CoverLeftDownBack = { center.x - height, center.y - width / 2.0, center.z - length / 2.0 };
	point CoverRightDownBack = { center.x - height, center.y + width / 2.0, center.z - length / 2.0 };

	point N;
	N = CalculateNormal(CoverLeftUpFront, CoverRightUpFront, CoverLeftDownFront);
	glNormal3dv((double*)&N);

	Jockers[randnum[num]].Bind();

	glBegin(GL_QUADS);
	glTexCoord2d(0, 1);
	glVertex3dv((double*)&CoverLeftUpFront);
	glTexCoord2d(1, 1);
	glVertex3dv((double*)&CoverRightUpFront);
	glTexCoord2d(1, 0);
	glVertex3dv((double*)&CoverRightDownFront);
	glTexCoord2d(0, 0);
	glVertex3dv((double*)&CoverLeftDownFront);
	glEnd();

	N = CalculateNormal(CoverLeftUpBack, CoverRightDownBack, CoverRightUpBack);
	glNormal3dv((double*)&N);

	DeckBack[randdeckbacks].Bind();

	glBegin(GL_QUADS);
	glTexCoord2d(0, 1);
	glVertex3dv((double*)&CoverLeftUpBack);
	glTexCoord2d(1, 1);
	glVertex3dv((double*)&CoverRightUpBack);
	glTexCoord2d(1, 0);
	glVertex3dv((double*)&CoverRightDownBack);
	glTexCoord2d(0, 0);
	glVertex3dv((double*)&CoverLeftDownBack);
	glEnd();
}

float view_matrix[16];
double full_time = 0;
int location = 0;

double movement = 0;
double sin_move_card[4] = { 0 };


bool choice = false;

double rotate_cards = 0;

int count = 0;

void Render(double delta_time)
{    
	

	full_time += delta_time;
	
	//натройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	//забираем моделвью матрицу сразу после установки камера
	//так как в ней отсуствуют трансформации glRotate...
	//она, фактически, является видовой.
	glGetFloatv(GL_MODELVIEW_MATRIX,view_matrix);

	

	light.SetUpLight();

	//рисуем оси
	//gl.DrawAxes();

	

	glBindTexture(GL_TEXTURE_2D, 0);

	//включаем нормализацию нормалей
	//чтобв glScaled не влияли на них.

	glEnable(GL_NORMALIZE);  
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	
	//включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
	if (lightning)
		glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
	}
		
	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
		
	//=============НАСТРОЙКА МАТЕРИАЛА==============


	//настройка материала, все что рисуется ниже будет иметь этот метериал.
	//массивы с настройками материала
	float  amb[] = { 0.2, 0.2, 0.2, 1. };
	float dif[] = { 0.9, 0.9, 0.9, 1. };
	float spec[] = { 0, 0, 0, 1. };
	float sh = 0.2f * 256;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec); 
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
			   //(GL_SMOOTH - плоская закраска)

	//============ РИСОВАТЬ ТУТ ==============

	int samples = 50;
	double step = 1. / (samples - 1);

	point CARD[4];

	point Card1CurveP0 = { -0.01, 0, 0 };
	point Card1CurveP1 = { -0.01, 0, 10 };
	point Card1CurveP2 = { -0.01, 1, 14 };
	point Card1CurveP3 = { 0, -12, 16 };

	point Card2CurveP0 = { -0.02, 0, 0 };
	point Card2CurveP1 = { -0.02, 0, 10 };
	point Card2CurveP2 = { -0.02, -1, 14 };
	point Card2CurveP3 = { 0, 12, 16 };

	point Card3CurveP0 = { -0.03, 0, 0 };
	point Card3CurveP1 = { -0.03, 0, 10 };
	point Card3CurveP2 = { -0.03, -1, 14 };
	point Card3CurveP3 = { 0, -6, 20 };

	point Card4CurveP0 = { -0.04, 0, 0 };
	point Card4CurveP1 = { -0.04, 0, 10 };
	point Card4CurveP2 = { -0.04, -1, 14 };
	point Card4CurveP3 = { 0, 6, 20 };


	double speed_choice1 = randspeed[0] * delta_time * step * choice;
	double speed_choice2 = randspeed[1] * delta_time * step * choice;
	double speed_choice3 = randspeed[2] * delta_time * step * choice;
	double speed_choice4 = randspeed[3] * delta_time * step * choice;

	double speed_reveal = 25 * delta_time * step * open;
	movement += speed_reveal * k;

	CARD[0] = BezierCurve(Card1CurveP0, Card1CurveP1, Card1CurveP2, Card1CurveP3, movement);
	CARD[1] = BezierCurve(Card2CurveP0, Card2CurveP1, Card2CurveP2, Card2CurveP3, movement);
	CARD[2] = BezierCurve(Card3CurveP0, Card3CurveP1, Card3CurveP2, Card3CurveP3, movement);
	CARD[3] = BezierCurve(Card4CurveP0, Card4CurveP1, Card4CurveP2, Card4CurveP3, movement);

	if(pull and CARD[0].y < Card1CurveP3.y and CARD[0].z > Card1CurveP3.z)
	{
		pull = false;
		open = false;
		choice = true;
	}

	if (change and CARD[0].y > Card1CurveP0.y and CARD[0].z < Card1CurveP0.z)
	{
		open = false;
		change = false;
	}

	if(choice)
	{
		sin_move_card[0] += speed_choice1;
		sin_move_card[1] += speed_choice2;
		sin_move_card[2] += speed_choice3;
		sin_move_card[3] += speed_choice4;
		CARD[0].z = Card1CurveP3.z + sin(sin_move_card[0]);
		CARD[1].z = Card2CurveP3.z + sin(sin_move_card[1]);
		CARD[2].z = Card3CurveP3.z + sin(sin_move_card[2]);
		CARD[3].z = Card4CurveP3.z + sin(sin_move_card[3]);
	}

	if(change)
	{
		pull = false;
		open = true;
		choice = false;
		k = -1;
	}

	if(pull and !change)
	{
		open = true;
	}

	double speed_rotated = 250 * delta_time * shuffle;
	rotated += speed_rotated;

	if(rotated >= 360)
	{
		rotated = 360;
		shuffle = false;
	}

	double speed_rotate = 240 * delta_time * rotate;
	rotate_cards += speed_rotate;

	if(rotate and rotate_cards >= 360 * (count + 1) and rotate_cards <= 360 * (count + 2))
	{
		count++;
	}

	if(!rotate and  rotate_cards > 360 * count and rotate_cards < 360 * (count + 1))
	{
		rotate_cards += 240 * delta_time;
	}
	else
	{
		if(!rotate)
		{
			rotate_cards = 0;
			count = 0;
		}
		
	}

	glPushMatrix();

	glRotated(rotated, 0, 0, 1);
	DrawBooster();
	

	glPushMatrix();
	glTranslated(CARD[0].x, CARD[0].y, CARD[0].z);
	glRotated(rotate_cards, 0, 0, 1);
	DrawCard(0);
	glPopMatrix();

	glPushMatrix();
	glTranslated(CARD[1].x, CARD[1].y, CARD[1].z);
	glRotated(rotate_cards, 0, 0, 1);
	DrawCard(1);
	glPopMatrix();
	
	glPushMatrix();
	glTranslated(CARD[2].x, CARD[2].y, CARD[2].z);
	glRotated(rotate_cards, 0, 0, 1);
	DrawCard(2);
	glPopMatrix();

	glPushMatrix();
	glTranslated(CARD[3].x, CARD[3].y, CARD[3].z);
	glRotated(rotate_cards, 0, 0, 1);
	DrawCard(3);
	glPopMatrix();

	glPopMatrix();

	//===============================================
	
	
	//сбрасываем все трансформации
	glLoadIdentity();
	camera.SetUpCamera();	
	Shader::DontUseShaders();
	//рисуем источник света
	light.DrawLightGizmo();

	//================Сообщение в верхнем левом углу=======================
	glActiveTexture(GL_TEXTURE0);
	//переключаемся на матрицу проекции
	glMatrixMode(GL_PROJECTION);
	//сохраняем текущую матрицу проекции с перспективным преобразованием
	glPushMatrix();
	//загружаем единичную матрицу в матрицу проекции
	glLoadIdentity();

	//устанавливаем матрицу паралельной проекции
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

	//переключаемся на моделвью матрицу
	glMatrixMode(GL_MODELVIEW);
	//сохраняем матрицу
	glPushMatrix();
    //сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
	glLoadIdentity();

	//отрисованное тут будет визуалзироватся в 2д системе координат
	//нижний левый угол окна - точка (0,0)
	//верхний правый угол (ширина_окна - 1, высота_окна - 1)

	
	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	/*ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;*/

	ss << L"Кнопки:\n P - вытащить карты\n R - вращать карты\n C - вернуть карты в бустер\n S - Перемешать карты\n L - свет " << std::endl;

	ss << L"Коорд. света: (" << std::setw(7) <<  light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5)<< delta_time << std::endl;
	ss << L"full_time: " << std::setprecision(2) << full_time << std::endl;

	
	
	text.setPosition(10, gl.getHeight() - 10 - 180);
	text.setText(ss.str().c_str());
	
	text.Draw();

	//восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
}   



