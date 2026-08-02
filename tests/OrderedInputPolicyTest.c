#include "OrderedInputPolicy.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void) {
    STREAM_CONFIGURATION config = {0};
    const uint32_t baseFeatureFlags = ML_FF_FEC_STATUS | ML_FF_SESSION_ID_V1;

    CHECK(LI_FF_ORDERED_INPUT == 0x04);
    CHECK(ML_FF_ORDERED_INPUT == 0x04);

    CHECK(!orderedInputIsEnabled(&config, LI_FF_ORDERED_INPUT));
    CHECK((orderedInputClientFeatureFlags(baseFeatureFlags,
                                          &config,
                                          LI_FF_ORDERED_INPUT) &
           ML_FF_ORDERED_INPUT) == 0);
    CHECK(orderedInputChannel(CTRL_CHANNEL_KEYBOARD,
                              &config,
                              LI_FF_ORDERED_INPUT) == CTRL_CHANNEL_KEYBOARD);

    config.enableOrderedInput = true;
    CHECK(!orderedInputIsEnabled(&config, 0));
    CHECK((orderedInputClientFeatureFlags(baseFeatureFlags, &config, 0) &
           ML_FF_ORDERED_INPUT) == 0);
    CHECK(orderedInputChannel(CTRL_CHANNEL_KEYBOARD,
                              &config,
                              0) == CTRL_CHANNEL_KEYBOARD);

    CHECK(orderedInputIsEnabled(&config, LI_FF_ORDERED_INPUT));
    CHECK(orderedInputClientFeatureFlags(baseFeatureFlags,
                                         &config,
                                         LI_FF_ORDERED_INPUT) ==
          (baseFeatureFlags | ML_FF_ORDERED_INPUT));
    CHECK(orderedInputChannel(CTRL_CHANNEL_KEYBOARD,
                              &config,
                              LI_FF_ORDERED_INPUT) == CTRL_CHANNEL_MOUSE);
    CHECK(orderedInputChannel(CTRL_CHANNEL_MOUSE,
                              &config,
                              LI_FF_ORDERED_INPUT) == CTRL_CHANNEL_MOUSE);

    return 0;
}
