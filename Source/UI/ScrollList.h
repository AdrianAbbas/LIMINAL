#pragma once
// A scrolling list's geometry and input (2026-11-02), shared by every list the editor scrolls:
// the presets and the instrument picker. Pure arithmetic (no Component), so the tests drive it
// exactly as the editor does.
//
// Why it exists: the preset list used to move two rows per wheel event by the event's sign only,
// with `deltaY < 0 ? down : up` - so every event with deltaY == 0 (a trackpad sends them at the
// start and end of a gesture and through its momentum phase, and for a sideways swipe) scrolled the
// list UP. Scrolling to the bottom on a trackpad bounced back and the last presets were out of
// reach. Here:
//   - the offset is in pixels, clamped to [0, content - view]; content = rows * rowHeight, so the
//     last row ends exactly at the bottom of the view when scrolled to the end;
//   - wheel movement is proportional to deltaY (JUCE's Viewport scale); deltaY == 0 never moves it,
//     and nothing else (a paint, a timer, a preset load) resets the offset - only a new list
//     (another tab) starts at the top;
//   - a scrollbar: the thumb can be dragged, a click on the track pages towards the click;
//   - keys: up / down a row, page up / down, home / end;
//   - ensureVisible() keeps a chosen row in view.
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace liminal::ui {

class ScrollList {
public:
    // The list's area (in its component), the row height and how many rows. Keeps the offset in range.
    void setGeometry(juce::Rectangle<int> viewArea, int rowHeightPixels, int rows) {
        view = viewArea;
        rowHeight = std::max(1, rowHeightPixels);
        count = std::max(0, rows);
        setOffset(offset);
    }
    void setRows(int rows) { setGeometry(view, rowHeight, rows); }
    // A different list (a new tab): back to its top.
    void restart(int rows) {
        offset = 0;
        dragging = false;
        setRows(rows);
    }

    juce::Rectangle<int> viewArea() const { return view; }
    int rows() const { return count; }
    int rowHeightPixels() const { return rowHeight; }
    float contentHeight() const { return float(count * rowHeight); }
    float maxOffset() const { return std::max(0.0f, contentHeight() - float(view.getHeight())); }
    float position() const { return offset; }
    bool scrollable() const { return maxOffset() > 0.0f; }
    // Returns true if the offset changed.
    bool setOffset(float pixels) {
        const float next = std::clamp(pixels, 0.0f, maxOffset());
        if (next == offset) return false;
        offset = next;
        return true;
    }

    // Row `i`'s rectangle in the component (it may lie partly outside the view: clip to viewArea()).
    juce::Rectangle<int> rowBounds(int i) const {
        return { view.getX(), view.getY() + i * rowHeight - int(std::lround(offset)), view.getWidth() - barGap - barWidth, rowHeight - 1 };
    }
    int firstVisible() const { return std::clamp(int(offset) / rowHeight, 0, std::max(0, count - 1)); }
    int lastVisible() const { return std::clamp(int(std::ceil((offset + float(view.getHeight())) / float(rowHeight))) - 1, 0, std::max(0, count - 1)); }
    // The row under a point in the component, or -1 (outside the view, between rows past the end, or on the bar).
    int rowAt(juce::Point<int> p) const {
        if (!view.contains(p) || (scrollable() && p.x >= barArea().getX() - 1)) return -1;
        const int i = int(std::floor((float(p.y - view.getY()) + offset) / float(rowHeight)));
        return i >= 0 && i < count ? i : -1;
    }
    // Fully visible?
    bool rowVisible(int i) const {
        const auto r = rowBounds(i);
        return i >= 0 && i < count && r.getY() >= view.getY() && r.getY() + rowHeight <= view.getBottom() + 1;
    }
    bool ensureVisible(int i) {
        if (i < 0 || i >= count) return false;
        const float top = float(i * rowHeight), bottom = top + float(rowHeight);
        if (top < offset) return setOffset(top);
        if (bottom > offset + float(view.getHeight())) return setOffset(bottom - float(view.getHeight()));
        return false;
    }

    // The wheel (a mouse notch or a trackpad's stream of small deltas, momentum included).
    bool wheel(const juce::MouseWheelDetails& w) {
        if (w.deltaY == 0.0f || !std::isfinite(w.deltaY)) return false;   // a gesture's start / end, a sideways swipe: never a jump
        float pixels = w.deltaY * 14.0f * float(rowHeight);                  // JUCE's Viewport scale (one row per ~0.07 of delta)
        if (std::abs(pixels) < 1.0f) pixels = std::copysign(1.0f, pixels);
        return setOffset(offset - pixels);
    }

    // The scrollbar beside the rows (only when there is something to scroll).
    juce::Rectangle<int> barArea() const { return { view.getRight() - barWidth, view.getY(), barWidth, view.getHeight() }; }
    juce::Rectangle<int> thumbArea() const {
        const auto bar = barArea();
        if (!scrollable()) return bar;
        const float visible = float(view.getHeight()) / contentHeight();
        const int length = std::max(minThumb, int(std::lround(float(bar.getHeight()) * visible)));
        const int travel = bar.getHeight() - length;
        const int y = bar.getY() + int(std::lround(float(travel) * offset / maxOffset()));
        return { bar.getX(), y, bar.getWidth(), length };
    }
    // Mouse down: true if it was on the bar (the thumb is then grabbed, or the track pages).
    bool mouseDown(juce::Point<int> p) {
        dragging = false;
        if (!scrollable() || !barArea().expanded(3, 0).contains(p)) return false;
        const auto thumb = thumbArea();
        if (thumb.expanded(3, 0).contains(p)) {
            dragging = true;
            grab = p.y - thumb.getY();
        } else {
            setOffset(offset + (p.y < thumb.getY() ? -1.0f : 1.0f) * pageStep());
        }
        return true;
    }
    bool mouseDrag(juce::Point<int> p) {
        if (!dragging) return false;
        const auto bar = barArea();
        const int travel = bar.getHeight() - thumbArea().getHeight();
        if (travel <= 0) return false;
        const float t = float(p.y - grab - bar.getY()) / float(travel);
        setOffset(t * maxOffset());
        return true;
    }
    void mouseUp() { dragging = false; }
    bool draggingThumb() const { return dragging; }

    // Keys: up / down a row, page up / down, home / end. True if the key was one of these (used).
    bool key(const juce::KeyPress& k) {
        if (k == juce::KeyPress::upKey) { setOffset(offset - float(rowHeight)); return true; }
        if (k == juce::KeyPress::downKey) { setOffset(offset + float(rowHeight)); return true; }
        if (k == juce::KeyPress::pageUpKey) { setOffset(offset - pageStep()); return true; }
        if (k == juce::KeyPress::pageDownKey) { setOffset(offset + pageStep()); return true; }
        if (k == juce::KeyPress::homeKey) { setOffset(0.0f); return true; }
        if (k == juce::KeyPress::endKey) { setOffset(maxOffset()); return true; }
        return false;
    }

    // Draws the bar (track and thumb) in the list's colour.
    void paintBar(juce::Graphics& g, juce::Colour colour) const {
        if (!scrollable()) return;
        const auto bar = barArea().toFloat();
        g.setColour(colour.withAlpha(0.10f));
        g.fillRoundedRectangle(bar.withSizeKeepingCentre(2.0f, bar.getHeight()), 1.0f);
        g.setColour(colour.withAlpha(dragging ? 0.75f : 0.45f));
        g.fillRoundedRectangle(thumbArea().toFloat().withSizeKeepingCentre(4.0f, float(thumbArea().getHeight())), 2.0f);
    }

    static constexpr int barWidth = 8, barGap = 6, minThumb = 18;

private:
    float pageStep() const { return float(std::max(rowHeight, view.getHeight() - rowHeight)); }
    juce::Rectangle<int> view;
    int rowHeight = 22, count = 0;
    float offset = 0;
    bool dragging = false;
    int grab = 0;
};

} // namespace liminal::ui
