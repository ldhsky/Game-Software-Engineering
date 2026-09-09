#pragma once

#include <string>
#include <vector>
#include "Dependencies\glew.h"

// 배칭 렌더러.
// 모든 좌표는 화면 픽셀, 원점은 창 중앙, +y 위쪽.
// 도형은 CPU에서 정점으로 펼쳐 한 버퍼에 모은 뒤 한 번에 그린다.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const { return m_Initialized; }

	void BeginFrame(float r, float g, float b);
	void EndFrame();
	void Flush();

	// 결손 연출 — 화면 전체 색 성분 배율
	void SetColorFilter(float r, float g, float b) { m_FR = r; m_FG = g; m_FB = b; }

	void Rect(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);
	void Diamond(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);
	void Tri(float x0, float y0, float x1, float y1, float x2, float y2,
		float r, float g, float b, float a = 1.f);
	void Quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
		float r, float g, float b, float a = 1.f);
	// 아이소메트릭 육면체 — 상단면 + 좌/우 측면
	void IsoBox(float sx, float sy, float hw, float hh, float height,
		float r, float g, float b, float a = 1.f);

	// 글리프 아틀라스
	GLuint CreateAlphaTexture(int w, int h);
	void UpdateAlphaTexture(GLuint tex, int x, int y, int w, int h, const unsigned char* px);
	void SetGlyphAtlas(GLuint tex) { if (tex != m_Atlas) { if (!m_GV.empty()) Flush(); m_Atlas = tex; } }
	void GlyphQuad(float x, float y, float w, float h,
		float u0, float v0, float u1, float v1,
		float r, float g, float b, float a);

	int Width() const { return m_W; }
	int Height() const { return m_H; }
	int LastDrawCalls() const { return m_Calls; }

private:
	enum Mode { M_NONE, M_SHAPE, M_GLYPH };

	void Initialize();
	bool ReadFile(const char* filename, std::string* target);
	bool AddShader(GLuint program, const char* text, GLenum type);
	GLuint CompileShaders(const char* vs, const char* fs);
	void EnsureMode(Mode m);
	void PushV(float x, float y, float r, float g, float b, float a);

	bool m_Initialized = false;
	int m_W = 0, m_H = 0;
	int m_Calls = 0;

	GLuint m_ShapeProg = 0, m_GlyphProg = 0;
	GLuint m_ShapeVAO = 0, m_ShapeVBO = 0;
	GLuint m_GlyphVAO = 0, m_GlyphVBO = 0;
	GLint m_uShapeVP = -1, m_uGlyphVP = -1, m_uTex = -1;
	GLuint m_Atlas = 0;

	std::vector<float> m_SV;   // 도형 정점: x y r g b a
	std::vector<float> m_GV;   // 글리프 정점: x y u v r g b a
	Mode m_Mode = M_NONE;

	float m_FR = 1.f, m_FG = 1.f, m_FB = 1.f;
};
