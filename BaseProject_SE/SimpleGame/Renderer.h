#pragma once

#include <string>
#include <vector>
#include "Dependencies\glew.h"

// 배칭 렌더러 + 후처리.
// 좌표는 화면 픽셀, 원점은 창 중앙, +y 위쪽.
// 씬은 부동소수 프레임버퍼에 그린 뒤 후처리 셰이더를 한 번 통과한다.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const { return m_Initialized; }

	void BeginFrame(float r, float g, float b);
	void EndFrame();                       // 플러시 + 후처리 통과
	void Flush();

	void SetColorFilter(float r, float g, float b) { m_FR = r; m_FG = g; m_FB = b; }
	void SetFade(float f) { m_Fade = f; }
	void SetTime(float t) { m_Time = t; }

	void Rect(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);
	void Diamond(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);
	// 네 꼭짓점 색을 따로 주는 다이아몬드 (아래·왼·위·오른 순) — 타일 경계의 계단감 제거
	void Diamond4(float cx, float cy, float w, float h, const float* rgba4);
	void Tri(float x0, float y0, float x1, float y1, float x2, float y2,
		float r, float g, float b, float a = 1.f);
	void Quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
		float r, float g, float b, float a = 1.f);
	void QuadC(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
		const float* rgba4);
	void IsoBox(float sx, float sy, float hw, float hh, float height,
		float r, float g, float b, float a = 1.f);
	// 타원 (부채꼴 분할)
	void Ellipse(float cx, float cy, float rw, float rh, int segs,
		float r, float g, float b, float a);
	// 부드러운 접지 그림자 — 여러 겹으로 감쇠
	void SoftShadow(float cx, float cy, float rw, float rh, float alpha);

	GLuint CreateAlphaTexture(int w, int h);
	void UpdateAlphaTexture(GLuint tex, int x, int y, int w, int h, const unsigned char* px);
	void SetGlyphAtlas(GLuint tex) { if (tex != m_Atlas) { if (!m_GV.empty()) Flush(); m_Atlas = tex; } }
	void GlyphQuad(float x, float y, float w, float h,
		float u0, float v0, float u1, float v1,
		float r, float g, float b, float a);

	int Width() const { return m_W; }
	int Height() const { return m_H; }
	int LastVertexCount() const { return m_VertsLastFrame; }

private:
	enum Mode { M_NONE, M_SHAPE, M_GLYPH };

	void Initialize();
	bool CreateTargets();
	bool ReadFile(const char* filename, std::string* target);
	bool AddShader(GLuint program, const char* text, GLenum type);
	GLuint CompileShaders(const char* vs, const char* fs);
	void EnsureMode(Mode m);
	inline void PushV(float x, float y, float r, float g, float b, float a)
	{
		float* p = m_SVBuf + m_SVN;
		p[0] = x; p[1] = y; p[2] = r; p[3] = g; p[4] = b; p[5] = a;
		m_SVN += 6;
	}

	bool m_Initialized = false;
	int m_W = 0, m_H = 0;
	int m_VertsLastFrame = 0;

	GLuint m_ShapeProg = 0, m_GlyphProg = 0, m_PostProg = 0;
	GLuint m_ShapeVAO = 0, m_ShapeVBO = 0;
	GLuint m_GlyphVAO = 0, m_GlyphVBO = 0;
	GLuint m_PostVAO = 0, m_PostVBO = 0;
	GLint m_uShapeVP = -1, m_uGlyphVP = -1, m_uTex = -1;
	GLint m_uScene = -1, m_uTexel = -1, m_uFilter = -1, m_uTime = -1;
	GLint m_uExposure = -1, m_uBloomAmt = -1, m_uFade = -1;

	GLuint m_FBO = 0, m_SceneTex = 0;
	GLuint m_BloomFBO = 0, m_BloomTex = 0, m_BloomProg = 0;
	GLint m_uBlScene = -1, m_uBlTexel = -1, m_uPostBloom = -1;
	int m_BW = 0, m_BH = 0;
	GLuint m_Atlas = 0;

	// 도형 정점은 고정 버퍼에 직접 기록한다 (push_back 오버헤드 제거)
	float* m_SVBuf = 0;
	size_t m_SVN = 0, m_SVCap = 0;
	std::vector<float> m_GV;
	Mode m_Mode = M_NONE;

	float m_FR = 1.f, m_FG = 1.f, m_FB = 1.f;
	float m_Fade = 0.f, m_Time = 0.f;
};
