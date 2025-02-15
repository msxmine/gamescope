/////////////////////////////
// SRGB Encoding Helpers
/////////////////////////////

// Go from sRGB encoding -> linear
vec3 srgbToLinear(vec3 color) {
    bvec3 isLo = lessThanEqual(color, vec3(0.04045f));

    vec3 loPart = color / 12.92f;
    vec3 hiPart = pow((color + 0.055f) / 1.055f, vec3(12.0f / 5.0f));
    return mix(hiPart, loPart, isLo);
}

vec4 srgbToLinear(vec4 color) {
  return vec4(srgbToLinear(color.rgb), color.a);
}

// Go from linear -> sRGB encoding.
vec3 linearToSrgb(vec3 color) {
    bvec3 isLo = lessThanEqual(color, vec3(0.0031308f));

    vec3 loPart = color * 12.92f;
    vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
    return mix(hiPart, loPart, isLo);
}

vec4 linearToSrgb(vec4 color) {
  return vec4(linearToSrgb(color.rgb), color.a);
}

/////////////////////////////
// PQ Encoding Helpers
/////////////////////////////

// Converts nits -> pq and pq -> nits
// Does NOT affect primaries at all.
vec3 nitsToPq(vec3 nits) {
    vec3 y = clamp(nits / 10000.0, vec3(0.0), vec3(1.0));
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    vec3 num = c1 + c2 * pow(y, vec3(m1));
    vec3 den = 1.0 + c3 * pow(y, vec3(m1));
    vec3 n = pow(num / den, vec3(m2));
    return n;
}

vec3 pqToNits(vec3 pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    const float oo_m1 = 1.0 / 0.1593017578125;
    const float oo_m2 = 1.0 / 78.84375;

    vec3 num = max(pow(pq, vec3(oo_m2)) - c1, vec3(0.0));
    vec3 den = c2 - c3 * pow(pq, vec3(oo_m2));

    return 10000.0 * pow(num / den, vec3(oo_m1));
}

// This is apparently defined at 80 nits...
// May want to take liberties with this when displaying
// on SDR though... 100 may be a better fit for most content
// to match typical sRGB mastering.
const float c_scRGBLightScale = 80.0f;
vec3 scrgbToNits(vec3 scRGB) {
    return scRGB * c_scRGBLightScale;
}

vec3 nitsToScRGB(vec3 nits) {
    return nits / c_scRGBLightScale;
}

// nits -> linear (nits / scale)
vec3 nitsToLinear(vec3 nits) {
    return nits * u_nitsToLinear;
}

// linear -> nits (linear * scale)
vec3 linearToNits(vec3 linear) {
    return linear * u_linearToNits;
}

/////////////////////////////
// Primary Conversion Helpers
/////////////////////////////

struct PrimaryInfo {
    vec2 displayPrimaryRed;
    vec2 displayPrimaryGreen;
    vec2 displayPrimaryBlue;
    vec2 whitePoint;
};

vec3 convert_primary(vec2 xy) {
	float X = xy.x / xy.y;
	float Y = 1.0f;
	float Z = (1.0f - xy.x - xy.y) / xy.y;
	return vec3(X, Y, Z);
}

mat3 compute_xyz_matrix(PrimaryInfo metadata) {
	vec3 red = convert_primary(metadata.displayPrimaryRed);
	vec3 green = convert_primary(metadata.displayPrimaryGreen);
	vec3 blue = convert_primary(metadata.displayPrimaryBlue);
	vec3 white = convert_primary(metadata.whitePoint);

	vec3 component_scale = inverse(mat3(red, green, blue)) * white;
	return transpose(mat3(red * component_scale.x, green * component_scale.y, blue * component_scale.z));
}

const PrimaryInfo rec709_primaries = {
    vec2(0.640f, 0.330f),     // red
    vec2(0.300f, 0.600f),     // green
    vec2(0.150f, 0.060f),     // blue
    vec2(0.3127f, 0.3290f),   // whitepoint
};
/*const*/ mat3 rec709_to_xyz = compute_xyz_matrix(rec709_primaries);
/*const*/ mat3 xyz_to_rec709 = inverse(rec709_to_xyz);

const PrimaryInfo rec2020_primaries = {
    vec2(0.708f, 0.292f),     // red
    vec2(0.170f, 0.797f),     // green
    vec2(0.131f, 0.046f),     // blue
    vec2(0.3127f, 0.3290f),   // whitepoint
};
/*const*/ mat3 rec2020_to_xyz = compute_xyz_matrix(rec2020_primaries);
/*const*/ mat3 xyz_to_rec2020 = inverse(rec2020_to_xyz);

vec3 convert_primaries(vec3 color, mat3 src_to_xyz, mat3 xyz_to_dst) {
    return color * mat3(src_to_xyz * xyz_to_dst);
}

const float A = 0.15f;
const float B = 0.50f;
const float C = 0.10f;
const float D = 0.20f;
const float E = 0.02f;
const float F = 0.30f;
const float W = 11.2f;

vec3 Uncharted2Tonemap(vec3 x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float Uncharted2Tonemap(float x) {
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemap_filmic(vec3 color) {
    vec3 curr = Uncharted2Tonemap(color);
    float white_scale = 1.0 / Uncharted2Tonemap(W);
    return curr * white_scale;
}

vec3 tonemap_reinhard(vec3 color) {
    color = color / (1.0 + color);
    return color;
}

vec3 tonemap(vec3 color) {
     if (checkDebugFlag(compositedebug_Tonemap_Reinhard))
        return tonemap_reinhard(color);
    return tonemap_filmic(color);
}

// Rep. ITU-R BT.2446-1 Table 2-4 (inversed)
// BT.2446 Method A inverse tone mapping (itm)
vec3 bt2446a_inverse_tonemapping(
    vec3  color,
    float sdr_nits,
    float target_nits)
{
    const vec3 k_bt2020 = vec3(0.262698338956556, 0.678008765772817, 0.0592928952706273);
    const float k_bt2020_r_helper = 1.47460332208689; // 2 - 2 * 0.262698338956556
    const float k_bt2020_b_helper = 1.88141420945875; // 2 - 2 * 0.0592928952706273

    //gamma
    const float inverse_gamma = 2.4f;
    const float gamma = 1.f / inverse_gamma;

    //RGB->R'G'B' gamma compression
    color = pow(color, vec3(gamma));

    // Rec. ITU-R BT.2020-2 Table 4
    //Y'tmo
    const float y_tmo = dot(color, k_bt2020);
    //C'b,tmo
    const float c_b_tmo = (color.b - y_tmo) /
                          k_bt2020_b_helper;
    //C'r,tmo
    const float c_r_tmo = (color.r - y_tmo) /
                          k_bt2020_r_helper;

    // fast path as per Rep. ITU-R BT.2446-1 Table 4
    // matches the output of the inversed version for the given input
    if ((sdr_nits > 99.f && sdr_nits < 101.f) && (target_nits > 999.f && target_nits < 1001.f))
    //avoid float issues
    {
        sdr_nits = 100.f;
        target_nits = 1000.f;

        const float a1 =  1.8712e-5;
        const float b1 = -2.7334e-3;
        const float c1 =  1.3141;
        const float a2 =  2.8305e-6;
        const float b2 = -7.4622e-4;
        const float c2 =  1.2328;

        const float yy_ = 255.0f * y_tmo;

        const float t = 70;

        float e = yy_ <= t ?
            a1 * pow(yy_, 2.f) + b1 * yy_ + c1 : 
            a2 * pow(yy_, 2.f) + b2 * yy_ + c2;

        const float y_hdr = pow(yy_, e);

        float s_c = y_tmo > 0.f ?
            1.075f * (y_hdr / y_tmo) : 
            1.f;

        const float c_b_hdr = c_b_tmo * s_c;
        const float c_r_hdr = c_r_tmo * s_c;

        color = vec3(clamp(y_hdr + k_bt2020_r_helper * c_r_hdr, 0.f, 1000.f),
                     clamp(y_hdr - 0.16455312684366 * c_b_hdr - 0.57135312684366 * c_r_hdr, 0.f, 1000.f),
                     clamp(y_hdr + k_bt2020_b_helper * c_b_hdr, 0.f, 1000.f));
        color /= 1000.f;
    }
    else
    {
        // adjusted luma component (inverse)
        // get Y'sdr
        const float y_sdr = y_tmo + max(0.1f * c_r_tmo, 0.f);

        // Tone mapping step 3 (inverse)
        // get Y'c
        const float p_sdr = 1 + 32 * pow(
                                         sdr_nits /
                                         10000.f
                                    , gamma);
        //Y'c
        const float y_c = log((y_sdr * (p_sdr - 1)) + 1) /
                          log(p_sdr); //log = ln

        // Tone mapping step 2 (inverse)
        // get Y'p
        float y_p = 0.f;

        const float y_p_0 = y_c / 1.0770f;
        const float y_p_2 = (y_c - 0.5000f) /
                            0.5000f;

        const float _first = -2.7811f;
        const float  _sqrt = sqrt(4.83307641 - 4.604 * y_c);
        const float   _div = -2.302f;
        const float  y_p_1 = (_first + _sqrt) /
                             _div;

        if (y_p_0 <= 0.7399f)
            y_p = y_p_0;
        else if (y_p_1 > 0.7399f && y_p_1 < 0.9909f)
            y_p = y_p_1;
        else if (y_p_2 >= 0.9909f)
            y_p = y_p_2;
        else //y_p_1 sometimes (about 0.12% out of the full RGB range)
             //is less than 0.7399f or more than 0.9909f because of float inaccuracies
        {
            //error is small enough (less than 0.001) for this to be OK
            //ideally you would choose between y_p_0 and y_p_1 if y_p_1 < 0.7399f depending on which is closer to 0.7399f
            //or between y_p_1 and y_p_2 if y_p_1 > 0.9909f depending on which is closer to 0.9909f
            y_p = y_p_1;

            //this clamps it to 2 float steps above 0.7399f or 2 float steps below 0.9909f
            //if (y_p_1 < 0.7399f)
            //	y_p = 0.7399001f;
            //else
            //	y_p = 0.99089986f;
        }

        // Tone mapping step 1 (inverse)
        // get Y'
        const float p_hdr = 1 + 32 * pow(
                                         target_nits /
                                         10000.f
                                    , gamma);
        //Y'
        const float y_ = (pow(p_hdr, y_p) - 1) /
                         (p_hdr - 1);

        // Colour scaling function
        const float col_scale = y_sdr /
                                (1.1f * y_);

        // Colour difference signals (inverse) and Luma (inverse)
        // get R'G'B'
        color.b = ((c_b_tmo * k_bt2020_b_helper) /
                  col_scale) + y_;
        color.r = ((c_r_tmo * k_bt2020_r_helper) /
                  col_scale) + y_;
        color.g = (y_ - (k_bt2020.r * color.r + k_bt2020.b * color.b)) /
                  k_bt2020.g;

        //safety
        color.r = clamp(color.r, 0.f, 1.f);
        color.g = clamp(color.g, 0.f, 1.f);
        color.b = clamp(color.b, 0.f, 1.f);
    }

    // R'G'B' gamma expansion
    color = pow(color, vec3(inverse_gamma));

    // map target luminance into 10000 nits
    color = color * target_nits;

    return color;
}

#include "heatmap.h"
