#pragma once

#include "Limelight-internal.h"

static inline bool orderedInputIsEnabled(const STREAM_CONFIGURATION* streamConfig,
                                         uint32_t hostFeatureFlags)
{
    return streamConfig->enableOrderedInput &&
           (hostFeatureFlags & LI_FF_ORDERED_INPUT) != 0;
}

static inline uint32_t orderedInputClientFeatureFlags(uint32_t featureFlags,
                                                      const STREAM_CONFIGURATION* streamConfig,
                                                      uint32_t hostFeatureFlags)
{
    return orderedInputIsEnabled(streamConfig, hostFeatureFlags) ?
        featureFlags | ML_FF_ORDERED_INPUT : featureFlags;
}

static inline uint8_t orderedInputChannel(uint8_t channelId,
                                          const STREAM_CONFIGURATION* streamConfig,
                                          uint32_t hostFeatureFlags)
{
    return channelId == CTRL_CHANNEL_KEYBOARD &&
           orderedInputIsEnabled(streamConfig, hostFeatureFlags) ?
        CTRL_CHANNEL_MOUSE : channelId;
}
