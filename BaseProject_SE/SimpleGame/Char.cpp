#include "stdafx.h"
#include "Char.h"
#include "Renderer.h"

#include <cmath>

static unsigned int H(unsigned int x)
{
	x = (x ^ 61u) ^ (x >> 16); x *= 9u; x = x ^ (x >> 4); x *= 0x27d4eb2du;
	return x ^ (x >> 15);
}
static float F01(unsigned int s) { return (H(s) & 0xFFFF) / 65535.f; }

CharStyle MakeStyle(float r, float g, float b, int build, int hat, bool child, unsigned int seed)
{
	CharStyle s;
	s.cloth[0] = r; s.cloth[1] = g; s.cloth[2] = b;
	// 외투는 옷보다 어둡고 살짝 푸르게
	s.cloak[0] = r * 0.52f; s.cloak[1] = g * 0.52f; s.cloak[2] = b * 0.60f;
	float sk = 0.62f + F01(seed * 7u + 1u) * 0.22f;
	s.skin[0] = sk; s.skin[1] = sk * 0.82f; s.skin[2] = sk * 0.68f;
	float hv = 0.10f + F01(seed * 13u + 5u) * 0.16f;
	s.hair[0] = hv * 1.25f; s.hair[1] = hv; s.hair[2] = hv * 0.85f;
	s.accent[0] = 0.46f; s.accent[1] = 0.14f; s.accent[2] = 0.10f;
	s.height = child ? 0.70f : (0.94f + F01(seed * 17u + 3u) * 0.16f);
	s.build = build; s.hat = hat; s.cloakOn = (hat == 3) || (F01(seed * 5u) > 0.55f);
	s.child = child;
	return s;
}

static float g_FogT = 0.f;
static float g_FogC[3] = { 0.f, 0.f, 0.f };

void CharSetFog(float t, float r, float g, float b)
{
	g_FogT = t; g_FogC[0] = r; g_FogC[1] = g; g_FogC[2] = b;
}

static inline void Lit(float* c, float lr, float lg, float lb)
{
	for (int i = 0; i < 3; ++i) c[i] = c[i] * (1.f - g_FogT) + g_FogC[i] * g_FogT;
	c[0] += lr; c[1] += lg; c[2] += lb;
}

void DrawHuman(Renderer& R, float sx, float sy, float dirX, float dirY,
	float walk, bool moving, const CharStyle& st,
	float lr, float lg, float lb, float t)
{
	const float S = st.height;
	// 쿼터뷰 4방향 — 깊이가 늘어나는 쪽이 정면
	bool front = (dirX + dirY) >= 0.f;
	float mir = ((dirX - dirY) < 0.f) ? -1.f : 1.f;

	float sw = moving ? sinf(walk * 10.5f) : 0.f;
	float sw2 = moving ? sinf(walk * 10.5f + 3.14159f) : 0.f;
	float bob = moving ? fabsf(sinf(walk * 10.5f)) * 2.0f
		: sinf(t * 2.0f) * 0.55f;

	float cloak[3] = { st.cloak[0], st.cloak[1], st.cloak[2] }; Lit(cloak, lr, lg, lb);
	float cloth[3] = { st.cloth[0], st.cloth[1], st.cloth[2] }; Lit(cloth, lr, lg, lb);
	float dark[3] = { st.cloth[0] * 0.45f, st.cloth[1] * 0.45f, st.cloth[2] * 0.52f }; Lit(dark, lr, lg, lb);
	float skin[3] = { st.skin[0], st.skin[1], st.skin[2] }; Lit(skin, lr, lg, lb);
	float hair[3] = { st.hair[0], st.hair[1], st.hair[2] }; Lit(hair, lr, lg, lb);
	float acc[3] = { st.accent[0], st.accent[1], st.accent[2] }; Lit(acc, lr, lg, lb);

	float bodyW = (st.build == 0 ? 10.f : (st.build == 2 ? 15.f : 12.5f)) * S;
	float legTop = 15.f * S, headY = 31.f * S, headR = 5.2f * S;

	// 다리
	for (int i = 0; i < 2; ++i)
	{
		float s = (i == 0) ? sw : sw2;
		float ox = (i == 0 ? -3.4f : 3.4f) * S * mir;
		float lift = moving ? (s > 0.f ? s * 2.6f : 0.f) : 0.f;
		R.Quad(sx + ox - 2.3f * S, sy + lift,
			sx + ox - 2.3f * S + s * 2.2f, sy + legTop,
			sx + ox + 2.3f * S + s * 2.2f, sy + legTop,
			sx + ox + 2.3f * S, sy + lift,
			dark[0], dark[1], dark[2]);
		// 신발
		R.Rect(sx + ox + s * 0.6f, sy + 1.6f * S + lift, 5.6f * S, 3.2f * S,
			dark[0] * 0.62f, dark[1] * 0.62f, dark[2] * 0.62f);
	}

	// 외투 (뒤쪽)
	if (st.cloakOn && !front)
		R.Quad(sx - bodyW * 0.62f, sy + 5.f * S, sx - bodyW * 0.52f, sy + legTop + 14.f * S + bob,
			sx + bodyW * 0.52f, sy + legTop + 14.f * S + bob, sx + bodyW * 0.62f, sy + 5.f * S,
			cloak[0], cloak[1], cloak[2]);

	// 몸통
	float ty0 = sy + legTop - 1.f, ty1 = sy + legTop + 14.f * S + bob;
	R.Quad(sx - bodyW * 0.5f, ty0, sx - bodyW * 0.44f, ty1,
		sx + bodyW * 0.44f, ty1, sx + bodyW * 0.5f, ty0,
		cloth[0], cloth[1], cloth[2]);
	// 몸통 좌측 음영
	R.Quad(sx - bodyW * 0.5f, ty0, sx - bodyW * 0.44f, ty1,
		sx - bodyW * 0.12f, ty1, sx - bodyW * 0.16f, ty0,
		cloth[0] * 0.72f, cloth[1] * 0.72f, cloth[2] * 0.76f);
	// 허리띠
	R.Rect(sx, ty0 + 3.f, bodyW * 0.98f, 2.6f * S, dark[0], dark[1], dark[2]);

	// 팔
	for (int i = 0; i < 2; ++i)
	{
		float s = (i == 0) ? sw2 : sw;
		float ox = (i == 0 ? -1.f : 1.f) * (bodyW * 0.52f) * mir;
		R.Quad(sx + ox - 1.9f * S, ty1 - 2.f, sx + ox - 1.9f * S + s * 2.6f, ty0 + 1.f,
			sx + ox + 1.9f * S + s * 2.6f, ty0 + 1.f, sx + ox + 1.9f * S, ty1 - 2.f,
			cloth[0] * 0.86f, cloth[1] * 0.86f, cloth[2] * 0.88f);
		// 손
		R.Rect(sx + ox + s * 2.6f, ty0 + 0.5f, 3.2f * S, 3.2f * S, skin[0], skin[1], skin[2]);
	}

	// 외투 (앞쪽 — 어깨만)
	if (st.cloakOn && front)
		R.Quad(sx - bodyW * 0.58f, ty1 - 5.f, sx - bodyW * 0.5f, ty1 + 1.f,
			sx + bodyW * 0.5f, ty1 + 1.f, sx + bodyW * 0.58f, ty1 - 5.f,
			cloak[0], cloak[1], cloak[2]);

	// 목
	R.Rect(sx, ty1 + 1.5f, 4.2f * S, 3.4f * S, skin[0] * 0.86f, skin[1] * 0.86f, skin[2] * 0.86f);

	// 머리
	float hy = sy + headY + bob;
	R.Ellipse(sx, hy, headR, headR * 1.06f, 12, skin[0], skin[1], skin[2], 1.f);
	// 얼굴 음영
	R.Ellipse(sx - headR * 0.42f * mir, hy, headR * 0.58f, headR * 0.95f, 10,
		skin[0] * 0.80f, skin[1] * 0.80f, skin[2] * 0.80f, 1.f);

	if (front)
	{
		R.Rect(sx - 1.9f * S * mir, hy + 0.6f * S, 1.5f * S, 1.7f * S, 0.06f, 0.05f, 0.06f);
		R.Rect(sx + 1.9f * S * mir, hy + 0.6f * S, 1.5f * S, 1.7f * S, 0.06f, 0.05f, 0.06f);
	}

	// 머리카락 / 모자
	if (st.hat == 0)
	{
		R.Ellipse(sx, hy + headR * 0.42f, headR * 1.04f, headR * 0.72f, 12, hair[0], hair[1], hair[2], 1.f);
		if (!front) R.Ellipse(sx, hy - headR * 0.1f, headR * 0.95f, headR * 0.85f, 12, hair[0], hair[1], hair[2], 1.f);
	}
	else if (st.hat == 1)   // 두건
	{
		R.Ellipse(sx, hy + headR * 0.40f, headR * 1.10f, headR * 0.80f, 12, cloth[0], cloth[1], cloth[2], 1.f);
		R.Quad(sx - headR * 1.1f, hy + headR * 0.3f, sx - headR * 0.7f, hy - headR * 0.8f,
			sx - headR * 0.2f, hy - headR * 0.6f, sx - headR * 0.3f, hy + headR * 0.4f,
			cloth[0] * 0.8f, cloth[1] * 0.8f, cloth[2] * 0.84f);
	}
	else if (st.hat == 2)   // 챙모자
	{
		R.Ellipse(sx, hy + headR * 0.55f, headR * 1.75f, headR * 0.62f, 14,
			dark[0] * 1.1f, dark[1] * 1.1f, dark[2] * 1.1f, 1.f);
		R.Ellipse(sx, hy + headR * 1.05f, headR * 0.86f, headR * 0.60f, 12,
			dark[0] * 1.25f, dark[1] * 1.25f, dark[2] * 1.25f, 1.f);
	}
	else                    // 후드
	{
		R.Ellipse(sx, hy + headR * 0.25f, headR * 1.28f, headR * 1.16f, 14,
			cloak[0] * 1.1f, cloak[1] * 1.1f, cloak[2] * 1.1f, 1.f);
		R.Ellipse(sx + headR * 0.15f * mir, hy - headR * 0.05f, headR * 0.82f, headR * 0.86f, 12,
			skin[0] * 0.62f, skin[1] * 0.62f, skin[2] * 0.66f, 1.f);
	}

	// 소품 — 허리의 장부
	if (!st.child)
		R.Rect(sx + bodyW * 0.46f * mir, ty0 + 4.f, 4.6f * S, 6.4f * S, acc[0], acc[1], acc[2]);
}

/* ---------- 야생 짐승 ---------- */

const char* BeastName(int kind)
{
	switch (kind)
	{
	case BK_WOLF: return "늑대";
	case BK_DEER: return "사슴";
	case BK_BOAR: return "멧돼지";
	default:      return "까마귀";
	}
}

void DrawBeast(Renderer& R, float sx, float sy, float dirX, float dirY,
	float walk, bool moving, int kind, float lr, float lg, float lb, float t)
{
	float mir = ((dirX - dirY) < 0.f) ? -1.f : 1.f;
	float sw = moving ? sinf(walk * 13.f) : 0.f;
	float sw2 = moving ? sinf(walk * 13.f + 3.14159f) : 0.f;

	float body[3], leg[3], acc[3];
	float bodyLen = 22.f, bodyH = 8.f, legH = 10.f, headUp = 12.f;

	switch (kind)
	{
	case BK_WOLF:
		body[0] = 0.20f; body[1] = 0.19f; body[2] = 0.20f;
		bodyLen = 23.f; bodyH = 7.5f; legH = 10.f; headUp = 11.f; break;
	case BK_DEER:
		body[0] = 0.30f; body[1] = 0.22f; body[2] = 0.15f;
		bodyLen = 21.f; bodyH = 7.f; legH = 14.f; headUp = 17.f; break;
	case BK_BOAR:
		body[0] = 0.17f; body[1] = 0.13f; body[2] = 0.11f;
		bodyLen = 21.f; bodyH = 10.f; legH = 7.f; headUp = 8.f; break;
	default:
		body[0] = 0.075f; body[1] = 0.075f; body[2] = 0.090f;
		bodyLen = 11.f; bodyH = 5.f; legH = 4.f; headUp = 5.f; break;
	}
	leg[0] = body[0] * 0.66f; leg[1] = body[1] * 0.66f; leg[2] = body[2] * 0.70f;
	Lit(body, lr, lg, lb); Lit(leg, lr, lg, lb);
	acc[0] = 0.42f; acc[1] = 0.38f; acc[2] = 0.30f; Lit(acc, lr, lg, lb);

	if (kind == BK_CROW)
	{
		float flap = moving ? sinf(t * 16.f) * 6.f : sinf(t * 2.f) * 0.8f;
		R.Ellipse(sx, sy + legH + 2.f, bodyLen * 0.5f, bodyH * 0.6f, 10, body[0], body[1], body[2], 1.f);
		R.Tri(sx, sy + legH + 3.f, sx - 11.f * mir, sy + legH + 3.f + flap,
			sx - 3.f * mir, sy + legH + 6.f, body[0] * 1.3f, body[1] * 1.3f, body[2] * 1.4f);
		R.Tri(sx, sy + legH + 3.f, sx + 11.f * mir, sy + legH + 3.f - flap,
			sx + 3.f * mir, sy + legH + 6.f, body[0] * 1.1f, body[1] * 1.1f, body[2] * 1.2f);
		R.Ellipse(sx + 6.f * mir, sy + legH + 7.f, 3.2f, 3.f, 8, body[0] * 1.2f, body[1] * 1.2f, body[2] * 1.3f, 1.f);
		R.Tri(sx + 9.f * mir, sy + legH + 7.5f, sx + 14.f * mir, sy + legH + 6.6f,
			sx + 9.f * mir, sy + legH + 6.f, 0.52f, 0.42f, 0.16f);
		R.Rect(sx - 1.f, sy + legH * 0.5f, 1.4f, legH, leg[0], leg[1], leg[2]);
		return;
	}

	// 다리 네 개
	for (int i = 0; i < 4; ++i)
	{
		float ox = ((i < 2) ? -bodyLen * 0.30f : bodyLen * 0.30f) * mir
			+ ((i % 2) ? 2.2f : -2.2f);
		float s = ((i == 0 || i == 3) ? sw : sw2) * 2.4f;
		R.Quad(sx + ox - 1.6f, sy, sx + ox - 1.6f + s, sy + legH,
			sx + ox + 1.6f + s, sy + legH, sx + ox + 1.6f, sy,
			leg[0], leg[1], leg[2]);
	}

	// 몸통
	R.Ellipse(sx, sy + legH + bodyH * 0.5f, bodyLen * 0.5f, bodyH * 0.62f, 14,
		body[0], body[1], body[2], 1.f);
	// 등의 밝은 면
	R.Ellipse(sx, sy + legH + bodyH * 0.85f, bodyLen * 0.40f, bodyH * 0.34f, 12,
		body[0] * 1.28f, body[1] * 1.28f, body[2] * 1.24f, 1.f);

	// 목·머리
	float hx = sx + bodyLen * 0.44f * mir;
	R.Quad(hx - 3.5f, sy + legH + bodyH * 0.3f, hx - 2.5f, sy + headUp + 4.f,
		hx + 3.5f, sy + headUp + 4.f, hx + 3.5f, sy + legH + bodyH * 0.3f,
		body[0] * 0.92f, body[1] * 0.92f, body[2] * 0.94f);
	R.Ellipse(hx + 1.f * mir, sy + headUp + 6.f, 5.2f, 4.4f, 12, body[0], body[1], body[2], 1.f);

	if (kind == BK_WOLF)
	{
		// 주둥이·귀·꼬리, 눈빛
		R.Tri(hx + 3.f * mir, sy + headUp + 6.6f, hx + 11.f * mir, sy + headUp + 4.6f,
			hx + 3.f * mir, sy + headUp + 3.2f, body[0] * 0.85f, body[1] * 0.85f, body[2] * 0.88f);
		R.Tri(hx - 1.f * mir, sy + headUp + 9.f, hx + 2.5f * mir, sy + headUp + 15.f,
			hx + 4.5f * mir, sy + headUp + 9.f, body[0] * 1.1f, body[1] * 1.1f, body[2] * 1.15f);
		R.Rect(hx + 4.f * mir, sy + headUp + 7.4f, 1.8f, 1.8f, 1.7f, 1.25f, 0.55f);
		R.Quad(sx - bodyLen * 0.5f * mir, sy + legH + bodyH * 0.7f,
			sx - bodyLen * 0.86f * mir, sy + legH + bodyH * 1.15f + sw * 2.f,
			sx - bodyLen * 0.86f * mir, sy + legH + bodyH * 0.75f + sw * 2.f,
			sx - bodyLen * 0.5f * mir, sy + legH + bodyH * 0.35f,
			body[0] * 1.15f, body[1] * 1.15f, body[2] * 1.2f);
	}
	else if (kind == BK_DEER)
	{
		// 뿔
		for (int i = 0; i < 2; ++i)
		{
			float ox = (i ? 2.6f : -1.4f) * mir;
			R.Quad(hx + ox - 0.8f, sy + headUp + 9.f, hx + ox - 0.8f + 3.f * mir, sy + headUp + 19.f,
				hx + ox + 0.8f + 3.f * mir, sy + headUp + 19.f, hx + ox + 0.8f, sy + headUp + 9.f,
				acc[0] * 0.7f, acc[1] * 0.62f, acc[2] * 0.5f);
			R.Quad(hx + ox + 2.4f * mir, sy + headUp + 14.f, hx + ox + 7.f * mir, sy + headUp + 17.f,
				hx + ox + 7.f * mir, sy + headUp + 15.6f, hx + ox + 2.4f * mir, sy + headUp + 12.8f,
				acc[0] * 0.7f, acc[1] * 0.62f, acc[2] * 0.5f);
		}
		// 흰 반점
		R.Ellipse(sx - 3.f, sy + legH + bodyH * 0.9f, 1.7f, 1.1f, 8, 0.62f, 0.56f, 0.46f, 0.75f);
		R.Ellipse(sx + 4.f, sy + legH + bodyH * 0.75f, 1.5f, 1.f, 8, 0.62f, 0.56f, 0.46f, 0.6f);
	}
	else if (kind == BK_BOAR)
	{
		R.Tri(hx + 3.f * mir, sy + headUp + 6.4f, hx + 10.f * mir, sy + headUp + 5.2f,
			hx + 3.f * mir, sy + headUp + 2.6f, body[0] * 0.8f, body[1] * 0.8f, body[2] * 0.84f);
		R.Tri(hx + 8.f * mir, sy + headUp + 5.f, hx + 13.f * mir, sy + headUp + 9.f,
			hx + 8.5f * mir, sy + headUp + 4.f, 0.72f, 0.68f, 0.56f);
		// 등 갈기
		for (int i = 0; i < 5; ++i)
			R.Tri(sx - 7.f + i * 3.6f, sy + legH + bodyH * 1.05f,
				sx - 6.f + i * 3.6f, sy + legH + bodyH * 1.05f + 5.f,
				sx - 5.f + i * 3.6f, sy + legH + bodyH * 1.05f,
				body[0] * 1.4f, body[1] * 1.4f, body[2] * 1.4f);
	}
}
