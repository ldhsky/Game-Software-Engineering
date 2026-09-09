#include "stdafx.h"
#include "Game.h"
#include "Renderer.h"
#include "Iso.h"

#include "Dependencies\freeglut.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static const char* RECORD_KR[3] = {
	"[장부 0001] 어부 — 호수를 볼 수 없다. 눈은 멀쩡하다.",
	"[장부 0002] 나무꾼 — 손이 도구를 기억하지 못한다.",
	"[장부 0003] 우물가 아이 — 그림자를 잃었다.",
};
static const char* RECORD_SHORT[3] = {
	"0001  어부 — 호수를 볼 수 없다",
	"0002  나무꾼 — 손이 도구를 기억하지 못한다",
	"0003  아이 — 그림자를 잃었다",
};

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
static float Frand() { return (rand() % 10000) / 10000.f; }

void Game::Init(Renderer* renderer, int windowW, int windowH)
{
	m_R = renderer;
	m_W = windowW;
	m_H = windowH;
	m_World.Init();
	m_World.EnsureAround(m_PX, m_PY, 3);

	m_F.Init(m_R, 17);
	m_FB.Init(m_R, 26);

	m_CamWX = m_PX; m_CamWY = m_PY;

	for (int i = 0; i < 170; ++i)
	{
		m_Motes[i].x = (Frand() - 0.5f) * m_W;
		m_Motes[i].y = (Frand() - 0.5f) * m_H;
		m_Motes[i].vx = (Frand() - 0.5f) * 9.f;
		m_Motes[i].vy = 3.f + Frand() * 7.f;
		m_Motes[i].a = 0.05f + Frand() * 0.16f;
		m_Motes[i].s = 1.f + Frand() * 2.2f;
	}
	Toast("촌장에게 말을 걸어라.  [E]");
}

void Game::Shutdown()
{
	m_F.Shutdown();
	m_FB.Shutdown();
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
	if (key == 9) m_LedgerOpen = !m_LedgerOpen;
}

void Game::OnKeyUp(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
	m_Keys[key] = false;
}

void Game::Toast(const char* msg)
{
	sprintf_s(m_Toast, "%s", msg);
	m_ToastTimer = 5.f;
}

void Game::TryInteract()
{
	// NPC 우선
	int best = -1;
	float bestD = 1.9f;
	for (int i = 0; i < NPC_COUNT; ++i)
	{
		float dx = m_World.npcs[i].hx - m_PX, dy = m_World.npcs[i].hy - m_PY;
		float d = sqrtf(dx * dx + dy * dy);
		if (d < bestD) { bestD = d; best = i; }
	}
	if (best >= 0)
	{
		const Npc& n = m_World.npcs[best];
		m_TalkingTo = best;
		m_LineIdx = 0;
		if (n.isElder)
		{
			if (m_RecordCount >= 3) m_Lines = World_ElderDone(&m_LineCount);
			else                    m_Lines = World_ElderIntro(&m_LineCount);
		}
		else { m_Lines = n.lines; m_LineCount = n.lineCount; }
		m_State = GS_DIALOG;
		return;
	}

	// 유적의 장부 조각
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			int tx = (int)floorf(m_PX + 0.5f) + dx;
			int ty = (int)floorf(m_PY + 0.5f) + dy;
			int idx = 0;
			if (m_World.TakeRuin(tx, ty, &idx))
			{
				if (!m_FoundFragment[idx]) { m_FoundFragment[idx] = true; ++m_Fragments; }
				Toast(World_FieldRecord(idx));
				return;
			}
		}
	}
}

void Game::AdvanceDialog()
{
	++m_LineIdx;
	if (m_LineIdx < m_LineCount) return;

	const Npc& n = m_World.npcs[m_TalkingTo];

	if (n.recordId >= 0 && !m_Recorded[n.recordId])
	{
		m_Recorded[n.recordId] = true;
		++m_RecordCount;
		Toast(RECORD_KR[n.recordId]);
		if (m_RecordCount >= 3) Toast("세 건 모두 기입되었다. 촌장에게 돌아가라.");
	}
	else if (n.isElder && m_RecordCount >= 3 && !m_Reported)
	{
		m_Reported = true;
		m_State = GS_END;
		m_EndTimer = 0.f;
		if (m_R) m_R->SetColorFilter(0.20f, 0.97f, 1.f);   // 결손 — 붉은색이 빠진다
		return;
	}

	m_State = GS_PLAY;
	m_TalkingTo = -1;
}

/* ---------- 갱신 ---------- */

void Game::Update(float dt)
{
	m_Time += dt;
	if (m_State == GS_END) m_EndTimer += dt;
	else m_Elapsed += dt;
	if (m_ToastTimer > 0.f) m_ToastTimer -= dt;

	// 카메라는 항상 따라온다
	m_CamWX = Lerp(m_CamWX, m_PX, Clamp01(dt * 6.f));
	m_CamWY = Lerp(m_CamWY, m_PY, Clamp01(dt * 6.f));
	Iso::WorldToScreen(m_CamWX, m_CamWY, 0.f, &m_CamSX, &m_CamSY);

	for (int i = 0; i < 170; ++i)
	{
		m_Motes[i].x += m_Motes[i].vx * dt;
		m_Motes[i].y += m_Motes[i].vy * dt;
		if (m_Motes[i].y > m_H * 0.5f) { m_Motes[i].y = -m_H * 0.5f; m_Motes[i].x = (Frand() - 0.5f) * m_W; }
		if (m_Motes[i].x > m_W * 0.5f) m_Motes[i].x = -m_W * 0.5f;
		if (m_Motes[i].x < -m_W * 0.5f) m_Motes[i].x = m_W * 0.5f;
	}

	if (m_State != GS_PLAY) return;

	float mx = 0.f, my = 0.f;
	if (m_Keys['w']) { mx -= 1.f; my -= 1.f; }
	if (m_Keys['s']) { mx += 1.f; my += 1.f; }
	if (m_Keys['a']) { mx -= 1.f; my += 1.f; }
	if (m_Keys['d']) { mx += 1.f; my -= 1.f; }

	float len = sqrtf(mx * mx + my * my);
	if (len < 0.001f) { m_Walk = 0.f; return; }

	m_FaceX = mx / len; m_FaceY = my / len;
	m_Walk += dt;

	float speed = m_Keys[' '] ? 8.4f : 4.6f;   // Space = 달리기
	mx = m_FaceX * speed * dt;
	my = m_FaceY * speed * dt;

	float nx = m_PX + mx;
	if (!m_World.Blocked((int)floorf(nx + 0.5f), (int)floorf(m_PY + 0.5f))) m_PX = nx;
	float ny = m_PY + my;
	if (!m_World.Blocked((int)floorf(m_PX + 0.5f), (int)floorf(ny + 0.5f))) m_PY = ny;

	m_World.EnsureAround(m_PX, m_PY, 3);
}

/* ---------- 음영 ---------- */

void Game::Shade(float wx, float wy, float sx, float sy, float* r, float* g, float* b)
{
	// 등불 — 화면 거리 기준 감쇠
	float lr = 0.f, lg = 0.f, lb = 0.f;
	for (int i = 0; i < m_LightCount; ++i)
	{
		float dx = sx - m_Lights[i].sx, dy = sy - m_Lights[i].sy;
		float d2 = dx * dx + dy * dy;
		const float R2 = 175.f * 175.f;
		if (d2 > R2) continue;
		float f = 1.f - d2 / R2;
		f = f * f * m_Lights[i].str;
		lr += m_Lights[i].r * f; lg += m_Lights[i].g * f; lb += m_Lights[i].b * f;
	}
	*r += lr; *g += lg; *b += lb;

	// 거리 안개
	float dx = wx - m_PX, dy = wy - m_PY;
	float t = Clamp01(sqrtf(dx * dx + dy * dy) / 26.f) * 0.86f;
	*r = Lerp(*r, 0.070f, t);
	*g = Lerp(*g, 0.098f, t);
	*b = Lerp(*b, 0.122f, t);
}

/* ---------- 지면 ---------- */

void Game::DrawGround(int tx, int ty, float sx, float sy)
{
	unsigned char g = m_World.G(tx, ty);
	float v = (m_World.V(tx, ty) / 255.f - 0.5f) * 0.05f;

	float r, gg, b;
	switch (g)
	{
	case G_PATH:  r = 0.255f; gg = 0.225f; b = 0.180f; break;
	case G_DIRT:  r = 0.215f; gg = 0.190f; b = 0.155f; break;
	case G_PLAZA: r = 0.275f; gg = 0.262f; b = 0.235f; break;
	case G_SHORE: r = 0.225f; gg = 0.215f; b = 0.180f; break;
	case G_MARSH: r = 0.120f; gg = 0.155f; b = 0.130f; break;
	case G_WATER:
	{
		float w = sinf(m_Time * 0.8f + (tx + ty) * 0.5f) * 0.016f
			+ sinf(m_Time * 1.7f - (tx - ty) * 0.31f) * 0.010f;
		r = 0.062f + w; gg = 0.118f + w; b = 0.178f + w * 1.7f;
		break;
	}
	default:      r = 0.132f; gg = 0.182f; b = 0.140f; break;
	}
	r += v; gg += v; b += v;

	Shade((float)tx, (float)ty, sx, sy, &r, &gg, &b);
	m_R->Diamond(sx, sy, Iso::TILE_W, Iso::TILE_H, r, gg, b);

	// 물가 흰 거품선
	if (g == G_SHORE && m_World.G(tx, ty + 1) == G_WATER)
	{
		float f = 0.5f + sinf(m_Time * 1.6f + tx * 0.7f) * 0.5f;
		m_R->Diamond(sx, sy - 5.f, Iso::TILE_W * 0.72f, Iso::TILE_H * 0.34f,
			0.42f, 0.48f, 0.50f, 0.10f + f * 0.13f);
	}
}

/* ---------- 오브젝트 ---------- */

void Game::DrawObject(int tx, int ty, float sx, float sy)
{
	unsigned char o = m_World.O(tx, ty);
	if (o == O_NONE) return;

	float v = m_World.V(tx, ty) / 255.f;
	float sway = sinf(m_Time * 0.9f + tx * 0.7f + ty * 0.4f) * 1.7f;

	switch (o)
	{
	case O_TREE:
	{
		float r = 0.115f, g = 0.078f, b = 0.060f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 4.f, 2.f, 22.f + v * 8.f, r, g, b);

		float cr = 0.085f, cg = 0.150f, cb = 0.105f;
		Shade((float)tx, (float)ty, sx, sy + 40.f, &cr, &cg, &cb);
		float base = 28.f + v * 10.f;
		m_R->Diamond(sx + sway, sy + base + 8.f, 62.f, 40.f, cr * 0.82f, cg * 0.82f, cb * 0.82f);
		m_R->Diamond(sx + sway * 1.3f, sy + base + 22.f, 50.f, 34.f, cr, cg, cb);
		m_R->Diamond(sx + sway * 1.6f, sy + base + 34.f, 34.f, 24.f, cr * 1.28f, cg * 1.28f, cb * 1.22f);
		break;
	}
	case O_PINE:
	{
		float r = 0.100f, g = 0.070f, b = 0.055f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 3.f, 1.5f, 14.f, r, g, b);

		float cr = 0.062f, cg = 0.118f, cb = 0.092f;
		Shade((float)tx, (float)ty, sx, sy + 44.f, &cr, &cg, &cb);
		for (int i = 0; i < 4; ++i)
		{
			float w = 46.f - i * 10.f;
			float y = sy + 16.f + i * 15.f;
			float f = 1.f + i * 0.14f;
			m_R->Tri(sx - w * 0.5f + sway * (i * 0.4f), y,
				sx + w * 0.5f + sway * (i * 0.4f), y,
				sx + sway * (i * 0.5f), y + 26.f,
				cr * f, cg * f, cb * f);
		}
		break;
	}
	case O_HOUSE:
	{
		float r = 0.255f, g = 0.232f, b = 0.200f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 32.f, 16.f, 34.f, r, g, b);

		// 지붕 — 능선까지 올라가는 사면
		float rr = 0.235f, rg = 0.108f, rb = 0.086f;
		Shade((float)tx, (float)ty, sx, sy + 50.f, &rr, &rg, &rb);
		float by = sy + 34.f;
		m_R->Tri(sx - 32.f, by, sx, by - 16.f, sx, by + 20.f, rr * 0.72f, rg * 0.72f, rb * 0.72f);
		m_R->Tri(sx - 32.f, by, sx, by + 20.f, sx, by + 16.f, rr * 0.80f, rg * 0.80f, rb * 0.80f);
		m_R->Tri(sx + 32.f, by, sx, by - 16.f, sx, by + 20.f, rr, rg, rb);
		m_R->Tri(sx + 32.f, by, sx, by + 20.f, sx, by + 16.f, rr * 1.10f, rg * 1.10f, rb * 1.10f);

		// 창문 불빛
		float fl = 0.82f + sinf(m_Time * 2.6f + tx * 1.9f + ty) * 0.18f;
		m_R->Rect(sx + 13.f, sy + 12.f, 9.f, 11.f, 0.96f * fl, 0.74f * fl, 0.40f * fl, 0.96f);
		m_R->Rect(sx - 14.f, sy + 12.f, 8.f, 10.f, 0.62f * fl, 0.48f * fl, 0.28f * fl, 0.72f);
		break;
	}
	case O_FENCE:
	{
		float r = 0.190f, g = 0.162f, b = 0.128f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->Quad(sx - 30.f, sy + 2.f, sx - 30.f, sy + 8.f, sx + 30.f, sy + 8.f, sx + 30.f, sy + 2.f, r, g, b);
		m_R->IsoBox(sx - 26.f, sy, 2.5f, 1.2f, 17.f, r, g, b);
		m_R->IsoBox(sx + 26.f, sy, 2.5f, 1.2f, 17.f, r, g, b);
		break;
	}
	case O_WELL:
	{
		float r = 0.235f, g = 0.228f, b = 0.220f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 18.f, 9.f, 15.f, r, g, b);
		m_R->Diamond(sx, sy + 15.f, 26.f, 13.f, 0.030f, 0.042f, 0.055f);
		m_R->IsoBox(sx - 14.f, sy, 2.f, 1.f, 40.f, r * 0.8f, g * 0.8f, b * 0.8f);
		m_R->IsoBox(sx + 14.f, sy, 2.f, 1.f, 40.f, r * 0.8f, g * 0.8f, b * 0.8f);
		m_R->Diamond(sx, sy + 48.f, 48.f, 22.f, 0.180f, 0.092f, 0.072f);
		break;
	}
	case O_ROCK:
	{
		float r = 0.175f, g = 0.180f, b = 0.182f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx - 4.f, sy - 2.f, 11.f + v * 5.f, 6.f, 9.f + v * 7.f, r, g, b);
		m_R->IsoBox(sx + 7.f, sy + 1.f, 7.f, 4.f, 6.f, r * 1.1f, g * 1.1f, b * 1.1f);
		break;
	}
	case O_TUFT:
	case O_FLOWER:
	{
		float r = 0.140f, g = 0.190f, b = 0.130f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		for (int i = 0; i < 4; ++i)
		{
			float ox = -7.f + i * 4.5f;
			float hh = 6.f + ((tx * 3 + ty + i) % 4) * 2.f;
			m_R->Quad(sx + ox, sy, sx + ox + sway * 0.5f, sy + hh,
				sx + ox + 1.6f + sway * 0.5f, sy + hh, sx + ox + 1.6f, sy, r, g, b);
		}
		if (o == O_FLOWER)
			m_R->Rect(sx + 1.f, sy + 11.f, 3.f, 3.f, 0.66f, 0.58f, 0.30f, 0.9f);
		break;
	}
	case O_REED:
	{
		float r = 0.155f, g = 0.165f, b = 0.115f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		for (int i = 0; i < 5; ++i)
		{
			float ox = -10.f + i * 5.f;
			float hh = 14.f + ((tx + ty * 2 + i) % 5) * 4.f;
			m_R->Quad(sx + ox, sy, sx + ox + sway * 1.6f, sy + hh,
				sx + ox + 1.4f + sway * 1.6f, sy + hh, sx + ox + 1.4f, sy, r, g, b);
		}
		break;
	}
	case O_STUMP:
	{
		float r = 0.150f, g = 0.108f, b = 0.078f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 12.f, 6.f, 8.f, r, g, b);
		break;
	}
	case O_CRATE:
	{
		float r = 0.170f, g = 0.135f, b = 0.098f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 11.f, 5.5f, 13.f, r, g, b);
		break;
	}
	case O_LAMP:
	{
		float r = 0.145f, g = 0.135f, b = 0.125f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 2.5f, 1.2f, 46.f, r, g, b);
		float fl = 0.86f + sinf(m_Time * 3.1f + tx) * 0.14f;
		// 광원 헤일로
		for (int i = 4; i >= 1; --i)
			m_R->Diamond(sx, sy + 50.f, 34.f * i, 22.f * i,
				0.98f, 0.76f, 0.42f, 0.032f * fl / i);
		m_R->Rect(sx, sy + 50.f, 9.f, 11.f, 1.f * fl, 0.84f * fl, 0.50f * fl, 1.f);
		break;
	}
	case O_RUIN:
	{
		float r = 0.185f, g = 0.180f, b = 0.172f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->Quad(sx - 22.f, sy - 2.f, sx - 26.f, sy + 6.f, sx - 2.f, sy + 14.f, sx + 3.f, sy + 4.f, r, g, b);
		m_R->IsoBox(sx + 10.f, sy + 2.f, 8.f, 4.f, 20.f, r * 0.9f, g * 0.9f, b * 0.9f);
		// 장부 조각 — 붉은 표식
		float p = 0.45f + sinf(m_Time * 2.2f + tx) * 0.35f;
		for (int i = 3; i >= 1; --i)
			m_R->Diamond(sx, sy + 20.f, 16.f * i, 11.f * i, 0.72f, 0.20f, 0.14f, 0.045f * p / i);
		m_R->Rect(sx, sy + 20.f, 7.f, 9.f, 0.70f, 0.24f, 0.18f, 0.55f + p * 0.4f);
		break;
	}
	default: break;
	}
}

/* ---------- 캐릭터 ---------- */

void Game::DrawNpc(const Npc& n)
{
	float sx, sy;
	Iso::WorldToScreen(n.hx, n.hy, 0.f, &sx, &sy);
	sx -= m_CamSX; sy -= m_CamSY;

	float bob = sinf(m_Time * 1.5f + n.phase) * 1.5f;
	m_R->Diamond(sx, sy, 24.f, 12.f, 0.f, 0.f, 0.f, 0.32f);

	float r = n.r, g = n.g, b = n.b;
	Shade(n.hx, n.hy, sx, sy, &r, &g, &b);
	// 외투 실루엣
	m_R->Quad(sx - 7.f, sy + 1.f, sx - 6.f, sy + 22.f + bob, sx + 6.f, sy + 22.f + bob, sx + 7.f, sy + 1.f,
		r * 0.42f, g * 0.42f, b * 0.46f);
	m_R->Rect(sx, sy + 27.f + bob, 10.f, 10.f, r, g, b);

	if (n.recordId >= 0 && !m_Recorded[n.recordId])
	{
		float p = 0.5f + sinf(m_Time * 3.f) * 0.4f;
		m_R->Diamond(sx, sy + 46.f + bob, 13.f, 13.f, 0.76f, 0.22f, 0.16f, p);
	}

	float dx = n.hx - m_PX, dy = n.hy - m_PY;
	bool isNear = sqrtf(dx * dx + dy * dy) < 1.9f;
	float nameA = isNear ? 1.f : 0.55f;
	float w = m_F.Measure(n.name);
	m_F.DrawShadowed(sx - w * 0.5f, sy - 6.f, n.name, 0.74f, 0.72f, 0.66f, nameA);
	if (isNear) m_F.DrawShadowed(sx - 8.f, sy + 64.f + bob, "E", 0.96f, 0.86f, 0.56f);
}

void Game::DrawPlayer()
{
	float sx, sy;
	Iso::WorldToScreen(m_PX, m_PY, 0.f, &sx, &sy);
	sx -= m_CamSX; sy -= m_CamSY;

	float step = (m_Walk > 0.f) ? sinf(m_Walk * 11.f) * 2.2f : 0.f;
	m_R->Diamond(sx, sy, 26.f, 13.f, 0.f, 0.f, 0.f, 0.40f);

	// 서기의 외투
	m_R->Quad(sx - 8.f, sy, sx - 7.f, sy + 24.f + step, sx + 7.f, sy + 24.f + step, sx + 8.f, sy,
		0.128f, 0.122f, 0.148f);
	m_R->Quad(sx - 8.f, sy + 10.f, sx - 7.f, sy + 24.f + step, sx + 0.f, sy + 24.f + step, sx + 0.f, sy + 10.f,
		0.088f, 0.084f, 0.108f);
	m_R->Rect(sx, sy + 29.f + step, 11.f, 11.f, 0.78f, 0.72f, 0.62f);
	// 허리의 장부
	m_R->Rect(sx + 9.f, sy + 12.f + step, 6.f, 9.f, 0.46f, 0.14f, 0.10f);
}

/* ---------- 분위기 ---------- */

void Game::DrawMotes()
{
	for (int i = 0; i < 170; ++i)
		m_R->Rect(m_Motes[i].x, m_Motes[i].y, m_Motes[i].s, m_Motes[i].s,
			0.62f, 0.68f, 0.66f, m_Motes[i].a);
}

void Game::DrawVignette()
{
	const int N = 12;
	float hw = m_W * 0.5f, hh = m_H * 0.5f;
	for (int i = 0; i < N; ++i)
	{
		float a = 0.048f * (float)(N - i) / N;
		float band = 30.f;
		float off = hh - band * 0.5f - i * band * 0.7f;
		m_R->Rect(0.f, off, (float)m_W, band, 0.f, 0.f, 0.f, a);
		m_R->Rect(0.f, -off, (float)m_W, band, 0.f, 0.f, 0.f, a);
		float ox = hw - band * 0.5f - i * band * 0.7f;
		m_R->Rect(ox, 0.f, band, (float)m_H, 0.f, 0.f, 0.f, a);
		m_R->Rect(-ox, 0.f, band, (float)m_H, 0.f, 0.f, 0.f, a);
	}
}

/* ---------- HUD ---------- */

void Game::DrawHud()
{
	float hw = m_W * 0.5f, hh = m_H * 0.5f;
	char buf[256];

	// 좌상단 상태
	m_R->Rect(-hw + 150.f, hh - 44.f, 292.f, 62.f, 0.035f, 0.048f, 0.050f, 0.62f);
	sprintf_s(buf, "「미납」  기록 %d / 3", m_RecordCount);
	m_F.DrawShadowed(-hw + 22.f, hh - 20.f, buf, 0.86f, 0.80f, 0.70f);
	sprintf_s(buf, "장부 조각 %d        %02d:%02d", m_Fragments, (int)m_Elapsed / 60, (int)m_Elapsed % 60);
	m_F.DrawShadowed(-hw + 22.f, hh - 44.f, buf, 0.56f, 0.58f, 0.55f);

	m_F.DrawShadowed(-hw + 22.f, -hh + 34.f,
		"WASD 이동   Space 달리기   E 상호작용   Tab 장부   Esc 종료", 0.42f, 0.46f, 0.45f);

	// 알림
	if (m_ToastTimer > 0.f && m_State != GS_DIALOG)
	{
		float a = m_ToastTimer > 1.f ? 1.f : m_ToastTimer;
		float w = m_F.Measure(m_Toast);
		m_R->Rect(0.f, hh - 96.f, w + 40.f, 38.f, 0.035f, 0.048f, 0.050f, 0.80f * a);
		m_R->Rect(0.f, hh - 115.f, w + 40.f, 1.5f, 0.52f, 0.18f, 0.13f, a);
		m_F.DrawShadowed(-w * 0.5f, hh - 86.f, m_Toast, 0.84f, 0.79f, 0.70f, a);
	}

	// 대화
	if (m_State == GS_DIALOG)
	{
		float bw = (float)m_W - 160.f, bh = 116.f;
		float cy = -hh + 96.f;
		m_R->Rect(0.f, cy, bw, bh, 0.030f, 0.042f, 0.044f, 0.94f);
		m_R->Rect(0.f, cy + bh * 0.5f, bw, 2.f, 0.52f, 0.18f, 0.13f, 1.f);
		m_R->Rect(0.f, cy - bh * 0.5f, bw, 1.f, 0.22f, 0.24f, 0.23f, 1.f);

		const char* who = m_World.npcs[m_TalkingTo].name;
		m_FB.DrawShadowed(-bw * 0.5f + 24.f, cy + bh * 0.5f - 14.f, who, 0.82f, 0.44f, 0.32f);
		m_F.DrawShadowed(-bw * 0.5f + 24.f, cy + 14.f, m_Lines[m_LineIdx], 0.88f, 0.86f, 0.80f);

		sprintf_s(buf, "%d / %d      [E] 계속", m_LineIdx + 1, m_LineCount);
		float w = m_F.Measure(buf);
		m_F.DrawShadowed(bw * 0.5f - 24.f - w, cy - bh * 0.5f + 26.f, buf, 0.48f, 0.50f, 0.48f);
	}

	// 장부 패널
	if (m_LedgerOpen)
	{
		m_R->Rect(0.f, 0.f, 620.f, 268.f, 0.032f, 0.044f, 0.046f, 0.96f);
		m_R->Rect(0.f, 100.f, 620.f, 2.f, 0.52f, 0.18f, 0.13f, 1.f);
		m_FB.DrawShadowed(-286.f, 124.f, "장부  ·  미납 항목", 0.84f, 0.46f, 0.34f);
		for (int i = 0; i < 3; ++i)
		{
			if (m_Recorded[i])
				m_F.DrawShadowed(-286.f, 68.f - i * 30.f, RECORD_SHORT[i], 0.82f, 0.78f, 0.70f);
			else
				m_F.DrawShadowed(-286.f, 68.f - i * 30.f, "----  아직 기입되지 않음", 0.34f, 0.36f, 0.35f);
		}
		char buf2[128];
		sprintf_s(buf2, "들에서 주운 장부 조각 : %d / %d", m_Fragments, FIELD_RECORD_COUNT);
		m_F.DrawShadowed(-286.f, -40.f, buf2, 0.60f, 0.62f, 0.58f);
		m_F.DrawShadowed(-286.f, -78.f,
			m_RecordCount >= 3 ? "광장의 촌장에게 보고하라." : "무엇이 빠졌는지 찾아라.",
			0.56f, 0.58f, 0.56f);
		sprintf_s(buf2, "불러온 청크 %d", m_World.LoadedChunks());
		m_F.DrawShadowed(-286.f, -110.f, buf2, 0.30f, 0.33f, 0.32f);
	}

	// 종료
	if (m_State == GS_END)
	{
		float a = Clamp01(m_EndTimer * 0.4f) * 0.86f;
		m_R->Rect(0.f, 0.f, (float)m_W, (float)m_H, 0.f, 0.f, 0.f, a);
		if (m_EndTimer > 1.0f)
		{
			float t = Clamp01((m_EndTimer - 1.f) * 0.8f);
			m_FB.DrawShadowed(-48.f, 40.f, "「미납」", 0.86f, 0.82f, 0.74f, t);
			m_F.DrawShadowed(-290.f, -4.f,
				"세 건이 접수되었다. 촌장은 당신의 이름을 적으려 했고,", 0.72f, 0.70f, 0.66f, t);
			m_F.DrawShadowed(-290.f, -28.f,
				"그 칸은 비어 있는 채로 남았다.", 0.72f, 0.70f, 0.66f, t);
			if (m_EndTimer > 2.6f)
			{
				float t2 = Clamp01((m_EndTimer - 2.6f) * 0.8f);
				m_F.DrawShadowed(-290.f, -74.f,
					"결손 : 이 세계에서 붉은색이 빠져나갔다.", 0.82f, 0.52f, 0.42f, t2);
				m_F.DrawShadowed(-290.f, -112.f,
					"튜토리얼 레벨 종료  ·  Esc 로 종료", 0.44f, 0.46f, 0.45f, t2);
			}
		}
	}
}

/* ---------- 렌더 ---------- */

void Game::Render()
{
	m_R->BeginFrame(0.045f, 0.068f, 0.080f);

	// 가시 범위 — 화면 네 꼭짓점을 역변환
	float hw = m_W * 0.5f + Iso::TILE_W * 2.f;
	float hh = m_H * 0.5f + Iso::TILE_H * 5.f;
	float ccx[4] = { -hw,  hw, -hw,  hw };
	float ccy[4] = { -hh, -hh,  hh,  hh };
	int minX = 1 << 29, maxX = -(1 << 29), minY = 1 << 29, maxY = -(1 << 29);
	for (int i = 0; i < 4; ++i)
	{
		float wx, wy;
		Iso::ScreenToWorld(ccx[i] + m_CamSX, ccy[i] + m_CamSY, &wx, &wy);
		int ix = (int)floorf(wx), iy = (int)floorf(wy);
		if (ix - 1 < minX) minX = ix - 1;
		if (ix + 2 > maxX) maxX = ix + 2;
		if (iy - 1 < minY) minY = iy - 1;
		if (iy + 2 > maxY) maxY = iy + 2;
	}

	// 1차: 광원 수집 (등불·창문)
	m_LightCount = 0;
	for (int ty = minY; ty <= maxY && m_LightCount < 96; ++ty)
	{
		for (int tx = minX; tx <= maxX && m_LightCount < 96; ++tx)
		{
			unsigned char o = m_World.O(tx, ty);
			if (o != O_LAMP && o != O_HOUSE) continue;
			float sx, sy;
			Iso::WorldToScreen((float)tx, (float)ty, 0.f, &sx, &sy);
			Light& L = m_Lights[m_LightCount++];
			L.sx = sx - m_CamSX;
			L.sy = sy - m_CamSY + (o == O_LAMP ? 50.f : 12.f);
			L.r = 0.34f; L.g = 0.23f; L.b = 0.10f;
			L.str = (o == O_LAMP) ? 1.f : 0.55f;
		}
	}

	// 2차: 화가 알고리즘 — depth(=tx+ty) 오름차순
	for (int s = minX + minY; s <= maxX + maxY; ++s)
	{
		for (int tx = minX; tx <= maxX; ++tx)
		{
			int ty = s - tx;
			if (ty < minY || ty > maxY) continue;
			float sx, sy;
			Iso::WorldToScreen((float)tx, (float)ty, 0.f, &sx, &sy);
			sx -= m_CamSX; sy -= m_CamSY;
			DrawGround(tx, ty, sx, sy);
		}
		for (int tx = minX; tx <= maxX; ++tx)
		{
			int ty = s - tx;
			if (ty < minY || ty > maxY) continue;
			float sx, sy;
			Iso::WorldToScreen((float)tx, (float)ty, 0.f, &sx, &sy);
			sx -= m_CamSX; sy -= m_CamSY;
			DrawObject(tx, ty, sx, sy);
		}
		for (int i = 0; i < NPC_COUNT; ++i)
			if ((int)floorf(m_World.npcs[i].hx + m_World.npcs[i].hy) == s) DrawNpc(m_World.npcs[i]);
		if ((int)floorf(m_PX + m_PY) == s) DrawPlayer();
	}

	DrawMotes();
	DrawVignette();
	DrawHud();

	m_R->EndFrame();
}
