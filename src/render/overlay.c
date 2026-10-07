#include "bongo_cat/overlay.h"
#include "bongo_cat/gl_api.h"
#include "bongo_cat/image.h"
#include "bongo_cat/path.h"
#include "mver_pointer_overlay.h"
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TextureSlot {
    char path[BONGO_CAT_PATH_CAP];
    GLuint texture;
    uint64_t used;
} TextureSlot;
struct BongoCatOverlay {
    BongoCatGL gl;
    BongoCatMverPointerOverlay *mver_pointer;
    GLuint program;
    GLint mirror_location;
    GLint image_location;
    GLint erase_left_location;
    GLint erase_right_location;
    GLuint vao;
    GLuint vbo;
    GLuint background;
    GLuint composite;
    bool composed_cover;
    bool clean_paws;
    bool composite_dirty;
    TextureSlot cache[4];
    GLuint left;
    GLuint right;
    GLuint effect;
    char left_name[BONGO_CAT_ID_CAP];
    char right_name[BONGO_CAT_ID_CAP];
    char background_path[BONGO_CAT_PATH_CAP];
    char left_path[BONGO_CAT_PATH_CAP];
    char right_path[BONGO_CAT_PATH_CAP];
    char effect_path[BONGO_CAT_PATH_CAP];
    char directory[BONGO_CAT_PATH_CAP];
    uint64_t clock;
};

static const char *vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos; layout(location=1) in vec2 uv;\n"
    "out vec2 tex; uniform bool mirror;\n"
    "void main(){float x=mirror?-pos.x:pos.x;gl_Position=vec4(x,pos.y,0,1);tex=uv;}";
static const char *fragment_source =
    "#version 330 core\n"
    "in vec2 tex; out vec4 color; uniform sampler2D image;\n"
    "uniform bool erase_left;uniform bool erase_right;\n"
    "void main(){color=texture(image,tex);if(color.a<=.001)discard;"
    "vec2 dl=(tex-vec2(.700,.515))/vec2(.080,.170);"
    "vec2 dr=(tex-vec2(.275,.397))/vec2(.070,.160);"
    "bool l=dot(dl,dl)<1.;bool r=dot(dr,dr)<1.;"
    "if((erase_left&&l)||(erase_right&&r))color.rgb=vec3(1);color.rgb*=color.a;}";
static void clear_textures(BongoCatOverlay *value) {
    if (value->background) glDeleteTextures(1, &value->background);
    if (value->composite) glDeleteTextures(1, &value->composite);
    value->background = 0;
    value->composite = 0;
    value->clean_paws = false;
    value->composed_cover = false;
    for (size_t i = 0; i < 4; ++i) {
        if (value->cache[i].texture) glDeleteTextures(1, &value->cache[i].texture);
        memset(&value->cache[i], 0, sizeof(value->cache[i]));
    }
    value->left = value->right = 0;
    value->effect = 0;
    value->left_name[0] = value->right_name[0] = '\0';
    value->left_path[0] = value->right_path[0] = '\0';
    value->effect_path[0] = '\0';
}

BongoCatOverlay *bongo_cat_overlay_create(BongoCatError *error) {
    BongoCatOverlay *value = calloc(1, sizeof(*value));
    if (!value || !bongo_cat_gl_load(&value->gl, error)) {
        free(value);
        return NULL;
    }
    value->program = bongo_cat_gl_program(&value->gl, vertex_source, fragment_source, error);
    if (!value->program) { free(value); return NULL; }
    value->mirror_location = value->gl.uniform_location(value->program, "mirror");
    value->image_location = value->gl.uniform_location(value->program, "image");
    value->erase_left_location = value->gl.uniform_location(value->program, "erase_left");
    value->erase_right_location = value->gl.uniform_location(value->program, "erase_right");
    const float vertices[] = {-1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0, 1, 1, 1, 0};
    value->gl.gen_vertex_arrays(1, &value->vao);
    value->gl.bind_vertex_array(value->vao);
    value->gl.gen_buffers(1, &value->vbo);
    value->gl.bind_buffer(GL_ARRAY_BUFFER, value->vbo);
    value->gl.buffer_data(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    value->gl.enable_attribute(0);
    value->gl.attribute_pointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
    value->gl.enable_attribute(1);
    value->gl.attribute_pointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
        (const void *)(sizeof(float) * 2));
    value->mver_pointer = bongo_cat_mver_pointer_overlay_create(error);
    if (!value->mver_pointer) {
        bongo_cat_overlay_destroy(value);
        return NULL;
    }
    return value;
}

void bongo_cat_overlay_destroy(BongoCatOverlay *value) {
    if (!value) return;
    bongo_cat_mver_pointer_overlay_destroy(value->mver_pointer);
    clear_textures(value);
    if (value->vbo) value->gl.delete_buffers(1, &value->vbo);
    if (value->vao) value->gl.delete_vertex_arrays(1, &value->vao);
    if (value->program) value->gl.delete_program(value->program);
    free(value);
}

BongoCatResult bongo_cat_overlay_load(BongoCatOverlay *value, const char *directory, BongoCatError *error) {
    if (!value || !directory) return BONGO_CAT_ERROR_ARGUMENT;
    clear_textures(value);
    snprintf(value->directory, sizeof(value->directory), "%s", directory);
    char path[BONGO_CAT_PATH_CAP];
    /* Without the licensed Cubism runtime there is no model renderer.  Use the
       model's composed preview so the desktop pet remains visually complete;
       Cubism builds keep the background-only layer behind the animated model. */
#ifdef BONGO_CAT_HAS_CUBISM
    bongo_cat_path_join(path, sizeof(path), directory, "resources/background.png");
#else
    bongo_cat_path_join(path, sizeof(path), directory, "resources/cover.png");
    value->composed_cover = true;
    value->clean_paws = true;
#endif
    snprintf(value->background_path, sizeof(value->background_path), "%s", path);
    if (bongo_cat_path_is_file(path)) value->background = bongo_cat_image_texture(path, NULL, NULL, error);
    return bongo_cat_mver_pointer_overlay_load(value->mver_pointer,
        directory, error) ? BONGO_CAT_OK : BONGO_CAT_ERROR_IO;
}

#ifdef BONGO_CAT_HAS_CUBISM
static GLuint cached_texture(BongoCatOverlay *value, const char *path) {
    value->clock++;
    TextureSlot *oldest = NULL;
    for (size_t i = 0; i < 4; ++i) {
        TextureSlot *slot = &value->cache[i];
        if (slot->texture && strcmp(slot->path, path) == 0) {
            slot->used = value->clock;
            return slot->texture;
        }
        if (slot->texture &&
            (slot->texture == value->left || slot->texture == value->right ||
            slot->texture == value->effect)) continue;
        if (!oldest || !slot->texture || slot->used < oldest->used) oldest = slot;
    }
    if (!oldest) return 0;
    if (oldest->texture) glDeleteTextures(1, &oldest->texture);
    BongoCatError ignored = {0};
    oldest->texture = bongo_cat_image_texture(path, NULL, NULL, &ignored);
    snprintf(oldest->path, sizeof(oldest->path), "%s", path);
    oldest->used = value->clock;
    return oldest->texture;
}
#endif

static bool key_path(BongoCatOverlay *value, const char *group, const char *name,
    char path[BONGO_CAT_PATH_CAP]) {
    char relative[BONGO_CAT_PATH_CAP];
    snprintf(relative, sizeof(relative), "resources/%s/%s.png", group, name);
    bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, value->directory, relative);
    if (bongo_cat_path_is_file(path)) return true;
    if (name[0] == 'F' && name[1] >= '0' && name[1] <= '9') {
        snprintf(relative, sizeof(relative), "resources/%s/Fn.png", group);
        bongo_cat_path_join(path, BONGO_CAT_PATH_CAP, value->directory, relative);
        return bongo_cat_path_is_file(path);
    }
    return false;
}

int bongo_cat_overlay_key(BongoCatOverlay *value, const char *name, bool pressed) {
    if (!value || !name) return -1;
    bool right;
    char path[BONGO_CAT_PATH_CAP];
    if (key_path(value, "right-keys", name, path)) right = true;
    else if (key_path(value, "left-keys", name, path)) right = false;
    else return -1;
    char *active_name = right ? value->right_name : value->left_name;
    GLuint *active = right ? &value->right : &value->left;
    char *active_path = right ? value->right_path : value->left_path;
    if (pressed) {
        snprintf(active_name, BONGO_CAT_ID_CAP, "%s", name);
#ifdef BONGO_CAT_HAS_CUBISM
        *active = cached_texture(value, path);
#else
        snprintf(active_path, BONGO_CAT_PATH_CAP, "%s", path);
        *active = 1;
#endif
    } else if (strcmp(active_name, name) == 0) {
        active_name[0] = '\0';
        *active = 0;
        active_path[0] = '\0';
    }
#ifndef BONGO_CAT_HAS_CUBISM
    value->composite_dirty = true;
#endif
    return right ? 1 : 0;
}

bool bongo_cat_overlay_hand_active(const BongoCatOverlay *value, bool right) {
    return value && (right ? value->right : value->left) != 0;
}

bool bongo_cat_overlay_mver_pointer_enabled(const BongoCatOverlay *value) {
    return value && bongo_cat_mver_pointer_overlay_enabled(value->mver_pointer);
}
bool bongo_cat_overlay_mver_pointer_left_handed(const BongoCatOverlay *value) {
    return value && bongo_cat_mver_pointer_overlay_left_handed(value->mver_pointer);
}
void bongo_cat_overlay_set_mver_pointer(BongoCatOverlay *value,
    float x_ratio, float y_ratio, bool left, bool right, bool side) {
    if (value) bongo_cat_mver_pointer_overlay_set(value->mver_pointer,
        x_ratio, y_ratio, left, right, side);
}
bool bongo_cat_overlay_effect(BongoCatOverlay *value, const char *path) {
    if (!value) return false;
    value->effect = 0;
    value->effect_path[0] = '\0';
    if (!path || !*path) return true;
    if (!bongo_cat_path_is_file(path)) return false;
#ifdef BONGO_CAT_HAS_CUBISM
    value->effect = cached_texture(value, path);
#else
    value->effect = 1;
#endif
    snprintf(value->effect_path, sizeof(value->effect_path), "%s", path);
    return value->effect != 0;
}

static void draw(BongoCatOverlay *value, GLuint texture, bool mirror, bool blend) {
    if (!value || !texture) return;
    if (blend) {
        glEnable(GL_BLEND);
        value->gl.blend_func_separate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else glDisable(GL_BLEND);
    value->gl.use_program(value->program);
    value->gl.uniform_1i(value->mirror_location, mirror);
    value->gl.uniform_1i(value->image_location, 0);
    value->gl.uniform_1i(value->erase_left_location,
        !blend && value->composed_cover && !value->composite && value->left != 0);
    value->gl.uniform_1i(value->erase_right_location,
        !blend && value->composed_cover && !value->composite && value->right != 0);
    value->gl.active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    value->gl.bind_vertex_array(value->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    value->gl.bind_vertex_array(0);
}

void bongo_cat_overlay_draw_background(BongoCatOverlay *value, bool mirror) {
    if (value) {
#ifndef BONGO_CAT_HAS_CUBISM
        if (value->composite_dirty) {
            value->composite_dirty = false;
            if (value->left || value->right) {
                BongoCatError ignored = {0};
                value->composite = bongo_cat_image_composite_texture(value->background_path,
                    value->left_path, value->right_path, value->composite,
                    value->clean_paws && value->left,
                    value->clean_paws && value->right, &ignored);
            }
        }
#endif
        bool active = value->left || value->right;
        draw(value, active && value->composite ? value->composite : value->background,
            mirror, false);
    }
}

void bongo_cat_overlay_draw_keys(BongoCatOverlay *value, bool mirror) {
    if (!value) return;
#ifdef BONGO_CAT_HAS_CUBISM
    draw(value, value->left, mirror, true);
    draw(value, value->right, mirror, true);
#else
    (void)mirror;
#endif
}
void bongo_cat_overlay_draw_pointer_before_keys(BongoCatOverlay *value) {
    if (value) bongo_cat_mver_pointer_overlay_draw_before_keys(value->mver_pointer);
}
void bongo_cat_overlay_draw_effect(BongoCatOverlay *value, bool mirror) {
    if (!value) return;
#ifdef BONGO_CAT_HAS_CUBISM
    draw(value, value->effect, mirror, true);
#else
    (void)mirror;
#endif
}
void bongo_cat_overlay_draw_pointer_after_keys(BongoCatOverlay *value) {
    if (value) bongo_cat_mver_pointer_overlay_draw_after_keys(value->mver_pointer);
}
