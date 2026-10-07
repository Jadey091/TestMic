#ifndef BONGO_CAT_MICROPHONE_H
#define BONGO_CAT_MICROPHONE_H

#include "bongo_cat/common.h"

typedef struct BongoCatMicrophone BongoCatMicrophone;

/**
 * 创建麦克风捕获实例。
 * 失败时返回 NULL 并通过 error 输出原因（可能为 NULL 忽略）。
 */
BongoCatMicrophone *bongo_cat_microphone_create(BongoCatError *error);

/**
 * 销毁麦克风实例，停止捕获并释放资源。
 */
void bongo_cat_microphone_destroy(BongoCatMicrophone *mic);

/**
 * 启动麦克风捕获。sample_rate 建议 48000，channels 固定 1。
 * 启动失败返回 false。
 */
bool bongo_cat_microphone_start(BongoCatMicrophone *mic, BongoCatError *error);

/**
 * 停止麦克风捕获。
 */
void bongo_cat_microphone_stop(BongoCatMicrophone *mic);

/**
 * 获取当前平滑后的音量电平，范围 [0.0, 1.0]。
 * 未启动或无信号时返回 0.0。
 */
float bongo_cat_microphone_level(const BongoCatMicrophone *mic);

/**
 * 设置音量灵敏度乘数，默认 1.0。
 * 大于 1 更灵敏，小于 1 更迟钝。
 */
void bongo_cat_microphone_set_gain(BongoCatMicrophone *mic, float gain);

/**
 * 设置静音阈值（0.0 - 1.0），低于此值的音量视为 0。
 * 默认 0.02，用于过滤环境底噪。
 */
void bongo_cat_microphone_set_threshold(BongoCatMicrophone *mic, float threshold);

/**
 * 麦克风是否正在捕获。
 */
bool bongo_cat_microphone_active(const BongoCatMicrophone *mic);

#endif
