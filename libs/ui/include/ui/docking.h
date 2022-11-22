#pragma once
#include "exo/collections/pool.h"
#include "exo/collections/vector.h"
#include "exo/maths/numerics.h"
#include "exo/maths/vectors.h"
#include "exo/option.h"

#include "painter/rect.h"

#include "exo/string.h"
#include <variant>

namespace ui
{
struct Ui;
}
namespace docking
{
struct Area;

struct AreaContainer
{
	Vec<usize> tabviews;
	usize selected = usize_invalid;
	Handle<Area> parent;
};

enum struct Direction
{
	Horizontal,
	Vertical
};

struct AreaSplitter
{
	Handle<Area> left_child;
	Handle<Area> right_child;
	float split;
	Direction direction;
};

struct Area
{
	Rect rect;
	Handle<Area> parent;
	union ContainerOrSplitter
	{
		AreaContainer container;
		AreaSplitter splitter;

		ContainerOrSplitter() {}
		ContainerOrSplitter(AreaContainer &&c) : container{std::move(c)} {}
		ContainerOrSplitter(AreaSplitter &&s) : splitter{std::move(s)} {}
		~ContainerOrSplitter() {}
	} value;
	bool is_container;

	Area(AreaContainer &&c) : rect{}, parent{}, value{std::move(c)}, is_container{true} {}
	Area(AreaSplitter &&s) : rect{}, parent{}, value{std::move(s)}, is_container{false} {}

	Area(Area &&other)
	{
		if (other.is_container) {
			new (&this->value.container) AreaContainer(std::move(other.value.container));
		} else {
			new (&this->value.splitter) AreaSplitter(std::move(other.value.splitter));
		}
		this->rect = other.rect;
		this->parent = other.parent;
		this->is_container = other.is_container;
	}

	Area &operator=(Area &&other)
	{
		if (other.is_container) {
			this->value.container = std::move(other.value.container);
		} else {
			this->value.splitter = std::move(other.value.splitter);
		}
		this->rect = other.rect;
		this->parent = other.parent;
		this->is_container = other.is_container;
		return *this;
	}

	~Area()
	{
		if (this->is_container) {
			this->value.container.~AreaContainer();
		} else {
			this->value.splitter.~AreaSplitter();
		}
	}

	AreaContainer &container()
	{
		ASSERT(this->is_container);
		return this->value.container;
	}

	const AreaContainer &container() const
	{
		ASSERT(this->is_container);
		return this->value.container;
	}

	AreaSplitter &splitter()
	{
		ASSERT(!this->is_container);
		return this->value.splitter;
	}

	const AreaSplitter &splitter() const
	{
		ASSERT(!this->is_container);
		return this->value.splitter;
	}
};

struct TabView
{
	exo::String title;
	Handle<Area> area;
};

namespace events
{
struct DropTab
{
	usize i_tabview;
	Handle<Area> in_container;
};

struct DetachTab
{
	usize i_tabview;
};

struct Split
{
	SplitDirection direction;
	usize i_tabview;
	Handle<Area> container;
};

struct MoveFloating
{
	usize i_floating;
	float2 position;
};
} // namespace events
using DockingEvent = std::variant<events::DropTab, events::DetachTab, events::Split, events::MoveFloating>;

struct DockingUi
{
	float em_size;
	usize active_tab;
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
	Rect rect;
};

struct Docking
{
	exo::Pool<Area> area_pool;
	Handle<Area> root;
	Handle<Area> default_area;
	Vec<TabView> tabviews;
	Vec<FloatingContainer> floating_containers;
	DockingUi ui;
};

Docking create();

Option<Rect> tabview(ui::Ui &ui, Docking &self, exo::StringView tabname);
void begin_docking(Docking &self, ui::Ui &ui, Rect rect);
void end_docking(Docking &self, ui::Ui &ui);

void inspector_ui(Docking &self, ui::Ui &ui, Rect rect);
} // namespace docking
