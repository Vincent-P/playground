#include "common.h"
#include "platform.h"

#include "exo/logger.h"
#include "rhi/context.h"
#include <cstdlib>

// -- Game

struct RenderState
{
	rhi::Context context;
};

struct GameState
{
	int counter;
	RenderState render;
};

void init_renderstate(Platform *platform, RenderState *render_state)
{
	render_state->context = rhi::Context::create(platform, {.enable_validation = true});
}

void shutdown_renderstate(Platform *platform, RenderState *render_state) { render_state->context.destroy(platform); }

extern "C" __declspec(dllexport) void init(Platform *platform)
{
	auto *state = static_cast<GameState *>(calloc(1, sizeof(GameState)));
	init_renderstate(platform, &state->render);
	platform->game_state = state;
}

extern "C" __declspec(dllexport) void reload(Platform *platform)
{
	// do nothing
	(void)(platform);
	auto b = make_tester(98);
	b.bark();
}

extern "C" __declspec(dllexport) void update(Platform *platform)
{
	auto *state = platform->game_state;
	state->counter += 1;
	state->counter += 3;
	state->counter -= 3;
	state->counter -= 2;
	state->counter += 2;

	char buffer[64] = {};
	snprintf(buffer, 64, "Value: %d\n", state->counter);
	platform->debug_print(buffer);
}

extern "C" __declspec(dllexport) void shutdown(Platform *platform)
{
	auto *state = platform->game_state;
	shutdown_renderstate(platform, &state->render);
	free(state);
}
