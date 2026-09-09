#pragma once

class Renderer;

// 부위별로 나눠 그리고 걸음 위상으로 움직이는 캐릭터.
// 외부 이미지 없이 절차적 스프라이트 애니메이션을 만든다.
struct CharStyle
{
	float cloak[3];
	float cloth[3];
	float skin[3];
	float hair[3];
	float accent[3];
	float height;     // 0.82 ~ 1.15
	int   build;      // 0 마름 1 보통 2 다부짐
	int   hat;        // 0 없음 1 두건 2 모자 3 후드
	bool  cloakOn;
	bool  child;
};

// 캐릭터에 적용할 거리 안개 — 그리기 전에 설정한다
void CharSetFog(float t, float r, float g, float b);

CharStyle MakeStyle(float r, float g, float b, int build, int hat, bool child, unsigned int seed);

// sx, sy = 접지점(화면 좌표). dirX/dirY = 월드 이동 방향. walk = 누적 걸음 시간.
void DrawHuman(Renderer& R, float sx, float sy, float dirX, float dirY,
	float walk, bool moving, const CharStyle& st,
	float lr, float lg, float lb, float t);

enum BeastKind { BK_WOLF = 0, BK_DEER, BK_BOAR, BK_CROW, BK_COUNT };

const char* BeastName(int kind);
void DrawBeast(Renderer& R, float sx, float sy, float dirX, float dirY,
	float walk, bool moving, int kind, float lr, float lg, float lb, float t);
