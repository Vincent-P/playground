#include "cross/jobs/waitable.h"

#include <exo/profile.h>

#include <windows.h>

namespace cross
{
void Waitable::wait()
{
	EXO_PROFILE_SCOPE

	int comperand = int(this->jobs.size());
	int done      = comperand + 1;
	while (true) {
		auto res = InterlockedCompareExchange64(&this->jobs_finished, done, comperand);
		if (res == done) {
			break;
		}
	}
}
} // namespace cross
