#pragma once
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <exo/maths/vectors.h>
#include <exo/option.h>

#include "painter/rect.h"

#include <string>
#include <variant>

namespace ui
{
struct Ui;
}
namespace docking
{

using Area = std::variant<struct AreaContainer, struct AreaSplitter>;

struct AreaContainer
{
	Vec<usize>   tabviews;
	usize        selected = usize_invalid;
	Handle<Area> parent;
	Rect         rect;
};

enum struct Direction
{
	Horizontal,
	Vertical
};

struct AreaSplitter
{
	Direction    direction;
	Handle<Area> left_child;
	Handle<Area> right_child;
	float        split;
	Handle<Area> parent;
	Rect         rect;
};

struct TabView
{
	std::string  title;
	Handle<Area> area;
};

namespace events
{
struct DropTab
{
	usize        i_tabview;
	Handle<Area> in_container;
};

struct DetachTab
{
	usize i_tabview;
};

struct Split
{
	SplitDirection direction;
	usize          i_tabview;
	Handle<Area>   container;
};

struct MoveFloating
{
	usize  i_floating;
	float2 position;
};
} // namespace events
using DockingEvent = std::variant<events::DropTab, events::DetachTab, events::Split, events::MoveFloating>;

struct DockingUi
{
	float             em_size;
	usize             active_tab;
	Vec<DockingEvent> events;
};

enum struct TabState
{
	Dragging,
	ClickedTitle,
	ClickedDetach,
	None,
};

struct FloatingContainer
{
	Handle<Area> area;
	Rect         rect;
	float2       drag_offset; // offset from the top_left corner when dragging
};

struct Docking
{
	exo::Pool<Area>        area_pool;
	Handle<Area>           root;
	Handle<Area>           default_area;
	Vec<TabView>           tabviews;
	Vec<FloatingContainer> floating_containers;
	DockingUi              ui;
};

Docking create();

Option<Rect> tabview(ui::Ui &ui, Docking &self, std::string_view tabname);
void         begin_docking(Docking &self, ui::Ui &ui, Rect rect);
void         end_docking(Docking &self, ui::Ui &ui);

void inspector_ui(Docking &self, ui::Ui &ui, Rect rect);
} // namespace docking
