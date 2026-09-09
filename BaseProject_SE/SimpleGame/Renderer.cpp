#include "stdafx.h"
#include "Renderer.h"

#include <cstdio>
#include <fstream>
#include <iostream>

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}

Renderer::~Renderer()
{
	if (m_VBOQuad)    glDeleteBuffers(1, &m_VBOQuad);
	if (m_VBODiamond) glDeleteBuffers(1, &m_VBODiamond);
	if (m_VAO)        glDeleteVertexArrays(1, &m_VAO);
	if (m_Shader)     glDeleteProgram(m_Shader);
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	m_W = windowSizeX;
	m_H = windowSizeY;

	// core profile 대비. 기본 VAO에 의존하지 않는다.
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	m_Shader = CompileShaders("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	if (m_Shader == 0)
		return;

	m_uTrans = glGetUniformLocation(m_Shader, "u_Trans");
	m_uColor = glGetUniformLocation(m_Shader, "u_Color");
	m_aPos = glGetAttribLocation(m_Shader, "a_Position");

	CreateVertexBufferObjects();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// 화가 알고리즘으로 정렬하므로 깊이 테스트는 쓰지 않는다.
	glDisable(GL_DEPTH_TEST);

	if (m_VBOQuad > 0 && m_VBODiamond > 0 && m_aPos >= 0)
		m_Initialized = true;
}

void Renderer::CreateVertexBufferObjects()
{
	// 단위 사각형 (-1..1)
	float quad[] = {
		-1.f, -1.f, 0.f,  -1.f, 1.f, 0.f,   1.f, 1.f, 0.f,
		-1.f, -1.f, 0.f,   1.f, 1.f, 0.f,   1.f, -1.f, 0.f,
	};
	glGenBuffers(1, &m_VBOQuad);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	// 단위 다이아몬드 — 아이소메트릭 지면 타일용
	float dia[] = {
		 0.f, -1.f, 0.f,  -1.f, 0.f, 0.f,   0.f, 1.f, 0.f,
		 0.f, -1.f, 0.f,   0.f, 1.f, 0.f,   1.f, 0.f, 0.f,
	};
	glGenBuffers(1, &m_VBODiamond);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBODiamond);
	glBufferData(GL_ARRAY_BUFFER, sizeof(dia), dia, GL_STATIC_DRAW);
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
	while (getline(file, line))
	{
		target->append(line);
		target->append("\n");
	}
	return true;
}

bool Renderer::AddShader(GLuint program, const char* text, GLenum type)
{
	GLuint obj = glCreateShader(type);
	if (obj == 0)
	{
		fprintf(stderr, "셰이더 오브젝트 생성 실패 (type %d)\n", type);
		return false;
	}

	const GLchar* src[1] = { text };
	GLint len[1] = { (GLint)strlen(text) };
	glShaderSource(obj, 1, src, len);
	glCompileShader(obj);

	GLint success = 0;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar log[1024] = { 0 };
		glGetShaderInfoLog(obj, sizeof(log), NULL, log);
		fprintf(stderr, "셰이더 컴파일 실패 (type %d): %s\n", type, log);
		glDeleteShader(obj);
		return false;
	}

	glAttachShader(program, obj);
	glDeleteShader(obj);   // 프로그램이 참조를 유지한다
	return true;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	GLuint program = glCreateProgram();
	if (program == 0)
	{
		fprintf(stderr, "셰이더 프로그램 생성 실패\n");
		return 0;   // GLuint는 부호 없음 — 실패는 반드시 0을 반환한다
	}

	std::string vs, fs;
	if (!ReadFile(filenameVS, &vs) || !ReadFile(filenameFS, &fs))
	{
		glDeleteProgram(program);
		return 0;
	}
	if (!AddShader(program, vs.c_str(), GL_VERTEX_SHADER) ||
		!AddShader(program, fs.c_str(), GL_FRAGMENT_SHADER))
	{
		glDeleteProgram(program);
		return 0;
	}

	GLint success = 0;
	GLchar log[1024] = { 0 };

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cout << "셰이더 링크 실패: " << log << "\n";
		glDeleteProgram(program);
		return 0;
	}

	glValidateProgram(program);
	glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cout << "셰이더 검증 실패: " << log << "\n";
		glDeleteProgram(program);
		return 0;
	}

	std::cout << "셰이더 컴파일 완료.\n";
	return program;
}

void Renderer::BeginFrame(float r, float g, float b)
{
	// glClearColor를 먼저, glClear를 나중에. (순서가 반대면 한 프레임 밀린다)
	glClearColor(r * m_FR, g * m_FG, b * m_FB, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::DrawShape(GLuint vbo, float cx, float cy, float w, float h,
	float r, float g, float b, float a)
{
	if (!m_Initialized) return;

	glUseProgram(m_Shader);

	float ndcX = cx * 2.f / m_W;
	float ndcY = cy * 2.f / m_H;
	float ndcHW = w / (float)m_W;     // 픽셀 폭 w의 반크기를 NDC로
	float ndcHH = h / (float)m_H;

	glUniform4f(m_uTrans, ndcX, ndcY, ndcHW, ndcHH);
	glUniform4f(m_uColor, r * m_FR, g * m_FG, b * m_FB, a);

	glEnableVertexAttribArray(m_aPos);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(m_aPos, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(m_aPos);
}

void Renderer::DrawRect(float cx, float cy, float w, float h, float r, float g, float b, float a)
{
	DrawShape(m_VBOQuad, cx, cy, w, h, r, g, b, a);
}

void Renderer::DrawDiamond(float cx, float cy, float w, float h, float r, float g, float b, float a)
{
	DrawShape(m_VBODiamond, cx, cy, w, h, r, g, b, a);
}
