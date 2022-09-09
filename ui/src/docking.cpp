#include "ui/docking.h"

#include "painter/painter.h"
#include "ui/ui.h"

#include <exo/buttons.h>
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <painter/rect.h>

namespace docking
{
Docking create()
{
	Docking docking;
	docking.root         = docking.area_pool.add(AreaContainer{});
	docking.default_area = docking.root;
	return docking;
}

// Tab and area management
static void         remove_tabview(Docking &docking, usize i_tabview);
static void         insert_tabview(Docking &docking, usize i_tabview, Handle<Area> area_handle);
static void         remove_empty_areas(Docking &docking, Handle<Area> area);
static Handle<Area> split_area(
	Docking &docking, Handle<Area> previous_area, SplitDirection direction, Handle<Area> new_child_handle);

// Drawing and update logic
static void update_area_rect(Docking &docking, Handle<Area> area, Rect rect);
static void draw_area_rec(Docking &self, ui::Ui &ui, Handle<Area> area_handle);
static void draw_floating_area(Docking &self, ui::Ui &ui, usize i_floating);
static void draw_docking(Docking &self, ui::Ui &ui);
static void draw_area_overlay(Docking &self, ui::Ui &ui, Handle<Area> area_handle);

// std::variant helper
static AreaContainer       &area_container(Area &area) { return std::get<0>(area); }
static const AreaContainer &area_container(const Area &area) { return std::get<0>(area); }
static AreaSplitter        &area_splitter(Area &area) { return std::get<1>(area); }
static Handle<Area>        *area_parent(Area &area)
{
	if (auto *splitter = std::get_if<AreaSplitter>(&area)) {
		return &splitter->parent;
	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		return &container->parent;
	} else {
		ASSERT(false);
		return nullptr;
	}
}
static void area_replace_child(Area &area, Handle<Area> previous_child, Handle<Area> new_child)
{
	auto &splitter = area_splitter(area);
	if (splitter.left_child == previous_child) {
		splitter.left_child = new_child;
	} else if (splitter.right_child == previous_child) {
		splitter.right_child = new_child;
	}
}

Option<Rect> tabview(Docking &self, std::string_view tabname)
{
	u32 i_tabview = u32_invalid;
	for (u32 i = 0; i < self.tabviews.size(); ++i) {
		if (self.tabviews[i].title == tabname) {
			i_tabview = i;
			break;
		}
	}
	if (i_tabview == u32_invalid) {
		self.tabviews.push_back(TabView{.title = std::string{tabname}, .area = self.default_area});
		i_tabview = u32(self.tabviews.size() - 1);
		insert_tabview(self, i_tabview, self.default_area);
	}

	const auto &tabview = self.tabviews[i_tabview];
	const auto &area    = self.area_pool.get(tabview.area);

	const auto &container = area_container(area);
	if (container.selected != u32_invalid && container.tabviews[container.selected] == i_tabview) {
		auto content_rect  = container.rect;
		auto _tabwell_rect = rect_split_top(content_rect, 1.5f * self.ui.em_size);
		return Some(content_rect);
	} else {
		return None;
	}
}

void begin_docking(Docking &self, ui::Ui &ui, Rect rect)
{
	self.ui.em_size    = ui.theme.font_size;
	self.ui.active_tab = usize_invalid;

	const auto em = self.ui.em_size;

	update_area_rect(self, self.root, rect);
	for (const auto &floating : self.floating_containers) {
		auto copy          = floating.rect;
		auto titlebar_rect = rect_split_top(copy, 1.5f * em);
		update_area_rect(self, floating.area, copy);
	}
}

void end_docking(Docking &self, ui::Ui &ui)
{
	draw_area_rec(self, ui, self.root);
	for (usize i = 0; i < self.floating_containers.size(); ++i) {
		draw_floating_area(self, ui, i);
	}

	for (const auto &element : self.ui.events) {
		if (auto *droptab = std::get_if<events::DropTab>(&element)) {
			auto previous_area = self.tabviews[droptab->i_tabview].area;
			if (droptab->in_container != previous_area) {
				remove_tabview(self, droptab->i_tabview);
				insert_tabview(self, droptab->i_tabview, droptab->in_container);
				remove_empty_areas(self, previous_area);
			}
		} else if (auto *split = std::get_if<events::Split>(&element)) {
			auto previous_tab_container = self.tabviews[split->i_tabview].area;
			remove_tabview(self, split->i_tabview);
			auto new_container = self.area_pool.add(AreaContainer{.selected = 0});
			insert_tabview(self, split->i_tabview, new_container);
			auto previous_splitted_container = split_area(self, split->container, split->direction, new_container);
			remove_empty_areas(self, previous_tab_container);
			remove_empty_areas(self, previous_splitted_container);

		} else if (auto *detachtab = std::get_if<events::DetachTab>(&element)) {
			auto previous_area = self.tabviews[detachtab->i_tabview].area;
			remove_tabview(self, detachtab->i_tabview);
			auto new_rect      = Rect{.pos = float2(200.0f), .size = float2(500.0f)};
			auto new_container = self.area_pool.add(AreaContainer{.tabviews = {detachtab->i_tabview}, .selected = 0});
			self.tabviews[detachtab->i_tabview].area = new_container;
			self.floating_containers.push_back(FloatingContainer{.area = new_container, .rect = new_rect});
			remove_empty_areas(self, previous_area);
		} else if (auto *movefloating = std::get_if<events::MoveFloating>(&element)) {
			self.floating_containers[movefloating->i_floating].rect.pos = movefloating->position;
		}
	}
	self.ui.events.clear();

	for (auto &floating_container : self.floating_containers) {
		auto &area = self.area_pool.get(floating_container.area);
		if (auto *container = std::get_if<AreaContainer>(&area)) {
			if (container->tabviews.empty()) {
				// TODO: remove the floating container
			}
		}
	}
}

// -- Details

static void remove_tabview(Docking &docking, usize i_tabview)
{
	auto &tabview = docking.tabviews[i_tabview];
	auto &area    = docking.area_pool.get(tabview.area);

	tabview.area = Handle<Area>::invalid();

	auto &container = area_container(area);
	ASSERT(!container.tabviews.empty());

	u32 i_to_remove = u32_invalid;
	for (u32 i = 0; i < container.tabviews.size(); ++i) {
		if (container.tabviews[i] == i_tabview) {
			i_to_remove = i;
			break;
		}
	}
	ASSERT(i_to_remove != u32_invalid);
	exo::swap_remove(container.tabviews, i_to_remove);
}

static void insert_tabview(Docking &docking, usize i_tabview, Handle<Area> area_handle)
{
	auto &area    = docking.area_pool.get(area_handle);
	auto &tabview = docking.tabviews[i_tabview];

	auto &container = area_container(area);
	container.tabviews.push_back(i_tabview);
	tabview.area = area_handle;
}

// Replace the area #previous_area_handle with a splitter containing two childs: the previous area and a new child
// specified by #new_child_handle. Returns the handle of the area replaced by the splitter.
static Handle<Area> split_area(
	Docking &docking, Handle<Area> previous_area_handle, SplitDirection direction, Handle<Area> new_child_handle)
{
	// Copy the previous area
	auto previous_area       = docking.area_pool.get(previous_area_handle);
	auto previous_parent     = *area_parent(previous_area);
	auto new_old_area_handle = docking.area_pool.add(std::move(previous_area));

	// Update all tabviews to use the new handle
	if (auto *container = std::get_if<AreaContainer>(&docking.area_pool.get(new_old_area_handle))) {
		for (auto &tabview : docking.tabviews) {
			if (tabview.area == previous_area_handle) {
				tabview.area = new_old_area_handle;
			}
		}
	}

	auto left_child  = new_child_handle;
	auto right_child = new_old_area_handle;
	if (direction == SplitDirection::Bottom || direction == SplitDirection::Right) {
		left_child  = new_old_area_handle;
		right_child = new_child_handle;
	}

	// Replace the old area slot with a new splitter
	docking.area_pool.get(previous_area_handle) = AreaSplitter{
		.direction   = split_is_horizontal(direction) ? Direction::Horizontal : Direction::Vertical,
		.left_child  = left_child,
		.right_child = right_child,
		.split       = 0.5,
		.parent      = previous_parent,
		.rect =
			Rect{
				.pos  = float2(0.0f),
				.size = float2(0.0f),
			},
	};

	*area_parent(docking.area_pool.get(new_child_handle))    = previous_area_handle;
	*area_parent(docking.area_pool.get(new_old_area_handle)) = previous_area_handle;

	return new_old_area_handle;
}

// Remove all redondant splitter with less than 2 children and containers without tabs, bubbling up from area_handle to
// the root.
static void remove_empty_areas(Docking &docking, Handle<Area> area_handle)
{
	if (!area_handle.is_valid()) {
		return;
	}
	auto &area          = docking.area_pool.get(area_handle);
	auto  parent_handle = *area_parent(area);

	if (auto *splitter = std::get_if<AreaSplitter>(&area)) {
		auto lchild = splitter->left_child;
		auto rchild = splitter->right_child;

		// There is an empty split
		if ((lchild.is_valid() && !rchild.is_valid()) || (!lchild.is_valid() && rchild.is_valid())) {
			auto child_handle = lchild.is_valid() ? lchild : rchild;

			// Reparent the child to the parent of the current node
			*area_parent(docking.area_pool.get(child_handle)) = parent_handle;

			// Update the parent to have the child as child instead of the current node
			if (parent_handle.is_valid()) {
				area_replace_child(docking.area_pool.get(parent_handle), area_handle, child_handle);
			} else {
				// We dont have a parent? We are probably the root.
				auto child            = docking.area_pool.get(child_handle);
				auto child_new_handle = area_handle;

				// Reparent the child's children to ourselves
				if (auto *splitter = std::get_if<AreaSplitter>(&area)) {
					auto lchild = splitter->left_child;
					auto rchild = splitter->right_child;

					*area_parent(docking.area_pool.get(lchild)) = child_new_handle;
					*area_parent(docking.area_pool.get(rchild)) = child_new_handle;
				} else if (auto *container = std::get_if<AreaContainer>(&area)) {
					for (auto i_tabview : container->tabviews) {
						docking.tabviews[i_tabview].area = child_new_handle;
					}
				}

				// Replace ourselves with the only child
				docking.area_pool.get(child_new_handle) = child;
				// Finally remove the child that we moved
				docking.area_pool.remove(child_handle);
			}
		}
	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		if (container->tabviews.empty() && parent_handle.is_valid()) {
			// Update the parent to have the child as child instead of the current node
			area_replace_child(docking.area_pool.get(parent_handle), area_handle, Handle<Area>::invalid());

			// Remove ourselves
			docking.area_pool.remove(area_handle);
		}
	}

	remove_empty_areas(docking, parent_handle);
}

// Propagate a rect down an area, and will update the selected tabs of the traversed containers.
static void update_area_rect(Docking &docking, Handle<Area> area_handle, Rect rect)
{
	if (!area_handle.is_valid()) {
		return;
	}

	auto &area = docking.area_pool.get(area_handle);
	if (auto *splitter = std::get_if<AreaSplitter>(&area)) {
		splitter->rect = rect;

		Rect left_child_rect  = rect;
		Rect right_child_rect = rect;
		if (splitter->direction == Direction::Horizontal) {
			right_child_rect = rect_split_bottom(left_child_rect, splitter->split * rect.size.y);
		} else if (splitter->direction == Direction::Vertical) {
			right_child_rect = rect_split_right(left_child_rect, splitter->split * rect.size.x);
		}

		update_area_rect(docking, splitter->left_child, left_child_rect);
		update_area_rect(docking, splitter->right_child, right_child_rect);

	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		container->rect = rect;
		if (container->selected == u32_invalid) {
			// Select first tab if none is selected
			if (!container->tabviews.empty()) {
				container->selected = 0;
			}
		} else {
			if (container->tabviews.empty()) {
				// Remove selection if there is no tabs
				container->selected = u32_invalid;
			} else if (container->selected >= container->tabviews.size()) {
				// Select the first one if selection is invalid
				container->selected = 0;
			}
		}
	}
}

static TabState draw_tab(DockingUi &docking, ui::Ui &ui, const TabView &tabview, Rect &rect)
{
	auto res = TabState::None;
	auto em  = docking.em_size;
	auto id  = ui::make_id(ui);

	auto label_size  = float2(50.0f, 1.0f * em); // TODO: compute size
	auto title_rect  = rect_split_left(rect, label_size.x + 1.0f * em);
	auto detach_rect = rect_split_left(rect, 1.5f * em);

	if (ui::is_hovering(ui, title_rect)) {
		ui.activation.focused = id;
		if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
			ui.activation.active = id;
		}
	} else if (ui.activation.active == id) {
		res = TabState::Dragging;
	}

	if (ui::has_clicked(ui, id)) {
		res = TabState::ClickedTitle;
	}

	if (ui::button(ui, {.label = "D", .rect = detach_rect})) {
		res = TabState::ClickedDetach;
	}

	auto color = ColorU32::from_floats(0.53f, 0.53f, 0.73f, 1.0f);
	if (ui.activation.focused == id && ui.activation.active == id) {
		color = ColorU32::from_floats(0.13f, 0.13f, 0.43f, 1.0f);
	} else if (ui.activation.focused == id) {
		color = ColorU32::from_floats(0.13f, 0.13f, 0.83f, 1.0f);
	}
	painter_draw_color_rect(*ui.painter, title_rect, ui.state.i_clip_rect, color);
	ui::label(ui, {.text = tabview.title.c_str(), .rect = rect_center(title_rect, label_size)});

	return res;
}

// Draw the ui for a docking area
static void draw_area_rec(Docking &self, ui::Ui &ui, Handle<Area> area_handle)
{
	if (!area_handle.is_valid()) {
		return;
	}

	auto em   = self.ui.em_size;
	auto &area = self.area_pool.get(area_handle);

	if (auto *splitter = std::get_if<AreaSplitter>(&area)) {
		if (splitter->direction == Direction::Horizontal) {
			ui::splitter_y(ui, splitter->rect, splitter->split);
		} else {
			ui::splitter_x(ui, splitter->rect, splitter->split);
		}
		draw_area_rec(self, ui, splitter->left_child);
		draw_area_rec(self, ui, splitter->right_child);

	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		if (container->tabviews.empty()) {
			return;
		}
		auto content_rect = container->rect;
		auto tabwell_rect = rect_split_top(content_rect, 1.5f * em);

		// Draw the tabwell background
		painter_draw_color_rect(*ui.painter, tabwell_rect, ui.state.i_clip_rect, ColorU32::from_greyscale(u8(0x3A)));

		for (usize i = 0; i < container->tabviews.size(); ++i) {
			auto        i_tabview = container->tabviews[i];
			const auto &tabview   = self.tabviews[i_tabview];

			auto _margin  = rect_split_left(tabwell_rect, 0.5f * em);
			auto tabstate = draw_tab(self.ui, ui, tabview, tabwell_rect);
			switch (tabstate) {
			case TabState::Dragging: {
				self.ui.active_tab = i_tabview;
				break;
			}
			case TabState::ClickedTitle: {
				container->selected = i;
				break;
			}
			case TabState::ClickedDetach: {
				self.ui.events.push_back(events::DetachTab{.i_tabview = i_tabview});
				break;
			}
			case TabState::None: {
				break;
			}
			}
		}
	}
}

// Draw the floating window UI and the docking area of a floating area
static void draw_floating_area(Docking &self, ui::Ui &ui, usize i_floating)
{
	const auto &floating_container = self.floating_containers[i_floating];
	auto        em                 = self.ui.em_size;

	auto rect          = floating_container.rect;
	auto titlebar_rect = rect_split_top(rect, 1.5f * em);

	// Draw titlebar
	{
		auto id = ui::make_id(ui);
		if (ui::is_hovering(ui, titlebar_rect)) {
			ui.activation.focused = id;
			if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
				ui.activation.active = id;
			}

			if (ui.activation.active == id) {
				auto mouse_pos = ui.inputs.mouse_position;
				self.ui.events.push_back(events::MoveFloating{.i_floating = i_floating, .position = float2(mouse_pos)});
			}

			painter_draw_color_rect(*ui.painter,
				titlebar_rect,
				ui.state.i_clip_rect,
				ColorU32::from_uints(0xFF, 0xFF, 0));
		}
	}

	draw_area_rec(self, ui, floating_container.area);
}

static void draw_docking(Docking &self, ui::Ui &ui)
{
	auto em = self.ui.em_size;
	if (self.ui.active_tab != usize_invalid) {
		const auto &tabview = self.tabviews[self.ui.active_tab];

		auto rect = Rect{.pos = float2(ui.inputs.mouse_position), .size = float2(10.0f * em, 1.5f * em)};
		painter_draw_color_rect(*ui.painter, rect, ui.state.i_clip_rect, ColorU32::from_floats(0.0f, 0.0f, 0.0f, 0.5f));

		auto label_size = float2(50.0f, 1.0f * em); // TODO: compute size
		ui::label(ui, {.text = tabview.title.c_str(), .rect = rect_center(rect, label_size)});
	}
}

static void draw_area_overlay(Docking &self, ui::Ui &ui, Handle<Area> area_handle) {}
} // namespace docking
