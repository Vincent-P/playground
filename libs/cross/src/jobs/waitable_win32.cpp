#include "cross/jobs/waitable.h"

#include "exo/profile.h"

#include <windows.h>

namespace cross
{
void Waitable::wait()
{
	EXO_PROFILE_SCOPE

	const int comperand = int(this->jobs.len());
	const int done      = comperand + 1;
	while (true) {
		auto res = InterlockedCompareExchange64(&this->jobs_finished, done, comperand);
		if (res == done) {
			break;
		}
	}
}

bool Waitable::is_done()
{
	EXO_PROFILE_SCOPE
	const int comperand = int(this->jobs.len());
	const int done      = comperand + 1;
	auto      res       = InterlockedCompareExchange64(&this->jobs_finished, done, comperand);
	return res == done;
}
} // namespace cross
