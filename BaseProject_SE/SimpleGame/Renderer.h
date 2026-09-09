#pragma once

#include <string>
#include "Dependencies\glew.h"

// 프로토타입 렌더러.
// 좌표는 모두 화면 픽셀 (원점 = 창 중앙, +y 위쪽).
// 단위 사각형 VBO와 단위 다이아몬드 VBO 두 개로 모든 것을 그린다.
class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized() const { return m_Initialized; }

	void BeginFrame(float r, float g, float b);

	void DrawRect(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);
	void DrawDiamond(float cx, float cy, float w, float h, float r, float g, float b, float a = 1.f);

	// 결손 연출 — 화면 전체에서 특정 색 성분을 빼는 데 쓴다.
	void SetColorFilter(float r, float g, float b) { m_FR = r; m_FG = g; m_FB = b; }

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string* target);
	bool AddShader(GLuint program, const char* text, GLenum type);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void DrawShape(GLuint vbo, float cx, float cy, float w, float h,
		float r, float g, float b, float a);

	bool m_Initialized = false;
	int m_W = 0;
	int m_H = 0;

	GLuint m_VAO = 0;
	GLuint m_VBOQuad = 0;
	GLuint m_VBODiamond = 0;
	GLuint m_Shader = 0;

	// 유니폼·어트리뷰트 위치는 초기화 시 캐싱한다 (매 드로우 문자열 조회 제거).
	GLint m_uTrans = -1;
	GLint m_uColor = -1;
	GLint m_aPos = -1;

	float m_FR = 1.f, m_FG = 1.f, m_FB = 1.f;
};
