#pragma once

class World;

struct Beast
{
	float x, y;
	float vx, vy;
	float timer, walk;
	int kind;
	int state;        // 0 배회 1 접근 2 도주
	bool moving;
};

// 플레이어 주변에 상주하는 야생 짐승. 멀어지면 재배치된다.
class Beasts
{
public:
	void Init(World& w, float px, float py);
	void Update(float dt, World& w, float px, float py);

	int Count() const { return m_N; }
	const Beast& At(int i) const { return m_B[i]; }

private:
	void Spawn(Beast& b, World& w, float px, float py);
	float Rnd();

	static const int MAX_BEASTS = 16;
	Beast m_B[MAX_BEASTS];
	int m_N = 0;
	unsigned int m_Seed = 20260909u;
};
