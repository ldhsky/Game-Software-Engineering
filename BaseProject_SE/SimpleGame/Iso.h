#pragma once
// 아이소메트릭 좌표 변환 (2:1 쿼터뷰).
// worldZ(화면상 높이)와 Depth(앞뒤 정렬)는 별개 개념이다. CLAUDE.md 4절 참조.
namespace Iso
{
	const float TILE_W = 64.f;
	const float TILE_H = 32.f;

	// 월드 격자 -> 화면 픽셀 (원점 = 화면 중앙, +y 위쪽)
	inline void WorldToScreen(float wx, float wy, float wz, float* sx, float* sy)
	{
		*sx = (wx - wy) * (TILE_W * 0.5f);
		*sy = -((wx + wy) * (TILE_H * 0.5f)) + wz * TILE_H;
	}

	// 화면 픽셀 -> 월드 격자 (z=0 평면). 마우스 피킹·가시 범위 계산에 사용.
	inline void ScreenToWorld(float sx, float sy, float* wx, float* wy)
	{
		float a = sx / (TILE_W * 0.5f);
		float b = -sy / (TILE_H * 0.5f);
		*wx = (a + b) * 0.5f;
		*wy = (b - a) * 0.5f;
	}

	inline float Depth(float wx, float wy) { return wx + wy; }
}
