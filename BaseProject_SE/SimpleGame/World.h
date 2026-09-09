#pragma once

#include <unordered_map>
#include <unordered_set>

// 청크 단위로 필요할 때 생성하는 무한 절차적 월드.
// 원점 주변이 마을(수작업 배치), 그 밖은 절차 생성.
const int CHUNK = 32;

enum Ground : unsigned char
{
	G_GRASS = 0, G_DIRT, G_PATH, G_PLAZA, G_WATER, G_SHORE, G_MARSH
};

enum Obj : unsigned char
{
	O_NONE = 0, O_TREE, O_PINE, O_HOUSE, O_FENCE, O_WELL, O_ROCK,
	O_TUFT, O_FLOWER, O_REED, O_STUMP, O_LAMP, O_RUIN, O_CRATE
};

struct Chunk
{
	int cx, cy;
	unsigned char g[CHUNK * CHUNK];
	unsigned char o[CHUNK * CHUNK];
	unsigned char v[CHUNK * CHUNK];   // 변주값 (색·크기 흔들림)
};

struct Npc
{
	float hx, hy;
	const char* name;              // 표시 이름 (한글)
	const char* const* lines;
	int lineCount;
	int recordId;                  // 0..2 마을 결손 기록, -1 아님
	bool isElder;
	float phase;
	float r, g, b;
};

const int NPC_COUNT = 9;
const int FIELD_RECORD_COUNT = 8;

class World
{
public:
	void Init();
	~World();

	void EnsureAround(float wx, float wy, int radiusChunks);

	unsigned char G(int wx, int wy);
	unsigned char O(int wx, int wy);
	unsigned char V(int wx, int wy);
	bool Blocked(int wx, int wy);

	bool TakeRuin(int wx, int wy, int* recordIdx);
	int LoadedChunks() const { return (int)m_Map.size(); }

	Npc npcs[NPC_COUNT];

	static const int START_X = 0;
	static const int START_Y = 4;

private:
	Chunk* Get(int cx, int cy);
	Chunk* Generate(int cx, int cy);
	void ApplyVillage(Chunk* c);

	std::unordered_map<long long, Chunk*> m_Map;
	std::unordered_set<long long> m_Taken;
};

const char* const* World_ElderIntro(int* count);
const char* const* World_ElderDone(int* count);
const char* World_FieldRecord(int idx);
int  World_RoadY(int wx);
int  World_RoadX(int wy);
