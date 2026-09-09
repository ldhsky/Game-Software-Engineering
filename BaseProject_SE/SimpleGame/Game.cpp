#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Iso.h"

#include "Dependencies\freeglut.h"

#include <cmath>
#include <cstdio>
#include <iostream>

const char* const* World_ElderDoneLines(int* count);

static const char* RECORD_TEXT[3] = {
	"0001  FISHER   - cannot see the lake",
	"0002  WOODSMAN - hands forget the tool",
	"0003  CHILD    - lost its shadow",
};
static const char* RECORD_KR[3] = {
	"[장부 0001] 어부 — 호수를 볼 수 없다. 눈은 멀쩡하다.",
	"[장부 0002] 나무꾼 — 손이 도구를 기억하지 못한다.",
	"[장부 0003] 우물가 아이 — 그림자를 잃었다.",
};

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

void Game::Init(Renderer* renderer, int windowW, int windowH)
{
	m_R = renderer;
	m_W = windowW;
	m_H = windowH;
	m_World.Generate();

	std::cout << "\n=== 등가의 장부 — 튜토리얼 레벨 ===\n";
	std::cout << "  이동: W A S D    상호작용/대화: E    장부: Tab    종료: Esc\n";
	std::cout << "  광장의 촌장(ELDER)에게 먼저 말을 걸어라.\n\n";
}

/* ---------- 입력 ---------- */

void Game::OnKeyDown(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
	m_Keys[key] = true;

	if (key == 27) { glutLeaveMainLoop(); return; }

	if (key == 'e')
	{
		if (m_State == GS_DIALOG) AdvanceDialog();
		else if (m_State == GS_PLAY) TryInteract();
	}
	if (key == 9) m_LedgerOpen = !m_LedgerOpen;   // Tab
}

void Game::OnKeyUp(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
	m_Keys[key] = false;
}

void Game::TryInteract()
{
	int best = -1;
	float bestD = 1.6f;   // 상호작용 반경 (타일)
	for (int i = 0; i < NPC_COUNT; ++i)
	{
		float dx = m_World.npcs[i].hx - m_PX;
		float dy = m_World.npcs[i].hy - m_PY;
		float d = sqrtf(dx * dx + dy * dy);
		if (d < bestD) { bestD = d; best = i; }
	}
	if (best < 0) return;

	const Npc& n = m_World.npcs[best];
	m_TalkingTo = best;
	m_LineIdx = 0;

	if (n.isElder && m_RecordCount >= 3)
		m_Lines = World_ElderDoneLines(&m_LineCount);
	else
	{
		m_Lines = n.lines;
		m_LineCount = n.lineCount;
	}

	m_State = GS_DIALOG;
	std::cout << m_Lines[0] << "\n";
}

void Game::AdvanceDialog()
{
	++m_LineIdx;
	if (m_LineIdx < m_LineCount)
	{
		std::cout << m_Lines[m_LineIdx] << "\n";
		return;
	}

	// 대화 종료 처리
	const Npc& n = m_World.npcs[m_TalkingTo];

	if (n.recordId >= 0 && !m_Recorded[n.recordId])
	{
		m_Recorded[n.recordId] = true;
		++m_RecordCount;
		std::cout << "\n" << RECORD_KR[n.recordId] << "\n";
		std::cout << ">> 장부에 기입되었다. (" << m_RecordCount << "/3)\n\n";
	}
	else if (n.isElder && m_RecordCount >= 3 && !m_Reported)
	{
		m_Reported = true;
		m_State = GS_END;
		m_EndTimer = 0.f;
		// 결손 발생 — 화면에서 붉은색이 빠진다.
		if (m_R) m_R->SetColorFilter(0.22f, 0.98f, 1.f);
		std::cout << "\n----------------------------------------\n";
		std::cout << "세 건이 접수되었다. 촌장은 당신의 이름을 적으려 했고,\n";
		std::cout << "그 칸은 비어 있는 채로 남았다.\n\n";
		std::cout << ">> 결손: 이 세계에서 붉은색이 빠져나갔다.\n";
		std::cout << "----------------------------------------\n";
		std::cout << "\n[튜토리얼 레벨 종료] Esc로 종료.\n";
		return;
	}

	m_State = GS_PLAY;
	m_TalkingTo = -1;
}

/* ---------- 갱신 ---------- */

void Game::Update(float dt)
{
	m_Time += dt;
	if (m_State != GS_END) m_Elapsed += dt;
	if (m_State == GS_END) m_EndTimer += dt;
	if (m_State != GS_PLAY) return;

	// WASD — 화면 기준 방향으로 매핑 (쿼터뷰이므로 월드 축은 45도 회전해 있다)
	float mx = 0.f, my = 0.f;
	if (m_Keys['w']) { mx -= 1.f; my -= 1.f; }
	if (m_Keys['s']) { mx += 1.f; my += 1.f; }
	if (m_Keys['a']) { mx -= 1.f; my += 1.f; }
	if (m_Keys['d']) { mx += 1.f; my -= 1.f; }

	float len = sqrtf(mx * mx + my * my);
	if (len < 0.001f) return;

	const float SPEED = 3.6f;   // 타일/초
	mx = mx / len * SPEED * dt;
	my = my / len * SPEED * dt;

	// 축별로 분리해 검사 — 벽에 붙어 미끄러지도록
	float nx = m_PX + mx;
	if (!m_World.Blocked((int)(nx + 0.5f), (int)(m_PY + 0.5f))) m_PX = nx;
	float ny = m_PY + my;
	if (!m_World.Blocked((int)(m_PX + 0.5f), (int)(ny + 0.5f))) m_PY = ny;
}

/* ---------- 분위기 ---------- */

void Game::Fog(float wx, float wy, float* r, float* g, float* b)
{
	float dx = wx - m_PX, dy = wy - m_PY;
	float d = sqrtf(dx * dx + dy * dy);
	float t = d / 22.f;
	if (t > 0.82f) t = 0.82f;
	if (t < 0.f) t = 0.f;
	// 안개 색 — 차가운 청록
	*r = Lerp(*r, 0.075f, t);
	*g = Lerp(*g, 0.105f, t);
	*b = Lerp(*b, 0.115f, t);
}

/* ---------- 렌더 ---------- */

void Game::DrawTileAt(int tx, int ty, float camX, float camY)
{
	unsigned char t = m_World.At(tx, ty);

	float sx, sy;
	Iso::WorldToScreen((float)tx, (float)ty, 0.f, &sx, &sy);
	sx -= camX; sy -= camY;

	float r = 0.16f, g = 0.22f, b = 0.17f;
	switch (t)
	{
	case T_PATH:  r = 0.27f; g = 0.24f; b = 0.19f; break;
	case T_PLAZA: r = 0.31f; g = 0.29f; b = 0.25f; break;
	case T_SHORE: r = 0.24f; g = 0.23f; b = 0.19f; break;
	case T_WATER:
	{
		float w = sinf(m_Time * 0.9f + (tx + ty) * 0.55f) * 0.02f;
		r = 0.085f + w; g = 0.155f + w; b = 0.215f + w * 1.6f;
		break;
	}
	case T_TREE:  r = 0.11f; g = 0.155f; b = 0.125f; break;
	case T_HOUSE: r = 0.19f; g = 0.175f; b = 0.155f; break;
	default: break;
	}
	// 지면 미세한 색 흔들림 — 격자감 완화
	float n = ((tx * 7 + ty * 13) % 5) * 0.006f;
	r += n; g += n; b += n;

	float fr = r, fg = g, fb = b;
	Fog((float)tx, (float)ty, &fr, &fg, &fb);
	m_R->DrawDiamond(sx, sy, Iso::TILE_W, Iso::TILE_H, fr, fg, fb);

	// 구조물
	if (t == T_TREE)
	{
		float trr = 0.14f, trg = 0.11f, trb = 0.09f;
		Fog((float)tx, (float)ty, &trr, &trg, &trb);
		m_R->DrawRect(sx, sy + 14.f, 7.f, 28.f, trr, trg, trb);

		float cr = 0.115f, cg = 0.20f, cb = 0.145f;
		Fog((float)tx, (float)ty, &cr, &cg, &cb);
		m_R->DrawDiamond(sx, sy + 40.f, 58.f, 46.f, cr, cg, cb);
		m_R->DrawDiamond(sx, sy + 52.f, 40.f, 32.f, cr * 1.25f, cg * 1.25f, cb * 1.25f);
	}
	else if (t == T_HOUSE)
	{
		float wr = 0.30f, wg = 0.27f, wb = 0.23f;
		Fog((float)tx, (float)ty, &wr, &wg, &wb);
		m_R->DrawRect(sx, sy + 20.f, 52.f, 40.f, wr, wg, wb);
		m_R->DrawRect(sx - 20.f, sy + 20.f, 12.f, 40.f, wr * 0.72f, wg * 0.72f, wb * 0.72f);

		float rr = 0.29f, rg = 0.135f, rb = 0.105f;   // 지붕 — 붉은 기와
		Fog((float)tx, (float)ty, &rr, &rg, &rb);
		m_R->DrawDiamond(sx, sy + 46.f, 64.f, 34.f, rr, rg, rb);

		// 창문 불빛 — 흔들린다
		float flick = 0.86f + sinf(m_Time * 2.3f + tx * 1.7f + ty) * 0.14f;
		m_R->DrawRect(sx + 6.f, sy + 18.f, 9.f, 11.f, 0.92f * flick, 0.72f * flick, 0.38f * flick, 0.95f);
	}
	else if (t == T_FENCE)
	{
		float cr = 0.22f, cg = 0.19f, cb = 0.15f;
		Fog((float)tx, (float)ty, &cr, &cg, &cb);
		m_R->DrawRect(sx, sy + 7.f, 46.f, 5.f, cr, cg, cb);
		m_R->DrawRect(sx - 18.f, sy + 8.f, 4.f, 16.f, cr, cg, cb);
		m_R->DrawRect(sx + 18.f, sy + 8.f, 4.f, 16.f, cr, cg, cb);
	}
	else if (t == T_WELL)
	{
		float cr = 0.26f, cg = 0.25f, cb = 0.24f;
		Fog((float)tx, (float)ty, &cr, &cg, &cb);
		m_R->DrawRect(sx, sy + 8.f, 30.f, 18.f, cr, cg, cb);
		m_R->DrawDiamond(sx, sy + 17.f, 30.f, 16.f, 0.045f, 0.06f, 0.07f);
	}
}

void Game::DrawNpc(const Npc& n, float camX, float camY)
{
	float sx, sy;
	Iso::WorldToScreen(n.hx, n.hy, 0.f, &sx, &sy);
	sx -= camX; sy -= camY;

	float bob = sinf(m_Time * 1.4f + n.phase) * 1.6f;

	m_R->DrawDiamond(sx, sy, 26.f, 13.f, 0.f, 0.f, 0.f, 0.30f);          // 그림자

	float r = n.r, g = n.g, b = n.b;
	Fog(n.hx, n.hy, &r, &g, &b);
	m_R->DrawRect(sx, sy + 13.f + bob, 13.f, 24.f, r * 0.55f, g * 0.55f, b * 0.55f);  // 몸
	m_R->DrawRect(sx, sy + 28.f + bob, 10.f, 10.f, r, g, b);                          // 머리

	// 기록 대상이고 아직 미기입이면 표식
	if (n.recordId >= 0 && !m_Recorded[n.recordId])
	{
		float p = 0.55f + sinf(m_Time * 3.f) * 0.35f;
		m_R->DrawDiamond(sx, sy + 44.f + bob, 12.f, 12.f, 0.55f, 0.16f, 0.11f, p);
	}
	// 상호작용 가능 표시
	float dx = n.hx - m_PX, dy = n.hy - m_PY;
	if (sqrtf(dx * dx + dy * dy) < 1.6f)
		Text(sx - 12.f, sy + 58.f, "E", 0.92f, 0.86f, 0.62f);

	Text(sx - (float)strlen(n.label) * 4.5f, sy - 16.f, n.label, 0.52f, 0.56f, 0.54f);
}

void Game::DrawPlayer(float camX, float camY)
{
	float sx, sy;
	Iso::WorldToScreen(m_PX, m_PY, 0.f, &sx, &sy);
	sx -= camX; sy -= camY;

	m_R->DrawDiamond(sx, sy, 28.f, 14.f, 0.f, 0.f, 0.f, 0.38f);
	m_R->DrawRect(sx, sy + 14.f, 14.f, 26.f, 0.20f, 0.19f, 0.22f);
	m_R->DrawRect(sx, sy + 30.f, 11.f, 11.f, 0.78f, 0.74f, 0.66f);
	// 서기의 장부 — 허리에 걸린 붉은 표지
	m_R->DrawRect(sx + 9.f, sy + 13.f, 6.f, 8.f, 0.48f, 0.14f, 0.09f);
}

void Game::DrawVignette()
{
	const int N = 10;
	float hw = m_W * 0.5f, hh = m_H * 0.5f;
	for (int i = 0; i < N; ++i)
	{
		float a = 0.055f * (float)(N - i) / N;
		float band = 26.f;
		float off = hh - band * 0.5f - i * band * 0.72f;
		m_R->DrawRect(0.f, off, (float)m_W, band, 0.f, 0.f, 0.f, a);
		m_R->DrawRect(0.f, -off, (float)m_W, band, 0.f, 0.f, 0.f, a);
		float offx = hw - band * 0.5f - i * band * 0.72f;
		m_R->DrawRect(offx, 0.f, band, (float)m_H, 0.f, 0.f, 0.f, a);
		m_R->DrawRect(-offx, 0.f, band, (float)m_H, 0.f, 0.f, 0.f, a);
	}
}

void Game::Text(float px, float py, const char* s, float r, float g, float b)
{
	glUseProgram(0);
	glColor3f(r, g, b);
	glRasterPos2f(px * 2.f / m_W, py * 2.f / m_H);
	glutBitmapString(GLUT_BITMAP_9_BY_15, (const unsigned char*)s);
}

void Game::DrawHud()
{
	float hw = m_W * 0.5f, hh = m_H * 0.5f;
	char buf[128];

	// 퀘스트 카운터
	sprintf_s(buf, "LEDGER  %d / 3", m_RecordCount);
	Text(-hw + 24.f, hh - 30.f, buf, 0.72f, 0.68f, 0.60f);
	sprintf_s(buf, "%02d:%02d", (int)m_Elapsed / 60, (int)m_Elapsed % 60);
	Text(hw - 76.f, hh - 30.f, buf, 0.45f, 0.50f, 0.48f);
	Text(-hw + 24.f, hh - 48.f, "WASD move   E interact   Tab ledger   Esc quit",
		0.38f, 0.42f, 0.41f);

	// 대화 상자
	if (m_State == GS_DIALOG)
	{
		float bh = 96.f;
		m_R->DrawRect(0.f, -hh + bh * 0.5f + 18.f, (float)m_W - 80.f, bh, 0.045f, 0.06f, 0.062f, 0.93f);
		m_R->DrawRect(0.f, -hh + bh + 17.f, (float)m_W - 80.f, 2.f, 0.42f, 0.16f, 0.11f, 1.f);
		const char* who = m_World.npcs[m_TalkingTo].label;
		Text(-m_W * 0.5f + 56.f, -hh + bh + 0.f, who, 0.80f, 0.44f, 0.32f);
		sprintf_s(buf, "line %d / %d          [E] continue   (text in console)",
			m_LineIdx + 1, m_LineCount);
		Text(-m_W * 0.5f + 56.f, -hh + 44.f, buf, 0.62f, 0.60f, 0.55f);
	}

	// 장부 패널
	if (m_LedgerOpen)
	{
		m_R->DrawRect(0.f, 0.f, 560.f, 220.f, 0.055f, 0.07f, 0.068f, 0.95f);
		m_R->DrawRect(0.f, 104.f, 560.f, 2.f, 0.42f, 0.16f, 0.11f, 1.f);
		Text(-250.f, 82.f, "LEDGER  -  unpaid entries", 0.80f, 0.44f, 0.32f);
		for (int i = 0; i < 3; ++i)
		{
			if (m_Recorded[i])
				Text(-250.f, 40.f - i * 26.f, RECORD_TEXT[i], 0.74f, 0.70f, 0.62f);
			else
				Text(-250.f, 40.f - i * 26.f, "----  not recorded yet", 0.34f, 0.36f, 0.35f);
		}
		Text(-250.f, -62.f, m_RecordCount >= 3 ? "report to ELDER at the plaza"
			: "find what is missing", 0.55f, 0.58f, 0.56f);
	}

	// 종료 연출
	if (m_State == GS_END)
	{
		float a = m_EndTimer * 0.35f;
		if (a > 0.72f) a = 0.72f;
		m_R->DrawRect(0.f, 0.f, (float)m_W, (float)m_H, 0.f, 0.f, 0.f, a);
		if (m_EndTimer > 1.2f)
		{
			Text(-64.f, 10.f, "UNPAID", 0.80f, 0.76f, 0.70f);
			Text(-128.f, -16.f, "tutorial level complete  -  Esc", 0.48f, 0.50f, 0.48f);
		}
	}
}

void Game::Render()
{
	m_R->BeginFrame(0.055f, 0.085f, 0.082f);

	float camX, camY;
	Iso::WorldToScreen(m_PX, m_PY, 0.f, &camX, &camY);

	// 가시 범위 — 화면 네 꼭짓점을 역변환해 월드 격자 범위를 얻는다
	float hw = m_W * 0.5f + Iso::TILE_W, hh = m_H * 0.5f + Iso::TILE_H * 3.f;
	float cx[4] = { -hw,  hw, -hw,  hw };
	float cy[4] = { -hh, -hh,  hh,  hh };
	int minX = MAP_W, maxX = 0, minY = MAP_H, maxY = 0;
	for (int i = 0; i < 4; ++i)
	{
		float wx, wy;
		Iso::ScreenToWorld(cx[i] + camX, cy[i] + camY, &wx, &wy);
		if ((int)wx - 1 < minX) minX = (int)wx - 1;
		if ((int)wx + 2 > maxX) maxX = (int)wx + 2;
		if ((int)wy - 1 < minY) minY = (int)wy - 1;
		if ((int)wy + 2 > maxY) maxY = (int)wy + 2;
	}
	if (minX < 0) minX = 0; if (minY < 0) minY = 0;
	if (maxX > MAP_W - 1) maxX = MAP_W - 1;
	if (maxY > MAP_H - 1) maxY = MAP_H - 1;

	// 화가 알고리즘 — depth(=wx+wy) 오름차순, 대각선 단위로 순회
	for (int s = minX + minY; s <= maxX + maxY; ++s)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			int y = s - x;
			if (y < minY || y > maxY) continue;
			DrawTileAt(x, y, camX, camY);
		}
		// 같은 대각선에 속한 엔티티
		for (int i = 0; i < NPC_COUNT; ++i)
		{
			const Npc& n = m_World.npcs[i];
			if ((int)(n.hx + n.hy) == s) DrawNpc(n, camX, camY);
		}
		if ((int)(m_PX + m_PY) == s) DrawPlayer(camX, camY);
	}

	DrawVignette();
	DrawHud();
}
