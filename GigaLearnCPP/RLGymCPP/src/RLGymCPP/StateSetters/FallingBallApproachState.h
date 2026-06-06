#pragma once
#include "StateSetter.h"

namespace RLGC {
	class FallingBallApproachState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}
