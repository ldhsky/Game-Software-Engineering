#version 330
layout(location=0) in vec2 a_Pos;
layout(location=1) in vec2 a_UV;
layout(location=2) in vec4 a_Col;
uniform vec2 u_HalfViewport;
out vec2 v_UV;
out vec4 v_Col;
void main()
{
	v_UV = a_UV;
	v_Col = a_Col;
	gl_Position = vec4(a_Pos / u_HalfViewport, 0.0, 1.0);
}
