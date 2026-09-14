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

// 안개 색 (황혼)
static const float FOG_R = 0.062f, FOG_G = 0.086f, FOG_B = 0.112f;

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
static float Frand() { return (rand() % 10000) / 10000.f; }
static unsigned int Hsh(int x, int y, int s)
{
	unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + s * 144269);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

void Game::ToScreen(float wx, float wy, float* sx, float* sy)
{
	Iso::WorldToScreen(wx, wy, 0.f, sx, sy);
	*sx -= m_CamSX; *sy -= m_CamSY;
}

void Game::Init(Renderer* renderer, int windowW, int windowH)
{
	m_R = renderer;
	m_W = windowW;
	m_H = windowH;
	m_World.Init();
	m_World.EnsureAround(m_PX, m_PY, 3);
	m_Beasts.Init(m_World, m_PX, m_PY);

	m_F.Init(m_R, 17);
	m_FB.Init(m_R, 27);

	m_PlayerStyle = MakeStyle(0.26f, 0.24f, 0.30f, 1, 3, false, 7u);
	m_PlayerStyle.cloakOn = true;
	m_PlayerStyle.accent[0] = 0.52f; m_PlayerStyle.accent[1] = 0.14f; m_PlayerStyle.accent[2] = 0.10f;

	for (int i = 0; i < NPC_COUNT; ++i)
	{
		const Npc& n = m_World.npcs[i];
		m_NpcStyle[i] = MakeStyle(n.r, n.g, n.b, n.build, n.hat, n.child, (unsigned int)(i * 31 + 5));
	}

	m_CamWX = m_PX; m_CamWY = m_PY;

	for (int i = 0; i < 190; ++i)
	{
		m_Motes[i].x = (Frand() - 0.5f) * m_W;
		m_Motes[i].y = (Frand() - 0.5f) * m_H;
		m_Motes[i].vx = (Frand() - 0.5f) * 10.f;
		m_Motes[i].vy = 3.f + Frand() * 8.f;
		m_Motes[i].a = 0.06f + Frand() * 0.20f;
		m_Motes[i].s = 1.f + Frand() * 2.4f;
	}
	for (int i = 0; i < 32; ++i) m_Puffs[i].life = 0.f;
	for (int i = 0; i < TC * TC; ++i) m_ToneStamp[i] = -1;

	Toast("촌장에게 말을 걸어라.   [E]");
}

void Game::Shutdown() { m_F.Shutdown(); m_FB.Shutdown(); }

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

void Game::Toast(const char* msg) { sprintf_s(m_Toast, "%s", msg); m_ToastTimer = 5.5f; }

void Game::TryInteract()
{
	int best = -1;
	float bestD = 2.0f;
	for (int i = 0; i < NPC_COUNT; ++i)
	{
		float dx = m_World.npcs[i].hx - m_PX, dy = m_World.npcs[i].hy - m_PY;
		float d = sqrtf(dx * dx + dy * dy);
		if (d < bestD) { bestD = d; best = i; }
	}
	if (best >= 0)
	{
		const Npc& n = m_World.npcs[best];
		m_TalkingTo = best; m_LineIdx = 0;
		if (n.isElder)
			m_Lines = (m_RecordCount >= 3) ? World_ElderDone(&m_LineCount) : World_ElderIntro(&m_LineCount);
		else { m_Lines = n.lines; m_LineCount = n.lineCount; }
		m_State = GS_DIALOG;
		return;
	}
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx)
		{
			int tx = (int)floorf(m_PX + 0.5f) + dx, ty = (int)floorf(m_PY + 0.5f) + dy, idx = 0;
			if (m_World.TakeRuin(tx, ty, &idx))
			{
				if (!m_FoundFragment[idx]) { m_FoundFragment[idx] = true; ++m_Fragments; }
				Toast(World_FieldRecord(idx));
				return;
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
		if (m_RecordCount >= 3) Toast("세 건 모두 기입되었다. 촌장에게 돌아가라.");
		else Toast(RECORD_KR[n.recordId]);
	}
	else if (n.isElder && m_RecordCount >= 3 && !m_Reported)
	{
		m_Reported = true; m_State = GS_END; m_EndTimer = 0.f;
		if (m_R) m_R->SetColorFilter(0.18f, 0.96f, 1.f);
		return;
	}
	m_State = GS_PLAY;
	m_TalkingTo = -1;
}

void Game::SpawnPuff(float wx, float wy, int type)
{
	for (int i = 0; i < 32; ++i)
		if (m_Puffs[i].life <= 0.f)
		{
			m_Puffs[i].x = wx; m_Puffs[i].y = wy;
			m_Puffs[i].t = 0.f;
			m_Puffs[i].life = (type == 0) ? 0.55f : 1.5f;
			m_Puffs[i].type = type;
			return;
		}
}

/* ---------- 갱신 ---------- */

void Game::Update(float dt)
{
	m_Time += dt;
	if (m_R) m_R->SetTime(m_Time);
	m_FpsAcc += dt; ++m_FpsFrames;
	if (m_FpsAcc >= 0.4f) { m_Fps = m_FpsFrames / m_FpsAcc; m_FpsAcc = 0.f; m_FpsFrames = 0; }
	if (m_State == GS_END) m_EndTimer += dt; else m_Elapsed += dt;
	if (m_ToastTimer > 0.f) m_ToastTimer -= dt;

	m_CamWX = Lerp(m_CamWX, m_PX, Clamp01(dt * 6.5f));
	m_CamWY = Lerp(m_CamWY, m_PY, Clamp01(dt * 6.5f));
	Iso::WorldToScreen(m_CamWX, m_CamWY, 0.f, &m_CamSX, &m_CamSY);

	for (int i = 0; i < 190; ++i)
	{
		m_Motes[i].x += m_Motes[i].vx * dt;
		m_Motes[i].y += m_Motes[i].vy * dt;
		if (m_Motes[i].y > m_H * 0.5f) { m_Motes[i].y = -m_H * 0.5f; m_Motes[i].x = (Frand() - 0.5f) * m_W; }
		if (m_Motes[i].x > m_W * 0.5f) m_Motes[i].x = -m_W * 0.5f;
		if (m_Motes[i].x < -m_W * 0.5f) m_Motes[i].x = m_W * 0.5f;
	}
	for (int i = 0; i < 32; ++i)
		if (m_Puffs[i].life > 0.f) { m_Puffs[i].t += dt; if (m_Puffs[i].t >= m_Puffs[i].life) m_Puffs[i].life = 0.f; }

	m_Beasts.Update(dt, m_World, m_PX, m_PY);

	if (m_State != GS_PLAY) { m_Moving = false; return; }

	float mx = 0.f, my = 0.f;
	if (m_Keys['w']) { mx -= 1.f; my -= 1.f; }
	if (m_Keys['s']) { mx += 1.f; my += 1.f; }
	if (m_Keys['a']) { mx -= 1.f; my += 1.f; }
	if (m_Keys['d']) { mx += 1.f; my -= 1.f; }

	float len = sqrtf(mx * mx + my * my);
	m_Moving = (len > 0.001f);
	if (!m_Moving) return;

	m_FaceX = mx / len; m_FaceY = my / len;
	m_Walk += dt;

	bool run = m_Keys[' '] != 0;
	float speed = run ? 8.6f : 4.7f;
	float nx = m_PX + m_FaceX * speed * dt;
	float ny = m_PY + m_FaceY * speed * dt;

	if (!m_World.Blocked((int)floorf(nx + 0.5f), (int)floorf(m_PY + 0.5f))) m_PX = nx;
	if (!m_World.Blocked((int)floorf(m_PX + 0.5f), (int)floorf(ny + 0.5f))) m_PY = ny;

	// 발자국 먼지 / 물가 물결
	m_StepTimer -= dt;
	if (m_StepTimer <= 0.f)
	{
		m_StepTimer = run ? 0.16f : 0.28f;
		int tx = (int)floorf(m_PX + 0.5f), ty = (int)floorf(m_PY + 0.5f);
		unsigned char g = m_World.G(tx, ty);
		bool nearWater = (m_World.G(tx + 1, ty) == G_WATER || m_World.G(tx, ty + 1) == G_WATER ||
			m_World.G(tx - 1, ty) == G_WATER || m_World.G(tx, ty - 1) == G_WATER);
		if (g == G_SHORE && nearWater) SpawnPuff(m_PX, m_PY, 1);
		else if (g == G_PATH || g == G_DIRT || g == G_PLAZA) SpawnPuff(m_PX, m_PY, 0);
	}

	m_World.EnsureAround(m_PX, m_PY, 3);
}

/* ---------- 음영 ---------- */

float Game::FogT(float wx, float wy)
{
	float dx = wx - m_PX, dy = wy - m_PY;
	return Clamp01(sqrtf(dx * dx + dy * dy) / 30.f) * 0.80f;
}

void Game::LightAt(float sx, float sy, float* lr, float* lg, float* lb)
{
	float r = 0.f, g = 0.f, b = 0.f;
	for (int i = 0; i < m_LightCount; ++i)
	{
		float dx = sx - m_Lights[i].sx, dy = sy - m_Lights[i].sy;
		float d2 = dx * dx + dy * dy;
		const float R2 = 168.f * 168.f;
		if (d2 > R2) continue;
		float f = 1.f - d2 / R2;
		f = f * f * m_Lights[i].str;
		r += m_Lights[i].r * f; g += m_Lights[i].g * f; b += m_Lights[i].b * f;
	}
	*lr = r; *lg = g; *lb = b;
}

void Game::Shade(float wx, float wy, float sx, float sy, float* r, float* g, float* b)
{
	float lr, lg, lb;
	LightAt(sx, sy, &lr, &lg, &lb);
	float t = FogT(wx, wy);
	*r = Lerp(*r, FOG_R, t) + lr;
	*g = Lerp(*g, FOG_G, t) + lg;
	*b = Lerp(*b, FOG_B, t) + lb;
}

/* ---------- 지면 ---------- */

// 꼭짓점 (i+0.5, j+0.5) 의 지면 톤. 인접 네 타일이 공유하므로 프레임당 한 번만 계산한다.
float Game::ToneAt(int i, int j)
{
	int gi = i - m_TCX0, gj = j - m_TCY0;
	if (gi < 0 || gj < 0 || gi >= TC || gj >= TC)
		return World_GroundTone(i + 0.5f, j + 0.5f);
	int k = gj * TC + gi;
	if (m_ToneStamp[k] != m_Frame)
	{
		m_ToneStamp[k] = m_Frame;
		m_ToneCache[k] = World_GroundTone(i + 0.5f, j + 0.5f);
	}
	return m_ToneCache[k];
}

void Game::DrawGround(int tx, int ty, float sx, float sy, float lr, float lg, float lb)
{
	unsigned char g = m_World.G(tx, ty);

	float r, gg, b;
	switch (g)
	{
	case G_PATH:  r = 0.238f; gg = 0.206f; b = 0.164f; break;
	case G_DIRT:  r = 0.198f; gg = 0.172f; b = 0.140f; break;
	case G_PLAZA: r = 0.252f; gg = 0.240f; b = 0.216f; break;
	case G_SHORE: r = 0.214f; gg = 0.202f; b = 0.168f; break;
	case G_MARSH: r = 0.106f; gg = 0.138f; b = 0.116f; break;
	case G_WATER: r = 0.048f; gg = 0.098f; b = 0.156f; break;
	default:      r = 0.116f; gg = 0.162f; b = 0.124f; break;
	}

	// 안개는 타일 중심 한 번, 조명은 호출자가 이미 계산해 넘겨준다
	float t = FogT((float)tx, (float)ty);

	// 꼭짓점 색만 따로 — 인접 타일과 값을 공유하므로 경계가 사라진다
	static const int CI[4] = { 0, -1, -1,  0 };
	static const int CJ[4] = { 0,  0, -1, -1 };
	float c[16];
	for (int i = 0; i < 4; ++i)
	{
		float k = 0.80f + ToneAt(tx + CI[i], ty + CJ[i]) * 0.42f;
		float rr = r * k, rg = gg * k, rb = b * k;
		if (g == G_WATER)
		{
			float wx = tx + CI[i] + 0.5f, wy = ty + CJ[i] + 0.5f;
			float w = sinf(m_Time * 0.85f + (wx + wy) * 0.52f) * 0.020f
				+ sinf(m_Time * 1.75f - (wx - wy) * 0.33f) * 0.013f;
			rr += w; rg += w * 1.15f; rb += w * 1.9f;
		}
		c[i * 4 + 0] = Lerp(rr, FOG_R, t) + lr;
		c[i * 4 + 1] = Lerp(rg, FOG_G, t) + lg;
		c[i * 4 + 2] = Lerp(rb, FOG_B, t) + lb;
		c[i * 4 + 3] = 1.f;
	}
	m_R->Diamond4(sx, sy, Iso::TILE_W, Iso::TILE_H, c);
}

void Game::DrawGroundDetail(int tx, int ty, float sx, float sy)
{
	unsigned char g = m_World.G(tx, ty);
	unsigned int h = Hsh(tx, ty, 17);
	float t = FogT((float)tx, (float)ty);
	if (t > 0.52f) return;                     // 멀리 있는 디테일은 생략
	float fade = 1.f - t;

	float lr = 0.f, lg = 0.f, lb = 0.f;
	if (t < 0.34f) LightAt(sx, sy, &lr, &lg, &lb);
	float sway = sinf(m_Time * 1.1f + tx * 0.6f + ty * 0.35f) * 1.3f;

	if (g == G_GRASS || g == G_MARSH)
	{
		int n = (h % 100 < 46) ? 3 : 1;
		for (int i = 0; i < n; ++i)
		{
			unsigned int hh = Hsh(tx, ty, 31 + i);
			float ox = ((hh % 100) / 100.f - 0.5f) * 40.f;
			float oy = (((hh >> 8) % 100) / 100.f - 0.5f) * 18.f;
			float len = 4.f + ((hh >> 16) % 5);
			float br = (0.126f + ((hh >> 4) % 30) * 0.0016f) * fade;
			m_R->Quad(sx + ox, sy + oy, sx + ox + sway * 0.6f, sy + oy + len,
				sx + ox + 1.3f + sway * 0.6f, sy + oy + len, sx + ox + 1.3f, sy + oy,
				br * 1.15f + lr * 0.5f, br * 1.55f + lg * 0.5f, br * 1.05f + lb * 0.5f, 0.95f);
		}
	}
	else if (g == G_PATH || g == G_DIRT)
	{
		// 바퀴 자국
		if (g == G_PATH)
		{
			float d = 0.020f * fade;
			m_R->Quad(sx - 26.f, sy - 5.f, sx - 4.f, sy + 6.f, sx - 2.f, sy + 5.f, sx - 24.f, sy - 6.f,
				-d, -d, -d, 0.30f);
			m_R->Quad(sx + 4.f, sy - 8.f, sx + 26.f, sy + 3.f, sx + 28.f, sy + 2.f, sx + 6.f, sy - 9.f,
				-d, -d, -d, 0.30f);
		}
		// 자갈
		int n = 2 + (h % 2);
		for (int i = 0; i < n; ++i)
		{
			unsigned int hh = Hsh(tx, ty, 71 + i);
			float ox = ((hh % 100) / 100.f - 0.5f) * 44.f;
			float oy = (((hh >> 8) % 100) / 100.f - 0.5f) * 20.f;
			float s = 1.4f + ((hh >> 16) % 3) * 0.7f;
			float br = (0.30f + ((hh >> 5) % 20) * 0.004f) * fade;
			m_R->Ellipse(sx + ox, sy + oy, s, s * 0.55f, 6,
				br + lr * 0.6f, br * 0.94f + lg * 0.6f, br * 0.84f + lb * 0.6f, 0.55f);
		}
	}
	else if (g == G_PLAZA)
	{
		// 포석 이음선
		float d = 0.016f * fade;
		m_R->Quad(sx - 32.f, sy, sx, sy + 16.f, sx + 1.f, sy + 15.f, sx - 31.f, sy - 1.f, -d, -d, -d, 0.34f);
		m_R->Quad(sx, sy - 16.f, sx + 32.f, sy, sx + 31.f, sy + 1.f, sx - 1.f, sy - 15.f, -d, -d, -d, 0.28f);
	}
	else if (g == G_SHORE)
	{
		int n = 2 + (h % 3);
		for (int i = 0; i < n; ++i)
		{
			unsigned int hh = Hsh(tx, ty, 91 + i);
			float ox = ((hh % 100) / 100.f - 0.5f) * 40.f;
			float oy = (((hh >> 8) % 100) / 100.f - 0.5f) * 18.f;
			float s = 1.8f + ((hh >> 16) % 4) * 0.8f;
			m_R->Ellipse(sx + ox, sy + oy, s, s * 0.5f, 7,
				0.26f * fade + lr * 0.5f, 0.25f * fade + lg * 0.5f, 0.22f * fade + lb * 0.5f, 0.6f);
		}
		if (m_World.G(tx, ty + 1) == G_WATER)
		{
			float f = 0.5f + sinf(m_Time * 1.7f + tx * 0.8f) * 0.5f;
			m_R->Diamond(sx, sy - 6.f, Iso::TILE_W * 0.78f, Iso::TILE_H * 0.30f,
				0.40f, 0.46f, 0.48f, (0.09f + f * 0.13f) * fade);
		}
	}
	else if (g == G_WATER)
	{
		// 물비늘 반짝임
		float ph = ((h % 100) / 100.f) * 6.2831853f;
		float sp = sinf(m_Time * 2.2f + ph);
		if (sp > 0.86f)
			m_R->Ellipse(sx + ((h >> 9) % 30) - 15.f, sy + ((h >> 13) % 12) - 6.f,
				2.6f, 1.2f, 6, 1.5f, 1.6f, 1.7f, (sp - 0.86f) * 5.f * fade);
		// 옅은 안개 띠
		if ((h % 7) == 0)
		{
			float dr = sinf(m_Time * 0.35f + tx * 0.4f) * 8.f;
			m_R->Ellipse(sx + dr, sy + 4.f, 30.f, 7.f, 10, 0.30f, 0.36f, 0.40f, 0.055f * fade);
		}
	}
}

/* ---------- 그림자 ---------- */

void Game::DrawShadow(int tx, int ty, float sx, float sy)
{
	unsigned char o = m_World.O(tx, ty);
	if (o == O_NONE) return;
	float t = FogT((float)tx, (float)ty);
	if (t > 0.58f) return;
	float a = (1.f - t) * 0.9f;

	// 광원은 좌상단에 있다고 가정 — 그림자는 우하단으로 눕는다
	switch (o)
	{
	case O_TREE:  m_R->SoftShadow(sx + 15.f, sy - 6.f, 21.f, 9.5f, a * 0.72f); break;
	case O_PINE:  m_R->SoftShadow(sx + 12.f, sy - 5.f, 15.f, 7.f, a * 0.72f); break;
	case O_HOUSE: m_R->SoftShadow(sx + 24.f, sy - 9.f, 34.f, 15.f, a * 0.80f); break;
	case O_WELL:  m_R->SoftShadow(sx + 10.f, sy - 4.f, 16.f, 8.f, a * 0.66f); break;
	case O_ROCK:  m_R->SoftShadow(sx + 7.f, sy - 3.f, 11.f, 5.f, a * 0.60f); break;
	case O_STUMP:
	case O_CRATE: m_R->SoftShadow(sx + 7.f, sy - 3.f, 11.f, 5.f, a * 0.56f); break;
	case O_FENCE: m_R->SoftShadow(sx + 8.f, sy - 3.f, 24.f, 5.f, a * 0.40f); break;
	case O_LAMP:  m_R->SoftShadow(sx + 4.f, sy - 2.f, 5.f, 3.f, a * 0.40f); break;
	case O_RUIN:  m_R->SoftShadow(sx + 10.f, sy - 4.f, 18.f, 8.f, a * 0.50f); break;
	default: break;
	}
}

/* ---------- 집 (재질) ---------- */

void Game::DrawHouse(int tx, int ty, float sx, float sy)
{
	const float HW = 32.f, HH = 16.f;
	float lr, lg, lb; LightAt(sx, sy + 20.f, &lr, &lg, &lb);
	float t = FogT((float)tx, (float)ty);
	unsigned int h = Hsh(tx, ty, 5);

	// 기초 석재
	float st[3] = { 0.166f, 0.160f, 0.152f };
	Shade((float)tx, (float)ty, sx, sy, &st[0], &st[1], &st[2]);
	m_R->IsoBox(sx, sy, HW, HH, 9.f, st[0], st[1], st[2]);
	// 석재 이음
	for (int i = 0; i < 5; ++i)
	{
		float u = -0.85f + i * 0.42f;
		m_R->Quad(sx + u * HW, sy + HH * (1.f - fabsf(u)) * 0.0f + 1.f,
			sx + u * HW + 1.2f, sy + 2.f, sx + u * HW + 1.2f, sy + 8.f,
			sx + u * HW, sy + 7.f, 0.f, 0.f, 0.f, 0.22f * (1.f - t));
	}

	// 회벽
	float wl[3] = { 0.262f, 0.246f, 0.216f };
	Shade((float)tx, (float)ty, sx, sy + 20.f, &wl[0], &wl[1], &wl[2]);
	float y0 = 9.f, y1 = 38.f;
	// 좌면
	m_R->Quad(sx - HW, sy + y0, sx, sy - HH + y0, sx, sy - HH + y1, sx - HW, sy + y1,
		wl[0] * 0.60f, wl[1] * 0.60f, wl[2] * 0.64f);
	// 우면
	m_R->Quad(sx, sy - HH + y0, sx + HW, sy + y0, sx + HW, sy + y1, sx, sy - HH + y1,
		wl[0], wl[1], wl[2]);

	// 목골조 — 기둥과 사재
	float bm[3] = { 0.112f, 0.082f, 0.062f };
	Shade((float)tx, (float)ty, sx, sy + 22.f, &bm[0], &bm[1], &bm[2]);
	for (int i = 0; i <= 3; ++i)
	{
		float u = i / 3.f;
		float bx = Lerp(sx - HW, sx, u), by = Lerp(sy + y0, sy - HH + y0, u);
		m_R->Quad(bx, by, bx + 2.6f, by, bx + 2.6f, by + (y1 - y0), bx, by + (y1 - y0),
			bm[0], bm[1], bm[2]);
		float cx2 = Lerp(sx, sx + HW, u), cy2 = Lerp(sy - HH + y0, sy + y0, u);
		m_R->Quad(cx2, cy2, cx2 + 2.6f, cy2, cx2 + 2.6f, cy2 + (y1 - y0), cx2, cy2 + (y1 - y0),
			bm[0] * 1.25f, bm[1] * 1.25f, bm[2] * 1.25f);
	}
	// 수평 띠장
	m_R->Quad(sx - HW, sy + y0 + 15.f, sx, sy - HH + y0 + 15.f, sx, sy - HH + y0 + 18.f, sx - HW, sy + y0 + 18.f,
		bm[0] * 0.9f, bm[1] * 0.9f, bm[2] * 0.9f);
	m_R->Quad(sx, sy - HH + y0 + 15.f, sx + HW, sy + y0 + 15.f, sx + HW, sy + y0 + 18.f, sx, sy - HH + y0 + 18.f,
		bm[0] * 1.15f, bm[1] * 1.15f, bm[2] * 1.15f);
	// 사재
	m_R->Quad(sx + 4.f, sy - HH + y0 + 2.f, sx + 6.6f, sy - HH + y0 + 2.f,
		sx + HW - 4.f, sy + y1 - 4.f, sx + HW - 6.6f, sy + y1 - 4.f,
		bm[0] * 1.1f, bm[1] * 1.1f, bm[2] * 1.1f);

	// 문
	float dr[3] = { 0.128f, 0.094f, 0.070f };
	Shade((float)tx, (float)ty, sx, sy + 16.f, &dr[0], &dr[1], &dr[2]);
	float dx0 = sx + 12.f, dy0 = sy + y0 + 6.f;
	m_R->Quad(dx0, dy0, dx0 + 12.f, dy0 + 6.f, dx0 + 12.f, dy0 + 24.f, dx0, dy0 + 18.f,
		dr[0], dr[1], dr[2]);
	m_R->Ellipse(dx0 + 10.f, dy0 + 14.f, 1.4f, 1.4f, 6, 0.42f, 0.34f, 0.16f, 1.f);

	// 창 — 따뜻한 빛 (HDR로 1을 넘겨 블룸이 걸린다)
	float fl = 0.90f + sinf(m_Time * 2.7f + tx * 1.9f + ty) * 0.10f;
	float wx0 = sx - 22.f, wy0 = sy + y0 + 12.f;
	m_R->Quad(wx0, wy0, wx0 + 11.f, wy0 + 5.5f, wx0 + 11.f, wy0 + 17.f, wx0, wy0 + 11.5f,
		2.05f * fl, 1.42f * fl, 0.66f * fl);
	m_R->Quad(wx0 + 4.6f, wy0 + 2.f, wx0 + 6.f, wy0 + 2.7f, wx0 + 6.f, wy0 + 13.f, wx0 + 4.6f, wy0 + 12.3f,
		0.10f, 0.07f, 0.05f);
	// 창빛이 벽에 번지는 느낌
	m_R->Ellipse(wx0 + 5.f, wy0 + 8.f, 16.f, 12.f, 10, 0.55f, 0.36f, 0.14f, 0.13f);

	// 이엉 지붕 — 층층이 겹친 이엉
	float rf[3] = { 0.196f, 0.104f, 0.084f };
	Shade((float)tx, (float)ty, sx, sy + 50.f, &rf[0], &rf[1], &rf[2]);
	float by = sy + y1;
	const int ROWS = 7;
	for (int i = 0; i < ROWS; ++i)
	{
		float u0 = i / (float)ROWS, u1 = (i + 1) / (float)ROWS;
		float k = 0.82f + i * 0.05f + ((h >> i) & 3) * 0.012f;
		// 좌사면
		float ax0 = Lerp(sx - HW, sx, u0), ay0 = Lerp(by, by + 22.f, u0);
		float ax1 = Lerp(sx - HW, sx, u1), ay1 = Lerp(by, by + 22.f, u1);
		m_R->Quad(ax0, ay0 - HH * (1.f - u0) * 0.0f, ax1, ay1,
			ax1, ay1 - 15.f, ax0, ay0 - 15.f,
			rf[0] * k * 0.72f, rf[1] * k * 0.72f, rf[2] * k * 0.76f);
		// 우사면
		float bx0 = Lerp(sx + HW, sx, u0), by0 = Lerp(by, by + 22.f, u0);
		float bx1 = Lerp(sx + HW, sx, u1), by1 = Lerp(by, by + 22.f, u1);
		m_R->Quad(bx0, by0, bx1, by1, bx1, by1 - 15.f, bx0, by0 - 15.f,
			rf[0] * k, rf[1] * k, rf[2] * k);
	}
	// 능선
	m_R->Quad(sx - 3.f, by + 20.f, sx + 3.f, by + 20.f, sx + 3.f, by + 24.f, sx - 3.f, by + 24.f,
		rf[0] * 1.35f, rf[1] * 1.35f, rf[2] * 1.3f);

	// 굴뚝과 연기
	float cx3 = sx + 16.f, cy3 = by + 14.f;
	m_R->IsoBox(cx3, cy3, 6.f, 3.f, 14.f, st[0] * 1.1f, st[1] * 1.1f, st[2] * 1.1f);
	for (int i = 0; i < 4; ++i)
	{
		float ph = m_Time * 0.42f + i * 0.25f + (h % 100) * 0.01f;
		float k = ph - floorf(ph);
		float rise = k * 46.f;
		m_R->Ellipse(cx3 + sinf(ph * 3.f + i) * 7.f, cy3 + 16.f + rise,
			4.f + k * 11.f, 2.6f + k * 7.f, 9,
			0.30f, 0.31f, 0.33f, (1.f - k) * 0.14f * (1.f - t));
	}
}

/* ---------- 오브젝트 ---------- */

void Game::DrawObject(int tx, int ty, float sx, float sy)
{
	unsigned char o = m_World.O(tx, ty);
	if (o == O_NONE) return;
	if (o == O_HOUSE) { DrawHouse(tx, ty, sx, sy); return; }

	float v = m_World.V(tx, ty) / 255.f;
	float sway = sinf(m_Time * 0.95f + tx * 0.7f + ty * 0.4f) * 1.9f;
	float t = FogT((float)tx, (float)ty);

	switch (o)
	{
	case O_TREE:
	{
		float r = 0.104f, g = 0.072f, b = 0.056f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 4.2f, 2.1f, 24.f + v * 9.f, r, g, b);
		float base = 30.f + v * 11.f;
		float cr = 0.070f, cg = 0.132f, cb = 0.094f;
		Shade((float)tx, (float)ty, sx, sy + base, &cr, &cg, &cb);
		// 잎덩이로 실루엣을 부순다 (멀면 개수를 줄인다)
		m_R->Ellipse(sx + sway, sy + base + 8.f, 31.f, 19.f, 10, cr * 0.78f, cg * 0.78f, cb * 0.80f, 1.f);
		m_R->Ellipse(sx + sway * 1.5f, sy + base + 26.f, 21.f, 15.f, 9, cr * 1.22f, cg * 1.22f, cb * 1.16f, 1.f);
		if (t < 0.45f)
		{
			m_R->Ellipse(sx - 13.f + sway * 1.2f, sy + base + 17.f, 17.f, 12.f, 8, cr * 0.92f, cg * 0.92f, cb * 0.94f, 1.f);
			m_R->Ellipse(sx + 13.f + sway * 1.2f, sy + base + 19.f, 16.f, 12.f, 8, cr, cg, cb, 1.f);
			m_R->Ellipse(sx - 4.f + sway * 1.7f, sy + base + 33.f, 12.f, 8.5f, 8, cr * 1.5f, cg * 1.5f, cb * 1.36f, 1.f);
		}
		break;
	}
	case O_PINE:
	{
		float r = 0.092f, g = 0.066f, b = 0.052f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 3.2f, 1.6f, 15.f, r, g, b);
		float cr = 0.050f, cg = 0.100f, cb = 0.080f;
		Shade((float)tx, (float)ty, sx, sy + 46.f, &cr, &cg, &cb);
		for (int i = 0; i < 5; ++i)
		{
			float w = 48.f - i * 9.f;
			float y = sy + 14.f + i * 14.f;
			float f = 1.f + i * 0.13f;
			float ox = sway * (i * 0.35f);
			m_R->Tri(sx - w * 0.5f + ox, y, sx + w * 0.5f + ox, y, sx + ox * 1.2f, y + 25.f,
				cr * f, cg * f, cb * f);
			if (t < 0.45f)
				m_R->Tri(sx - w * 0.32f + ox, y + 3.f, sx - w * 0.05f + ox, y + 3.f, sx - w * 0.2f + ox, y + 16.f,
					cr * f * 1.3f, cg * f * 1.3f, cb * f * 1.22f);
		}
		break;
	}
	case O_FENCE:
	{
		float r = 0.172f, g = 0.146f, b = 0.116f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->Quad(sx - 30.f, sy + 3.f, sx - 30.f, sy + 8.f, sx + 30.f, sy + 8.f, sx + 30.f, sy + 3.f, r, g, b);
		m_R->Quad(sx - 30.f, sy + 11.f, sx - 30.f, sy + 15.f, sx + 30.f, sy + 15.f, sx + 30.f, sy + 11.f,
			r * 0.88f, g * 0.88f, b * 0.88f);
		m_R->IsoBox(sx - 26.f, sy, 2.6f, 1.3f, 19.f, r * 1.1f, g * 1.1f, b * 1.1f);
		m_R->IsoBox(sx + 26.f, sy, 2.6f, 1.3f, 19.f, r * 1.1f, g * 1.1f, b * 1.1f);
		break;
	}
	case O_WELL:
	{
		float r = 0.204f, g = 0.198f, b = 0.190f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 19.f, 9.5f, 16.f, r, g, b);
		for (int i = 0; i < 6; ++i)
		{
			float a = 6.2831853f * i / 6.f;
			m_R->Ellipse(sx + cosf(a) * 15.f, sy + 8.f + sinf(a) * 7.f, 3.4f, 2.2f, 6,
				r * 1.2f, g * 1.2f, b * 1.2f, 1.f);
		}
		m_R->Diamond(sx, sy + 16.f, 27.f, 13.5f, 0.024f, 0.036f, 0.050f);
		float rip = 0.5f + sinf(m_Time * 1.3f) * 0.5f;
		m_R->Ellipse(sx, sy + 16.f, 4.f + rip * 6.f, 2.f + rip * 3.f, 10, 0.20f, 0.30f, 0.38f, 0.25f * (1.f - rip));
		m_R->IsoBox(sx - 15.f, sy, 2.2f, 1.1f, 42.f, r * 0.7f, g * 0.7f, b * 0.7f);
		m_R->IsoBox(sx + 15.f, sy, 2.2f, 1.1f, 42.f, r * 0.7f, g * 0.7f, b * 0.7f);
		float rr = 0.158f, rg = 0.084f, rb = 0.066f;
		Shade((float)tx, (float)ty, sx, sy + 50.f, &rr, &rg, &rb);
		for (int i = 0; i < 4; ++i)
			m_R->Diamond(sx, sy + 46.f + i * 2.6f, 50.f - i * 9.f, 23.f - i * 4.f,
				rr * (1.f + i * 0.1f), rg * (1.f + i * 0.1f), rb * (1.f + i * 0.1f));
		break;
	}
	case O_ROCK:
	{
		float r = 0.156f, g = 0.160f, b = 0.164f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->Ellipse(sx - 3.f, sy + 4.f + v * 3.f, 12.f + v * 5.f, 8.f + v * 4.f, 9, r, g, b, 1.f);
		m_R->Ellipse(sx - 5.f, sy + 8.f + v * 4.f, 7.f + v * 3.f, 4.5f, 8, r * 1.3f, g * 1.3f, b * 1.3f, 1.f);
		m_R->Ellipse(sx + 8.f, sy + 2.f, 6.f, 4.f, 7, r * 1.1f, g * 1.1f, b * 1.1f, 1.f);
		break;
	}
	case O_TUFT:
	case O_FLOWER:
	{
		float r = 0.128f, g = 0.176f, b = 0.118f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		for (int i = 0; i < 6; ++i)
		{
			float ox = -10.f + i * 3.6f;
			float hh = 7.f + ((tx * 3 + ty + i) % 5) * 2.2f;
			m_R->Quad(sx + ox, sy, sx + ox + sway * 0.55f, sy + hh,
				sx + ox + 1.5f + sway * 0.55f, sy + hh, sx + ox + 1.5f, sy,
				r * (0.85f + i * 0.05f), g * (0.85f + i * 0.05f), b * (0.85f + i * 0.05f));
		}
		if (o == O_FLOWER)
		{
			float fr = 0.52f, fg = 0.44f, fb = 0.24f;
			Shade((float)tx, (float)ty, sx, sy, &fr, &fg, &fb);
			m_R->Ellipse(sx + 1.f, sy + 13.f, 2.4f, 2.4f, 7, fr, fg, fb, 1.f);
			m_R->Ellipse(sx - 6.f, sy + 10.f, 1.8f, 1.8f, 6, fr, fg * 0.9f, fb, 1.f);
		}
		break;
	}
	case O_REED:
	{
		float r = 0.138f, g = 0.150f, b = 0.104f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		for (int i = 0; i < 6; ++i)
		{
			float ox = -12.f + i * 4.6f;
			float hh = 16.f + ((tx + ty * 2 + i) % 6) * 4.f;
			m_R->Quad(sx + ox, sy, sx + ox + sway * 1.8f, sy + hh,
				sx + ox + 1.4f + sway * 1.8f, sy + hh, sx + ox + 1.4f, sy, r, g, b);
			if (i % 2 == 0)
				m_R->Ellipse(sx + ox + sway * 1.8f, sy + hh + 2.f, 1.6f, 3.2f, 6,
					r * 1.6f, g * 1.4f, b * 1.2f, 1.f);
		}
		break;
	}
	case O_STUMP:
	{
		float r = 0.134f, g = 0.098f, b = 0.072f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 12.f, 6.f, 9.f, r, g, b);
		m_R->Ellipse(sx, sy + 9.f, 7.f, 3.5f, 9, r * 1.4f, g * 1.4f, b * 1.35f, 1.f);
		break;
	}
	case O_CRATE:
	{
		float r = 0.152f, g = 0.118f, b = 0.086f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 11.f, 5.5f, 14.f, r, g, b);
		m_R->Quad(sx - 11.f, sy + 7.f, sx, sy + 1.5f, sx, sy + 4.f, sx - 11.f, sy + 9.5f,
			r * 0.4f, g * 0.4f, b * 0.4f);
		break;
	}
	case O_LAMP:
	{
		float r = 0.128f, g = 0.120f, b = 0.114f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->IsoBox(sx, sy, 2.6f, 1.3f, 48.f, r, g, b);
		m_R->Quad(sx - 1.f, sy + 46.f, sx + 9.f, sy + 50.f, sx + 9.f, sy + 48.f, sx - 1.f, sy + 44.f, r, g, b);
		float fl = 0.88f + sinf(m_Time * 3.3f + tx) * 0.12f;
		float f2 = 1.f - t;
		for (int i = 5; i >= 1; --i)
			m_R->Ellipse(sx + 8.f, sy + 50.f, 15.f * i, 11.f * i, 12,
				1.00f, 0.70f, 0.32f, 0.030f * fl * f2 / i);
		// 램프 코어는 1을 넘겨 블룸을 만든다
		m_R->Ellipse(sx + 8.f, sy + 50.f, 5.2f, 6.2f, 9, 3.2f * fl, 2.2f * fl, 0.95f * fl, 1.f);
		m_R->Quad(sx + 3.f, sy + 44.f, sx + 3.f, sy + 57.f, sx + 13.f, sy + 57.f, sx + 13.f, sy + 44.f,
			r * 1.2f, g * 1.2f, b * 1.2f, 0.35f);
		break;
	}
	case O_RUIN:
	{
		float r = 0.166f, g = 0.162f, b = 0.156f;
		Shade((float)tx, (float)ty, sx, sy, &r, &g, &b);
		m_R->Quad(sx - 24.f, sy - 2.f, sx - 27.f, sy + 7.f, sx - 3.f, sy + 15.f, sx + 3.f, sy + 4.f, r, g, b);
		m_R->IsoBox(sx + 11.f, sy + 2.f, 8.f, 4.f, 22.f, r * 0.94f, g * 0.94f, b * 0.94f);
		m_R->Quad(sx - 14.f, sy + 6.f, sx - 16.f, sy + 12.f, sx - 6.f, sy + 16.f, sx - 4.f, sy + 10.f,
			r * 1.15f, g * 1.15f, b * 1.15f);
		float p = 0.45f + sinf(m_Time * 2.3f + tx) * 0.35f;
		float f2 = 1.f - t;
		for (int i = 4; i >= 1; --i)
			m_R->Ellipse(sx, sy + 21.f, 8.f * i, 6.f * i, 10, 0.95f, 0.26f, 0.18f, 0.035f * p * f2 / i);
		m_R->Quad(sx - 4.f, sy + 17.f, sx - 3.f, sy + 26.f, sx + 4.f, sy + 25.f, sx + 4.f, sy + 16.f,
			1.30f * (0.6f + p * 0.5f), 0.42f, 0.30f);
		break;
	}
	default: break;
	}
}

/* ---------- 캐릭터 ---------- */

void Game::DrawNpc(int i)
{
	const Npc& n = m_World.npcs[i];
	float sx, sy; ToScreen(n.hx, n.hy, &sx, &sy);

	float lr, lg, lb; LightAt(sx, sy + 20.f, &lr, &lg, &lb);
	float t = FogT(n.hx, n.hy);
	CharSetFog(t, FOG_R, FOG_G, FOG_B);

	// 제자리에서 두리번거린다
	float dx = sinf(m_Time * 0.35f + n.phase), dy = cosf(m_Time * 0.27f + n.phase * 1.7f);
	DrawHuman(*m_R, sx, sy, dx, dy, 0.f, false, m_NpcStyle[i], lr, lg, lb, m_Time + n.phase);
	CharSetFog(0.f, 0.f, 0.f, 0.f);

	if (n.recordId >= 0 && !m_Recorded[n.recordId])
	{
		float p = 0.5f + sinf(m_Time * 3.f) * 0.4f;
		m_R->Ellipse(sx, sy + 56.f, 4.5f, 5.5f, 8, 1.6f * p, 0.34f, 0.24f, 1.f);
		for (int k = 3; k >= 1; --k)
			m_R->Ellipse(sx, sy + 56.f, 6.f * k, 6.f * k, 9, 0.9f, 0.24f, 0.18f, 0.045f * p / k);
	}

	float d = sqrtf((n.hx - m_PX) * (n.hx - m_PX) + (n.hy - m_PY) * (n.hy - m_PY));
	bool isNear = d < 2.0f;
	float a = (1.f - t) * (isNear ? 1.f : 0.6f);
	float w = m_F.Measure(n.name);
	m_F.DrawShadowed(sx - w * 0.5f, sy - 4.f, n.name, 0.78f, 0.75f, 0.68f, a);
	if (isNear) m_F.DrawShadowed(sx - 5.f, sy + 76.f, "E", 1.f, 0.88f, 0.52f, 1.f);
}

void Game::DrawBeast(int i)
{
	const Beast& b = m_Beasts.At(i);
	float sx, sy; ToScreen(b.x, b.y, &sx, &sy);

	float lr, lg, lb; LightAt(sx, sy + 12.f, &lr, &lg, &lb);
	float t = FogT(b.x, b.y);
	CharSetFog(t, FOG_R, FOG_G, FOG_B);
	::DrawBeast(*m_R, sx, sy, b.vx, b.vy, b.walk, b.moving, b.kind, lr, lg, lb, m_Time);
	CharSetFog(0.f, 0.f, 0.f, 0.f);

	float d = sqrtf((b.x - m_PX) * (b.x - m_PX) + (b.y - m_PY) * (b.y - m_PY));
	if (d < 6.f)
	{
		const char* nm = BeastName(b.kind);
		float w = m_F.Measure(nm);
		m_F.DrawShadowed(sx - w * 0.5f, sy - 4.f, nm, 0.62f, 0.58f, 0.54f, (1.f - t) * 0.8f);
	}
}

void Game::DrawPlayer()
{
	float sx, sy; ToScreen(m_PX, m_PY, &sx, &sy);
	float lr, lg, lb; LightAt(sx, sy + 20.f, &lr, &lg, &lb);
	CharSetFog(0.f, 0.f, 0.f, 0.f);
	DrawHuman(*m_R, sx, sy, m_FaceX, m_FaceY, m_Walk, m_Moving, m_PlayerStyle, lr, lg, lb, m_Time);
}

/* ---------- 이펙트 ---------- */

void Game::DrawPuffs()
{
	for (int i = 0; i < 32; ++i)
	{
		if (m_Puffs[i].life <= 0.f) continue;
		float k = m_Puffs[i].t / m_Puffs[i].life;
		float sx, sy; ToScreen(m_Puffs[i].x, m_Puffs[i].y, &sx, &sy);
		if (m_Puffs[i].type == 0)
			m_R->Ellipse(sx, sy + 2.f + k * 5.f, 4.f + k * 9.f, 2.f + k * 4.5f, 9,
				0.34f, 0.31f, 0.26f, (1.f - k) * 0.22f);
		else
		{
			float rr = 4.f + k * 26.f;
			m_R->Ellipse(sx, sy, rr, rr * 0.5f, 14, 0.42f, 0.52f, 0.58f, (1.f - k) * 0.16f);
			m_R->Ellipse(sx, sy, rr * 0.72f, rr * 0.36f, 12, 0.05f, 0.09f, 0.13f, (1.f - k) * 0.10f);
		}
	}
}

void Game::DrawMotes()
{
	for (int i = 0; i < 190; ++i)
		m_R->Rect(m_Motes[i].x, m_Motes[i].y, m_Motes[i].s * 1.6f, m_Motes[i].s * 1.6f,
			0.66f, 0.72f, 0.70f, m_Motes[i].a);
}

/* ---------- HUD ---------- */

void Game::DrawHud()
{
	float hw = m_W * 0.5f, hh = m_H * 0.5f;
	char buf[256];

	m_R->Rect(-hw + 156.f, hh - 46.f, 300.f, 66.f, 0.026f, 0.034f, 0.038f, 0.60f);
	m_R->Rect(-hw + 156.f, hh - 79.f, 300.f, 1.4f, 0.44f, 0.16f, 0.12f, 0.85f);
	sprintf_s(buf, "「미납」  기록 %d / 3", m_RecordCount);
	m_F.DrawShadowed(-hw + 22.f, hh - 20.f, buf, 0.88f, 0.82f, 0.72f);
	sprintf_s(buf, "장부 조각 %d        %02d:%02d        %.0f FPS",
		m_Fragments, (int)m_Elapsed / 60, (int)m_Elapsed % 60, m_Fps);
	m_F.DrawShadowed(-hw + 22.f, hh - 46.f, buf, 0.56f, 0.58f, 0.55f);
	m_F.DrawShadowed(-hw + 22.f, -hh + 34.f,
		"WASD 이동   Space 달리기   E 상호작용   Tab 장부   Esc 종료", 0.44f, 0.48f, 0.46f);

	if (m_ToastTimer > 0.f && m_State != GS_DIALOG)
	{
		float a = m_ToastTimer > 1.f ? 1.f : m_ToastTimer;
		float w = m_F.Measure(m_Toast);
		m_R->Rect(0.f, hh - 98.f, w + 44.f, 40.f, 0.026f, 0.034f, 0.038f, 0.82f * a);
		m_R->Rect(0.f, hh - 118.f, w + 44.f, 1.4f, 0.46f, 0.16f, 0.12f, a);
		m_F.DrawShadowed(-w * 0.5f, hh - 88.f, m_Toast, 0.86f, 0.80f, 0.70f, a);
	}

	if (m_State == GS_DIALOG)
	{
		float bw = (float)m_W - 160.f, bh = 120.f, cy = -hh + 98.f;
		m_R->Rect(0.f, cy, bw, bh, 0.022f, 0.030f, 0.034f, 0.95f);
		m_R->Rect(0.f, cy + bh * 0.5f, bw, 2.f, 0.50f, 0.17f, 0.12f, 1.f);
		m_R->Rect(0.f, cy - bh * 0.5f, bw, 1.f, 0.20f, 0.22f, 0.22f, 1.f);
		const char* who = m_World.npcs[m_TalkingTo].name;
		m_FB.DrawShadowed(-bw * 0.5f + 26.f, cy + bh * 0.5f - 14.f, who, 0.86f, 0.46f, 0.34f);
		m_F.DrawShadowed(-bw * 0.5f + 26.f, cy + 16.f, m_Lines[m_LineIdx], 0.90f, 0.88f, 0.82f);
		sprintf_s(buf, "%d / %d      [E] 계속", m_LineIdx + 1, m_LineCount);
		float w = m_F.Measure(buf);
		m_F.DrawShadowed(bw * 0.5f - 26.f - w, cy - bh * 0.5f + 28.f, buf, 0.48f, 0.50f, 0.48f);
	}

	if (m_LedgerOpen)
	{
		m_R->Rect(0.f, 0.f, 640.f, 286.f, 0.024f, 0.032f, 0.036f, 0.96f);
		m_R->Rect(0.f, 106.f, 640.f, 2.f, 0.50f, 0.17f, 0.12f, 1.f);
		m_FB.DrawShadowed(-296.f, 132.f, "장부  ·  미납 항목", 0.86f, 0.48f, 0.36f);
		for (int i = 0; i < 3; ++i)
			m_F.DrawShadowed(-296.f, 72.f - i * 30.f,
				m_Recorded[i] ? RECORD_SHORT[i] : "----  아직 기입되지 않음",
				m_Recorded[i] ? 0.84f : 0.34f, m_Recorded[i] ? 0.80f : 0.36f, m_Recorded[i] ? 0.72f : 0.35f);
		sprintf_s(buf, "들에서 주운 장부 조각 : %d / %d", m_Fragments, FIELD_RECORD_COUNT);
		m_F.DrawShadowed(-296.f, -38.f, buf, 0.62f, 0.64f, 0.60f);
		m_F.DrawShadowed(-296.f, -74.f,
			m_RecordCount >= 3 ? "광장의 촌장에게 보고하라." : "무엇이 빠졌는지 찾아라.", 0.58f, 0.60f, 0.58f);
		sprintf_s(buf, "%.0f FPS      불러온 청크 %d      정점 %d",
		m_Fps, m_World.LoadedChunks(), m_R->LastVertexCount());
		m_F.DrawShadowed(-296.f, -112.f, buf, 0.30f, 0.33f, 0.32f);
	}

	if (m_State == GS_END)
	{
		m_R->SetFade(Clamp01(m_EndTimer * 0.35f) * 0.80f);
		if (m_EndTimer > 1.0f)
		{
			float t = Clamp01((m_EndTimer - 1.f) * 0.8f);
			m_FB.DrawShadowed(-52.f, 46.f, "「미납」", 3.0f * t, 2.6f * t, 2.2f * t, 1.f);
			m_F.DrawShadowed(-300.f, 0.f, "세 건이 접수되었다. 촌장은 당신의 이름을 적으려 했고,",
				1.6f, 1.55f, 1.45f, t);
			m_F.DrawShadowed(-300.f, -26.f, "그 칸은 비어 있는 채로 남았다.", 1.6f, 1.55f, 1.45f, t);
			if (m_EndTimer > 2.6f)
			{
				float t2 = Clamp01((m_EndTimer - 2.6f) * 0.8f);
				m_F.DrawShadowed(-300.f, -74.f, "결손 : 이 세계에서 붉은색이 빠져나갔다.",
					1.8f, 1.1f, 0.9f, t2);
				m_F.DrawShadowed(-300.f, -112.f, "튜토리얼 레벨 종료  ·  Esc 로 종료", 0.9f, 0.95f, 0.92f, t2);
			}
		}
	}
}

/* ---------- 렌더 ---------- */

void Game::Render()
{
	m_R->BeginFrame(0.028f, 0.042f, 0.056f);
	++m_Frame;

	// 월드 좌표 AABB는 실제 화면보다 6배 넓다. 그래서 타일마다 화면 밖인지 직접 검사한다.
	m_CullX = m_W * 0.5f + Iso::TILE_W;
	m_CullY = m_H * 0.5f + Iso::TILE_H * 4.f;

	float hw = m_W * 0.5f + Iso::TILE_W * 2.f;
	float hh = m_H * 0.5f + Iso::TILE_H * 6.f;
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
	m_TCX0 = minX - 1;
	m_TCY0 = minY - 1;

	// 화면 밖 판정 — 타일의 화면 좌표로 직접 자른다
	#define VISIBLE(sx, sy) ((sx) > -m_CullX && (sx) < m_CullX && (sy) > -m_CullY && (sy) < m_CullY)

	// 1) 광원 수집 (화면 안의 등불·창문만)
	m_LightCount = 0;
	for (int ty = minY; ty <= maxY && m_LightCount < 64; ++ty)
		for (int tx = minX; tx <= maxX && m_LightCount < 64; ++tx)
		{
			float sx, sy; ToScreen((float)tx, (float)ty, &sx, &sy);
			if (!VISIBLE(sx, sy)) continue;
			unsigned char o = m_World.O(tx, ty);
			if (o != O_LAMP && o != O_HOUSE && o != O_RUIN) continue;
			Light& L = m_Lights[m_LightCount++];
			L.sx = sx + (o == O_LAMP ? 8.f : 0.f);
			L.sy = sy + (o == O_LAMP ? 50.f : 16.f);
			if (o == O_RUIN) { L.r = 0.16f; L.g = 0.05f; L.b = 0.04f; L.str = 0.5f; }
			else { L.r = 0.30f; L.g = 0.20f; L.b = 0.085f; L.str = (o == O_LAMP) ? 1.f : 0.52f; }
		}

	// 2) 지면 — 전부 먼저 그린다 (앞쪽 타일이 캐릭터를 덮는 문제 해결)
	for (int ty = minY; ty <= maxY; ++ty)
		for (int tx = minX; tx <= maxX; ++tx)
		{
			float sx, sy; ToScreen((float)tx, (float)ty, &sx, &sy);
			if (!VISIBLE(sx, sy)) continue;
			float lr, lg, lb; LightAt(sx, sy, &lr, &lg, &lb);   // 타일당 1회
			DrawGround(tx, ty, sx, sy, lr, lg, lb);
		}
	for (int ty = minY; ty <= maxY; ++ty)
		for (int tx = minX; tx <= maxX; ++tx)
		{
			float sx, sy; ToScreen((float)tx, (float)ty, &sx, &sy);
			if (!VISIBLE(sx, sy)) continue;
			DrawGroundDetail(tx, ty, sx, sy);
		}

	// 3) 그림자 — 지면 위, 오브젝트 아래
	for (int ty = minY; ty <= maxY; ++ty)
		for (int tx = minX; tx <= maxX; ++tx)
		{
			float sx, sy; ToScreen((float)tx, (float)ty, &sx, &sy);
			if (!VISIBLE(sx, sy)) continue;
			DrawShadow(tx, ty, sx, sy);
		}
	for (int i = 0; i < NPC_COUNT; ++i)
	{
		float sx, sy; ToScreen(m_World.npcs[i].hx, m_World.npcs[i].hy, &sx, &sy);
		if (!VISIBLE(sx, sy)) continue;
		m_R->SoftShadow(sx + 5.f, sy - 1.f, 8.5f, 4.2f, (1.f - FogT(m_World.npcs[i].hx, m_World.npcs[i].hy)));
	}
	for (int i = 0; i < m_Beasts.Count(); ++i)
	{
		const Beast& b = m_Beasts.At(i);
		float sx, sy; ToScreen(b.x, b.y, &sx, &sy);
		if (!VISIBLE(sx, sy)) continue;
		m_R->SoftShadow(sx + 5.f, sy - 1.f, 11.f, 4.6f, (1.f - FogT(b.x, b.y)) * 0.9f);
	}
	{
		float sx, sy; ToScreen(m_PX, m_PY, &sx, &sy);
		m_R->SoftShadow(sx + 5.f, sy - 1.f, 9.f, 4.4f, 1.f);
	}
	DrawPuffs();

	// 4) 오브젝트와 캐릭터 — depth 오름차순
	for (int s = minX + minY; s <= maxX + maxY; ++s)
	{
		for (int tx = minX; tx <= maxX; ++tx)
		{
			int ty = s - tx;
			if (ty < minY || ty > maxY) continue;
			float sx, sy; ToScreen((float)tx, (float)ty, &sx, &sy);
			if (!VISIBLE(sx, sy)) continue;
			DrawObject(tx, ty, sx, sy);
		}
		for (int i = 0; i < NPC_COUNT; ++i)
			if ((int)floorf(m_World.npcs[i].hx + m_World.npcs[i].hy) == s)
			{
				float sx, sy; ToScreen(m_World.npcs[i].hx, m_World.npcs[i].hy, &sx, &sy);
				if (VISIBLE(sx, sy)) DrawNpc(i);
			}
		for (int i = 0; i < m_Beasts.Count(); ++i)
		{
			const Beast& b = m_Beasts.At(i);
			if ((int)floorf(b.x + b.y) != s) continue;
			float sx, sy; ToScreen(b.x, b.y, &sx, &sy);
			if (VISIBLE(sx, sy)) DrawBeast(i);
		}
		if ((int)floorf(m_PX + m_PY) == s) DrawPlayer();
	}

	#undef VISIBLE

	DrawMotes();
	DrawHud();
	m_R->EndFrame();
}
