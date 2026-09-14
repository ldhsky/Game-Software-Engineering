#version 330
in vec2 v_UV;
uniform sampler2D u_Scene;
uniform vec2 u_Texel;
layout(location=0) out vec4 FragColor;

// 1/4 해상도에서 밝은 성분만 추출해 흐린다. 전체 해상도 다중 탭보다 훨씬 싸다.
void main()
{
	vec3 sum = max(texture(u_Scene, v_UV).rgb - 0.55, 0.0) * 0.28;
	for (int i = 0; i < 6; ++i)
	{
		float a = 6.2831853 * float(i) / 6.0;
		vec2 d = vec2(cos(a), sin(a));
		sum += max(texture(u_Scene, v_UV + d * u_Texel * 3.0).rgb - 0.55, 0.0) * 0.080;
		sum += max(texture(u_Scene, v_UV + d * u_Texel * 7.0).rgb - 0.55, 0.0) * 0.040;
	}
	FragColor = vec4(sum, 1.0);
}
