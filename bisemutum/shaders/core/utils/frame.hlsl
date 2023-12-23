#pragma once

struct Frame {
    float3 x;
    float3 y;
    float3 z;
};

Frame create_frame(float3 normal) {
    Frame frame;
    frame.z = normal;
    float sign = normal.z > 0.0 ? 1.0 : -1.0;
    const float a = -1.0f / (sign + normal.z);
    const float b = normal.x * normal.y * a;
    frame.x = float3(1.0 + sign * normal.x * normal.x * a, sign * b, -sign * normal.x);
    frame.y = float3(b, sign + normal.y * normal.y * a, -normal.y);
    return frame;
}

Frame create_frame(float3 normal, float3 tangent) {
    Frame frame;
    frame.z = normal;
    frame.y = normalize(cross(normal, tangent));
    frame.x = cross(frame.y, normal);
    return frame;
}

float3 frame_to_local(Frame frame, float3 v) {
    return float3(dot(v, frame.x), dot(v, frame.y), dot(v, frame.z));
}

float3 frame_to_world(Frame frame, float3 v) {
    return v.x * frame.x + v.y * frame.y + v.z * frame.z;
}
