#pragma once

#include <ktui/animation/motion.hpp>
#include <ktui/animation/spring_curve.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

namespace ktui {

// ============================================================================
// 跨页面共享元素转场 (Connected / Hero Transition)
// 基于物理弹簧曲线 (SpringSolver) 在页面切换或卡片展开前后衔接边界、G2 超椭圆圆角、
// OkLab 表面色与投影高度，支持任意时刻打断与速度连续。
// ============================================================================
struct HeroSnapshot {
    std::string tag;
    Rect abs_bounds{};
    float corner_radius{8.0f};
    Color surface_color{Color::transparent()};
    Color border_color{Color::transparent()};
    float border_width{0.0f};
    float elevation{0.0f};
    uint64_t route_epoch{0};
    std::chrono::steady_clock::time_point last_seen{};
};

class HeroCoordinator {
public:
    static HeroCoordinator& instance() noexcept {
        static HeroCoordinator s_inst;
        return s_inst;
    }

    [[nodiscard]] uint64_t current_epoch() const noexcept { return m_epoch; }

    void begin_route_transition() noexcept {
        ++m_epoch;
        m_last_transition_time = std::chrono::steady_clock::now();
    }

    void record(const HeroSnapshot& snap) {
        if (snap.tag.empty() || snap.abs_bounds.is_empty()) return;
        HeroSnapshot copy = snap;
        copy.last_seen = std::chrono::steady_clock::now();
        m_snapshots[snap.tag] = std::move(copy);
    }

    void seed_departure(const HeroSnapshot& snap) {
        if (snap.tag.empty() || snap.abs_bounds.is_empty()) return;
        HeroSnapshot copy = snap;
        copy.last_seen = std::chrono::steady_clock::now();
        m_pending_departures[snap.tag] = std::move(copy);
    }

    bool try_claim_flight(const std::string& tag,
                          const HeroSnapshot& target,
                          HeroSnapshot& out_from) {
        if (tag.empty() || target.abs_bounds.is_empty()) return false;
        const auto now = std::chrono::steady_clock::now();

        auto it_dep = m_pending_departures.find(tag);
        if (it_dep != m_pending_departures.end()) {
            const float age_sec = std::chrono::duration<float>(now - it_dep->second.last_seen).count();
            if (age_sec <= 2.5f && differs_enough(it_dep->second.abs_bounds, target.abs_bounds)) {
                out_from = it_dep->second;
                m_pending_departures.erase(it_dep);
                return true;
            }
            m_pending_departures.erase(it_dep);
        }

        auto it = m_snapshots.find(tag);
        if (it != m_snapshots.end()) {
            const float age_sec = std::chrono::duration<float>(now - it->second.last_seen).count();
            if (age_sec <= 2.0f &&
                (it->second.route_epoch < m_epoch || differs_enough(it->second.abs_bounds, target.abs_bounds)) &&
                differs_enough(it->second.abs_bounds, target.abs_bounds)) {
                out_from = it->second;
                return true;
            }
        }
        return false;
    }

    void clear() {
        m_snapshots.clear();
        m_pending_departures.clear();
        m_epoch = 0;
    }

private:
    HeroCoordinator() = default;

    static bool differs_enough(const Rect& a, const Rect& b) noexcept {
        return std::abs(a.min_x - b.min_x) > 1.5f ||
               std::abs(a.min_y - b.min_y) > 1.5f ||
               std::abs(a.width() - b.width()) > 1.5f ||
               std::abs(a.height() - b.height()) > 1.5f;
    }

    uint64_t m_epoch{0};
    std::chrono::steady_clock::time_point m_last_transition_time{};
    std::unordered_map<std::string, HeroSnapshot> m_snapshots;
    std::unordered_map<std::string, HeroSnapshot> m_pending_departures;
};

class RenderHero : public RenderBox {
public:
    RenderHero() noexcept = default;
    ~RenderHero() override {
        if (!m_tag.empty() && !m_last_abs.is_empty()) {
            HeroSnapshot dep = make_snapshot(m_last_abs);
            HeroCoordinator::instance().seed_departure(dep);
        }
    }

    void set_properties(std::string tag,
                        float corner_radius,
                        Color surface_color,
                        Color border_color,
                        float border_width,
                        float elevation,
                        float stiffness,
                        float damping) {
        if (m_tag != tag) {
            m_tag = std::move(tag);
            m_claimed = false;
        }
        m_corner_radius = std::max(0.0f, corner_radius);
        m_surface_color = surface_color;
        m_border_color = border_color;
        m_border_width = std::max(0.0f, border_width);
        m_elevation = std::max(0.0f, elevation);
        m_spring_cfg = SpringConfig(std::max(40.0f, stiffness), std::max(4.0f, damping));
        mark_needs_paint();
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child_box == child) return;
        if (m_child_box) remove_child(m_child_box.get());
        m_child_box = std::move(child);
        if (m_child_box) append_child(m_child_box.get());
        mark_needs_layout();
    }

    [[nodiscard]] const std::shared_ptr<RenderBox>& child_box() const noexcept { return m_child_box; }
    [[nodiscard]] const std::string& tag() const noexcept { return m_tag; }
    [[nodiscard]] bool is_flying() const noexcept { return m_flight_active; }
    [[nodiscard]] float flight_progress() const noexcept { return m_flight_active ? m_progress_spring.value() : 1.0f; }

    void start_flight_from(const HeroSnapshot& from) {
        m_from = from;
        m_progress_spring = SpringSolver(0.0f, m_spring_cfg);
        m_progress_spring.rebind(0.0f, 0.0f, 1.0f);
        m_flight_active = true;
        m_last_tick = std::chrono::steady_clock::now();
        mark_needs_paint();
    }

    void perform_layout() override {
        Vec2 child_sz{0.0f, 0.0f};
        if (m_child_box) {
            m_child_box->layout(m_constraints, true);
            m_child_box->set_offset(Vec2{0.0f, 0.0f});
            child_sz = m_child_box->size();
        }
        m_size = m_constraints.constrain(child_sz);
    }

    void paint(PaintContext& ctx) override {
        const Rect my_abs = Rect::from_xywh(
            ctx.current_offset().x, ctx.current_offset().y, m_size.x, m_size.y);
        m_last_abs = my_abs;
        const HeroSnapshot target = make_snapshot(my_abs);

        if (!m_claimed && !m_tag.empty()) {
            m_claimed = true;
            HeroSnapshot from{};
            if (HeroCoordinator::instance().try_claim_flight(m_tag, target, from)) {
                start_flight_from(from);
            }
        }

        const Theme& theme = ThemeContext::current();
        if (m_flight_active) {
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::clamp(std::chrono::duration<float>(now - m_last_tick).count(), 0.001f, 0.05f);
            m_last_tick = now;
            m_progress_spring.update(dt);
            const float t = m_progress_spring.value();
            const float tc = std::clamp(t, 0.0f, 1.0f);

            const auto lerpf = [](float a, float b, float u) noexcept { return a + (b - a) * u; };
            const Rect cur_abs{
                lerpf(m_from.abs_bounds.min_x, target.abs_bounds.min_x, t),
                lerpf(m_from.abs_bounds.min_y, target.abs_bounds.min_y, t),
                lerpf(m_from.abs_bounds.max_x, target.abs_bounds.max_x, t),
                lerpf(m_from.abs_bounds.max_y, target.abs_bounds.max_y, t)
            };
            const Rect cur_local = Rect::from_xywh(
                cur_abs.min_x - ctx.current_offset().x,
                cur_abs.min_y - ctx.current_offset().y,
                std::max(1.0f, cur_abs.width()),
                std::max(1.0f, cur_abs.height()));
            const float cur_r = std::max(0.0f, lerpf(m_from.corner_radius, target.corner_radius, t));
            const float cur_elev = std::max(0.0f, lerpf(m_from.elevation, target.elevation, tc));
            const Color cur_bg = Motion::lerp(m_from.surface_color, target.surface_color, tc);
            const Color cur_bd = Motion::lerp(m_from.border_color, target.border_color, tc);
            const float cur_bw = std::max(0.0f, lerpf(m_from.border_width, target.border_width, tc));

            if (cur_elev > 0.5f) {
                ctx.draw_elevation_shadow(cur_local, cur_r, theme.is_dark, cur_elev, 0.90f);
            }
            if (cur_bg.a > 0.001f) {
                ctx.draw_rect(cur_local, cur_bg, cur_r, 0.0f);
            }
            if (cur_bw > 0.0f && cur_bd.a > 0.001f) {
                ctx.draw_rect(cur_local, cur_bd, cur_r, cur_bw);
            }

            ctx.push_clip(cur_local, cur_r);
            const Vec2 cur_center{
                cur_local.min_x + cur_local.width() * 0.5f,
                cur_local.min_y + cur_local.height() * 0.5f
            };
            const Vec2 dst_center{m_size.x * 0.5f, m_size.y * 0.5f};
            PaintContext::OffsetScope shift(ctx, cur_center - dst_center);
            RenderBox::paint(ctx);
            ctx.pop_clip();

            HeroSnapshot cur_snap = target;
            cur_snap.abs_bounds = cur_abs;
            cur_snap.corner_radius = cur_r;
            HeroCoordinator::instance().record(cur_snap);

            if (m_progress_spring.is_settled()) {
                m_flight_active = false;
            } else {
                mark_needs_paint();
            }
            return;
        }

        if (m_elevation > 0.5f) {
            ctx.draw_elevation_shadow(bounds(), m_corner_radius, theme.is_dark, m_elevation, 0.90f);
        }
        if (m_surface_color.a > 0.001f) {
            ctx.draw_rect(bounds(), m_surface_color, m_corner_radius, 0.0f);
        }
        if (m_border_width > 0.0f && m_border_color.a > 0.001f) {
            ctx.draw_rect(bounds(), m_border_color, m_corner_radius, m_border_width);
        }

        ctx.push_clip(bounds(), m_corner_radius);
        RenderBox::paint(ctx);
        ctx.pop_clip();

        HeroCoordinator::instance().record(target);
    }

private:
    HeroSnapshot make_snapshot(const Rect& abs_r) const {
        HeroSnapshot s;
        s.tag = m_tag;
        s.abs_bounds = abs_r;
        s.corner_radius = m_corner_radius;
        s.surface_color = m_surface_color;
        s.border_color = m_border_color;
        s.border_width = m_border_width;
        s.elevation = m_elevation;
        s.route_epoch = HeroCoordinator::instance().current_epoch();
        return s;
    }

    std::string m_tag;
    float m_corner_radius{8.0f};
    Color m_surface_color{Color::transparent()};
    Color m_border_color{Color::transparent()};
    float m_border_width{0.0f};
    float m_elevation{0.0f};
    SpringConfig m_spring_cfg{280.0f, 28.0f};

    std::shared_ptr<RenderBox> m_child_box;
    HeroSnapshot m_from{};
    HeroSnapshot m_last_abs_snap{};
    Rect m_last_abs{};
    bool m_claimed{false};
    bool m_flight_active{false};
    SpringSolver m_progress_spring{1.0f};
    std::chrono::steady_clock::time_point m_last_tick{};
};

struct Hero : BindableMixin<Hero, RenderHero> {
    std::string tag;
    float corner_radius{8.0f};
    Color surface_color{Color::transparent()};
    Color border_color{Color::transparent()};
    float border_width{0.0f};
    float elevation{0.0f};
    float spring_stiffness{280.0f};
    float spring_damping{28.0f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class HeroWidget final : public Widget {
public:
    explicit HeroWidget(const Hero& desc) : m_desc(desc) {}
    explicit HeroWidget(Hero&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderHero>();
        if (m_desc.child) {
            auto c = m_desc.child->create_render_node();
            m_desc.child->set_render_node(c);
            node->set_child_box(std::move(c));
        }
        apply_props(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto h = std::static_pointer_cast<RenderHero>(node)) {
            if (m_desc.child) {
                if (!h->child_box()) {
                    auto c = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(c);
                    h->set_child_box(std::move(c));
                } else {
                    m_desc.child->update_render_node(h->child_box());
                }
            } else {
                h->set_child_box(nullptr);
            }
            apply_props(h);
        }
    }

private:
    void apply_props(const std::shared_ptr<RenderHero>& h) const {
        h->set_properties(m_desc.tag, m_desc.corner_radius, m_desc.surface_color,
                          m_desc.border_color, m_desc.border_width, m_desc.elevation,
                          m_desc.spring_stiffness, m_desc.spring_damping);
    }

    Hero m_desc;
};

inline Hero::operator WidgetPtr() const& { return make_widget<HeroWidget>(*this); }
inline Hero::operator WidgetPtr() && { return make_widget<HeroWidget>(std::move(*this)); }

} // namespace ktui
