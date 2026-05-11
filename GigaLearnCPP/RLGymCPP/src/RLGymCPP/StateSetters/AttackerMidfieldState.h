#pragma once
#include "StateSetter.h"

namespace RLGC {
	class AttackerMidfieldState : public StateSetter {
	public:
		AttackerMidfieldState() {}

		virtual void ResetArena(Arena* arena) override;
	};
}
