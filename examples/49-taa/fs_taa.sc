$input v_cs_pos, v_ss_txc

#include "../common/common.sh"
#include "NoiseLibs.sh"
#include "depthLibs.sh"

SAMPLER2D(s_mainTex, 0);
SAMPLER2D(s_velocityBuffer, 1);
SAMPLER2D(s_prevBuffer, 2);
uniform vec4 mainTexel;

vec3 RGB_YCoCg(vec3 c)
{
	// Y = R/4 + G/2 + B/4
	// Co = R/2 - B/2
	// Cg = -R/4 + G/2 - B/4
	return vec3(
		 c.x/4.0 + c.y/2.0 + c.z/4.0,
		 c.x/2.0 - c.z/2.0,
		-c.x/4.0 + c.y/2.0 - c.z/4.0
	);
}

vec3 YCoCg_RGB(vec3 c)
{
	// R = Y + Co - Cg
	// G = Y + Cg
	// B = Y - Co - Cg
	return saturate(vec3(
		c.x + c.y - c.z,
		c.x + c.z,
		c.x - c.y - c.z
	));
}

vec4 sample_color(sampler2D tex, vec2 uv)
{
#if USE_YCOCG
	vec4 c = tex2D(tex, uv);
	return vec4(RGB_YCoCg(c.rgb), c.a);
#else
	return tex2D(tex, uv);
#endif
}

vec4 resolve_color(vec4 c)
{
#if USE_YCOCG
	return vec4(YCoCg_RGB(c.rgb).rgb, c.a);
#else
	return c;
#endif
}

vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
{
	vec4 r = q - p;
	vec3 rmax = aabb_max - p.xyz;
	vec3 rmin = aabb_min - p.xyz;

	const float eps = FLT_EPS;

	if (r.x > rmax.x + eps)
		r *= (rmax.x / r.x);
	if (r.y > rmax.y + eps)
		r *= (rmax.y / r.y);
	if (r.z > rmax.z + eps)
		r *= (rmax.z / r.z);

	if (r.x < rmin.x - eps)
		r *= (rmin.x / r.x);
	if (r.y < rmin.y - eps)
		r *= (rmin.y / r.y);
	if (r.z < rmin.z - eps)
		r *= (rmin.z / r.z);

	return p + r;
}

vec2 sample_velocity_dilated(sampler2D tex, vec2 uv, int support)
{
	vec2 du = vec2(mainTexel.x, 0.0);
	vec2 dv = vec2(0.0, mainTexel.y);
	vec2 mv = 0.0;
	float rmv = 0.0;

	int end = support + 1;
	for (int i = -support; i != end; i++)
	{
		for (int j = -support; j != end; j++)
		{
			vec2 v = tex2D(tex, uv + i * dv + j * du).xy;
			float rv = dot(v, v);
			if (rv > rmv)
			{
				mv = v;
				rmv = rv;
			}
		}
	}
	return mv;
}

vec4 sample_color_motion(sampler2D tex, vec2 uv, vec2 ss_vel)
{
	const vec2 v = 0.5 * ss_vel;
	const int taps = 3;// on either side!

	float srand = srand(uv + _SinTime.xx);
	vec2 vtap = v / taps;
	vec2 pos0 = uv + vtap * (0.5 * srand);
	vec4 accu = 0.0;
	float wsum = 0.0;

	[unroll]
	for (int i = -taps; i <= taps; i++)
	{
		float w = 1.0;// box
		accu += w * sample_color(tex, pos0 + i * vtap);
		wsum += w;
	}

	return accu / wsum;
}

vec4 temporal_reprojection(vec2 ss_txc, vec2 ss_vel, float vs_dist)
{
	// read texels
#if UNJITTER_COLORSAMPLES
	vec4 texel0 = sample_color(s_mainTex, ss_txc - _JitterUV.xy);
#else
	vec4 texel0 = sample_color(s_mainTex, ss_txc);
#endif
	vec4 texel1 = sample_color(s_prevBuffer, ss_txc - ss_vel);

	// calc min-max of current neighbourhood
#if UNJITTER_NEIGHBORHOOD
	vec2 uv = ss_txc - _JitterUV.xy;
#else
	vec2 uv = ss_txc;
#endif

#if MINMAX_3X3 || MINMAX_3X3_ROUNDED

	vec2 du = vec2(mainTexel.x, 0.0);
	vec2 dv = vec2(0.0, mainTexel.y);

	vec4 ctl = sample_color(s_mainTex, uv - dv - du);
	vec4 ctc = sample_color(s_mainTex, uv - dv);
	vec4 ctr = sample_color(s_mainTex, uv - dv + du);
	vec4 cml = sample_color(s_mainTex, uv - du);
	vec4 cmc = sample_color(s_mainTex, uv);
	vec4 cmr = sample_color(s_mainTex, uv + du);
	vec4 cbl = sample_color(s_mainTex, uv + dv - du);
	vec4 cbc = sample_color(s_mainTex, uv + dv);
	vec4 cbr = sample_color(s_mainTex, uv + dv + du);

	vec4 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	vec4 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	#if MINMAX_3X3_ROUNDED || USE_YCOCG || USE_CLIPPING
		vec4 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;
	#endif

	#if MINMAX_3X3_ROUNDED
		vec4 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
		vec4 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
		vec4 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
		cmin = 0.5 * (cmin + cmin5);
		cmax = 0.5 * (cmax + cmax5);
		cavg = 0.5 * (cavg + cavg5);
	#endif

#elif MINMAX_4TAP_VARYING// this is the method used in v2 (PDTemporalReprojection2)

	const float _SubpixelThreshold = 0.5;
	const float _GatherBase = 0.5;
	const float _GatherSubpixelMotion = 0.1666;

	vec2 texel_vel = ss_vel / mainTexel.xy;
	float texel_vel_mag = length(texel_vel) * vs_dist;
	float k_subpixel_motion = saturate(_SubpixelThreshold / (FLT_EPS + texel_vel_mag));
	float k_min_max_support = _GatherBase + _GatherSubpixelMotion * k_subpixel_motion;

	vec2 ss_offset01 = k_min_max_support * vec2(-mainTexel.x, mainTexel.y);
	vecs ss_offset11 = k_min_max_support * vec2(mainTexel.x, mainTexel.y);
	vec4 c00 = sample_color(s_mainTex, uv - ss_offset11);
	vec4 c10 = sample_color(s_mainTex, uv - ss_offset01);
	vec4 c01 = sample_color(s_mainTex, uv + ss_offset01);
	vec4 c11 = sample_color(s_mainTex, uv + ss_offset11);

	vec4 cmin = min(c00, min(c10, min(c01, c11)));
	vec4 cmax = max(c00, max(c10, max(c01, c11)));

	#if USE_YCOCG || USE_CLIPPING
		vec4 cavg = (c00 + c10 + c01 + c11) / 4.0;
	#endif

#else
		#error "missing keyword MINMAX_..."
#endif

	// shrink chroma min-max
#if USE_YCOCG
	vec2 chroma_extent = 0.25 * 0.5 * (cmax.r - cmin.r);
	vec2 chroma_center = texel0.gb;
	cmin.yz = chroma_center - chroma_extent;
	cmax.yz = chroma_center + chroma_extent;
	cavg.yz = chroma_center;
#endif

// clamp to neighbourhood of current sample
#if USE_CLIPPING
	texel1 = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), texel1);
#else
	texel1 = clamp(texel1, cmin, cmax);
#endif

	// feedback weight from unbiased luminance diff (t.lottes)
#if USE_YCOCG
	float lum0 = texel0.r;
	float lum1 = texel1.r;
#else
	float lum0 = Luminance(texel0.rgb);
	float lum1 = Luminance(texel1.rgb);
#endif
	float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
	float unbiased_weight = 1.0 - unbiased_diff;
	float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
	float k_feedback = lerp(_FeedbackMin, _FeedbackMax, unbiased_weight_sqr);

	// output
	return lerp(texel0, texel1, k_feedback);
}

void main()
{
#if UNJITTER_REPROJECTION
	vec2 uv = v_ss_txc - _JitterUV.xy;
#else
	vec2 uv = Iv_ss_txc;
#endif

#if USE_DILATION

	//--- 3x3 nearest (good)
	vec3 c_frag = find_closest_fragment_3x3(uv);
	vec2 ss_vel = tex2D(_VelocityBuffer, c_frag.xy).xy;
	float vs_dist = depth_resolve_linear(c_frag.z);
#else
	vec2 ss_vel = tex2D(_VelocityBuffer, uv).xy;
	float vs_dist = depth_sample_linear(uv);
#endif

	// temporal resolve
	vec4 color_temporal = temporal_reprojection(v_ss_txc, ss_vel, vs_dist);

	// prepare outputs
	vec4 to_buffer = resolve_color(color_temporal);
		
#if USE_MOTION_BLUR
	#if USE_MOTION_BLUR_NEIGHBORMAX
		ss_vel = _MotionScale * tex2D(_VelocityNeighborMax, v_ss_txc).xy;
	#else
		ss_vel = _MotionScale * ss_vel;
	#endif

	float vel_mag = length(ss_vel * mainTexel.zw);
	const float vel_trust_full = 2.0;
	const float vel_trust_none = 15.0;
	const float vel_trust_span = vel_trust_none - vel_trust_full;
	float trust = 1.0 - clamp(vel_mag - vel_trust_full, 0.0, vel_trust_span) / vel_trust_span;

	#if UNJITTER_COLORSAMPLES
		vec4 color_motion = sample_color_motion(s_mainTex, v_ss_txc - _JitterUV.xy, ss_vel);
	#else
		vec4 color_motion = sample_color_motion(s_mainTex, v_ss_txc, ss_vel);
	#endif

	vec4 to_screen = resolve_color(lerp(color_motion, color_temporal, trust));
#else
	vec4 to_screen = resolve_color(color_temporal);
#endif

	// NOTE: velocity debug

	// add noise
	vec4 noise4 = srand4(v_ss_txc + _SinTime.x + 0.6959174) / 510.0;
	gl_FragColor0 = saturate(to_buffer + noise4);
	gl_FragColor1 = saturate(to_screen + noise4);
}
