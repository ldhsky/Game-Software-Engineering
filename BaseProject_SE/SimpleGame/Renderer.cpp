#include "stdafx.h"
#include "Renderer.h"

#include <fstream>
#include <iostream>
#include <cmath>

static const size_t FLUSH_LIMIT = 90000 * 6;

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	m_W = windowSizeX;
	m_H = windowSizeY;
	m_SV.reserve(FLUSH_LIMIT);
	m_GV.reserve(8 * 8192);
	Initialize();
}

Renderer::~Renderer()
{
	if (m_ShapeVBO) glDeleteBuffers(1, &m_ShapeVBO);
	if (m_GlyphVBO) glDeleteBuffers(1, &m_GlyphVBO);
	if (m_PostVBO)  glDeleteBuffers(1, &m_PostVBO);
	if (m_ShapeVAO) glDeleteVertexArrays(1, &m_ShapeVAO);
	if (m_GlyphVAO) glDeleteVertexArrays(1, &m_GlyphVAO);
	if (m_PostVAO)  glDeleteVertexArrays(1, &m_PostVAO);
	if (m_ShapeProg) glDeleteProgram(m_ShapeProg);
	if (m_GlyphProg) glDeleteProgram(m_GlyphProg);
	if (m_PostProg)  glDeleteProgram(m_PostProg);
	if (m_SceneTex) glDeleteTextures(1, &m_SceneTex);
	if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
	if (m_Atlas) glDeleteTextures(1, &m_Atlas);
}

void Renderer::Initialize()
{
	m_ShapeProg = CompileShaders("./Shaders/Shape.vs", "./Shaders/Shape.fs");
	m_GlyphProg = CompileShaders("./Shaders/Glyph.vs", "./Shaders/Glyph.fs");
	m_PostProg = CompileShaders("./Shaders/Post.vs", "./Shaders/Post.fs");
	if (!m_ShapeProg || !m_GlyphProg || !m_PostProg) return;

	m_uShapeVP = glGetUniformLocation(m_ShapeProg, "u_HalfViewport");
	m_uGlyphVP = glGetUniformLocation(m_GlyphProg, "u_HalfViewport");
	m_uTex = glGetUniformLocation(m_GlyphProg, "u_Tex");
	m_uScene = glGetUniformLocation(m_PostProg, "u_Scene");
	m_uTexel = glGetUniformLocation(m_PostProg, "u_Texel");
	m_uFilter = glGetUniformLocation(m_PostProg, "u_Filter");
	m_uTime = glGetUniformLocation(m_PostProg, "u_Time");
	m_uExposure = glGetUniformLocation(m_PostProg, "u_Exposure");
	m_uBloomAmt = glGetUniformLocation(m_PostProg, "u_Bloom");
	m_uFade = glGetUniformLocation(m_PostProg, "u_Fade");

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

	// 후처리 전체화면 사각형
	const float fq[] = {
		-1.f, -1.f, 0.f, 0.f,   -1.f,  1.f, 0.f, 1.f,    1.f,  1.f, 1.f, 1.f,
		-1.f, -1.f, 0.f, 0.f,    1.f,  1.f, 1.f, 1.f,    1.f, -1.f, 1.f, 0.f,
	};
	glGenVertexArrays(1, &m_PostVAO);
	glGenBuffers(1, &m_PostVBO);
	glBindVertexArray(m_PostVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_PostVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fq), fq, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

	glBindVertexArray(0);

	if (!CreateTargets()) return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	m_Initialized = true;
}

bool Renderer::CreateTargets()
{
	glGenTextures(1, &m_SceneTex);
	glBindTexture(GL_TEXTURE_2D, m_SceneTex);
	// 부동소수 타깃 — 등불이 1을 넘겨 블룸이 걸리도록
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_W, m_H, 0, GL_RGBA, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &m_FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneTex, 0);
	GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (st != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "프레임버퍼 생성 실패 (0x" << std::hex << st << ")\n";
		return false;
	}
	return true;
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
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		GLchar log[1024] = { 0 };
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cout << vsPath << " 링크 실패: " << log << "\n";
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

/* ---------- 프레임 ---------- */

void Renderer::BeginFrame(float r, float g, float b)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glViewport(0, 0, m_W, m_H);
	glClearColor(r, g, b, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	m_VertsLastFrame = 0;
	m_Mode = M_NONE;
}

void Renderer::EndFrame()
{
	Flush();

	// 후처리 통과
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_W, m_H);
	glDisable(GL_BLEND);

	glUseProgram(m_PostProg);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneTex);
	glUniform1i(m_uScene, 0);
	glUniform2f(m_uTexel, 1.f / m_W, 1.f / m_H);
	glUniform3f(m_uFilter, m_FR, m_FG, m_FB);
	glUniform1f(m_uTime, m_Time);
	glUniform1f(m_uExposure, 1.75f);
	glUniform1f(m_uBloomAmt, 2.4f);
	glUniform1f(m_uFade, m_Fade);

	glBindVertexArray(m_PostVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glEnable(GL_BLEND);
}

void Renderer::Flush()
{
	if (!m_Initialized && !m_ShapeProg) { m_SV.clear(); m_GV.clear(); return; }

	if (!m_SV.empty())
	{
		m_VertsLastFrame += (int)(m_SV.size() / 6);
		glBindVertexArray(m_ShapeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_ShapeVBO);
		glBufferData(GL_ARRAY_BUFFER, m_SV.size() * sizeof(float), m_SV.data(), GL_STREAM_DRAW);
		glUseProgram(m_ShapeProg);
		glUniform2f(m_uShapeVP, m_W * 0.5f, m_H * 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(m_SV.size() / 6));
		m_SV.clear();
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
	}
	glBindVertexArray(0);
	m_Mode = M_NONE;
}

void Renderer::EnsureMode(Mode m)
{
	if (m_Mode != M_NONE && m_Mode != m) Flush();
	m_Mode = m;
}

/* ---------- 도형 ---------- */

void Renderer::Tri(float x0, float y0, float x1, float y1, float x2, float y2,
	float r, float g, float b, float a)
{
	EnsureMode(M_SHAPE);
	if (m_SV.size() >= FLUSH_LIMIT) { Flush(); m_Mode = M_SHAPE; }
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

void Renderer::QuadC(float x0, float y0, float x1, float y1, float x2, float y2,
	float x3, float y3, const float* c)
{
	EnsureMode(M_SHAPE);
	if (m_SV.size() >= FLUSH_LIMIT) { Flush(); m_Mode = M_SHAPE; }
	PushV(x0, y0, c[0], c[1], c[2], c[3]);
	PushV(x1, y1, c[4], c[5], c[6], c[7]);
	PushV(x2, y2, c[8], c[9], c[10], c[11]);
	PushV(x0, y0, c[0], c[1], c[2], c[3]);
	PushV(x2, y2, c[8], c[9], c[10], c[11]);
	PushV(x3, y3, c[12], c[13], c[14], c[15]);
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

void Renderer::Diamond4(float cx, float cy, float w, float h, const float* c)
{
	float hw = w * 0.5f, hh = h * 0.5f;
	QuadC(cx, cy - hh, cx - hw, cy, cx, cy + hh, cx + hw, cy, c);
}

void Renderer::IsoBox(float sx, float sy, float hw, float hh, float height,
	float r, float g, float b, float a)
{
	Quad(sx - hw, sy, sx, sy - hh, sx, sy - hh + height, sx - hw, sy + height,
		r * 0.52f, g * 0.52f, b * 0.56f, a);
	Quad(sx, sy - hh, sx + hw, sy, sx + hw, sy + height, sx, sy - hh + height,
		r * 0.76f, g * 0.76f, b * 0.79f, a);
	Diamond(sx, sy + height, hw * 2.f, hh * 2.f, r, g, b, a);
}

void Renderer::Ellipse(float cx, float cy, float rw, float rh, int segs,
	float r, float g, float b, float a)
{
	if (segs < 6) segs = 6;
	float prevX = cx + rw, prevY = cy;
	for (int i = 1; i <= segs; ++i)
	{
		float t = 6.2831853f * i / segs;
		float x = cx + cosf(t) * rw, y = cy + sinf(t) * rh;
		Tri(cx, cy, prevX, prevY, x, y, r, g, b, a);
		prevX = x; prevY = y;
	}
}

void Renderer::SoftShadow(float cx, float cy, float rw, float rh, float alpha)
{
	// 세 겹으로 겹쳐 가장자리를 흐린다
	Ellipse(cx, cy, rw * 1.42f, rh * 1.42f, 14, 0.f, 0.f, 0.f, alpha * 0.20f);
	Ellipse(cx, cy, rw * 1.14f, rh * 1.14f, 14, 0.f, 0.f, 0.f, alpha * 0.28f);
	Ellipse(cx, cy, rw, rh, 16, 0.f, 0.f, 0.f, alpha * 0.44f);
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
	const float vx[6] = { x,  x,     x + w, x,     x + w, x + w };
	const float vy[6] = { y,  y + h, y + h, y,     y + h, y };
	const float vu[6] = { u0, u0,    u1,    u0,    u1,    u1 };
	const float vv[6] = { v1, v0,    v0,    v1,    v0,    v1 };
	for (int i = 0; i < 6; ++i)
	{
		m_GV.push_back(vx[i]); m_GV.push_back(vy[i]);
		m_GV.push_back(vu[i]); m_GV.push_back(vv[i]);
		m_GV.push_back(r); m_GV.push_back(g); m_GV.push_back(b); m_GV.push_back(a);
	}
}
