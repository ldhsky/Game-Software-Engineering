#pragma once

// 튜토리얼 레벨 — 작은 마을, 주변의 숲과 호수.
enum TileType
{
	T_GRASS = 0, T_PATH, T_PLAZA, T_WATER, T_SHORE, T_TREE, T_HOUSE, T_FENCE, T_WELL
};

const int MAP_W = 48;
const int MAP_H = 48;

struct Npc
{
	float hx, hy;              // 고정 위치 (월드 격자)
	const char* label;         // 화면 표기 (ASCII — 화면 텍스트는 비트맵 폰트라 한글 불가)
	const char* const* lines;  // 대사 (콘솔 출력, 한글)
	int lineCount;
	int recordId;              // 0..2 = 결손 기록 대상, -1 = 아님
	bool isElder;              // 보고 대상
	float phase;               // 제자리 흔들림 위상
	float r, g, b;             // 표시 색
};

const int NPC_COUNT = 8;

class World
{
public:
	void Generate();
	unsigned char At(int x, int y) const;
	bool Blocked(int x, int y) const;
	float Height(int x, int y) const;    // 타일 위 구조물 높이 (0 = 평지)

	Npc npcs[NPC_COUNT];

	static const int START_X = 24;
	static const int START_Y = 26;

private:
	unsigned char m_Tiles[MAP_H][MAP_W];
};
