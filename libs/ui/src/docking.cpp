#include "ui/docking.h"

#include "painter/painter.h"
#include "ui/ui.h"

#include "cross/buttons.h"
#include "exo/collections/vector.h"
#include "exo/format.h"
#include "exo/maths/numerics.h"
#include "exo/memory/scope_stack.h"
#include "painter/rect.h"

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

Option<Rect> tabview(ui::Ui &ui, Docking &self, exo::StringView tabname)
{
	u32 i_tabview = u32_invalid;
	for (u32 i = 0; i < self.tabviews.len(); ++i) {
		if (self.tabviews[i].title == tabname) {
			i_tabview = i;
			break;
		}
	}
	if (i_tabview == u32_invalid) {
		self.tabviews.push(TabView{.title = exo::String{tabname}, .area = self.default_area});
		i_tabview = u32(self.tabviews.len() - 1);
		insert_tabview(self, i_tabview, self.default_area);
	}

	const auto &tabview = self.tabviews[i_tabview];
	const auto &area    = self.area_pool.get(tabview.area);

	const auto &container = area_container(area);
	if (container.selected != u32_invalid && container.tabviews[container.selected] == i_tabview) {
		auto content_rect = container.rect;
		/*auto tabwell_rect =*/rect_split_top(content_rect, 2.0f * self.ui.em_size);
		painter_draw_color_rect(*ui.painter, content_rect, ui.state.i_clip_rect, ColorU32::from_greyscale(u8(0x1A)));
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
		auto copy = floating.rect;
		/*auto titlebar_rect =*/rect_split_top(copy, 0.25f * em);
		update_area_rect(self, floating.area, copy);
	}
}

void end_docking(Docking &self, ui::Ui &ui)
{
	draw_area_rec(self, ui, self.root);
	for (usize i = 0; i < self.floating_containers.len(); ++i) {
		draw_floating_area(self, ui, i);
	}

	draw_docking(self, ui);

	for (auto [area_handle, area] : self.area_pool) {
		draw_area_overlay(self, ui, area_handle);
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
			auto       new_rect = Rect{.pos = float2(200.0f), .size = float2(500.0f)};
			Vec<usize> tabviews;
			tabviews.push(detachtab->i_tabview);
			auto new_container = self.area_pool.add(AreaContainer{.tabviews = std::move(tabviews), .selected = 0});
			self.tabviews[detachtab->i_tabview].area = new_container;
			self.floating_containers.push(FloatingContainer{.area = new_container, .rect = new_rect});
			remove_empty_areas(self, previous_area);
		} else if (auto *movefloating = std::get_if<events::MoveFloating>(&element)) {
			auto &floating_container    = self.floating_containers[movefloating->i_floating];
			floating_container.rect.pos = movefloating->position - ui.state.active_drag_offset;
		}
	}
	self.ui.events.clear();

	for (u32 i_container = 0; i_container < self.floating_containers.len();) {
		const auto &floating_container = self.floating_containers[i_container];
		auto       &area               = self.area_pool.get(floating_container.area);
		if (auto *container = std::get_if<AreaContainer>(&area)) {
			if (container->tabviews.is_empty()) {
				self.area_pool.remove(floating_container.area);
				self.floating_containers.swap_remove(i_container);
				continue;
			}
		}
		i_container += 1;
	}
}

void inspector_ui(Docking &self, ui::Ui &ui, Rect rect)
{
	auto content_rect      = rect;
	auto content_rectsplit = RectSplit{content_rect, SplitDirection::Top};

	exo::ScopeStack scope;
	auto            mouse_pos = ui::mouse_position(ui);
	ui::label_split(ui, content_rectsplit, exo::formatf(scope, "mouse pos (%d, %d))", mouse_pos.x, mouse_pos.y));

	for (const auto &floating_container : self.floating_containers) {
		ui::label_split(ui, content_rectsplit, "Floating window:");
		ui::label_split(ui,
			content_rectsplit,
			exo::formatf(scope,
				"  - pos: (%f, %f), size (%f, %f))",
				floating_container.rect.pos.x,
				floating_container.rect.pos.y,
				floating_container.rect.size.x,
				floating_container.rect.size.y));
	}
}

// -- Details

static void remove_tabview(Docking &docking, usize i_tabview)
{
	auto &tabview = docking.tabviews[i_tabview];
	auto &area    = docking.area_pool.get(tabview.area);

	tabview.area = Handle<Area>::invalid();

	auto &container = area_container(area);
	ASSERT(!container.tabviews.is_empty());

	u32 i_to_remove = u32_invalid;
	for (u32 i = 0; i < container.tabviews.len(); ++i) {
		if (container.tabviews[i] == i_tabview) {
			i_to_remove = i;
			break;
		}
	}
	ASSERT(i_to_remove != u32_invalid);
	container.tabviews.swap_remove(i_to_remove);
}

static void insert_tabview(Docking &docking, usize i_tabview, Handle<Area> area_handle)
{
	auto &area    = docking.area_pool.get(area_handle);
	auto &tabview = docking.tabviews[i_tabview];

	auto &container = area_container(area);
	container.tabviews.push(i_tabview);
	tabview.area = area_handle;
}

// Replace the area #previous_area_handle with a splitter containing two childs: the previous area and a new child
// specified by #new_child_handle. Returns the handle of the area replaced by the splitter.
static Handle<Area> split_area(
	Docking &docking, Handle<Area> previous_area_handle, SplitDirection direction, Handle<Area> new_child_handle)
{
	// Copy the previous area
	auto previous_area       = std::move(docking.area_pool.get(previous_area_handle));
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

	// Splitting from the top or bottom results in a horizontal split
	auto split_is_horizontal = [](SplitDirection direction) {
		switch (direction) {
		case SplitDirection::Top:
		case SplitDirection::Bottom:
			return true;
		case SplitDirection::Left:
		case SplitDirection::Right:
		default:
			return false;
		}
	};

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
				auto child            = std::move(docking.area_pool.get(child_handle));
				auto child_new_handle = area_handle;

				// Reparent the child's children to ourselves
				if (auto *child_splitter = std::get_if<AreaSplitter>(&child)) {
					auto child_lchild = child_splitter->left_child;
					auto child_rchild = child_splitter->right_child;

					*area_parent(docking.area_pool.get(child_lchild)) = child_new_handle;
					*area_parent(docking.area_pool.get(child_rchild)) = child_new_handle;
				} else if (auto *child_container = std::get_if<AreaContainer>(&child)) {
					for (auto i_tabview : child_container->tabviews) {
						docking.tabviews[i_tabview].area = child_new_handle;
					}
				}

				// Replace ourselves with the only child
				docking.area_pool.get(child_new_handle) = std::move(child);
				// Finally remove the child that we moved
				docking.area_pool.remove(child_handle);
			}
		}
	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		if (container->tabviews.is_empty() && parent_handle.is_valid()) {
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
			right_child_rect = rect_split_top(left_child_rect, splitter->split * rect.size.y);
		} else if (splitter->direction == Direction::Vertical) {
			right_child_rect = rect_split_left(left_child_rect, splitter->split * rect.size.x);
		}

		update_area_rect(docking, splitter->left_child, left_child_rect);
		update_area_rect(docking, splitter->right_child, right_child_rect);

	} else if (auto *container = std::get_if<AreaContainer>(&area)) {
		container->rect = rect;
		if (container->selected == u32_invalid) {
			// Select first tab if none is selected
			if (!container->tabviews.is_empty()) {
				container->selected = 0;
			}
		} else {
			if (container->tabviews.is_empty()) {
				// Remove selection if there is no tabs
				container->selected = u32_invalid;
			} else if (container->selected >= container->tabviews.len()) {
				// Select the first one if selection is invalid
				container->selected = 0;
			}
		}
	}
}

static TabState draw_tab(DockingUi &docking, ui::Ui &ui, const TabView &tabview, Rect &rect, bool is_active)
{
	// Layout
	auto em          = docking.em_size;
	auto label_width = float(measure_label(*ui.painter, *ui.theme.main_font, tabview.title).x);

	auto rectsplit  = RectSplit{rect, SplitDirection::Left};
	auto title_rect = rectsplit.split(label_width + 1.0f * em);

	auto copy               = title_rect;
	auto bottom_border_rect = rect_split_bottom(copy, 0.1f * em);

	// Interaction
	auto res = TabState::None;
	auto id  = ui::make_id(ui);

	const bool is_hovering = ui::is_hovering(ui, title_rect);
	if (is_hovering) {
		ui.activation.focused = id;

		const bool has_pressed =
			ui::has_pressed(ui, exo::MouseButton::Left) || ui::has_pressed(ui, exo::MouseButton::Right);
		if (ui.activation.active == u64_invalid && has_pressed) {
			ui.activation.active = id;
		}
	} else if (ui.activation.active == id) {
		res = TabState::Dragging;
	}

	if (is_hovering && ui::has_clicked(ui, id)) {
		res = TabState::ClickedTitle;
	}

	if (is_hovering && ui::has_clicked(ui, id, exo::MouseButton::Right)) {
		res = TabState::ClickedDetach;
	}

	// Drawing
	auto color = ColorU32::from_greyscale(u8(0x33));
	if (ui.activation.focused == id && ui.activation.active == id) {
		color = ColorU32::from_greyscale(u8(0x38));
	} else if (ui.activation.focused == id) {
		color = ColorU32::from_greyscale(u8(0x42));
	}
	painter_draw_color_rect(*ui.painter, title_rect, ui.state.i_clip_rect, color);
	ui::label_in_rect(ui, title_rect, tabview.title);

	if (is_active) {
		painter_draw_color_rect(*ui.painter, bottom_border_rect, u32_invalid, ui.theme.accent_color);
	}

	/* auto margin =*/rectsplit.split(0.1f * em);
	return res;
}

// Draw the ui for a docking area
static void draw_area_rec(Docking &self, ui::Ui &ui, Handle<Area> area_handle)
{
	if (!area_handle.is_valid()) {
		return;
	}

	auto  em   = self.ui.em_size;
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
		if (container->tabviews.is_empty()) {
			return;
		}
		auto content_rect = container->rect;
		auto tabwell_rect = rect_split_top(content_rect, 2.0f * em);

		// Draw the tabwell background
		painter_draw_color_rect(*ui.painter, tabwell_rect, ui.state.i_clip_rect, ColorU32::from_greyscale(u8(0x28)));

		for (usize i = 0; i < container->tabviews.len(); ++i) {
			auto        i_tabview = container->tabviews[i];
			const auto &tabview   = self.tabviews[i_tabview];

			// /*auto margin  =*/rect_split_left(tabwell_rect, 0.5f * em);
			auto tabstate = draw_tab(self.ui, ui, tabview, tabwell_rect, i == container->selected);
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
				self.ui.events.push(events::DetachTab{.i_tabview = i_tabview});
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
	auto &floating_container = self.floating_containers[i_floating];
	auto  em                 = self.ui.em_size;

	auto rect          = floating_container.rect;
	auto titlebar_rect = rect_split_top(rect, 0.25f * em);

	// Draw titlebar to move window
	auto mouse_pos = ui::mouse_position(ui);
	{
		auto id = ui::make_id(ui);
		if (ui::is_hovering(ui, titlebar_rect)) {
			ui.activation.focused = id;
			if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
				ui.activation.active        = id;
				ui.state.active_drag_offset = ui::mouse_position(ui) - floating_container.rect.pos;
			}
		}

		if (ui.activation.active == id) {
			self.ui.events.push(events::MoveFloating{.i_floating = i_floating, .position = float2(mouse_pos)});
		}

		painter_draw_color_rect(*ui.painter, titlebar_rect, ui.state.i_clip_rect, ColorU32::from_uints(0xFF, 0xFF, 0));
	}

	draw_area_rec(self, ui, floating_container.area);

	// Draw the resize handle
	auto bottom_rect = rect_split_bottom(rect, 0.5f * em);
	auto handle_rect = rect_split_right(bottom_rect, 0.5f * em);
	{
		auto id = ui::make_id(ui);
		if (ui::is_hovering(ui, handle_rect)) {
			ui.activation.focused = id;
			if (ui.activation.active == u64_invalid && ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
				ui.activation.active        = id;
				ui.state.active_drag_offset = mouse_pos - handle_rect.pos;
			}
		}

		if (ui.activation.active == id) {
			floating_container.rect.size =
				mouse_pos - floating_container.rect.pos - ui.state.active_drag_offset + handle_rect.size;
		}

		painter_draw_color_rect(*ui.painter,
			handle_rect,
			ui.state.i_clip_rect,
			ColorU32::from_uints(0xFF, 0, 0xFF, 0xBB));
	}
}

static void draw_docking(Docking &self, ui::Ui &ui)
{
	auto em = self.ui.em_size;

	// Draw a titlebar at the mouse position when dragging a tab
	if (self.ui.active_tab != usize_invalid) {
		const auto &tabview = self.tabviews[self.ui.active_tab];

		auto titlebar_height = 1.5f * em;

		auto label_size = float2(measure_label(*ui.painter, *ui.theme.main_font, tabview.title));
		label_size.x += 1.0f * em;
		label_size.y = titlebar_height;

		auto rect = Rect{.pos = float2(ui.inputs.mouse_position), .size = float2(10.0f * em, 1.5f * em)};
		painter_draw_color_rect(*ui.painter, rect, ui.state.i_clip_rect, ColorU32::from_floats(0.0f, 0.0f, 0.0f, 0.5f));

		ui::label_in_rect(ui, rect, tabview.title);
	}
}

// Draw the "docking overlay" to split tabs
static void draw_area_overlay(Docking &self, ui::Ui &ui, Handle<Area> area_handle)
{
	auto em = self.ui.em_size;
	if (self.ui.active_tab == usize_invalid) {
		return;
	}

	auto &area = self.area_pool.get(area_handle);
	if (auto *container = std::get_if<AreaContainer>(&area)) {
		constexpr float HANDLE_SIZE   = 3.0f;
		constexpr float HANDLE_OFFSET = HANDLE_SIZE + 0.5f;

		auto drop_rect         = rect_center(container->rect, float2(HANDLE_SIZE * em, HANDLE_SIZE * em));
		auto split_top_rect    = rect_offset(drop_rect, float2(0.0f, -HANDLE_OFFSET * em));
		auto split_right_rect  = rect_offset(drop_rect, float2(HANDLE_OFFSET * em, 0.0f));
		auto split_bottom_rect = rect_offset(drop_rect, float2(0.0f, HANDLE_OFFSET * em));
		auto split_left_rect   = rect_offset(drop_rect, float2(-HANDLE_OFFSET * em, 0.0f));

		auto draw_rect = [&](const Rect &rect, DockingEvent event) {
			auto color = ColorU32::from_uints(0x1B, 0x83, 0xF7, u8(0.25f * 255));

			if (ui::is_hovering(ui, rect)) {
				if (!ui.inputs.mouse_buttons_pressed[exo::MouseButton::Left]) {
					self.ui.events.push(event);
				}
				color = ColorU32::from_uints(0x1B, 0x83, 0xF7, u8(0.50f * 255));
			}

			painter_draw_color_rect(*ui.painter, rect, ui.state.i_clip_rect, color);
		};

		draw_rect(drop_rect,
			events::DropTab{
				.i_tabview    = self.ui.active_tab,
				.in_container = area_handle,
			});

		draw_rect(split_top_rect,
			events::Split{
				.direction = SplitDirection::Bottom,
				.i_tabview = self.ui.active_tab,
				.container = area_handle,
			});

		draw_rect(split_right_rect,
			events::Split{
				.direction = SplitDirection::Left,
				.i_tabview = self.ui.active_tab,
				.container = area_handle,
			});

		draw_rect(split_bottom_rect,
			events::Split{
				.direction = SplitDirection::Top,
				.i_tabview = self.ui.active_tab,
				.container = area_handle,
			});

		draw_rect(split_left_rect,
			events::Split{
				.direction = SplitDirection::Right,
				.i_tabview = self.ui.active_tab,
				.container = area_handle,
			});
	}
}
} // namespace docking
