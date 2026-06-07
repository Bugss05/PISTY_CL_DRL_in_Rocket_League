#pragma once
#include "StateSetter.h"

namespace RLGC {
	class StaticAerialState : public StateSetter {
	public:
		virtual void ResetArena(Arena* arena) override;
	};
}
