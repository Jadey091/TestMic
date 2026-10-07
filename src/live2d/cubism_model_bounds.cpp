#include "cubism_model.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace bongo_cat {

NativeModel::ModelBounds NativeModel::capture_visible_bounds() const {
    ModelBounds bounds;
    if (!_model) return bounds;
    struct DrawableBounds { ModelBounds bounds; float area; };
    std::vector<DrawableBounds> drawables;
    size_t largest = 0;
    for (int i = 0; i < _model->GetDrawableCount(); ++i) {
        if (!_model->GetDrawableDynamicFlagIsVisible(i) ||
            _model->GetDrawableOpacity(i) <= 0.001f) continue;
        const float *vertices = _model->GetDrawableVertices(i);
        int count = _model->GetDrawableVertexCount(i);
        if (!vertices || count <= 0) continue;
        int texture = _model->GetDrawableTextureIndex(i);
        const BongoCatImageAlphaMask *mask = texture >= 0 &&
            (size_t)texture < texture_alpha_.size() ? &texture_alpha_[(size_t)texture] : nullptr;
        const auto *uvs = _model->GetDrawableVertexUvs(i);
        const auto *indices = _model->GetDrawableVertexIndices(i);
        int index_count = _model->GetDrawableVertexIndexCount(i);
        ModelBounds drawable = {FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, true};
        float visible_area = 0.0f;
        auto alpha = [&](float u, float v) {
            if (!mask || !mask->width || !mask->height) return 255;
            int x = std::max(0, std::min(mask->width - 1, (int)(u * mask->width)));
            int y = std::max(0, std::min(mask->height - 1, (int)(v * mask->height)));
            return (int)mask->pixels[(size_t)y * mask->width + x];
        };
        for (int j = 0; indices && uvs && j + 2 < index_count; j += 3) {
            int a = indices[j], b = indices[j + 1], c = indices[j + 2];
            if (a >= count || b >= count || c >= count) continue;
            float ax = vertices[a * 2], ay = vertices[a * 2 + 1];
            float bx = vertices[b * 2], by = vertices[b * 2 + 1];
            float cx = vertices[c * 2], cy = vertices[c * 2 + 1];
            float u = (uvs[a].X + uvs[b].X + uvs[c].X) / 3.0f;
            float v = (uvs[a].Y + uvs[b].Y + uvs[c].Y) / 3.0f;
            int opacity = std::max({alpha(u, v),
                alpha((uvs[a].X + uvs[b].X) * 0.5f, (uvs[a].Y + uvs[b].Y) * 0.5f),
                alpha((uvs[b].X + uvs[c].X) * 0.5f, (uvs[b].Y + uvs[c].Y) * 0.5f),
                alpha((uvs[c].X + uvs[a].X) * 0.5f, (uvs[c].Y + uvs[a].Y) * 0.5f)});
            if (opacity <= 8) continue;
            float area = std::fabs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
            visible_area += area * opacity / 255.0f;
            drawable.min_x = std::min({drawable.min_x, ax, bx, cx});
            drawable.min_y = std::min({drawable.min_y, ay, by, cy});
            drawable.max_x = std::max({drawable.max_x, ax, bx, cx});
            drawable.max_y = std::max({drawable.max_y, ay, by, cy});
        }
        if (visible_area <= FLT_EPSILON) continue;
        drawables.push_back({drawable, visible_area});
        if (visible_area > drawables[largest].area) largest = drawables.size() - 1;
    }
    if (drawables.empty()) return bounds;
    const DrawableBounds &anchor = drawables[largest];
    float extent = std::max(anchor.bounds.max_x - anchor.bounds.min_x,
        anchor.bounds.max_y - anchor.bounds.min_y);
    float padding = extent * 0.35f;
    bounds = anchor.bounds;
    // Small, remote effect particles must not dictate the scale of the subject.
    for (const DrawableBounds &drawable : drawables) {
        float dx = std::max({anchor.bounds.min_x - drawable.bounds.max_x,
            drawable.bounds.min_x - anchor.bounds.max_x, 0.0f});
        float dy = std::max({anchor.bounds.min_y - drawable.bounds.max_y,
            drawable.bounds.min_y - anchor.bounds.max_y, 0.0f});
        bool nearby = dx <= padding && dy <= padding;
        bool major = drawable.area >= anchor.area * 0.05f;
        if (drawable.area < anchor.area * 0.0005f || (!nearby && !major)) continue;
        bounds.min_x = std::min(bounds.min_x, drawable.bounds.min_x);
        bounds.min_y = std::min(bounds.min_y, drawable.bounds.min_y);
        bounds.max_x = std::max(bounds.max_x, drawable.bounds.max_x);
        bounds.max_y = std::max(bounds.max_y, drawable.bounds.max_y);
    }
    return bounds;
}

void NativeModel::record_visible_state(Csm::CubismMatrix44 &projection) {
    visual_state_.drawable_count = _model->GetDrawableCount();
    for (int i = 0; i < _model->GetDrawableCount(); ++i) {
        if (_model->GetDrawableDynamicFlagIsVisible(i) &&
            _model->GetDrawableOpacity(i) > 0.001f)
            ++visual_state_.drawable_visible;
        if (_model->GetDrawableDynamicFlagVertexPositionsDidChange(i))
            ++visual_state_.drawable_vertex_changed;
    }
    visual_state_.offscreen_count = _model->GetOffscreenCount();
    for (int i = 0; i < _model->GetOffscreenCount(); ++i)
        if (_model->GetOffscreenOpacity(i) > 0.001f)
            ++visual_state_.offscreen_positive;
    visual_state_.part_count = _model->GetPartCount();
    for (int i = 0; i < _model->GetPartCount(); ++i)
        if (_model->GetPartOpacity(i) > 0.001f) ++visual_state_.part_positive;
    ModelBounds bounds = capture_visible_bounds();
    if (!bounds.valid) return;
    float x0 = projection.TransformX(bounds.min_x);
    float x1 = projection.TransformX(bounds.max_x);
    float y0 = projection.TransformY(bounds.min_y);
    float y1 = projection.TransformY(bounds.max_y);
    visual_state_.visible_min_x = std::min(x0, x1);
    visual_state_.visible_max_x = std::max(x0, x1);
    visual_state_.visible_min_y = std::min(y0, y1);
    visual_state_.visible_max_y = std::max(y0, y1);
    visual_state_.visible = true;
}

bool NativeModel::visual_state(BongoCatLive2DVisualState *state) const {
    if (!state || !visual_state_ready_) return false;
    *state = visual_state_;
    return true;
}

} // namespace bongo_cat
