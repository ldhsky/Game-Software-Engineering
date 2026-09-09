#include "stdafx.h"
#include "World.h"

#include <cmath>

/* ---------- 대사 ---------- */

static const char* L_ELDER_0[] = {
	"[촌장] 처음 보는 얼굴이군. 길드에서 왔소?",
	"[촌장] ...이상하지. 자네를 본 적이 없는데 낯설지가 않아.",
	"[촌장] 요 며칠 마을이 조용해. 조용한 게 아니라, 뭔가 빠져 있어.",
	"[촌장] 세 사람을 만나보게. 어부, 나무꾼, 우물가 아이.",
	"[촌장] 무엇이 빠졌는지 적어서 가져오면 접수해 주지. 규정이니까.",
};
static const char* L_ELDER_DONE[] = {
	"[촌장] 세 건... 전부 같은 날이었군.",
	"[촌장] 우리 마을이 무언가를 얻었다는 뜻이야. 아무도 그게 뭔지 기억하지 못하고.",
	"[촌장] 자네 이름을 접수 서류에 적어야 하는데.",
	"[촌장] ...자네, 이름이 뭐였지?",
};
static const char* L_FISHER[] = {
	"[어부] 오늘도 안 보여요.",
	"[어부] 호수요. 저 앞에 있는 거 아는데, 눈에 안 들어옵니다.",
	"[어부] 물소리는 들려요. 그물도 던져요. 그런데 물이 안 보여.",
	"[어부] 아들은 돌아왔는데. 어디서 빠졌는지는 이제 모르겠고.",
};
static const char* L_WOODSMAN[] = {
	"[나무꾼] 손이 도구를 기억하지 못합니다.",
	"[나무꾼] 도끼를 쥐면 매번 처음 쥐는 것 같아요. 20년을 썼는데.",
	"[나무꾼] 나무는 넘어갑니다. 다만 어떻게 넘겼는지 남지 않아요.",
};
static const char* L_CHILD[] = {
	"[아이] 그림자 못 봤어요?",
	"[아이] 어제까지 있었는데, 오늘 아침엔 없어요.",
	"[아이] 엄마는 원래 없었다고 해요. 근데 나는 기억나요.",
};
static const char* L_SMITH[] = {
	"[대장장이] 길드 사람은 값을 안 냅니까? 나는 냅니다. 매달.",
	"[대장장이] 환산표대로 냈는데 표가 틀렸다더군. 환불은 규정에 없다고.",
};
static const char* L_PEDDLER[] = {
	"[행상인] 최신판 환산표! 작년 것보다 12퍼센트 정확합니다!",
	"[행상인] 뭐가 12퍼센트냐고요? 그건 표 뒷장에 있습니다. 뒷장은 별매입니다.",
	"[행상인] 손님, 값을 모르고 거래하는 것보다는 틀린 표라도 있는 게 낫지 않습니까?",
};
static const char* L_PRIEST[] = {
	"[사제] 죽음은 신의 영역이오. 값을 매길 수 없소.",
	"[사제] ...우리가 하는 건 다릅니다. 그건 기적이라고 부르지요.",
};
static const char* L_GUARD[] = {
	"[순찰병] 밤엔 숲으로 들어가지 마시오.",
	"[순찰병] 뭐가 있어서가 아니오. 들어간 사람이 뭔가를 두고 나와서 그렇소.",
};

/* ---------- 지형 생성 ---------- */

static unsigned int Hash(int x, int y)
{
	unsigned int h = (unsigned int)(x * 374761393 + y * 668265263);
	h = (h ^ (h >> 13)) * 1274126177;
	return h ^ (h >> 16);
}

void World::Generate()
{
	for (int y = 0; y < MAP_H; ++y)
		for (int x = 0; x < MAP_W; ++x)
			m_Tiles[y][x] = T_GRASS;

	// 호수 — 마을 남서쪽
	const float lx = 13.f, ly = 33.f;
	for (int y = 0; y < MAP_H; ++y)
		for (int x = 0; x < MAP_W; ++x)
		{
			float d = sqrtf((x - lx) * (x - lx) + (y - ly) * (y - ly));
			float wob = (Hash(x, y) % 100) * 0.012f;
			if (d < 6.2f + wob)       m_Tiles[y][x] = T_WATER;
			else if (d < 7.4f + wob)  m_Tiles[y][x] = T_SHORE;
		}

	// 숲 — 마을에서 멀어질수록 빽빽하게
	const float cx = 24.f, cy = 24.f;
	for (int y = 0; y < MAP_H; ++y)
		for (int x = 0; x < MAP_W; ++x)
		{
			if (m_Tiles[y][x] != T_GRASS) continue;
			float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
			if (d < 9.f) continue;
			unsigned int density = (unsigned int)(20 + (d - 9.f) * 7.f);
			if (density > 78) density = 78;
			if (Hash(x, y) % 100 < density) m_Tiles[y][x] = T_TREE;
		}

	// 길 — 십자로
	for (int x = 12; x < 36; ++x) m_Tiles[24][x] = T_PATH;
	for (int y = 12; y < 36; ++y) m_Tiles[y][24] = T_PATH;
	// 호수로 가는 갈림길
	for (int i = 0; i < 12; ++i)
	{
		int x = 24 - i, y = 24 + (i * 2) / 3;
		if (x > 0 && y < MAP_H - 1 && m_Tiles[y][x] != T_WATER) m_Tiles[y][x] = T_PATH;
	}
	// 숲으로 가는 갈림길
	for (int i = 0; i < 10; ++i)
	{
		int x = 25 + i, y = 26 + (i * 2) / 3;
		if (x < MAP_W - 1 && y < MAP_H - 1) m_Tiles[y][x] = T_PATH;
	}

	// 광장
	for (int y = 22; y <= 26; ++y)
		for (int x = 22; x <= 26; ++x)
			m_Tiles[y][x] = T_PLAZA;

	// 집 — 2x2 블록
	const int houses[][2] = { {19,21},{28,20},{19,28},{29,27},{27,22},{20,25} };
	for (int i = 0; i < 6; ++i)
		for (int dy = 0; dy < 2; ++dy)
			for (int dx = 0; dx < 2; ++dx)
				m_Tiles[houses[i][1] + dy][houses[i][0] + dx] = T_HOUSE;

	// 우물
	m_Tiles[26][27] = T_WELL;

	// 마을 울타리 일부
	for (int x = 18; x <= 30; ++x)
	{
		if (m_Tiles[18][x] == T_GRASS) m_Tiles[18][x] = T_FENCE;
		if (m_Tiles[31][x] == T_GRASS) m_Tiles[31][x] = T_FENCE;
	}

	/* ---------- NPC ---------- */
	int i = 0;
	npcs[i++] = { 24.f, 22.5f, "ELDER",    L_ELDER_0,   5, -1, true,  0.0f, 0.86f, 0.80f, 0.62f };
	npcs[i++] = { 16.5f, 30.5f, "FISHER",  L_FISHER,    4,  0, false, 1.1f, 0.55f, 0.70f, 0.72f };
	npcs[i++] = { 31.f, 29.f,  "WOODSMAN", L_WOODSMAN,  3,  1, false, 2.3f, 0.62f, 0.52f, 0.38f };
	npcs[i++] = { 27.6f, 25.2f, "CHILD",   L_CHILD,     3,  2, false, 0.7f, 0.80f, 0.74f, 0.55f };
	npcs[i++] = { 20.8f, 24.2f, "SMITH",   L_SMITH,     2, -1, false, 3.1f, 0.58f, 0.45f, 0.40f };
	npcs[i++] = { 25.6f, 23.2f, "PEDDLER", L_PEDDLER,   3, -1, false, 1.7f, 0.72f, 0.62f, 0.34f };
	npcs[i++] = { 22.4f, 26.6f, "PRIEST",  L_PRIEST,    2, -1, false, 2.7f, 0.68f, 0.68f, 0.72f };
	npcs[i++] = { 24.4f, 19.2f, "GUARD",   L_GUARD,     2, -1, false, 0.4f, 0.48f, 0.52f, 0.50f };
}

const char* const* World_ElderDoneLines(int* count)
{
	*count = 4;
	return L_ELDER_DONE;
}

unsigned char World::At(int x, int y) const
{
	if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return T_TREE;
	return m_Tiles[y][x];
}

bool World::Blocked(int x, int y) const
{
	unsigned char t = At(x, y);
	return (t == T_WATER || t == T_TREE || t == T_HOUSE || t == T_FENCE || t == T_WELL);
}

float World::Height(int x, int y) const
{
	switch (At(x, y))
	{
	case T_TREE:  return 1.7f;
	case T_HOUSE: return 2.1f;
	case T_FENCE: return 0.5f;
	case T_WELL:  return 0.6f;
	default:      return 0.f;
	}
}
