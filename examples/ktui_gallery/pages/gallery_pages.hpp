#pragma once

#include "../gallery_app.hpp"

namespace gallery::pages {

// ==================== 首页与系统页 ====================
ktui::WidgetPtr build_home_page(GalleryApp& app);
ktui::WidgetPtr build_settings_page(GalleryApp& app);
ktui::WidgetPtr build_about_page(GalleryApp& app);

// ==================== 基础输入 (Basic Input) ====================
ktui::WidgetPtr build_button_page(GalleryApp& app);
ktui::WidgetPtr build_toggle_button_page(GalleryApp& app);
ktui::WidgetPtr build_segmented_control_page(GalleryApp& app);
ktui::WidgetPtr build_dropdown_button_page(GalleryApp& app);
ktui::WidgetPtr build_checkbox_page(GalleryApp& app);
ktui::WidgetPtr build_toggle_switch_page(GalleryApp& app);
ktui::WidgetPtr build_radio_button_page(GalleryApp& app);
ktui::WidgetPtr build_radio_card_page(GalleryApp& app);
ktui::WidgetPtr build_combobox_page(GalleryApp& app);
ktui::WidgetPtr build_slider_page(GalleryApp& app);
ktui::WidgetPtr build_calendar_date_picker_page(GalleryApp& app);
ktui::WidgetPtr build_time_picker_page(GalleryApp& app);
ktui::WidgetPtr build_color_picker_page(GalleryApp& app);
ktui::WidgetPtr build_rating_control_page(GalleryApp& app);

// ==================== 文本与数值 (Text & Numbers) ====================
ktui::WidgetPtr build_label_page(GalleryApp& app);
ktui::WidgetPtr build_textbox_page(GalleryApp& app);
ktui::WidgetPtr build_text_area_page(GalleryApp& app);
ktui::WidgetPtr build_auto_suggest_box_page(GalleryApp& app);
ktui::WidgetPtr build_password_box_page(GalleryApp& app);
ktui::WidgetPtr build_number_box_page(GalleryApp& app);

// ==================== 导航与容器 (Navigation & Containers) ====================
ktui::WidgetPtr build_navigation_view_page(GalleryApp& app);
ktui::WidgetPtr build_tab_view_page(GalleryApp& app);
ktui::WidgetPtr build_breadcrumb_bar_page(GalleryApp& app);
ktui::WidgetPtr build_expander_page(GalleryApp& app);
ktui::WidgetPtr build_tree_view_page(GalleryApp& app);
ktui::WidgetPtr build_virtual_list_page(GalleryApp& app);
ktui::WidgetPtr build_data_grid_page(GalleryApp& app);
ktui::WidgetPtr build_flip_view_page(GalleryApp& app);
ktui::WidgetPtr build_pips_pager_page(GalleryApp& app);

// ==================== 菜单与工具栏 (Menus & Toolbars) ====================
ktui::WidgetPtr build_command_bar_page(GalleryApp& app);
ktui::WidgetPtr build_menu_flyout_page(GalleryApp& app);

// ==================== 弹窗与状态 (Dialogs & Status) ====================
ktui::WidgetPtr build_content_dialog_page(GalleryApp& app);
ktui::WidgetPtr build_teaching_tip_page(GalleryApp& app);
ktui::WidgetPtr build_tooltip_page(GalleryApp& app);
ktui::WidgetPtr build_info_bar_page(GalleryApp& app);
ktui::WidgetPtr build_info_badge_page(GalleryApp& app);
ktui::WidgetPtr build_person_picture_page(GalleryApp& app);
ktui::WidgetPtr build_toast_page(GalleryApp& app);
ktui::WidgetPtr build_progress_bar_page(GalleryApp& app);
ktui::WidgetPtr build_progress_ring_page(GalleryApp& app);

// ==================== 图形与矢量 (Media & Vector) ====================
ktui::WidgetPtr build_svg_page(GalleryApp& app);

} // namespace gallery::pages
