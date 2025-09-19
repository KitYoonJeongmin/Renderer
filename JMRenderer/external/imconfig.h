//------------------------------------------------------------------------------
// imconfig.h
// ImGui 사용자 설정 헤더 (선택적)
//------------------------------------------------------------------------------
// ⚠️ 지금은 커스텀 설정이 필요 없으니 비워둬도 됩니다.
//     나중에 GLM, DirectXMath 같은 수학 라이브러리와 연결할 때
//     이 파일에서 매크로를 정의하면 됩니다.
//------------------------------------------------------------------------------

#pragma once

// 예시: GLM과 연동하고 싶을 때 이렇게 확장할 수 있습니다.
//#define IM_VEC2_CLASS_EXTRA                                             \
//        ImVec2(const glm::vec2& f) { x = f.x; y = f.y; }                \
//        operator glm::vec2() const { return glm::vec2(x,y); }
//
//#define IM_VEC4_CLASS_EXTRA                                             \
//        ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; } \
//        operator glm::vec4() const { return glm::vec4(x,y,z,w); }
