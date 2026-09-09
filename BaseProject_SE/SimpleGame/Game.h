#pragma once

#include "World.h"
#include "Text.h"
#include "Char.h"
#include "Actors.h"

class Renderer;

enum GameState { GS_PLAY = 0, GS_DIALOG, GS_END };

struct Light { float sx, sy, r, g, b, str; };
struct Mote { float x, y, vx, vy, a, s; };
struct Puff { float x, y, t, life; int type; };   // 0 먼지 1 물결

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
	void SpawnPuff(float wx, float wy, int type);

	float FogT(float wx, float wy);
	void LightAt(float sx, float sy, float* lr, float* lg, float* lb);
	void Shade(float wx, float wy, float sx, float sy, float* r, float* g, float* b);

	void DrawGround(int tx, int ty, float sx, float sy);
	void DrawGroundDetail(int tx, int ty, float sx, float sy);
	void DrawShadow(int tx, int ty, float sx, float sy);
	void DrawObject(int tx, int ty, float sx, float sy);
	void DrawHouse(int tx, int ty, float sx, float sy);
	void DrawNpc(int i);
	void DrawBeast(int i);
	void DrawPlayer();
	void DrawPuffs();
	void DrawMotes();
	void DrawHud();

	void ToScreen(float wx, float wy, float* sx, float* sy);

	Renderer* m_R = 0;
	World m_World;
	Beasts m_Beasts;
	TextRenderer m_F;
	TextRenderer m_FB;
	int m_W = 0, m_H = 0;

	float m_PX = (float)World::START_X;
	float m_PY = (float)World::START_Y;
	float m_FaceX = 0.f, m_FaceY = 1.f;
	float m_CamWX = 0.f, m_CamWY = 0.f;
	float m_CamSX = 0.f, m_CamSY = 0.f;
	float m_Time = 0.f, m_Elapsed = 0.f, m_Walk = 0.f;
	bool m_Moving = false;
	float m_StepTimer = 0.f;

	CharStyle m_PlayerStyle;
	CharStyle m_NpcStyle[NPC_COUNT];

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

	Light m_Lights[80];
	int m_LightCount = 0;
	Mote m_Motes[190];
	Puff m_Puffs[32];
};
