#pragma once

#include "World.h"

class Renderer;

enum GameState { GS_PLAY = 0, GS_DIALOG, GS_END };

class Game
{
public:
	void Init(Renderer* renderer, int windowW, int windowH);
	void Update(float dt);
	void Render();

	void OnKeyDown(unsigned char key);
	void OnKeyUp(unsigned char key);

private:
	void TryInteract();
	void AdvanceDialog();
	void DrawTileAt(int tx, int ty, float camX, float camY);
	void DrawNpc(const Npc& n, float camX, float camY);
	void DrawPlayer(float camX, float camY);
	void DrawVignette();
	void DrawHud();
	void Text(float px, float py, const char* s, float r, float g, float b);
	void Fog(float wx, float wy, float* r, float* g, float* b);

	Renderer* m_R = nullptr;
	World m_World;
	int m_W = 0, m_H = 0;

	float m_PX = (float)World::START_X;
	float m_PY = (float)World::START_Y;
	float m_Time = 0.f;
	float m_Elapsed = 0.f;

	bool m_Keys[256] = { false };

	GameState m_State = GS_PLAY;
	bool m_LedgerOpen = false;

	// 대화
	const char* const* m_Lines = nullptr;
	int m_LineCount = 0;
	int m_LineIdx = 0;
	int m_TalkingTo = -1;

	// 퀘스트 「미납」
	bool m_Recorded[3] = { false, false, false };
	int m_RecordCount = 0;
	bool m_Reported = false;
	float m_EndTimer = 0.f;
};
