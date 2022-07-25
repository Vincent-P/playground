#pragma once
#include <painter/font.h>
#include <ui/ui.h>

struct Ui
{
	Painter *painter = nullptr;
	UiTheme  ui_theme;
	UiState  ui_state;
	Font     ui_font;
};
