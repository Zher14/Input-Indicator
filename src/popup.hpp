#pragma once
#include "overlay.hpp"

// ---------------------------------------------------------------------
// Mini menu de configuracion con 3 paginas (se abre desde la pausa)
// ---------------------------------------------------------------------
class IVSettingsPopup : public Popup {
protected:
	struct SliderDef {
		char const* key;
		int name;
		double min;
		double max;
	};
	static constexpr SliderDef SLIDERS[9] = {
		{"opacity", iv::T_OPACITY, 0.1, 1.0},
		{"scale", iv::T_SCALE, 0.5, 2.0},
		{"text-scale", iv::T_TEXTSIZE, 0.5, 2.0},
		{"pos-x", iv::T_POSX, 0.0, 100.0},
		{"pos-y", iv::T_POSY, 0.0, 100.0},
		{"speed", iv::T_SPEED, 0.25, 4.0},
		{"length", iv::T_LENGTH, 60.0, 240.0},
		{"outline-size", iv::T_OUTLINE, 0.0, 4.0},
		{"gap", iv::T_GAP, 2.0, 16.0},
	};
	static constexpr char const* TOGGLE_KEYS[7] = {
		"enabled", "show-cps", "show-total", "timeline", "vertical", "flip", "fade"
	};

	int m_page = 0;
	float m_cx = 0.f;
	std::vector<CCNode*> m_items;
	CCLabelBMFont* m_pageTitle = nullptr;
	CCLabelBMFont* m_pageNum = nullptr;
	CCLabelBMFont* m_labels[9] = {};

	static std::string sliderText(int i, double v) {
		return fmt::format("{}: {:.2f}", iv::tr(SLIDERS[i].name), v);
	}

	static char const* toggleName(int id) {
		switch (id) {
			case 0: return iv::tr(iv::T_ENABLED);
			case 1: return "CPS";
			case 2: return iv::tr(iv::T_CLICKS);
			case 3: return iv::tr(iv::T_TIMELINE);
			case 4: return iv::tr(iv::T_VERTICAL);
			case 5: return iv::tr(iv::T_FLIP);
			default: return iv::tr(iv::T_FADE);
		}
	}

	void track(CCNode* n, CCNode* parent) {
		parent->addChild(n);
		m_items.push_back(n);
	}

	// ---- Piezas de interfaz ----
	void addLabel(char const* text, float x, float y, float scale, bool left) {
		auto l = CCLabelBMFont::create(text, "chatFont.fnt");
		l->setScale(scale);
		if (left) l->setAnchorPoint({0.f, 0.5f});
		l->setPosition({x, y});
		this->track(l, m_mainLayer);
	}

	void addSlider(int i, float y) {
		double cur = iv::S<double>(SLIDERS[i].key);

		m_labels[i] = CCLabelBMFont::create(sliderText(i, cur).c_str(), "chatFont.fnt");
		m_labels[i]->setScale(0.65f);
		m_labels[i]->setPosition({m_cx, y + 10.f});
		this->track(m_labels[i], m_mainLayer);

		auto slider = Slider::create(this, menu_selector(IVSettingsPopup::onSlider), 0.7f);
		slider->setPosition({m_cx, y - 4.f});
		slider->getThumb()->setTag(i);
		slider->setValue(static_cast<float>((cur - SLIDERS[i].min) / (SLIDERS[i].max - SLIDERS[i].min)));
		this->track(slider, m_mainLayer);
	}

	void addToggle(int id, float x, float y) {
		auto t = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(IVSettingsPopup::onToggle), 0.5f
		);
		t->toggle(iv::S<bool>(TOGGLE_KEYS[id]));
		t->setTag(id);
		t->setPosition({x, y});
		this->track(t, m_buttonMenu);

		this->addLabel(toggleName(id), x + 16.f, y, 0.65f, true);
	}

	CCNode* makeButton(char const* text, float w, float h) {
		auto holder = CCNode::create();
		holder->setContentSize({w, h});

		auto bg = CCScale9Sprite::create("GJ_button_04.png");
		bg->setContentSize({w, h});
		bg->setPosition({w / 2.f, h / 2.f});
		holder->addChild(bg);

		auto lbl = CCLabelBMFont::create(text, "chatFont.fnt");
		lbl->setScale(0.75f);
		lbl->setPosition({w / 2.f, h / 2.f});
		holder->addChild(lbl);
		return holder;
	}

	void addCycle(char const* label, char const* value, float y, SEL_MenuHandler cb) {
		this->addLabel(label, 24.f, y, 0.75f, true);
		auto btn = CCMenuItemSpriteExtra::create(this->makeButton(value, 150.f, 26.f), this, cb);
		btn->setPosition({235.f, y});
		this->track(btn, m_buttonMenu);
	}

	void addSwatches(bool outline, char const* label, float y) {
		this->addLabel(label, 20.f, y, 0.7f, true);
		for (int i = 0; i < 8; i++) {
			auto holder = CCNode::create();
			holder->setContentSize({20.f, 20.f});
			auto sq = CCLayerColor::create(ccc4(255, 255, 255, 255), 20.f, 20.f);
			sq->setColor(iv::SWATCHES[i]);
			holder->addChild(sq);

			auto btn = CCMenuItemSpriteExtra::create(
				holder, this, menu_selector(IVSettingsPopup::onSwatch)
			);
			btn->setTag(i + (outline ? 100 : 0));
			btn->setPosition({126.f + 26.f * i, y});
			this->track(btn, m_buttonMenu);
		}
	}

	// ---- Paginas ----
	void buildPage0() {
		this->addCycle(iv::tr(iv::T_STYLE), iv::STYLE_NAMES[iv::styleIdx()][iv::lang()],
			208.f, menu_selector(IVSettingsPopup::onStyle));
		this->addCycle(iv::tr(iv::T_LANG), iv::LANG_NAMES[iv::lang()],
			178.f, menu_selector(IVSettingsPopup::onLang));
		this->addSlider(0, 143.f);
		this->addSlider(1, 109.f);
		this->addSlider(2, 75.f);
		this->addToggle(0, 30.f, 46.f);
		this->addToggle(1, 140.f, 46.f);
		this->addToggle(2, 250.f, 46.f);
	}

	void buildPage1() {
		this->addSlider(3, 205.f);
		this->addSlider(4, 167.f);
		this->addSlider(5, 129.f);
		this->addSlider(6, 91.f);
		this->addToggle(3, 22.f, 52.f);
		this->addToggle(4, 102.f, 52.f);
		this->addToggle(5, 182.f, 52.f);
		this->addToggle(6, 262.f, 52.f);
	}

	void buildPage2() {
		this->addSwatches(false, iv::tr(iv::T_BARCOLOR), 205.f);
		this->addSwatches(true, iv::tr(iv::T_OUTCOLOR), 172.f);
		this->addSlider(7, 132.f);
		this->addSlider(8, 94.f);
		this->addCycle(iv::tr(iv::T_FONT), iv::FONT_NAMES[iv::fontIdx()],
			56.f, menu_selector(IVSettingsPopup::onFont));
	}

	void rebuild() {
		for (auto n : m_items) n->removeFromParentAndCleanup(true);
		m_items.clear();
		for (auto& l : m_labels) l = nullptr;

		m_pageTitle->setString(iv::tr(iv::T_PAGE0 + m_page));
		m_pageNum->setString(fmt::format("{}/3", m_page + 1).c_str());

		if (m_page == 0) this->buildPage0();
		else if (m_page == 1) this->buildPage1();
		else this->buildPage2();
	}

	void requestRebuild() {
		this->scheduleOnce(schedule_selector(IVSettingsPopup::doRebuild), 0.f);
	}

	void doRebuild(float) {
		this->rebuild();
	}

	bool init() {
		if (!Popup::init(340.f, 280.f)) return false;

		auto size = m_mainLayer->getContentSize();
		m_cx = size.width / 2.f;

		auto title = CCLabelBMFont::create("Input Indicator", "goldFont.fnt");
		title->setScale(0.7f);
		title->setPosition({m_cx, size.height - 16.f});
		m_mainLayer->addChild(title);

		m_pageTitle = CCLabelBMFont::create("", "chatFont.fnt");
		m_pageTitle->setScale(0.85f);
		m_pageTitle->setPosition({m_cx, size.height - 38.f});
		m_mainLayer->addChild(m_pageTitle);

		// Navegacion entre paginas
		auto lspr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
		lspr->setScale(0.5f);
		auto lbtn = CCMenuItemSpriteExtra::create(lspr, this, menu_selector(IVSettingsPopup::onPrev));
		lbtn->setPosition({m_cx - 55.f, 22.f});
		m_buttonMenu->addChild(lbtn);

		auto rspr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
		rspr->setScale(0.5f);
		rspr->setFlipX(true);
		auto rbtn = CCMenuItemSpriteExtra::create(rspr, this, menu_selector(IVSettingsPopup::onNext));
		rbtn->setPosition({m_cx + 55.f, 22.f});
		m_buttonMenu->addChild(rbtn);

		m_pageNum = CCLabelBMFont::create("1/3", "chatFont.fnt");
		m_pageNum->setScale(0.8f);
		m_pageNum->setPosition({m_cx, 22.f});
		m_mainLayer->addChild(m_pageNum);

		this->rebuild();
		return true;
	}

	void applyNow() {
		if (auto pl = PlayLayer::get()) {
			if (auto o = pl->getChildByID("input-overlay"_spr)) {
				static_cast<InputOverlay*>(o)->refresh();
			}
		}
	}

	// ---- Callbacks ----
	void onPrev(CCObject*) {
		m_page = (m_page + 2) % 3;
		this->rebuild();
	}

	void onNext(CCObject*) {
		m_page = (m_page + 1) % 3;
		this->rebuild();
	}

	void onSlider(CCObject* sender) {
		auto thumb = static_cast<SliderThumb*>(sender);
		int i = thumb->getTag();
		if (i < 0 || i > 8) return;

		double val = SLIDERS[i].min + thumb->getValue() * (SLIDERS[i].max - SLIDERS[i].min);
		val = std::round(val * 100.0) / 100.0;
		Mod::get()->setSettingValue<double>(SLIDERS[i].key, val);
		if (m_labels[i]) m_labels[i]->setString(sliderText(i, val).c_str());
		this->applyNow();
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool on = !t->isToggled(); // el callback se llama antes de cambiar el estado
		int i = t->getTag();
		if (i < 0 || i > 6) return;
		Mod::get()->setSettingValue<bool>(TOGGLE_KEYS[i], on);
		this->applyNow();
	}

	void onSwatch(CCObject* sender) {
		int tag = static_cast<CCNode*>(sender)->getTag();
		bool outline = tag >= 100;
		int i = tag % 100;
		if (i < 0 || i > 7) return;
		Mod::get()->setSettingValue<ccColor3B>(outline ? "outline-color" : "bar-color", iv::SWATCHES[i]);
		this->applyNow();
	}

	void onStyle(CCObject*) {
		iv::seti("ui-style", (iv::styleIdx() + 1) % 5);
		this->requestRebuild();
		this->applyNow();
	}

	void onLang(CCObject*) {
		iv::seti("language", (iv::lang() + 1) % 5);
		this->requestRebuild();
	}

	void onFont(CCObject*) {
		iv::seti("font", (iv::fontIdx() + 1) % 8);
		this->requestRebuild();
		this->applyNow();
	}

public:
	static IVSettingsPopup* create() {
		auto ret = new IVSettingsPopup();
		if (ret->init()) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};