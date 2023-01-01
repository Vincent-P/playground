#include <cstdlib>

struct GameState
{
	const char *awwhowhwowho;
	char buf[1024];
	int counter;
};

extern "C" __declspec(dllexport) GameState *init()
{
	auto *state = static_cast<GameState *>(calloc(1, sizeof(GameState)));
	return state;
}

extern "C" __declspec(dllexport) int update(GameState *state)
{
	state->counter += 1;
	state->counter += 3;
	state->counter -= 3;
	state->counter -= 2;
	state->counter += 2;
	return state->counter;
}

extern "C" __declspec(dllexport) void shutdown(GameState *state)
{
	// free
	free(state);
}
