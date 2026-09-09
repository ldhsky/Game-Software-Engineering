/*
등가의 장부 — 튜토리얼 레벨 프로토타입
베이스: SimpleGame (Copyright 2022 Lee Taek Hee, Tech University of Korea)
*/

#include "stdafx.h"
#include <iostream>
#include <windows.h>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Game.h"

const int WIN_W = 1280;
const int WIN_H = 800;

Renderer* g_Renderer = NULL;
Game* g_Game = NULL;
int g_PrevTimeMs = 0;

void RenderScene(void)
{
	if (g_Game) g_Game->Render();
	glutSwapBuffers();
}

void Idle(void)
{
	int now = glutGet(GLUT_ELAPSED_TIME);
	float dt = (now - g_PrevTimeMs) / 1000.f;
	g_PrevTimeMs = now;
	if (dt > 0.1f) dt = 0.1f;

	if (g_Game) g_Game->Update(dt);
	glutPostRedisplay();
}

void KeyDown(unsigned char key, int x, int y) { if (g_Game) g_Game->OnKeyDown(key); }
void KeyUp(unsigned char key, int x, int y) { if (g_Game) g_Game->OnKeyUp(key); }

int main(int argc, char** argv)
{
	SetConsoleOutputCP(CP_UTF8);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(60, 30);
	glutInitWindowSize(WIN_W, WIN_H);
	glutCreateWindow("Ledger of Equal Value - Tutorial Level");

	// 창 제목을 한글로 — freeglut은 ANSI만 받으므로 직접 설정한다
	{
		HWND hwnd = FindWindowA(NULL, "Ledger of Equal Value - Tutorial Level");
		if (!hwnd) hwnd = GetActiveWindow();
		if (hwnd) SetWindowTextW(hwnd, L"등가의 장부 — 튜토리얼 레벨");
	}
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	if (glewInit() != GLEW_OK)
	{
		std::cout << "GLEW 초기화 실패\n";
		return 1;
	}

	g_Renderer = new Renderer(WIN_W, WIN_H);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "렌더러 초기화 실패 — Shaders 폴더 경로를 확인하십시오.\n";
		delete g_Renderer;
		return 1;
	}

	g_Game = new Game();
	g_Game->Init(g_Renderer, WIN_W, WIN_H);

	glutIgnoreKeyRepeat(1);
	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);

	g_PrevTimeMs = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();

	g_Game->Shutdown();
	delete g_Game;
	delete g_Renderer;
	return 0;
}
