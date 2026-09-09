#include "stdafx.h"
#include "World.h"

#include <cmath>
#include <cstdlib>

/* ---------- 노이즈 ---------- */

static unsigned int Hash3(int x, int y, int s)
{
	unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + s * 1442695041);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}
static float Rand01(int x, int y, int s) { return (Hash3(x, y, s) & 0xFFFFFF) / 16777215.f; }

static float Smooth(float t) { return t * t * (3.f - 2.f * t); }

static float Noise(float x, float y, int s)
{
	int xi = (int)floorf(x), yi = (int)floorf(y);
	float tx = Smooth(x - xi), ty = Smooth(y - yi);
	float a = Rand01(xi, yi, s), b = Rand01(xi + 1, yi, s);
	float c = Rand01(xi, yi + 1, s), d = Rand01(xi + 1, yi + 1, s);
	return (a + (b - a) * tx) * (1.f - ty) + (c + (d - c) * tx) * ty;
}

static float FBM(float x, float y, int s)
{
	return Noise(x, y, s) * 0.55f + Noise(x * 2.1f, y * 2.1f, s + 31) * 0.30f
		+ Noise(x * 4.3f, y * 4.3f, s + 67) * 0.15f;
}

/* ---------- 길 ---------- */

// 길은 완만하게 휜다 — 따라가면 계속 새 지역이 나온다.
int World_RoadY(int wx) { return (int)roundf(sinf(wx * 0.055f) * 4.f + sinf(wx * 0.017f) * 7.f); }
int World_RoadX(int wy) { return (int)roundf(sinf(wy * 0.048f) * 5.f); }

/* ---------- 대사 ---------- */

static const char* L_ELDER_INTRO[] = {
	"처음 보는 얼굴이군. 길드에서 왔소?",
	"…이상하지. 자네를 본 적이 없는데 낯설지가 않아.",
	"요 며칠 마을이 조용해. 조용한 게 아니라, 뭔가 빠져 있어.",
	"세 사람을 만나보게. 호숫가의 어부, 동쪽 숲의 나무꾼, 우물가 아이.",
	"무엇이 빠졌는지 적어서 가져오면 접수해 주지. 규정이니까.",
};
static const char* L_ELDER_DONE[] = {
	"세 건… 전부 같은 날이었군.",
	"우리 마을이 무언가를 얻었다는 뜻이야. 아무도 그게 뭔지 기억하지 못하고.",
	"자네 이름을 접수 서류에 적어야 하는데.",
	"…자네, 이름이 뭐였지?",
};
static const char* L_FISHER[] = {
	"오늘도 안 보입니다.",
	"호수요. 저 앞에 있는 거 아는데, 눈에 안 들어와요.",
	"물소리는 들려요. 그물도 던져요. 그런데 물이 안 보입니다.",
	"아들은 돌아왔는데. 어디서 빠졌는지는 이제 모르겠고.",
};
static const char* L_WOODSMAN[] = {
	"손이 도구를 기억하지 못합니다.",
	"도끼를 쥐면 매번 처음 쥐는 것 같아요. 20년을 썼는데.",
	"나무는 넘어갑니다. 다만 어떻게 넘겼는지 남지 않아요.",
};
static const char* L_CHILD[] = {
	"내 그림자 못 봤어요?",
	"어제까지 있었는데, 오늘 아침엔 없어요.",
	"엄마는 원래 없었다고 해요. 근데 나는 기억나요.",
};
static const char* L_SMITH[] = {
	"길드 사람은 값을 안 냅니까? 나는 냅니다. 매달.",
	"환산표대로 냈는데 표가 틀렸다더군. 환불은 규정에 없다고.",
};
static const char* L_PEDDLER[] = {
	"최신판 환산표! 작년 것보다 12퍼센트 정확합니다!",
	"뭐가 12퍼센트냐고요? 그건 표 뒷장에 있습니다. 뒷장은 별매입니다.",
	"손님, 값을 모르고 거래하는 것보다는 틀린 표라도 있는 게 낫지 않습니까?",
};
static const char* L_PRIEST[] = {
	"죽음은 신의 영역이오. 값을 매길 수 없소.",
	"…우리가 하는 건 다릅니다. 그건 기적이라고 부르지요.",
};
static const char* L_GUARD[] = {
	"길을 벗어나지 마시오. 벗어나도 막지는 않겠지만.",
	"들어간 사람이 뭔가를 두고 나와서 그렇소. 본인은 모르고.",
};
static const char* L_SCRIBE[] = {
	"길에서 장부 조각을 보면 주워 오시오. 값이야 안 나가지만.",
	"누군가 적어둔 것이 남아 있다면, 그건 아직 정산되지 않은 겁니다.",
};

static const char* FIELD_RECORDS[FIELD_RECORD_COUNT] = {
	"[장부 조각] 대장간 아들을 되찾았다. 이후 그 집 쇠는 식지 않는다.",
	"[장부 조각] 눈먼 노파가 앞을 보게 되었다. 마을의 우물이 마른 날이다.",
	"[장부 조각] 이 다리는 두 번 놓였다. 두 번째 것을 아무도 건너지 않는다.",
	"[장부 조각] 어느 사냥꾼이 개를 되살렸다. 개는 그를 따르지 않는다.",
	"[장부 조각] 세 자매가 하나를 되찾았다. 이제 둘은 서로를 모른다.",
	"[장부 조각] 이 밭은 매년 같은 양을 낸다. 씨를 뿌리지 않아도.",
	"[장부 조각] 한 사내가 자기 죽음을 미뤘다. 마을의 시계가 모두 멈췄다.",
	"[장부 조각] 여기 적힌 값은 지워져 있다. 먹칠은 은폐가 아니라 보호였다.",
};

const char* const* World_ElderIntro(int* c) { *c = 5; return L_ELDER_INTRO; }
const char* const* World_ElderDone(int* c) { *c = 4; return L_ELDER_DONE; }
const char* World_FieldRecord(int i)
{
	if (i < 0 || i >= FIELD_RECORD_COUNT) i = 0;
	return FIELD_RECORDS[i];
}

/* ---------- 마을 ---------- */

static const int VILLAGE_R = 14;
// 호수 (수작업 배치 — 어부의 결손 장소)
static const float LAKE_X = -17.f, LAKE_Y = 13.f, LAKE_R = 6.6f;

static bool InVillage(int wx, int wy)
{
	return (wx > -VILLAGE_R && wx < VILLAGE_R && wy > -VILLAGE_R && wy < VILLAGE_R);
}

/* ---------- 생성 ---------- */

void World::Init()
{
	int i = 0;
	npcs[i++] = { 0.f,  -2.5f, "촌장",     0,           0, -1, true,  0.0f, 0.88f, 0.82f, 0.62f };
	npcs[i++] = { -11.f, 10.5f, "어부",     L_FISHER,    4,  0, false, 1.1f, 0.55f, 0.72f, 0.74f };
	npcs[i++] = { 12.f,  7.f,  "나무꾼",   L_WOODSMAN,  3,  1, false, 2.3f, 0.64f, 0.52f, 0.36f };
	npcs[i++] = { 3.4f,  1.2f, "아이",     L_CHILD,     3,  2, false, 0.7f, 0.82f, 0.76f, 0.56f };
	npcs[i++] = { -4.2f, 0.4f, "대장장이", L_SMITH,     2, -1, false, 3.1f, 0.60f, 0.44f, 0.38f };
	npcs[i++] = { 1.8f, -1.4f, "행상인",   L_PEDDLER,   3, -1, false, 1.7f, 0.74f, 0.62f, 0.32f };
	npcs[i++] = { -1.6f, 2.8f, "사제",     L_PRIEST,    2, -1, false, 2.7f, 0.70f, 0.70f, 0.76f };
	npcs[i++] = { 0.6f, -6.2f, "순찰병",   L_GUARD,     2, -1, false, 0.4f, 0.46f, 0.52f, 0.50f };
	npcs[i++] = { -2.4f,-3.2f, "서기",     L_SCRIBE,    2, -1, false, 2.1f, 0.66f, 0.60f, 0.72f };
}

World::~World()
{
	for (std::unordered_map<long long, Chunk*>::iterator it = m_Map.begin(); it != m_Map.end(); ++it)
		delete it->second;
	m_Map.clear();
}

static long long Key(int cx, int cy)
{
	return ((long long)(unsigned int)cx << 32) | (unsigned int)cy;
}

Chunk* World::Get(int cx, int cy)
{
	std::unordered_map<long long, Chunk*>::iterator it = m_Map.find(Key(cx, cy));
	if (it != m_Map.end()) return it->second;
	return Generate(cx, cy);
}

Chunk* World::Generate(int cx, int cy)
{
	Chunk* c = new Chunk();
	c->cx = cx; c->cy = cy;

	for (int ly = 0; ly < CHUNK; ++ly)
	{
		for (int lx = 0; lx < CHUNK; ++lx)
		{
			int wx = cx * CHUNK + lx;
			int wy = cy * CHUNK + ly;
			int idx = ly * CHUNK + lx;

			float e = FBM(wx * 0.030f, wy * 0.030f, 11);
			float m = FBM(wx * 0.046f, wy * 0.046f, 71);

			// 수작업 호수
			float ld = sqrtf((wx - LAKE_X) * (wx - LAKE_X) + (wy - LAKE_Y) * (wy - LAKE_Y));
			float lw = Rand01(wx, wy, 5) * 0.9f;

			unsigned char g = G_GRASS, o = O_NONE;

			if (ld < LAKE_R + lw)            g = G_WATER;
			else if (ld < LAKE_R + 1.4f + lw) g = G_SHORE;
			else if (e < 0.335f)              g = G_WATER;
			else if (e < 0.375f)              g = G_SHORE;
			else if (m > 0.68f && e < 0.47f)  g = G_MARSH;

			// 길
			bool road = false;
			if (wy == World_RoadY(wx) || wy == World_RoadY(wx) + 1) road = true;
			if (wx == World_RoadX(wy) || wx == World_RoadX(wy) + 1) road = true;
			if (road && g != G_WATER) { g = G_PATH; }

			// 오브젝트
			if (g == G_GRASS || g == G_MARSH)
			{
				float dens = (m - 0.36f) * 1.9f;
				if (dens < 0.f) dens = 0.f;
				if (dens > 0.82f) dens = 0.82f;
				float rr = Rand01(wx, wy, 13);
				if (!road && rr < dens)
					o = (Rand01(wx, wy, 19) < 0.35f) ? O_PINE : O_TREE;
				else if (!road && rr < dens + 0.05f) o = O_ROCK;
				else if (!road && rr > 0.93f)        o = O_TUFT;
				else if (!road && rr > 0.90f)        o = O_FLOWER;
				else if (g == G_MARSH && rr > 0.72f) o = O_REED;
			}
			else if (g == G_SHORE)
			{
				if (Rand01(wx, wy, 23) > 0.80f) o = O_REED;
			}

			// 길가 등불 — 마을에서 뻗어 나가며 점점 드물어진다
			if (g == G_PATH && (wx % 9 == 0) && Rand01(wx, wy, 41) > 0.35f)
			{
				int dist = abs(wx) + abs(wy);
				if (dist < 90 && wy == World_RoadY(wx)) o = O_LAMP;
			}

			// 유적 — 장부 조각
			if (!InVillage(wx, wy) && g != G_WATER && o == O_NONE)
			{
				if (Hash3(wx, wy, 907) % 1300 == 0) o = O_RUIN;
			}

			c->g[idx] = g;
			c->o[idx] = o;
			c->v[idx] = (unsigned char)(Hash3(wx, wy, 3) & 0xFF);
		}
	}

	// 마을 영역과 겹치면 덮어쓴다
	int x0 = cx * CHUNK, y0 = cy * CHUNK;
	if (x0 < VILLAGE_R + 4 && x0 + CHUNK > -VILLAGE_R - 4 &&
		y0 < VILLAGE_R + 4 && y0 + CHUNK > -VILLAGE_R - 4)
		ApplyVillage(c);

	m_Map[Key(cx, cy)] = c;
	return c;
}

void World::ApplyVillage(Chunk* c)
{
	static const int HOUSES[][2] = {
		{-8,-6},{5,-7},{-9,5},{7,4},{3,-4},{-6,2},{8,-2},{-3,7}
	};

	for (int ly = 0; ly < CHUNK; ++ly)
	{
		for (int lx = 0; lx < CHUNK; ++lx)
		{
			int wx = c->cx * CHUNK + lx, wy = c->cy * CHUNK + ly;
			int idx = ly * CHUNK + lx;
			if (!InVillage(wx, wy)) continue;

			float d = sqrtf((float)(wx * wx + wy * wy));

			// 마을 안의 나무·바위는 걷어낸다 (호수는 남긴다)
			if (c->g[idx] != G_WATER && c->g[idx] != G_SHORE)
			{
				if (c->o[idx] == O_TREE || c->o[idx] == O_PINE || c->o[idx] == O_ROCK ||
					c->o[idx] == O_RUIN)
					c->o[idx] = (Rand01(wx, wy, 77) > 0.85f) ? O_TUFT : O_NONE;
				if (c->g[idx] == G_MARSH) c->g[idx] = G_GRASS;
			}

			// 광장
			if (wx >= -3 && wx <= 3 && wy >= -3 && wy <= 3)
			{
				c->g[idx] = G_PLAZA;
				c->o[idx] = O_NONE;
			}
			// 흙 마당
			else if (d < 10.f && c->g[idx] == G_GRASS && Rand01(wx, wy, 91) > 0.55f)
				c->g[idx] = G_DIRT;

			// 울타리
			if (d > (float)VILLAGE_R - 1.6f && d < (float)VILLAGE_R - 0.4f &&
				c->g[idx] != G_WATER && c->g[idx] != G_PATH && c->g[idx] != G_SHORE)
				c->o[idx] = O_FENCE;

			// 우물
			if (wx == 3 && wy == 2) { c->g[idx] = G_PLAZA; c->o[idx] = O_WELL; }

			// 광장 등불
			if ((wx == -4 && wy == -4) || (wx == 4 && wy == 4) ||
				(wx == 4 && wy == -4) || (wx == -4 && wy == 4))
				c->o[idx] = O_LAMP;

			// 나무꾼 작업장
			if (wx >= 10 && wx <= 13 && wy >= 5 && wy <= 8)
			{
				if (Rand01(wx, wy, 55) > 0.6f) c->o[idx] = O_STUMP;
				else if (Rand01(wx, wy, 56) > 0.85f) c->o[idx] = O_CRATE;
			}
		}
	}

	// 집 배치 (2x2)
	for (int h = 0; h < 8; ++h)
	{
		for (int dy = 0; dy < 2; ++dy)
		{
			for (int dx = 0; dx < 2; ++dx)
			{
				int wx = HOUSES[h][0] + dx, wy = HOUSES[h][1] + dy;
				int lx = wx - c->cx * CHUNK, ly = wy - c->cy * CHUNK;
				if (lx < 0 || ly < 0 || lx >= CHUNK || ly >= CHUNK) continue;
				c->g[ly * CHUNK + lx] = G_DIRT;
				c->o[ly * CHUNK + lx] = O_HOUSE;
			}
		}
	}
}

void World::EnsureAround(float wx, float wy, int radiusChunks)
{
	int ccx = (int)floorf(wx / CHUNK), ccy = (int)floorf(wy / CHUNK);
	for (int y = ccy - radiusChunks; y <= ccy + radiusChunks; ++y)
		for (int x = ccx - radiusChunks; x <= ccx + radiusChunks; ++x)
			Get(x, y);

	// 멀어진 청크 해제
	if (m_Map.size() < 100) return;
	for (std::unordered_map<long long, Chunk*>::iterator it = m_Map.begin(); it != m_Map.end(); )
	{
		Chunk* c = it->second;
		if (abs(c->cx - ccx) > radiusChunks + 2 || abs(c->cy - ccy) > radiusChunks + 2)
		{
			delete c;
			it = m_Map.erase(it);
		}
		else ++it;
	}
}

static void Split(int w, int* c, int* l)
{
	int cc = (int)floorf(w / (float)CHUNK);
	*c = cc;
	*l = w - cc * CHUNK;
}

unsigned char World::G(int wx, int wy)
{
	int cx, lx, cy, ly;
	Split(wx, &cx, &lx); Split(wy, &cy, &ly);
	Chunk* c = Get(cx, cy);
	return c->g[ly * CHUNK + lx];
}

unsigned char World::O(int wx, int wy)
{
	int cx, lx, cy, ly;
	Split(wx, &cx, &lx); Split(wy, &cy, &ly);
	Chunk* c = Get(cx, cy);
	unsigned char o = c->o[ly * CHUNK + lx];
	if (o == O_RUIN && m_Taken.find(Key(wx, wy)) != m_Taken.end()) return O_CRATE;
	return o;
}

unsigned char World::V(int wx, int wy)
{
	int cx, lx, cy, ly;
	Split(wx, &cx, &lx); Split(wy, &cy, &ly);
	Chunk* c = Get(cx, cy);
	return c->v[ly * CHUNK + lx];
}

bool World::Blocked(int wx, int wy)
{
	unsigned char g = G(wx, wy);
	if (g == G_WATER) return true;
	unsigned char o = O(wx, wy);
	return (o == O_TREE || o == O_PINE || o == O_HOUSE || o == O_FENCE ||
		o == O_WELL || o == O_ROCK);
}

bool World::TakeRuin(int wx, int wy, int* recordIdx)
{
	if (O(wx, wy) != O_RUIN) return false;
	m_Taken.insert(Key(wx, wy));
	*recordIdx = (int)(Hash3(wx, wy, 313) % FIELD_RECORD_COUNT);
	return true;
}
