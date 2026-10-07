#ifndef BONGO_CAT_CUBISM_MODEL_HPP
#define BONGO_CAT_CUBISM_MODEL_HPP

#include "bongo_cat/model.h"
#include "bongo_cat/image.h"

#include <Model/CubismUserModel.hpp>
#include <CubismModelSettingJson.hpp>
#include <Motion/ACubismMotion.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <SDL3/SDL_opengl.h>
#include <map>
#include <string>
#include <vector>

namespace bongo_cat {

bool validate_model_setting_json(const std::vector<unsigned char> &json,
    const char *setting_file, BongoCatError *error);
class ViewerLookUpdater;

class NativeModel final : public Csm::CubismUserModel {
public:
    NativeModel();
    ~NativeModel() override;
    bool load(const char *directory, const char *setting_file, bool direct_textures,
        BongoCatError *error);
    bool load_textures(BongoCatError *error);
    void release_render_resources();
    void resize(int width, int height);
    void reshape(int width, int height);
    bool update(float delta_seconds);
    void draw();
    void set_mirror(bool mirror);
    void set_render_options(const BongoCatLive2DRenderOptions &options);
    void set_dragging(float x, float y);
    void prepare_viewer_audit();
    bool set_parameter(const char *id, float value);
    bool parameter(const char *id, float *minimum, float *maximum, float *value);
    bool start_motion(const char *group, int index);
    bool set_expression(int index);
    int expression() const { return expression_index_; }
    bool visual_state(BongoCatLive2DVisualState *state) const;

private:
    using MotionMap = std::map<std::string, Csm::ACubismMotion *>;
    struct ModelBounds {
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        bool valid = false;
    };
    struct LockMotion {
        std::vector<int> parameters;
        std::vector<float> initial_values;
        bool enabled = false;
    };
    bool load_model(BongoCatError *error);
    void load_expressions();
    void load_effects();
    void load_motions();
    void start_idle_motion();
    ModelBounds capture_visible_bounds() const;
    void record_visible_state(Csm::CubismMatrix44 &projection);
    void load_lock_motion(const std::string &key,
        const std::vector<unsigned char> &bytes);
    bool toggle_lock_motion(const std::string &key, Csm::ACubismMotion *motion);
    void release_textures();
    void release_renderer();
    void bind_textures();
    std::vector<unsigned char> read(const std::string &path,
        size_t maximum = (size_t)-1) const;
    std::string path(const char *relative) const;

    Csm::CubismModelSettingJson *setting_ = nullptr;
    MotionMap motions_;
    std::map<std::string, LockMotion> lock_motions_;
    MotionMap expressions_;
    std::vector<std::string> expression_names_;
    std::vector<GLuint> textures_;
    std::vector<BongoCatImageAlphaMask> texture_alpha_;
    std::vector<float> parameter_snapshot_;
    std::vector<float> part_snapshot_;
    std::vector<float> pending_parameter_values_;
    std::vector<unsigned char> pending_parameters_;
    Csm::csmVector<Csm::CubismIdHandle> eye_blink_ids_;
    Csm::csmVector<Csm::CubismIdHandle> lip_sync_ids_;
    std::string directory_;
    int width_ = 612;
    int height_ = 354;
    int renderer_width_ = 0;
    int renderer_height_ = 0;
    int expression_index_ = -1;
    BongoCatLive2DVisualState visual_state_{};
    bool visual_state_ready_ = false;
    bool motion_updated_ = false;
    bool suppress_eye_blink_ = false;
    bool automatic_idle_ = true;
    bool mirror_ = false;
    BongoCatLive2DRenderOptions render_options_{};
    bool direct_textures_ = false;
    bool external_parameters_dirty_ = false;
    std::vector<std::string> idle_motion_keys_;
    ViewerLookUpdater *viewer_look_ = nullptr;
    int last_idle_motion_ = -1;
    float opacity_snapshot_ = -1.0f;
};

} // namespace bongo_cat

#endif
