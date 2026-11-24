#pragma once

#include "../gallery_app.hpp"

namespace gallery::pages {

// 可复用控件卡片构建器
ktui::WidgetPtr build_buttons_card(GalleryApp& app);
ktui::WidgetPtr build_toggles_card(GalleryApp& app);
ktui::WidgetPtr build_combobox_card(GalleryApp& app);
ktui::WidgetPtr build_radio_button_card(GalleryApp& app);
ktui::WidgetPtr build_radio_card_group_card(GalleryApp& app);
ktui::WidgetPtr build_slider_progress_card(GalleryApp& app);
ktui::WidgetPtr build_progress_ring_card(GalleryApp& app);
ktui::WidgetPtr build_text_input_card(GalleryApp& app);
ktui::WidgetPtr build_svg_showcase_card(GalleryApp& app);
ktui::WidgetPtr build_menus_and_buttons_card(GalleryApp& app);
ktui::WidgetPtr build_dialog_and_infobar_card(GalleryApp& app);

// 完整页面构建器
ktui::WidgetPtr build_home_page(GalleryApp& app);
ktui::WidgetPtr build_buttons_page(GalleryApp& app);
ktui::WidgetPtr build_inputs_page(GalleryApp& app);
ktui::WidgetPtr build_selection_page(GalleryApp& app);
ktui::WidgetPtr build_progress_page(GalleryApp& app);
ktui::WidgetPtr build_svg_page(GalleryApp& app);
ktui::WidgetPtr build_dialogs_menus_page(GalleryApp& app);
ktui::WidgetPtr build_settings_page(GalleryApp& app);
ktui::WidgetPtr build_about_page(GalleryApp& app);

} // namespace gallery::pages
