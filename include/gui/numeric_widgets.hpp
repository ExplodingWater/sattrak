#pragma once

#include <algorithm>
#include <cmath>
#include "imgui.h"

namespace gui {

inline bool sliderFloatExact(const char* label, float* value, float minimum, float maximum,
                             const char* sliderFormat, const char* inputFormat,
                             ImGuiSliderFlags flags = 0) {
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const float editWidth = 82.0f;
    const float sliderWidth = std::max(54.0f, ImGui::GetContentRegionAvail().x - editWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::SetNextItemWidth(sliderWidth);
    bool changed = ImGui::SliderFloat("##slider", value, minimum, maximum, sliderFormat,
        flags | ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(editWidth);
    changed |= ImGui::InputFloat("##exact", value, 0, 0, inputFormat,
        ImGuiInputTextFlags_CharsScientific);
    if (!std::isfinite(*value)) *value = minimum;
    *value = std::clamp(*value, minimum, maximum);
    ImGui::PopID();
    ImGui::EndGroup();
    return changed;
}

inline bool sliderIntExact(const char* label, int* value, int minimum, int maximum,
                           const char* sliderFormat = "%d") {
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    const float editWidth = 72.0f;
    const float sliderWidth = std::max(48.0f, ImGui::GetContentRegionAvail().x - editWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::SetNextItemWidth(sliderWidth);
    bool changed = ImGui::SliderInt("##slider", value, minimum, maximum, sliderFormat,
        ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(editWidth);
    changed |= ImGui::InputInt("##exact", value, 0, 0, ImGuiInputTextFlags_CharsDecimal);
    *value = std::clamp(*value, minimum, maximum);
    ImGui::PopID();
    ImGui::EndGroup();
    return changed;
}

} // namespace gui
