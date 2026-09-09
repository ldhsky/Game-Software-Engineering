#version 330
layout(location=0) in vec2 a_Pos;    // 화면 픽셀 (원점 = 창 중앙)
layout(location=1) in vec4 a_Col;
uniform vec2 u_HalfViewport;
out vec4 v_Col;
void main()
{
	v_Col = a_Col;
	gl_Position = vec4(a_Pos / u_HalfViewport, 0.0, 1.0);
}
