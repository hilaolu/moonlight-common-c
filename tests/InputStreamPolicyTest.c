#include "InputStreamPolicy.h"

int AppVersionQuad[4];

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    AppVersionQuad[0] = 7;

    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_KEYBOARD,
                                    LE32(KEY_DOWN_EVENT_MAGIC)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_KEYBOARD,
                                    LE32(KEY_UP_EVENT_MAGIC)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(SCROLL_MAGIC_GEN5)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(SS_HSCROLL_MAGIC)));

    CHECK(!inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                     LE32(MOUSE_MOVE_REL_MAGIC_GEN5)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(MOUSE_BUTTON_DOWN_EVENT_MAGIC_GEN5)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(MOUSE_BUTTON_UP_EVENT_MAGIC_GEN5)));
    CHECK(!inputPacketIsFlushBarrier(CTRL_CHANNEL_GENERIC,
                                     LE32(SCROLL_MAGIC_GEN5)));

    AppVersionQuad[0] = 4;
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(SCROLL_MAGIC)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(BUTTON_ACTION_PRESS)));
    CHECK(inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                    LE32(BUTTON_ACTION_RELEASE)));
    CHECK(!inputPacketIsFlushBarrier(CTRL_CHANNEL_MOUSE,
                                     LE32(SCROLL_MAGIC_GEN5)));

    return 0;
}
