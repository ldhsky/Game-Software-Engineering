#version 330

in vec3 a_Position;
uniform vec4 u_Trans;   // xy = 중심(NDC), zw = 반크기(NDC)

void main()
{
	gl_Position = vec4(a_Position.xy * u_Trans.zw + u_Trans.xy, 0.0, 1.0);
}
