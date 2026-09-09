#pragma once

#include <unordered_map>
#include "Dependencies\glew.h"

class Renderer;

// GDI로 글리프를 즉석 래스터화해 아틀라스에 캐싱하는 텍스트 렌더러.
// 한글을 화면에 직접 그리기 위한 것. 외부 라이브러리 없음.
class TextRenderer
{
public:
	bool Init(Renderer* r, int pixelHeight, const wchar_t* faceName = 0);
	void Shutdown();

	// (x, y) = 줄의 좌측 상단. utf8 입력.
	void Draw(float x, float y, const char* utf8, float cr, float cg, float cb, float ca = 1.f);
	void DrawShadowed(float x, float y, const char* utf8, float cr, float cg, float cb, float ca = 1.f);
	float Measure(const char* utf8);
	int LineHeight() const { return m_Line; }

private:
	struct Glyph { float u0, v0, u1, v1; int w, h, bx, by, adv; };
	const Glyph* Get(wchar_t c);

	Renderer* m_R = 0;
	GLuint m_Tex = 0;
	int m_AtlasW = 1024, m_AtlasH = 1024;
	int m_PenX = 1, m_PenY = 1, m_RowH = 0;
	int m_Line = 0, m_Size = 0;
	int m_CellW = 0, m_CellH = 0;

	std::unordered_map<unsigned int, Glyph> m_Map;

	void* m_DC = 0;
	void* m_Bmp = 0;
	void* m_Font = 0;
	unsigned char* m_Bits = 0;
	unsigned char* m_Gray = 0;
};
