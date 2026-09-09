#include "stdafx.h"
#include "Actors.h"
#include "World.h"
#include "Char.h"

#include <cmath>

float Beasts::Rnd()
{
	m_Seed = m_Seed * 1664525u + 1013904223u;
	return ((m_Seed >> 8) & 0xFFFF) / 65535.f;
}

void Beasts::Spawn(Beast& b, World& w, float px, float py)
{
	for (int tries = 0; tries < 40; ++tries)
	{
		float ang = Rnd() * 6.2831853f;
		float dist = 16.f + Rnd() * 14.f;
		float x = px + cosf(ang) * dist;
		float y = py + sinf(ang) * dist;
		if (w.Blocked((int)floorf(x + 0.5f), (int)floorf(y + 0.5f))) continue;

		unsigned char g = w.G((int)floorf(x + 0.5f), (int)floorf(y + 0.5f));
		float r = Rnd();
		int kind;
		if (g == G_MARSH || g == G_SHORE) kind = (r < 0.55f) ? BK_BOAR : BK_CROW;
		else if (r < 0.30f) kind = BK_WOLF;
		else if (r < 0.68f) kind = BK_DEER;
		else if (r < 0.86f) kind = BK_BOAR;
		else kind = BK_CROW;

		b.x = x; b.y = y;
		b.vx = 0.f; b.vy = 0.f;
		b.kind = kind;
		b.timer = Rnd() * 2.f;
		b.walk = Rnd() * 10.f;
		b.state = 0;
		b.moving = false;
		return;
	}
	b.x = px + 20.f; b.y = py + 20.f;
}

void Beasts::Init(World& w, float px, float py)
{
	m_N = MAX_BEASTS;
	for (int i = 0; i < m_N; ++i) Spawn(m_B[i], w, px, py);
}

void Beasts::Update(float dt, World& w, float px, float py)
{
	for (int i = 0; i < m_N; ++i)
	{
		Beast& b = m_B[i];

		float dx = px - b.x, dy = py - b.y;
		float d = sqrtf(dx * dx + dy * dy);
		if (d > 46.f) { Spawn(b, w, px, py); continue; }

		float speed = 1.5f;
		switch (b.kind)
		{
		case BK_WOLF: speed = 2.6f; break;
		case BK_DEER: speed = 3.4f; break;
		case BK_BOAR: speed = 1.9f; break;
		default:      speed = 3.0f; break;
		}

		b.timer -= dt;

		// 늑대는 다가오고, 사슴과 까마귀는 달아나고, 멧돼지는 무심하다
		if (b.kind == BK_WOLF && d < 11.f)
		{
			b.state = 1;
			if (d > 4.f) { b.vx = dx / d; b.vy = dy / d; }
			else { b.vx = -dy / d; b.vy = dx / d; }   // 주변을 돈다
		}
		else if ((b.kind == BK_DEER || b.kind == BK_CROW) && d < 8.5f)
		{
			b.state = 2;
			b.vx = -dx / d; b.vy = -dy / d;
			speed *= 1.5f;
		}
		else if (b.timer <= 0.f)
		{
			b.state = 0;
			b.timer = 1.4f + Rnd() * 3.2f;
			if (Rnd() < 0.32f) { b.vx = 0.f; b.vy = 0.f; }
			else
			{
				float a = Rnd() * 6.2831853f;
				b.vx = cosf(a); b.vy = sinf(a);
			}
		}

		b.moving = (fabsf(b.vx) + fabsf(b.vy)) > 0.01f;
		if (!b.moving) continue;

		b.walk += dt;
		float nx = b.x + b.vx * speed * dt;
		float ny = b.y + b.vy * speed * dt;

		bool hitX = w.Blocked((int)floorf(nx + 0.5f), (int)floorf(b.y + 0.5f));
		bool hitY = w.Blocked((int)floorf(b.x + 0.5f), (int)floorf(ny + 0.5f));
		if (!hitX) b.x = nx; else b.vx = -b.vx;
		if (!hitY) b.y = ny; else b.vy = -b.vy;
	}
}
