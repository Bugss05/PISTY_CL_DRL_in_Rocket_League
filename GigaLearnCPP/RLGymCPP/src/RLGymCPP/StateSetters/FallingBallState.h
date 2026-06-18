#pragma once
#include "StateSetter.h"

namespace RLGC {
    class FallingBallState : public StateSetter {
    public:
        void ResetArena(Arena* arena) override;
    };
}