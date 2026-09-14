#version 330
in vec2 v_UV;

uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform vec3  u_Filter;
uniform float u_Time;
uniform float u_Exposure;
uniform float u_BloomAmt;
uniform float u_Fade;

layout(location=0) out vec4 FragColor;

void main()
{
	vec3 c = texture(u_Scene, v_UV).rgb * u_Exposure;
	c += texture(u_Bloom, v_UV).rgb * u_BloomAmt;   // 1/4 해상도 블룸을 선형 보간으로 확대

	// 색 보정 — 어두운 곳은 차갑게, 밝은 곳은 따뜻하게
	float l = dot(c, vec3(0.299, 0.587, 0.114));
	c *= mix(vec3(0.84, 0.95, 1.12), vec3(1.12, 1.01, 0.86), clamp(l * 1.7, 0.0, 1.0));

	// 필믹 톤맵
	c = (c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14);

	l = dot(c, vec3(0.299, 0.587, 0.114));
	c = mix(vec3(l), c, 0.90);

	// 비네트
	vec2 q = v_UV - 0.5;
	c *= clamp(1.0 - dot(q, q) * 1.30, 0.0, 1.0);

	// 필름 그레인
	float gr = fract(sin(dot(v_UV * vec2(1721.3, 977.7) + u_Time, vec2(12.9898, 78.233))) * 43758.5453);
	c += (gr - 0.5) * 0.020;

	c *= u_Filter;
	c *= (1.0 - u_Fade);
	FragColor = vec4(c, 1.0);
}
