#include "stdafx.h"
#include "Renderer.h"

#include <fstream>
#include <iostream>

static const size_t FLUSH_LIMIT = 60000 * 6;

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	m_W = windowSizeX;
	m_H = windowSizeY;
	m_SV.reserve(FLUSH_LIMIT);
	m_GV.reserve(8 * 4096);
	Initialize();
}

Renderer::~Renderer()
{
	if (m_ShapeVBO) glDeleteBuffers(1, &m_ShapeVBO);
	if (m_GlyphVBO) glDeleteBuffers(1, &m_GlyphVBO);
	if (m_ShapeVAO) glDeleteVertexArrays(1, &m_ShapeVAO);
	if (m_GlyphVAO) glDeleteVertexArrays(1, &m_GlyphVAO);
	if (m_ShapeProg) glDeleteProgram(m_ShapeProg);
	if (m_GlyphProg) glDeleteProgram(m_GlyphProg);
	if (m_Atlas) glDeleteTextures(1, &m_Atlas);
}

void Renderer::Initialize()
{
	m_ShapeProg = CompileShaders("./Shaders/Shape.vs", "./Shaders/Shape.fs");
	m_GlyphProg = CompileShaders("./Shaders/Glyph.vs", "./Shaders/Glyph.fs");
	if (m_ShapeProg == 0 || m_GlyphProg == 0) return;

	m_uShapeVP = glGetUniformLocation(m_ShapeProg, "u_HalfViewport");
	m_uGlyphVP = glGetUniformLocation(m_GlyphProg, "u_HalfViewport");
	m_uTex = glGetUniformLocation(m_GlyphProg, "u_Tex");

	glGenVertexArrays(1, &m_ShapeVAO);
	glGenBuffers(1, &m_ShapeVBO);
	glBindVertexArray(m_ShapeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_ShapeVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));

	glGenVertexArrays(1, &m_GlyphVAO);
	glGenBuffers(1, &m_GlyphVBO);
	glBindVertexArray(m_GlyphVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_GlyphVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 2));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 4));

	glBindVertexArray(0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);   // 화가 알고리즘

	m_Initialized = true;
}

bool Renderer::ReadFile(const char* filename, std::string* target)
{
	std::ifstream file(filename);
	if (file.fail())
	{
		std::cout << filename << " 파일을 열 수 없습니다.\n";
		return false;
	}
	std::string line;
	while (getline(file, line)) { target->append(line); target->append("\n"); }
	return true;
}

bool Renderer::AddShader(GLuint program, const char* text, GLenum type)
{
	GLuint obj = glCreateShader(type);
	if (obj == 0) return false;

	const GLchar* src[1] = { text };
	GLint len[1] = { (GLint)strlen(text) };
	glShaderSource(obj, 1, src, len);
	glCompileShader(obj);

	GLint ok = 0;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLchar log[1024] = { 0 };
		glGetShaderInfoLog(obj, sizeof(log), NULL, log);
		std::cout << "셰이더 컴파일 실패: " << log << "\n";
		glDeleteShader(obj);
		return false;
	}
	glAttachShader(program, obj);
	glDeleteShader(obj);
	return true;
}

GLuint Renderer::CompileShaders(const char* vsPath, const char* fsPath)
{
	GLuint program = glCreateProgram();
	if (program == 0) return 0;

	std::string vs, fs;
	if (!ReadFile(vsPath, &vs) || !ReadFile(fsPath, &fs)) { glDeleteProgram(program); return 0; }
	if (!AddShader(program, vs.c_str(), GL_VERTEX_SHADER) ||
		!AddShader(program, fs.c_str(), GL_FRAGMENT_SHADER)) { glDeleteProgram(program); return 0; }

	GLint ok = 0;
	GLchar log[1024] = { 0 };
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cout << vsPath << " 링크 실패: " << log << "\n";
		glDeleteProgram(program);
		return 0;   // GLuint는 부호 없음 — 실패는 0
	}
	return program;
}

/* ---------- 프레임 ---------- */

void Renderer::BeginFrame(float r, float g, float b)
{
	glClearColor(r * m_FR, g * m_FG, b * m_FB, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	m_Calls = 0;
	m_Mode = M_NONE;
}

void Renderer::EndFrame() { Flush(); }

void Renderer::Flush()
{
	if (!m_Initialized) return;

	if (!m_SV.empty())
	{
		glBindVertexArray(m_ShapeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_ShapeVBO);
		glBufferData(GL_ARRAY_BUFFER, m_SV.size() * sizeof(float), m_SV.data(), GL_STREAM_DRAW);
		glUseProgram(m_ShapeProg);
		glUniform2f(m_uShapeVP, m_W * 0.5f, m_H * 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_SV.size() / 6));
		m_SV.clear();
		++m_Calls;
	}
	if (!m_GV.empty())
	{
		glBindVertexArray(m_GlyphVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_GlyphVBO);
		glBufferData(GL_ARRAY_BUFFER, m_GV.size() * sizeof(float), m_GV.data(), GL_STREAM_DRAW);
		glUseProgram(m_GlyphProg);
		glUniform2f(m_uGlyphVP, m_W * 0.5f, m_H * 0.5f);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_Atlas);
		glUniform1i(m_uTex, 0);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_GV.size() / 8));
		m_GV.clear();
		++m_Calls;
	}
	glBindVertexArray(0);
	m_Mode = M_NONE;
}

void Renderer::EnsureMode(Mode m)
{
	// 도형과 글리프가 섞이면 그리기 순서가 어긋나므로 전환 시 비운다.
	if (m_Mode != M_NONE && m_Mode != m) Flush();
	m_Mode = m;
}

void Renderer::PushV(float x, float y, float r, float g, float b, float a)
{
	m_SV.push_back(x); m_SV.push_back(y);
	m_SV.push_back(r * m_FR); m_SV.push_back(g * m_FG); m_SV.push_back(b * m_FB);
	m_SV.push_back(a);
}

/* ---------- 도형 ---------- */

void Renderer::Tri(float x0, float y0, float x1, float y1, float x2, float y2,
	float r, float g, float b, float a)
{
	EnsureMode(M_SHAPE);
	if (m_SV.size() >= FLUSH_LIMIT) Flush(), m_Mode = M_SHAPE;
	PushV(x0, y0, r, g, b, a);
	PushV(x1, y1, r, g, b, a);
	PushV(x2, y2, r, g, b, a);
}

void Renderer::Quad(float x0, float y0, float x1, float y1, float x2, float y2,
	float x3, float y3, float r, float g, float b, float a)
{
	Tri(x0, y0, x1, y1, x2, y2, r, g, b, a);
	Tri(x0, y0, x2, y2, x3, y3, r, g, b, a);
}

void Renderer::Rect(float cx, float cy, float w, float h, float r, float g, float b, float a)
{
	float hw = w * 0.5f, hh = h * 0.5f;
	Quad(cx - hw, cy - hh, cx - hw, cy + hh, cx + hw, cy + hh, cx + hw, cy - hh, r, g, b, a);
}

void Renderer::Diamond(float cx, float cy, float w, float h, float r, float g, float b, float a)
{
	float hw = w * 0.5f, hh = h * 0.5f;
	Quad(cx, cy - hh, cx - hw, cy, cx, cy + hh, cx + hw, cy, r, g, b, a);
}

void Renderer::IsoBox(float sx, float sy, float hw, float hh, float height,
	float r, float g, float b, float a)
{
	// 좌측면 (어둡게)
	Quad(sx - hw, sy, sx, sy - hh, sx, sy - hh + height, sx - hw, sy + height,
		r * 0.55f, g * 0.55f, b * 0.58f, a);
	// 우측면
	Quad(sx, sy - hh, sx + hw, sy, sx + hw, sy + height, sx, sy - hh + height,
		r * 0.78f, g * 0.78f, b * 0.80f, a);
	// 상단면
	Diamond(sx, sy + height, hw * 2.f, hh * 2.f, r, g, b, a);
}

/* ---------- 텍스처 ---------- */

GLuint Renderer::CreateAlphaTexture(int w, int h)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector<unsigned char> zero((size_t)w * h, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, zero.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}

void Renderer::UpdateAlphaTexture(GLuint tex, int x, int y, int w, int h, const unsigned char* px)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, px);
}

void Renderer::GlyphQuad(float x, float y, float w, float h,
	float u0, float v0, float u1, float v1,
	float r, float g, float b, float a)
{
	EnsureMode(M_GLYPH);
	float R = r * m_FR, G = g * m_FG, B = b * m_FB;
	const float vx[6] = { x,     x,     x + w, x,     x + w, x + w };
	const float vy[6] = { y,     y + h, y + h, y,     y + h, y };
	const float vu[6] = { u0,    u0,    u1,    u0,    u1,    u1 };
	const float vv[6] = { v1,    v0,    v0,    v1,    v0,    v1 };
	for (int i = 0; i < 6; ++i)
	{
		m_GV.push_back(vx[i]); m_GV.push_back(vy[i]);
		m_GV.push_back(vu[i]); m_GV.push_back(vv[i]);
		m_GV.push_back(R); m_GV.push_back(G); m_GV.push_back(B); m_GV.push_back(a);
	}
}
