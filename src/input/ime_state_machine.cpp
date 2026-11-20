#include "kernetile/input/ime_state_machine.hpp"
#include <algorithm>

namespace kernetile::input {

void ImeStateMachine::update_cursor_anchor(float x, float y, float w, float h) {
    m_anchor.x = x;
    m_anchor.y = y;
    m_anchor.width = w;
    m_anchor.height = h;
}

std::string ImeStateMachine::truncate_utf8_safe(std::string_view str, size_t max_bytes) {
    if (str.size() <= max_bytes) {
        return std::string(str);
    }

    // 从 max_bytes 处向前回溯，寻找 UTF-8 字符首字节
    size_t i = max_bytes;
    while (i > 0) {
        uint8_t byte = static_cast<uint8_t>(str[i]);
        // 如果不是 UTF-8 延续字节 (10xxxxxx)
        if ((byte & 0xC0) != 0x80) {
            // 此时 i 是字符首字节
            // 计算此字符期望的字节数
            size_t char_len = 1;
            if ((byte & 0xE0) == 0xC0) char_len = 2;
            else if ((byte & 0xF0) == 0xE0) char_len = 3;
            else if ((byte & 0xF8) == 0xF0) char_len = 4;

            if (i + char_len <= max_bytes) {
                // 该字符完整容纳于 max_bytes 之内
                return std::string(str.substr(0, i + char_len));
            } else {
                // 该字符被截断，因此安全边界为 i
                return std::string(str.substr(0, i));
            }
        }
        --i;
    }

    return "";
}

bool ImeStateMachine::is_valid_utf8(std::string_view str) {
    size_t i = 0;
    size_t len = str.size();
    while (i < len) {
        uint8_t c = static_cast<uint8_t>(str[i]);
        if (c <= 0x7F) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len || 
                (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 2]) & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len || 
                (static_cast<uint8_t>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<uint8_t>(str[i + 3]) & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

void ImeStateMachine::execute_wayland_commit(const WaylandCommitTransaction& tx) {
    // 步骤 1: 剔除旧预编辑串
    m_composition.preedit_text.clear();
    m_composition.is_composing = false;

    // 步骤 2: 删除周边字符
    if (tx.delete_before > 0 && !m_document_text.empty()) {
        size_t del = std::min(static_cast<size_t>(tx.delete_before), m_document_text.size());
        // 确保回退至 UTF-8 字符边界
        size_t cut_pos = m_document_text.size() - del;
        while (cut_pos < m_document_text.size() && 
               (static_cast<uint8_t>(m_document_text[cut_pos]) & 0xC0) == 0x80) {
            cut_pos++;
        }
        m_document_text.resize(cut_pos);
    }

    // 步骤 3: 插入正式提交串
    if (!tx.commit_string.empty()) {
        m_document_text.append(tx.commit_string);
        m_state = ImeState::Committed;
    }

    // 步骤 4: 重新计算周边文本与光标 (内部状态一致)

    // 步骤 5: 插入新预编辑串
    if (!tx.preedit_string.empty()) {
        m_composition.preedit_text = tx.preedit_string;
        m_composition.cursor_position = tx.preedit_cursor;
        m_composition.is_composing = true;
        m_state = ImeState::Composing;
    } else if (tx.commit_string.empty() && tx.delete_before == 0 && tx.delete_after == 0) {
        // 无提交且无预编辑，表示用户主动取消
        m_state = ImeState::Canceled;
    }

    // 步骤 6: 重设光标定位 (保持 m_anchor 有效)
}

void ImeStateMachine::on_win32_start_composition() {
    m_state = ImeState::Composing;
    m_composition.is_composing = true;
    m_composition.preedit_text.clear();
    m_composition.cursor_position = 0;
}

void ImeStateMachine::on_win32_composition(
    std::string_view preedit_str,
    std::string_view result_str,
    int32_t cursor_pos)
{
    // 如果有正式提交文字上屏
    if (!result_str.empty()) {
        m_document_text.append(result_str);
        m_state = ImeState::Committed;
    }

    // 如果仍有未决拼音在组合中
    if (!preedit_str.empty()) {
        m_composition.preedit_text = std::string(preedit_str);
        m_composition.cursor_position = cursor_pos;
        m_composition.is_composing = true;
        m_state = ImeState::Composing;
    } else {
        m_composition.preedit_text.clear();
        m_composition.is_composing = false;
        if (result_str.empty()) {
            m_state = ImeState::Canceled;
        }
    }
}

void ImeStateMachine::on_win32_end_composition() {
    m_composition.is_composing = false;
    m_composition.preedit_text.clear();
    m_composition.cursor_position = 0;
    m_state = ImeState::Idle;
}

void ImeStateMachine::reset() {
    m_state = ImeState::Idle;
    m_composition = {};
    m_document_text.clear();
    m_anchor = {};
}

} // namespace kernetile::input
