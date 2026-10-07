#include "bongo_cat/microphone.h"

#include <miniaudio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct BongoCatMicrophone {
    ma_device device;
    bool initialized;
    bool active;
    float gain;
    float threshold;
    /* 音频线程写入，主线程读取。用简单的互斥保护。 */
    ma_mutex level_lock;
    float smoothed_level;  /* 经过低通平滑后的当前电平 [0,1] */
    float instantaneous;   /* 最近一帧的原始 RMS */
};

/* 低通平滑系数：越小越平滑，口型越跟手但不抖。0.3 是个舒服的值。 */
#define BONGO_CAT_MIC_SMOOTHING 0.30f
#define BONGO_CAT_MIC_DEFAULT_GAIN 1.0f
#define BONGO_CAT_MIC_DEFAULT_THRESHOLD 0.02f

static void mic_callback(ma_device *device, void *output, const void *input,
    ma_uint32 frame_count) {
    (void)output;
    BongoCatMicrophone *mic = (BongoCatMicrophone *)device->pUserData;
    if (!mic || !input || frame_count == 0) return;

    const float *samples = (const float *)input;
    double sum_sq = 0.0;
    for (ma_uint32 i = 0; i < frame_count; ++i) {
        float s = samples[i];
        sum_sq += (double)s * (double)s;
    }
    float rms = (float)sqrt(sum_sq / (double)frame_count);

    /* 应用增益并钳位到 [0,1] */
    rms *= mic->gain;
    if (rms > 1.0f) rms = 1.0f;

    /* 噪声门：低于阈值视为 0 */
    if (rms < mic->threshold) rms = 0.0f;

    ma_mutex_lock(&mic->level_lock);
    mic->instantaneous = rms;
    /* 一阶低通平滑，让口型开合更自然 */
    mic->smoothed_level += (rms - mic->smoothed_level) * BONGO_CAT_MIC_SMOOTHING;
    if (mic->smoothed_level < 0.0f) mic->smoothed_level = 0.0f;
    if (mic->smoothed_level > 1.0f) mic->smoothed_level = 1.0f;
    ma_mutex_unlock(&mic->level_lock);
}

BongoCatMicrophone *bongo_cat_microphone_create(BongoCatError *error) {
    (void)error;
    BongoCatMicrophone *mic = calloc(1, sizeof(*mic));
    if (!mic) return NULL;
    mic->gain = BONGO_CAT_MIC_DEFAULT_GAIN;
    mic->threshold = BONGO_CAT_MIC_DEFAULT_THRESHOLD;
    if (ma_mutex_init(&mic->level_lock) != MA_SUCCESS) {
        free(mic);
        return NULL;
    }
    return mic;
}

bool bongo_cat_microphone_start(BongoCatMicrophone *mic, BongoCatError *error) {
    if (!mic) return false;
    if (mic->active) return true;

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 48000;
    config.dataCallback = mic_callback;
    config.pUserData = mic;
    /* 用较小的 period 降低延迟，口型更跟手 */
    config.periodSizeInMilliseconds = 20;
    config.periods = 2;

    ma_result result = ma_device_init(NULL, &config, &mic->device);
    if (result != MA_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Microphone device init failed: %d", (int)result);
        return false;
    }
    mic->initialized = true;

    result = ma_device_start(&mic->device);
    if (result != MA_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Microphone start failed: %d", (int)result);
        ma_device_uninit(&mic->device);
        mic->initialized = false;
        return false;
    }
    mic->active = true;
    return true;
}

void bongo_cat_microphone_stop(BongoCatMicrophone *mic) {
    if (!mic || !mic->active) return;
    ma_device_stop(&mic->device);
    ma_device_uninit(&mic->device);
    mic->active = false;
    mic->initialized = false;
    ma_mutex_lock(&mic->level_lock);
    mic->smoothed_level = 0.0f;
    mic->instantaneous = 0.0f;
    ma_mutex_unlock(&mic->level_lock);
}

float bongo_cat_microphone_level(const BongoCatMicrophone *mic) {
    if (!mic || !mic->active) return 0.0f;
    /* 去掉 const 是因为 ma_mutex_lock 需要非 const，但这里只是读。
       miniaudio 的 mutex 没有 const 版本，用强制转换。 */
    BongoCatMicrophone *m = (BongoCatMicrophone *)mic;
    ma_mutex_lock(&m->level_lock);
    float level = m->smoothed_level;
    ma_mutex_unlock(&m->level_lock);
    return level;
}

void bongo_cat_microphone_set_gain(BongoCatMicrophone *mic, float gain) {
    if (!mic) return;
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 10.0f) gain = 10.0f;
    mic->gain = gain;
}

void bongo_cat_microphone_set_threshold(BongoCatMicrophone *mic, float threshold) {
    if (!mic) return;
    if (threshold < 0.0f) threshold = 0.0f;
    if (threshold > 1.0f) threshold = 1.0f;
    mic->threshold = threshold;
}

bool bongo_cat_microphone_active(const BongoCatMicrophone *mic) {
    return mic && mic->active;
}

void bongo_cat_microphone_destroy(BongoCatMicrophone *mic) {
    if (!mic) return;
    bongo_cat_microphone_stop(mic);
    ma_mutex_uninit(&mic->level_lock);
    free(mic);
}
