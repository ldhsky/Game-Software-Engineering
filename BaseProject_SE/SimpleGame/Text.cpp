#include "stdafx.h"
#include "Text.h"
#include "Renderer.h"

#include <windows.h>
#include <vector>

static const int PAD = 2;

bool TextRenderer::Init(Renderer* r, int pixelHeight, const wchar_t* faceName)
{
	m_R = r;
	m_Size = pixelHeight;
	m_CellW = pixelHeight * 3 + PAD * 2;
	m_CellH = pixelHeight * 2 + PAD * 2;

	HDC dc = CreateCompatibleDC(NULL);
	if (!dc) return false;

	BITMAPINFO bi = { 0 };
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = m_CellW;
	bi.bmiHeader.biHeight = -m_CellH;      // top-down
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void* bits = 0;
	HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!bmp) { DeleteDC(dc); return false; }
	SelectObject(dc, bmp);

	const wchar_t* face = faceName ? faceName : L"맑은 고딕";
	HFONT font = CreateFontW(-pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
	if (!font)
		font = CreateFontW(-pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Gulim");
	SelectObject(dc, font);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, RGB(255, 255, 255));

	TEXTMETRICW tm;
	GetTextMetricsW(dc, &tm);
	m_Line = tm.tmHeight;

	m_DC = dc; m_Bmp = bmp; m_Font = font;
	m_Bits = (unsigned char*)bits;
	m_Gray = new unsigned char[(size_t)m_CellW * m_CellH];

	m_Tex = m_R->CreateAlphaTexture(m_AtlasW, m_AtlasH);
	m_R->SetGlyphAtlas(m_Tex);
	return m_Tex != 0;
}

void TextRenderer::Shutdown()
{
	if (m_Font) DeleteObject((HFONT)m_Font);
	if (m_Bmp)  DeleteObject((HBITMAP)m_Bmp);
	if (m_DC)   DeleteDC((HDC)m_DC);
	delete[] m_Gray;
	m_Font = m_Bmp = m_DC = 0; m_Gray = 0;
}

const TextRenderer::Glyph* TextRenderer::Get(wchar_t c)
{
	std::unordered_map<unsigned int, Glyph>::iterator it = m_Map.find((unsigned int)c);
	if (it != m_Map.end()) return &it->second;
	if (!m_DC) return 0;

	HDC dc = (HDC)m_DC;
	memset(m_Bits, 0, (size_t)m_CellW * m_CellH * 4);
	TextOutW(dc, PAD, PAD, &c, 1);

	SIZE ext = { 0, 0 };
	GetTextExtentPoint32W(dc, &c, 1, &ext);

	// 알파 추출 및 바운딩 박스
	int minX = m_CellW, minY = m_CellH, maxX = -1, maxY = -1;
	for (int y = 0; y < m_CellH; ++y)
	{
		const unsigned char* src = m_Bits + (size_t)y * m_CellW * 4;
		unsigned char* dst = m_Gray + (size_t)y * m_CellW;
		for (int x = 0; x < m_CellW; ++x)
		{
			unsigned char v = src[x * 4];
			if (src[x * 4 + 1] > v) v = src[x * 4 + 1];
			if (src[x * 4 + 2] > v) v = src[x * 4 + 2];
			dst[x] = v;
			if (v)
			{
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}
	}

	Glyph g = { 0, 0, 0, 0, 0, 0, 0, 0, ext.cx };
	if (maxX >= 0)
	{
		int gw = maxX - minX + 1;
		int gh = maxY - minY + 1;

		if (m_PenX + gw + 1 >= m_AtlasW) { m_PenX = 1; m_PenY += m_RowH + 1; m_RowH = 0; }
		if (m_PenY + gh + 1 >= m_AtlasH) { m_PenX = 1; m_PenY = 1; m_RowH = 0; m_Map.clear(); }

		// 잘라낸 영역을 연속 버퍼로 모아 업로드
		std::vector<unsigned char> tight((size_t)gw * gh);
		for (int y = 0; y < gh; ++y)
			memcpy(&tight[(size_t)y * gw], m_Gray + (size_t)(minY + y) * m_CellW + minX, gw);
		m_R->UpdateAlphaTexture(m_Tex, m_PenX, m_PenY, gw, gh, tight.data());

		g.u0 = (float)m_PenX / m_AtlasW;
		g.v0 = (float)m_PenY / m_AtlasH;
		g.u1 = (float)(m_PenX + gw) / m_AtlasW;
		g.v1 = (float)(m_PenY + gh) / m_AtlasH;
		g.w = gw; g.h = gh;
		g.bx = minX - PAD;
		g.by = minY - PAD;

		m_PenX += gw + 1;
		if (gh > m_RowH) m_RowH = gh;
	}

	m_Map[(unsigned int)c] = g;
	return &m_Map[(unsigned int)c];
}

static int ToWide(const char* utf8, wchar_t* out, int cap)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
	return n > 0 ? n - 1 : 0;
}

void TextRenderer::Draw(float x, float y, const char* utf8, float cr, float cg, float cb, float ca)
{
	m_R->SetGlyphAtlas(m_Tex);
	wchar_t buf[512];
	int n = ToWide(utf8, buf, 512);
	float pen = x;
	for (int i = 0; i < n; ++i)
	{
		const Glyph* g = Get(buf[i]);
		if (!g) continue;
		if (g->w > 0)
		{
			// y는 줄 상단. 화면은 +y가 위쪽이므로 아래로 내려간다.
			float gx = pen + g->bx;
			float gy = y - g->by - g->h;
			m_R->GlyphQuad(gx, gy, (float)g->w, (float)g->h,
				g->u0, g->v0, g->u1, g->v1, cr, cg, cb, ca);
		}
		pen += g->adv;
	}
}

void TextRenderer::DrawShadowed(float x, float y, const char* utf8, float cr, float cg, float cb, float ca)
{
	Draw(x + 1.f, y - 1.f, utf8, 0.f, 0.f, 0.f, ca * 0.75f);
	Draw(x, y, utf8, cr, cg, cb, ca);
}

float TextRenderer::Measure(const char* utf8)
{
	wchar_t buf[512];
	int n = ToWide(utf8, buf, 512);
	float w = 0.f;
	for (int i = 0; i < n; ++i)
	{
		const Glyph* g = Get(buf[i]);
		if (g) w += g->adv;
	}
	return w;
}
