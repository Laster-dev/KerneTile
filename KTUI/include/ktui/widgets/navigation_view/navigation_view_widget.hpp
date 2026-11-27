#pragma once

namespace ktui {

// C++20 指定初始化器声明式 WinUI 3 NavigationView 控件
struct NavigationView : BindableMixin<NavigationView, RenderNavigationView> {
    std::vector<NavigationViewItem> menu_items;
    std::vector<NavigationViewItem> footer_items;

    std::string selected_id{""};
    std::string pane_title{""};
    std::string header{""};
    NavigationPaneDisplayMode pane_display_mode{NavigationPaneDisplayMode::Auto};
    bool is_pane_open{true};

    bool is_pane_toggle_button_visible{true};
    bool is_back_button_visible{true};
    bool is_back_enabled{false};
    bool is_settings_visible{true};
    bool is_search_box_visible{false};
    std::string search_placeholder{FrameworkStrings::current().nav_search_placeholder};
    std::string settings_label{FrameworkStrings::current().nav_settings_label};

    float open_pane_length{240.0f};
    float compact_pane_length{48.0f};
    float compact_mode_threshold_width{641.0f};
    float expanded_mode_threshold_width{900.0f};

    WidgetPtr content{nullptr};
    Fn<WidgetPtr(const std::string&)> page_builder{nullptr};
    Fn<void(const std::string&)> on_selection_changed{nullptr};
    Fn<void()> on_back_requested{nullptr};
    Fn<void(bool)> on_pane_toggled{nullptr};
    bool enable_pointer_reveal{false};

    NavigationView& pointer_reveal(bool enabled) & noexcept { enable_pointer_reveal = enabled; return *this; }
    NavigationView&& pointer_reveal(bool enabled) && noexcept { enable_pointer_reveal = enabled; return std::move(*this); }

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderNavigationView>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    if (!prop.get().empty()) {
        node->set_selected_id(prop.get(), false, true);
    }
    std::weak_ptr<RenderNavigationView> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& id) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_id(id, false, true);
        }
    });
    auto prev = node->data().on_selection_changed;
    Property<std::string>* p = &prop;
    node->set_callbacks(
        [prev = std::move(prev), p, guard](const std::string& id) {
            *guard = true;
            p->set(id);
            if (prev) prev(id);
            *guard = false;
        },
        node->data().on_back_requested,
        node->data().on_pane_toggled);
}

inline void ktui_bind_two_way(const std::shared_ptr<RenderNavigationView>& node, Property<bool>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_pane_open(prop.get(), false);
    std::weak_ptr<RenderNavigationView> weak = node;
    bg.subscribe(prop, [weak, guard](bool open) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_pane_open(open, false);
        }
    });
    auto prev = node->data().on_pane_toggled;
    Property<bool>* p = &prop;
    node->set_callbacks(
        node->data().on_selection_changed,
        node->data().on_back_requested,
        [prev = std::move(prev), p, guard](bool open) {
            *guard = true;
            p->set(open);
            if (prev) prev(open);
            *guard = false;
        });
}

class NavigationViewWidget : public Widget {
public:
    explicit NavigationViewWidget(const NavigationView& desc) : m_desc(desc) {}
    explicit NavigationViewWidget(NavigationView&& desc) noexcept : m_desc(std::move(desc)) {}
    ~NavigationViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderNavigationView>();
        apply_desc_to_node(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto nav = std::static_pointer_cast<RenderNavigationView>(node)) {
            apply_desc_to_node(nav);
        }
    }

private:
    void apply_desc_to_node(const std::shared_ptr<RenderNavigationView>& node) {
        node->set_chrome_options(
            m_desc.is_back_button_visible, m_desc.is_back_enabled,
            m_desc.is_pane_toggle_button_visible, m_desc.is_settings_visible,
            m_desc.settings_label, m_desc.pane_title, m_desc.header,
            m_desc.open_pane_length, m_desc.compact_pane_length,
            m_desc.compact_mode_threshold_width, m_desc.expanded_mode_threshold_width);
        node->set_pane_display_mode(m_desc.pane_display_mode);
        node->set_pane_open(m_desc.is_pane_open, false);
        node->set_menu_items(m_desc.menu_items);
        node->set_footer_items(m_desc.footer_items);
        node->set_search_options(m_desc.is_search_box_visible, m_desc.search_placeholder);
        node->set_pointer_reveal_enabled(m_desc.enable_pointer_reveal);

        node->set_callbacks(m_desc.on_selection_changed, m_desc.on_back_requested, m_desc.on_pane_toggled);

        if (!m_desc.selected_id.empty()) {
            node->set_selected_id(m_desc.selected_id, false, true);
        }
        if (m_desc.page_builder) {
            node->set_page_builder(m_desc.page_builder);
        } else if (m_desc.content) {
            node->set_content_widget(m_desc.content);
        }
    }

    NavigationView m_desc;
};

inline NavigationView::operator WidgetPtr() const& {
    return make_widget<NavigationViewWidget>(*this);
}
inline NavigationView::operator WidgetPtr() && {
    return make_widget<NavigationViewWidget>(std::move(*this));
}

} // namespace ktui
