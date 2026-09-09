#version 330
in vec2 v_UV;
in vec4 v_Col;
uniform sampler2D u_Tex;
layout(location=0) out vec4 FragColor;
void main()
{
	float a = texture(u_Tex, v_UV).r;
	FragColor = vec4(v_Col.rgb, v_Col.a * a);
}
