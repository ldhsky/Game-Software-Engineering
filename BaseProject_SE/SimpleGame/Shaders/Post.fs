#version 330
in vec2 v_UV;

uniform sampler2D u_Scene;
uniform vec2  u_Texel;
uniform vec3  u_Filter;     // 결손 — 색 성분 제거
uniform float u_Time;
uniform float u_Exposure;
uniform float u_Bloom;
uniform float u_Fade;       // 화면 암전 (연출)

layout(location=0) out vec4 FragColor;

vec3 samp(vec2 uv) { return texture(u_Scene, uv).rgb; }

void main()
{
	vec3 base = samp(v_UV);

	// 블룸 — 두 반경의 링 샘플에서 밝은 성분만 추출
	vec3 bl = vec3(0.0);
	for (int i = 0; i < 8; ++i)
	{
		float a = 6.2831853 * float(i) / 8.0;
		vec2 d = vec2(cos(a), sin(a));
		vec3 s1 = samp(v_UV + d * u_Texel * 5.0);
		vec3 s2 = samp(v_UV + d * u_Texel * 12.0);
		vec3 s3 = samp(v_UV + d * u_Texel * 22.0);
		bl += max(s1 - 0.50, 0.0) + max(s2 - 0.50, 0.0) * 0.65 + max(s3 - 0.50, 0.0) * 0.35;
	}
	bl /= 16.0;

	vec3 c = base * u_Exposure + bl * u_Bloom;

	// 색 보정 — 어두운 곳은 차갑게, 밝은 곳은 따뜻하게
	float l = dot(c, vec3(0.299, 0.587, 0.114));
	c *= mix(vec3(0.84, 0.95, 1.12), vec3(1.12, 1.01, 0.86), clamp(l * 1.7, 0.0, 1.0));

	// 필믹 톤맵
	c = (c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14);

	// 채도 조정
	l = dot(c, vec3(0.299, 0.587, 0.114));
	c = mix(vec3(l), c, 0.90);

	// 비네트
	vec2 q = v_UV - 0.5;
	float d2 = dot(q, q);
	c *= clamp(1.0 - d2 * 1.30, 0.0, 1.0);

	// 필름 그레인
	float gr = fract(sin(dot(v_UV * vec2(1721.3, 977.7) + u_Time, vec2(12.9898, 78.233))) * 43758.5453);
	c += (gr - 0.5) * 0.020;

	c *= u_Filter;
	c *= (1.0 - u_Fade);

	FragColor = vec4(c, 1.0);
}
