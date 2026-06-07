#pragma once
#include "StateSetter.h"

namespace RLGC {
	class WallDragState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}
