#pragma once

#include <cfloat>
#include <vector>
#include <set>
#include <mutex>

#include "Common/Math/geom2d.h"
#include "Common/Input/GestureDetector.h"
#include "Common/UI/View.h"

namespace UI {

class AnchorTranslateTween;

struct NeighborResult {
	NeighborResult() : view(0), score(0) {}
	NeighborResult(View *v, float s) : view(v), score(s) {}

	View *view;
	float score;
};

class ViewGroup : public View {
public:
	ViewGroup(LayoutParams *layoutParams = 0) : View(layoutParams) {}
	~ViewGroup();

	// Pass through external events to children.
	bool Key(const KeyInput &input) override;
	bool Touch(const TouchInput &input) override;
	void Axis(const AxisInput &input) override;

	// By default, a container will layout to its own bounds.
	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override = 0;
	void Layout() override = 0;
	void Update() override;
	void Query(float x, float y, std::vector<View *> &list) override;

	void DeviceLost() override;
	void DeviceRestored(Draw::DrawContext *draw) override;

	void Draw(UIContext &dc) override;

	// Takes ownership! DO NOT add a view to multiple parents!
	template <class T>
	T *Add(T *view) {
		std::lock_guard<std::mutex> guard(modifyLock_);
		views_.push_back(view);
		return view;
	}

	bool SetFocus() override;
	bool SubviewFocused(View *view) override;
	virtual void RemoveSubview(View *view);

	void SetDefaultFocusView(View *view) { defaultFocusView_ = view; }
	View *GetDefaultFocusView() { return defaultFocusView_; }

	// Assumes that layout has taken place.
	NeighborResult FindNeighbor(View *view, FocusDirection direction, NeighborResult best);
	virtual NeighborResult FindScrollNeighbor(View *view, const Point &target, FocusDirection direction, NeighborResult best);

	bool CanBeFocused() const override { return false; }
	bool IsViewGroup() const override { return true; }
	bool ContainsSubview(const View *view) const override;

	virtual void SetBG(const Drawable &bg) { bg_ = bg; }

	virtual void Clear();
	void PersistData(PersistStatus status, std::string anonId, PersistMap &storage) override;
	View *GetViewByIndex(int index) { return views_[index]; }
	int GetNumSubviews() const { return (int)views_.size(); }
	void SetHasDropShadow(bool has) { hasDropShadow_ = has; }
	void SetDropShadowExpand(float s) { dropShadowExpand_ = s; }
	void SetExclusiveTouch(bool exclusive) { exclusiveTouch_ = exclusive; }
	void SetClickableBackground(bool clickableBackground) { clickableBackground_ = clickableBackground; }

	void Lock() { modifyLock_.lock(); }
	void Unlock() { modifyLock_.unlock(); }

	void SetClip(bool clip) { clip_ = clip; }
	std::string DescribeLog() const override { return "ViewGroup: " + View::DescribeLog(); }
	std::string DescribeText() const override;

protected:
	std::string DescribeListUnordered(const char *heading) const;
	std::string DescribeListOrdered(const char *heading) const;

	std::mutex modifyLock_;  // Hold this when changing the subviews.
	std::vector<View *> views_;
	View *defaultFocusView_ = nullptr;
	Drawable bg_;
	float dropShadowExpand_ = 0.0f;
	bool hasDropShadow_ = false;
	bool clickableBackground_ = false;
	bool clip_ = false;
	bool exclusiveTouch_ = false;
};

// A frame layout contains a single child view (normally).
// It simply centers the child view.
class FrameLayout : public ViewGroup {
public:
	FrameLayout(LayoutParams *layoutParams = nullptr) : ViewGroup(layoutParams) {}
	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	void Layout() override;
};

const float NONE = -FLT_MAX;

class AnchorLayoutParams : public LayoutParams {
public:
	AnchorLayoutParams(Size w, Size h, float l, float t, float r, float b, bool c = false)
		: LayoutParams(w, h, LP_ANCHOR), left(l), top(t), right(r), bottom(b), center(c) {}
	// There's a small hack here to make this behave more intuitively - AnchorLayout ordinarily ignores FILL_PARENT.
	AnchorLayoutParams(Size w, Size h, bool c = false)
		: LayoutParams(w, h, LP_ANCHOR), left(0), top(0), right(w == FILL_PARENT ? 0 : NONE), bottom(h == FILL_PARENT ? 0 : NONE), center(c) {
	}
	AnchorLayoutParams(float l, float t, float r, float b, bool c = false)
		: LayoutParams(WRAP_CONTENT, WRAP_CONTENT, LP_ANCHOR), left(l), top(t), right(r), bottom(b), center(c) {}

	// These are not bounds, but distances from the container edges.
	// Set to NONE to not attach this edge to the container.
	// If two opposite edges are NONE, centering will happen.
	float left, top, right, bottom;
	bool center;  // If set, only two "sides" can be set, and they refer to the center, not the edge, of the view being layouted.

	static LayoutParamsType StaticType() {
		return LP_ANCHOR;
	}
};

class AnchorLayout : public ViewGroup {
public:
	AnchorLayout(LayoutParams *layoutParams = 0) : ViewGroup(layoutParams) {}
	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	void Layout() override;
	void Overflow(bool allow) {
		overflow_ = allow;
	}
	std::string DescribeLog() const override { return "AnchorLayout: " + View::DescribeLog(); }

private:
	void MeasureViews(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert);
	bool overflow_ = true;
};

class LinearLayoutParams : public LayoutParams {
public:
	LinearLayoutParams()
		: LayoutParams(LP_LINEAR), weight(0.0f), gravity(G_TOPLEFT), hasMargins_(false) {}
	explicit LinearLayoutParams(float wgt, Gravity grav = G_TOPLEFT)
		: LayoutParams(LP_LINEAR), weight(wgt), gravity(grav), hasMargins_(false) {}
	LinearLayoutParams(float wgt, const Margins &mgn)
		: LayoutParams(LP_LINEAR), weight(wgt), gravity(G_TOPLEFT), margins(mgn), hasMargins_(true) {}
	LinearLayoutParams(Size w, Size h, float wgt = 0.0f, Gravity grav = G_TOPLEFT)
		: LayoutParams(w, h, LP_LINEAR), weight(wgt), gravity(grav), margins(0), hasMargins_(false) {}
	LinearLayoutParams(Size w, Size h, float wgt, Gravity grav, const Margins &mgn)
		: LayoutParams(w, h, LP_LINEAR), weight(wgt), gravity(grav), margins(mgn), hasMargins_(true) {}
	LinearLayoutParams(Size w, Size h, const Margins &mgn)
		: LayoutParams(w, h, LP_LINEAR), weight(0.0f), gravity(G_TOPLEFT), margins(mgn), hasMargins_(true) {}
	LinearLayoutParams(Size w, Size h, float wgt, const Margins &mgn)
		: LayoutParams(w, h, LP_LINEAR), weight(wgt), gravity(G_TOPLEFT), margins(mgn), hasMargins_(true) {}
	LinearLayoutParams(const Margins &mgn)
		: LayoutParams(WRAP_CONTENT, WRAP_CONTENT, LP_LINEAR), weight(0.0f), gravity(G_TOPLEFT), margins(mgn), hasMargins_(true) {}

	float weight;
	Gravity gravity;
	Margins margins;

	bool HasMargins() const { return hasMargins_; }

	static LayoutParamsType StaticType() {
		return LP_LINEAR;
	}

private:
	bool hasMargins_;
};

class LinearLayout : public ViewGroup {
public:
	LinearLayout(Orientation orientation, LayoutParams *layoutParams = 0)
		: ViewGroup(layoutParams), orientation_(orientation), defaultMargins_(0) {}

	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	void Layout() override;
	void SetSpacing(float spacing) {
		spacing_ = spacing;
	}
	std::string DescribeLog() const override { return (orientation_ == ORIENT_HORIZONTAL ? "LinearLayoutHoriz: " : "LinearLayoutVert: ") + View::DescribeLog(); }
	Margins padding;

protected:
	Orientation orientation_;
private:
	Margins defaultMargins_;
	float spacing_ = 10.0f;
};

class LinearLayoutList : public LinearLayout {
public:
	LinearLayoutList(Orientation orientation, LayoutParams *layoutParams = nullptr)
		: LinearLayout(orientation, layoutParams) {
	}

	std::string DescribeText() const override;
};

// GridLayout is a little different from the Android layout. This one has fixed size
// rows and columns. Items are not allowed to deviate from the set sizes.
// Initially, only horizontal layout is supported.
struct GridLayoutSettings {
	GridLayoutSettings() : orientation(ORIENT_HORIZONTAL), columnWidth(100), rowHeight(50), spacing(5), fillCells(false) {}
	GridLayoutSettings(int colW, int colH, int spac = 5)
		: orientation(ORIENT_HORIZONTAL), columnWidth(colW), rowHeight(colH), spacing(spac), fillCells(false) {}

	Orientation orientation;
	int columnWidth;
	int rowHeight;
	int spacing;
	bool fillCells;
};

class GridLayoutParams : public LayoutParams {
public:
	GridLayoutParams()
		: LayoutParams(LP_GRID), gravity(G_CENTER) {}
	explicit GridLayoutParams(Gravity grav)
		: LayoutParams(LP_GRID), gravity(grav) {
	}

	Gravity gravity;

	static LayoutParamsType StaticType() {
		return LP_GRID;
	}
};

class GridLayout : public ViewGroup {
public:
	GridLayout(GridLayoutSettings settings, LayoutParams *layoutParams = 0);

	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	void Layout() override;
	std::string DescribeLog() const override { return "GridLayout: " + View::DescribeLog(); }

private:
	GridLayoutSettings settings_;
	int numColumns_ = 1;
};

class GridLayoutList : public GridLayout {
public:
	GridLayoutList(GridLayoutSettings settings, LayoutParams *layoutParams = nullptr)
		: GridLayout(settings, layoutParams) {
	}

	std::string DescribeText() const override;
};

// A scrollview usually contains just a single child - a linear layout or similar.
class ScrollView : public ViewGroup {
public:
	ScrollView(Orientation orientation, LayoutParams *layoutParams = 0)
		: ViewGroup(layoutParams), orientation_(orientation) {}
	~ScrollView();

	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	void Layout() override;

	bool Key(const KeyInput &input) override;
	bool Touch(const TouchInput &input) override;
	void Draw(UIContext &dc) override;
	std::string DescribeLog() const override { return "ScrollView: " + View::DescribeLog(); }

	void ScrollTo(float newScrollPos);
	void ScrollToBottom();
	void ScrollRelative(float distance);
	bool CanScroll() const;
	void Update() override;

	void RememberPosition(float *pos) {
		rememberPos_ = pos;
		ScrollTo(*pos);
	}

	// Get the last moved scroll view position
	static void GetLastScrollPosition(float &x, float &y);

	// Override so that we can scroll to the active one after moving the focus.
	bool SubviewFocused(View *view) override;
	void PersistData(PersistStatus status, std::string anonId, PersistMap &storage) override;
	void SetVisibility(Visibility visibility) override;

	// If the view is smaller than the scroll view, sets whether to align to the bottom/right instead of the left.
	void SetAlignOpposite(bool alignOpposite) {
		alignOpposite_ = alignOpposite;
	}

	NeighborResult FindScrollNeighbor(View *view, const Point &target, FocusDirection direction, NeighborResult best) override;

private:
	float ClampedScrollPos(float pos);

	GestureDetector gesture_;
	Orientation orientation_;
	float scrollPos_ = 0.0f;
	float scrollStart_ = 0.0f;
	float scrollTarget_ = 0.0f;
	int scrollTouchId_ = -1;
	bool scrollToTarget_ = false;
	float layoutScrollPos_ = 0.0f;
	float inertia_ = 0.0f;
	float pull_ = 0.0f;
	float lastViewSize_ = 0.0f;
	float *rememberPos_ = nullptr;
	bool alignOpposite_ = false;

	static float lastScrollPosX;
	static float lastScrollPosY;
};

class ChoiceStrip : public LinearLayout {
public:
	ChoiceStrip(Orientation orientation, LayoutParams *layoutParams = 0);

	void AddChoice(const std::string &title);
	void AddChoice(ImageID buttonImage);

	int GetSelection() const { return selected_; }
	void SetSelection(int sel, bool triggerClick);

	void EnableChoice(int choice, bool enabled);

	bool Key(const KeyInput &input) override;

	void SetTopTabs(bool tabs) { topTabs_ = tabs; }
	void Draw(UIContext &dc) override;

	std::string DescribeLog() const override { return "ChoiceStrip: " + View::DescribeLog(); }
	std::string DescribeText() const override;

	Event OnChoice;

private:
	StickyChoice *Choice(int index);
	EventReturn OnChoiceClick(EventParams &e);

	int selected_ = 0;   // Can be controlled with L/R.
	bool topTabs_ = false;
};


class TabHolder : public LinearLayout {
public:
	TabHolder(Orientation orientation, float stripSize, LayoutParams *layoutParams = 0);

	template <class T>
	T *AddTab(const std::string &title, T *tabContents) {
		AddTabContents(title, (View *)tabContents);
		return tabContents;
	}
	void EnableTab(int tab, bool enabled) {
		tabStrip_->EnableChoice(tab, enabled);
	}

	void SetCurrentTab(int tab, bool skipTween = false);

	int GetCurrentTab() const { return currentTab_; }
	std::string DescribeLog() const override { return "TabHolder: " + View::DescribeLog(); }

	void PersistData(PersistStatus status, std::string anonId, PersistMap &storage) override;

private:
	void AddTabContents(const std::string &title, View *tabContents);
	EventReturn OnTabClick(EventParams &e);

	ChoiceStrip *tabStrip_ = nullptr;
	ScrollView *tabScroll_ = nullptr;
	AnchorLayout *contents_ = nullptr;

	float stripSize_;
	int currentTab_ = 0;
	std::vector<View *> tabs_;
	std::vector<AnchorTranslateTween *> tabTweens_;
};

// Yes, this feels a bit Java-ish...
class ListAdaptor {
public:
	virtual ~ListAdaptor() {}
	virtual View *CreateItemView(int index) = 0;
	virtual int GetNumItems() = 0;
	virtual bool AddEventCallback(View *view, std::function<EventReturn(EventParams&)> callback) { return false; }
	virtual std::string GetTitle(int index) const { return ""; }
	virtual void SetSelected(int sel) { }
	virtual int GetSelected() { return -1; }
};

class ChoiceListAdaptor : public ListAdaptor {
public:
	ChoiceListAdaptor(const char *items[], int numItems) : items_(items), numItems_(numItems) {}
	View *CreateItemView(int index) override;
	int GetNumItems() override { return numItems_; }
	bool AddEventCallback(View *view, std::function<EventReturn(EventParams&)> callback) override;

private:
	const char **items_;
	int numItems_;
};


// The "selected" item is what was previously selected (optional). This items will be drawn differently.
class StringVectorListAdaptor : public ListAdaptor {
public:
	StringVectorListAdaptor() : selected_(-1) {}
	StringVectorListAdaptor(const std::vector<std::string> &items, int selected = -1) : items_(items), selected_(selected) {}
	View *CreateItemView(int index) override;
	int GetNumItems() override { return (int)items_.size(); }
	bool AddEventCallback(View *view, std::function<EventReturn(EventParams&)> callback) override;
	void SetSelected(int sel) override { selected_ = sel; }
	std::string GetTitle(int index) const override { return items_[index]; }
	int GetSelected() override { return selected_; }

private:
	std::vector<std::string> items_;
	int selected_;
};

// A list view is a scroll view with autogenerated items.
// In the future, it might be smart and load/unload items as they go, but currently not.
class ListView : public ScrollView {
public:
	ListView(ListAdaptor *a, std::set<int> hidden = std::set<int>(), LayoutParams *layoutParams = 0);

	int GetSelected() { return adaptor_->GetSelected(); }
	void Measure(const UIContext &dc, MeasureSpec horiz, MeasureSpec vert) override;
	virtual void SetMaxHeight(float mh) { maxHeight_ = mh; }
	Event OnChoice;
	std::string DescribeLog() const override { return "ListView: " + View::DescribeLog(); }
	std::string DescribeText() const override;

private:
	void CreateAllItems();
	EventReturn OnItemCallback(int num, EventParams &e);
	ListAdaptor *adaptor_;
	LinearLayout *linLayout_;
	float maxHeight_;
	std::set<int> hidden_;
};

}  // namespace UI
