#pragma once

// ==============================================================================
// KTUI Widgets Master Header (widgets.h)
// 统一导出所有声明式桌面控件；每个控件位于独立子目录 <name>/ 下，
// 严格遵循三层分离架构：<name>_data.hpp / <name>_style.hpp / <name>_logic.hpp
// ==============================================================================

#include "widget.hpp"

#include "box/box_data.hpp"
#include "box/box_style.hpp"
#include "box/box_logic.hpp"

#include "vstack/vstack_data.hpp"
#include "vstack/vstack_style.hpp"
#include "vstack/vstack_logic.hpp"

#include "hstack/hstack_data.hpp"
#include "hstack/hstack_style.hpp"
#include "hstack/hstack_logic.hpp"

#include "zstack/zstack_data.hpp"
#include "zstack/zstack_style.hpp"
#include "zstack/zstack_logic.hpp"

#include "layout_primitives/layout_primitives_data.hpp"
#include "layout_primitives/layout_primitives_style.hpp"
#include "layout_primitives/layout_primitives_logic.hpp"

#include "wrap/wrap_data.hpp"
#include "wrap/wrap_style.hpp"
#include "wrap/wrap_logic.hpp"

#include "grid/grid_data.hpp"
#include "grid/grid_style.hpp"
#include "grid/grid_logic.hpp"

#include "split_view/split_view_data.hpp"
#include "split_view/split_view_style.hpp"
#include "split_view/split_view_logic.hpp"

#include "dock_panel/dock_panel_data.hpp"
#include "dock_panel/dock_panel_style.hpp"
#include "dock_panel/dock_panel_logic.hpp"

#include "text/text_data.hpp"
#include "text/text_style.hpp"
#include "text/text_logic.hpp"

#include "label/label_data.hpp"
#include "label/label_style.hpp"
#include "label/label_logic.hpp"

#include "button/button_data.hpp"
#include "button/button_style.hpp"
#include "button/button_logic.hpp"

#include "checkbox/checkbox_data.hpp"
#include "checkbox/checkbox_style.hpp"
#include "checkbox/checkbox_logic.hpp"

#include "toggle_switch/toggle_switch_data.hpp"
#include "toggle_switch/toggle_switch_style.hpp"
#include "toggle_switch/toggle_switch_logic.hpp"

#include "radio/radio_data.hpp"
#include "radio/radio_style.hpp"
#include "radio/radio_logic.hpp"

#include "textbox/textbox_data.hpp"
#include "textbox/textbox_style.hpp"
#include "textbox/textbox_logic.hpp"

#include "password_box/password_box_data.hpp"
#include "password_box/password_box_style.hpp"
#include "password_box/password_box_logic.hpp"

#include "number_box/number_box_data.hpp"
#include "number_box/number_box_style.hpp"
#include "number_box/number_box_logic.hpp"

#include "slider/slider_data.hpp"
#include "slider/slider_style.hpp"
#include "slider/slider_logic.hpp"

#include "progress_bar/progress_bar_data.hpp"
#include "progress_bar/progress_bar_style.hpp"
#include "progress_bar/progress_bar_logic.hpp"

#include "progress_ring/progress_ring_data.hpp"
#include "progress_ring/progress_ring_style.hpp"
#include "progress_ring/progress_ring_logic.hpp"

#include "scroll_view/scroll_view_data.hpp"
#include "scroll_view/scroll_view_style.hpp"
#include "scroll_view/scroll_view_logic.hpp"

#include "scroll_linked/scroll_linked_data.hpp"
#include "scroll_linked/scroll_linked_style.hpp"
#include "scroll_linked/scroll_linked_logic.hpp"

#include "rolling_number/rolling_number_data.hpp"
#include "rolling_number/rolling_number_style.hpp"
#include "rolling_number/rolling_number_logic.hpp"

#include "combobox/combobox_data.hpp"
#include "combobox/combobox_style.hpp"
#include "combobox/combobox_logic.hpp"

#include "svg/svg_data.hpp"
#include "svg/svg_style.hpp"
#include "svg/svg_logic.hpp"

#include "image/image_data.hpp"
#include "image/image_style.hpp"
#include "image/image_logic.hpp"

#include "info_bar/info_bar_data.hpp"
#include "info_bar/info_bar_style.hpp"
#include "info_bar/info_bar_logic.hpp"

#include "toast/toast_data.hpp"
#include "toast/toast_style.hpp"
#include "toast/toast_logic.hpp"

#include "tooltip/tooltip_data.hpp"
#include "tooltip/tooltip_style.hpp"
#include "tooltip/tooltip_logic.hpp"

#include "content_dialog/content_dialog_data.hpp"
#include "content_dialog/content_dialog_style.hpp"
#include "content_dialog/content_dialog_logic.hpp"

#include "menu_flyout/menu_flyout_data.hpp"
#include "menu_flyout/menu_flyout_style.hpp"
#include "menu_flyout/menu_flyout_logic.hpp"

#include "menu_bar/menu_bar_data.hpp"
#include "menu_bar/menu_bar_style.hpp"
#include "menu_bar/menu_bar_logic.hpp"

#include "dropdown_button/dropdown_button_data.hpp"
#include "dropdown_button/dropdown_button_style.hpp"
#include "dropdown_button/dropdown_button_logic.hpp"

#include "command_bar/command_bar_data.hpp"
#include "command_bar/command_bar_style.hpp"
#include "command_bar/command_bar_logic.hpp"

#include "expander/expander_data.hpp"
#include "expander/expander_style.hpp"
#include "expander/expander_logic.hpp"

#include "breadcrumb_bar/breadcrumb_bar_data.hpp"
#include "breadcrumb_bar/breadcrumb_bar_style.hpp"
#include "breadcrumb_bar/breadcrumb_bar_logic.hpp"

#include "tab_view/tab_view_data.hpp"
#include "tab_view/tab_view_style.hpp"
#include "tab_view/tab_view_logic.hpp"

#include "auto_suggest_box/auto_suggest_box_data.hpp"
#include "auto_suggest_box/auto_suggest_box_style.hpp"
#include "auto_suggest_box/auto_suggest_box_logic.hpp"

#include "tree_view/tree_view_data.hpp"
#include "tree_view/tree_view_style.hpp"
#include "tree_view/tree_view_logic.hpp"

#include "calendar_date_picker/calendar_date_picker_data.hpp"
#include "calendar_date_picker/calendar_date_picker_style.hpp"
#include "calendar_date_picker/calendar_date_picker_logic.hpp"

#include "color_picker/color_picker_data.hpp"
#include "color_picker/color_picker_style.hpp"
#include "color_picker/color_picker_logic.hpp"

#include "rating_control/rating_control_data.hpp"
#include "rating_control/rating_control_style.hpp"
#include "rating_control/rating_control_logic.hpp"

#include "teaching_tip/teaching_tip_data.hpp"
#include "teaching_tip/teaching_tip_style.hpp"
#include "teaching_tip/teaching_tip_logic.hpp"

#include "virtual_list/virtual_list_data.hpp"
#include "virtual_list/virtual_list_style.hpp"
#include "virtual_list/virtual_list_logic.hpp"

#include "data_grid/data_grid_data.hpp"
#include "data_grid/data_grid_style.hpp"
#include "data_grid/data_grid_logic.hpp"

#include "time_picker/time_picker_data.hpp"
#include "time_picker/time_picker_style.hpp"
#include "time_picker/time_picker_logic.hpp"

#include "info_badge/info_badge_data.hpp"
#include "info_badge/info_badge_style.hpp"
#include "info_badge/info_badge_logic.hpp"

#include "person_picture/person_picture_data.hpp"
#include "person_picture/person_picture_style.hpp"
#include "person_picture/person_picture_logic.hpp"

#include "text_area/text_area_data.hpp"
#include "text_area/text_area_style.hpp"
#include "text_area/text_area_logic.hpp"

#include "pips_pager/pips_pager_data.hpp"
#include "pips_pager/pips_pager_style.hpp"
#include "pips_pager/pips_pager_logic.hpp"

#include "flip_view/flip_view_data.hpp"
#include "flip_view/flip_view_style.hpp"
#include "flip_view/flip_view_logic.hpp"

#include "segmented_control/segmented_control_data.hpp"
#include "segmented_control/segmented_control_style.hpp"
#include "segmented_control/segmented_control_logic.hpp"

#include "pivot/pivot_data.hpp"
#include "pivot/pivot_style.hpp"
#include "pivot/pivot_logic.hpp"

#include "toggle_button/toggle_button_data.hpp"
#include "toggle_button/toggle_button_style.hpp"
#include "toggle_button/toggle_button_logic.hpp"

#include "navigation_view/navigation_view_data.hpp"
#include "navigation_view/navigation_view_style.hpp"
#include "navigation_view/navigation_view_logic.hpp"

#include "query_filter_builder/query_filter_builder_data.hpp"
#include "query_filter_builder/query_filter_builder_style.hpp"
#include "query_filter_builder/query_filter_builder_logic.hpp"

#include "scrub_number_box/scrub_number_box_data.hpp"
#include "scrub_number_box/scrub_number_box_style.hpp"
#include "scrub_number_box/scrub_number_box_logic.hpp"

#include "hold_confirm_button/hold_confirm_button_data.hpp"
#include "hold_confirm_button/hold_confirm_button_style.hpp"
#include "hold_confirm_button/hold_confirm_button_logic.hpp"

#include "ghost_text_box/ghost_text_box_data.hpp"
#include "ghost_text_box/ghost_text_box_style.hpp"
#include "ghost_text_box/ghost_text_box_logic.hpp"

#include "minimap_timeline_slider/minimap_timeline_slider_data.hpp"
#include "minimap_timeline_slider/minimap_timeline_slider_style.hpp"
#include "minimap_timeline_slider/minimap_timeline_slider_logic.hpp"

#include "ip_port_input/ip_port_input_data.hpp"
#include "ip_port_input/ip_port_input_style.hpp"
#include "ip_port_input/ip_port_input_logic.hpp"

#include "spotlight_tour/spotlight_tour_data.hpp"
#include "spotlight_tour/spotlight_tour_style.hpp"
#include "spotlight_tour/spotlight_tour_logic.hpp"

#include "inspector_grid/inspector_grid_data.hpp"
#include "inspector_grid/inspector_grid_style.hpp"
#include "inspector_grid/inspector_grid_logic.hpp"

#include "hex_viewer/hex_viewer_data.hpp"
#include "hex_viewer/hex_viewer_style.hpp"
#include "hex_viewer/hex_viewer_logic.hpp"

#include "syntax_filter_bar/syntax_filter_bar_data.hpp"
#include "syntax_filter_bar/syntax_filter_bar_style.hpp"
#include "syntax_filter_bar/syntax_filter_bar_logic.hpp"

#include "status_bar/status_bar_data.hpp"
#include "status_bar/status_bar_style.hpp"
#include "status_bar/status_bar_logic.hpp"

#include "rich_text_block/rich_text_block_data.hpp"
#include "rich_text_block/rich_text_block_style.hpp"
#include "rich_text_block/rich_text_block_logic.hpp"

#include "terminal/terminal_data.hpp"
#include "terminal/terminal_style.hpp"
#include "terminal/terminal_logic.hpp"

#include "settings_card/settings_card_data.hpp"
#include "settings_card/settings_card_style.hpp"
#include "settings_card/settings_card_logic.hpp"

