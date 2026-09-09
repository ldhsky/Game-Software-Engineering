#pragma once

#include "World.h"
#include "Text.h"

class Renderer;

enum GameState { GS_PLAY = 0, GS_DIALOG, GS_END };

struct Light { float sx, sy, r, g, b, str; };
struct Mote { float x, y, vx, vy, a, s; };

class Game
{
public:
	void Init(Renderer* renderer, int windowW, int windowH);
	void Shutdown();
	void Update(float dt);
	void Render();

	void OnKeyDown(unsigned char key);
	void OnKeyUp(unsigned char key);

private:
	void TryInteract();
	void AdvanceDialog();
	void Toast(const char* msg);

	void Shade(float wx, float wy, float sx, float sy, float* r, float* g, float* b);
	void DrawGround(int tx, int ty, float sx, float sy);
	void DrawObject(int tx, int ty, float sx, float sy);
	void DrawNpc(const Npc& n);
	void DrawPlayer();
	void DrawMotes();
	void DrawVignette();
	void DrawHud();

	Renderer* m_R = 0;
	World m_World;
	TextRenderer m_F;      // 본문
	TextRenderer m_FB;     // 제목
	int m_W = 0, m_H = 0;

	float m_PX = (float)World::START_X;
	float m_PY = (float)World::START_Y;
	float m_FaceX = 0.f, m_FaceY = 1.f;
	float m_CamWX = 0.f, m_CamWY = 0.f;   // 부드럽게 따라오는 카메라 (월드)
	float m_CamSX = 0.f, m_CamSY = 0.f;   // 카메라 화면 오프셋
	float m_Time = 0.f, m_Elapsed = 0.f;
	float m_Walk = 0.f;

	bool m_Keys[256] = { false };

	GameState m_State = GS_PLAY;
	bool m_LedgerOpen = false;

	const char* const* m_Lines = 0;
	int m_LineCount = 0, m_LineIdx = 0, m_TalkingTo = -1;

	bool m_Recorded[3] = { false, false, false };
	int m_RecordCount = 0;
	int m_Fragments = 0;
	bool m_FoundFragment[FIELD_RECORD_COUNT] = { false };
	bool m_Reported = false;
	float m_EndTimer = 0.f;

	char m_Toast[256] = { 0 };
	float m_ToastTimer = 0.f;

	Light m_Lights[96];
	int m_LightCount = 0;
	Mote m_Motes[170];
};
