#pragma once

#include "Limelight-internal.h"

static inline bool inputPacketIsFlushBarrier(uint8_t channelId, uint32_t packetMagic)
{
    if (channelId == CTRL_CHANNEL_KEYBOARD) {
        return true;
    }

    if (channelId != CTRL_CHANNEL_MOUSE) {
        return false;
    }

    const uint32_t scrollMagic = AppVersionQuad[0] >= 5 ?
        LE32(SCROLL_MAGIC_GEN5) : LE32(SCROLL_MAGIC);

    return packetMagic == scrollMagic ||
           packetMagic == LE32(SS_HSCROLL_MAGIC);
}
