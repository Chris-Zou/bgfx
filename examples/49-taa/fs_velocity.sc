$input v_cs_pos, v_ss_pos, v_cs_xy_curr, v_cs_xy_prev

#include "../common/common.sh"
#include "depthLibs.sh"

void main()
{
	float2 ss_txc = v_ss_pos.xy / v_ss_pos.w;
	float scene_d = depth_sample_linear(ss_txc);

	clip(scene_d - v_ss_pos.z);

	float2 ndc_curr = v_cs_xy_curr.xy / v_cs_xy_curr.z;
	float2 ndc_prev = v_cs_xy_prev.xy / v_cs_xy_prev.z;

	gl_FragColor = vec4(0.5 * (ndc_curr - ndc_prev), 0.0, 0.0);
}
