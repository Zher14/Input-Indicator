#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <chrono>
#include <deque>
#include <vector>

using namespace geode::prelude;

// ---------------------------------------------------------------------
// Estado global de los inputs
// ---------------------------------------------------------------------
namespace iv {
	static double now() {
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	struct State {
		bool down[2][4] = {};      // [jugador][boton]  1=salto 2=izq 3=der
		int total = 0;
		std::deque<double> clicks; // timestamps para el CPS

		static State& get() {
			static State s;
			return s;
		}
		void reset() {
			for (auto& p : down) for (auto& b : p) b = false;
			total = 0;
			clicks.clear();
		}
		int cps() {
			double t = now();
			while (!clicks.empty() && t - clicks.front() > 1.0) clicks.pop_front();
			return static_cast<int>(clicks.size());
		}
	};

	template <class T>
	static T S(char const* key) {
		return Mod::get()->getSettingValue<T>(key);
	}
}

// ---------------------------------------------------------------------
// Overlay que se dibuja en el PlayLayer
// ---------------------------------------------------------------------
class InputOverlay : public CCNode {
protected:
	struct Box {
		CCScale9Sprite* bg = nullptr;
		CCSprite* iconSpr = nullptr;
		CCLabelBMFont* iconLbl = nullptr;
		int player = 0;
		int button = 0;
	};
	std::vector<Box> m_boxes;
	CCLabelBMFont* m_info = nullptr;

	void addBox(int player, int button, int idx, int n, float y, char const* sprite, char const* text) {
		Box b;
		b.player = player;
		b.button = button;

		b.bg = CCScale9Sprite::create("square02_001.png");
		b.bg->setContentSize({28.f, 28.f});
		b.bg->setPosition({(idx - (n - 1) / 2.f) * 32.f, y});
		this->addChild(b.bg);

		if (sprite) {
			b.iconSpr = CCSprite::createWithSpriteFrameName(sprite);
		}
		if (b.iconSpr) {
			limitNodeSize(b.iconSpr, {16.f, 16.f}, 1.f, 0.1f);
			b.iconSpr->setPosition({14.f, 14.f});
			b.bg->addChild(b.iconSpr);
		} else {
			b.iconLbl = CCLabelBMFont::create(text, "bigFont.fnt");
			b.iconLbl->setScale(0.45f);
			b.iconLbl->setPosition({14.f, 14.f});
			b.bg->addChild(b.iconLbl);
		}
		m_boxes.push_back(b);
	}

	bool init(bool platformer) {
		if (!CCNode::init()) return false;

		for (int p = 0; p < 2; p++) {
			float y = -32.f * p;
			if (platformer) {
				this->addBox(p, 2, 0, 3, y, "edit_leftBtn_001.png", "<");
				this->addBox(p, 1, 1, 3, y, "edit_upBtn_001.png", "J");
				this->addBox(p, 3, 2, 3, y, "edit_rightBtn_001.png", ">");
			} else {
				this->addBox(p, 1, 0, 1, y, nullptr, p == 0 ? "P1" : "P2");
			}
		}

		m_info = CCLabelBMFont::create("", "bigFont.fnt");
		m_info->setScale(0.4f);
		m_info->setPosition({0.f, 32.f});
		this->addChild(m_info);

		this->scheduleUpdate();
		this->refresh();
		return true;
	}

public:
	static InputOverlay* create(bool platformer) {
		auto ret = new InputOverlay();
		if (ret->init(platformer)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}

	void update(float) override {
		this->refresh();
	}

	// Aplica ajustes + estado de los botones. Se llama cada frame y
	// tambien desde el menu de configuracion (por si el juego esta pausado).
	void refresh() {
		bool enabled = iv::S<bool>("enabled");
		this->setVisible(enabled);
		if (!enabled) return;

		float opacity = static_cast<float>(iv::S<double>("opacity"));
		float scale = static_cast<float>(iv::S<double>("scale"));
		float px = static_cast<float>(iv::S<double>("pos-x"));
		float py = static_cast<float>(iv::S<double>("pos-y"));

		auto win = CCDirector::get()->getWinSize();
		this->setScale(scale);
		this->setPosition({win.width * px / 100.f, win.height * py / 100.f});

		auto pl = PlayLayer::get();
		bool dual = pl && pl->m_gameState.m_isDualMode;
		auto& st = iv::State::get();

		for (auto& b : m_boxes) {
			bool visible = b.player == 0 || dual;
			b.bg->setVisible(visible);
			if (!visible) continue;

			bool pressed = st.down[b.player][b.button];
			b.bg->setColor(pressed ? ccColor3B{255, 255, 255} : ccColor3B{25, 25, 25});
			b.bg->setOpacity(static_cast<GLubyte>(255.f * opacity * (pressed ? 1.f : 0.55f)));

			ccColor3B ic = pressed ? ccColor3B{0, 0, 0} : ccColor3B{255, 255, 255};
			GLubyte iop = static_cast<GLubyte>(255.f * opacity);
			if (b.iconSpr) { b.iconSpr->setColor(ic); b.iconSpr->setOpacity(iop); }
			if (b.iconLbl) { b.iconLbl->setColor(ic); b.iconLbl->setOpacity(iop); }
		}

		bool showCps = iv::S<bool>("show-cps");
		bool showTotal = iv::S<bool>("show-total");
		std::string txt;
		if (showCps) txt += fmt::format("CPS: {}", st.cps());
		if (showTotal) {
			if (!txt.empty()) txt += "   ";
			txt += fmt::format("Clicks: {}", st.total);
		}
		m_info->setString(txt.c_str());
		m_info->setOpacity(static_cast<GLubyte>(255.f * opacity));
		m_info->setVisible(!txt.empty());
	}
};

// ---------------------------------------------------------------------
// Mini menu de configuracion (se abre desde el menu de pausa)
// ---------------------------------------------------------------------
class IVSettingsPopup : public Popup {
protected:
	struct SliderDef {
		char const* key;
		char const* name;
		double min;
		double max;
	};
	static constexpr SliderDef SLIDERS[4] = {
		{"opacity", "Opacidad", 0.1, 1.0},
		{"scale", "Tamano", 0.5, 2.0},
		{"pos-x", "Posicion X", 0.0, 100.0},
		{"pos-y", "Posicion Y", 0.0, 100.0},
	};
	CCLabelBMFont* m_labels[4] = {};

	static std::string labelText(int i, double v) {
		return fmt::format("{}: {:.2f}", SLIDERS[i].name, v);
	}

	bool init() {
		if (!Popup::init(300.f, 310.f)) return false;
		this->setTitle("Input Indicator");

		auto size = m_mainLayer->getContentSize();
		float cx = size.width / 2.f;

		// Sliders
		for (int i = 0; i < 4; i++) {
			float y = 245.f - 40.f * i;
			double cur = iv::S<double>(SLIDERS[i].key);

			m_labels[i] = CCLabelBMFont::create(labelText(i, cur).c_str(), "bigFont.fnt");
			m_labels[i]->setScale(0.4f);
			m_labels[i]->setPosition({cx, y + 14.f});
			m_mainLayer->addChild(m_labels[i]);

			auto slider = Slider::create(this, menu_selector(IVSettingsPopup::onSlider), 0.7f);
			slider->setPosition({cx, y - 4.f});
			slider->getThumb()->setTag(i);
			slider->setValue(static_cast<float>((cur - SLIDERS[i].min) / (SLIDERS[i].max - SLIDERS[i].min)));
			m_mainLayer->addChild(slider);
		}

		// Toggles
		struct ToggleDef { char const* key; char const* name; };
		ToggleDef toggles[3] = {
			{"enabled", "Activado"},
			{"show-cps", "Mostrar CPS"},
			{"show-total", "Mostrar clicks totales"},
		};
		for (int i = 0; i < 3; i++) {
			float y = 85.f - 28.f * i;
			auto t = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(IVSettingsPopup::onToggle), 0.6f
			);
			t->toggle(iv::S<bool>(toggles[i].key));
			t->setTag(i);
			t->setPosition({55.f, y});
			m_buttonMenu->addChild(t);

			auto lbl = CCLabelBMFont::create(toggles[i].name, "bigFont.fnt");
			lbl->setScale(0.4f);
			lbl->setAnchorPoint({0.f, 0.5f});
			lbl->setPosition({80.f, y});
			m_mainLayer->addChild(lbl);
		}

		return true;
	}

	void applyNow() {
		if (auto pl = PlayLayer::get()) {
			if (auto o = pl->getChildByID("input-overlay"_spr)) {
				static_cast<InputOverlay*>(o)->refresh();
			}
		}
	}

	void onSlider(CCObject* sender) {
		auto thumb = static_cast<SliderThumb*>(sender);
		int i = thumb->getTag();
		if (i < 0 || i > 3) return;

		double val = SLIDERS[i].min + thumb->getValue() * (SLIDERS[i].max - SLIDERS[i].min);
		val = std::round(val * 100.0) / 100.0;
		Mod::get()->setSettingValue<double>(SLIDERS[i].key, val);
		m_labels[i]->setString(labelText(i, val).c_str());
		this->applyNow();
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool on = !t->isToggled(); // el callback se llama antes de cambiar el estado
		char const* keys[3] = {"enabled", "show-cps", "show-total"};
		int i = t->getTag();
		if (i < 0 || i > 2) return;
		Mod::get()->setSettingValue<bool>(keys[i], on);
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

// ---------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------
class $modify(IVPlayLayer, PlayLayer) {
	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		iv::State::get().reset();

		auto overlay = InputOverlay::create(m_level->isPlatformer());
		overlay->setID("input-overlay"_spr);
		overlay->setZOrder(100);
		this->addChild(overlay);
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		iv::State::get().reset();
	}
};

class $modify(IVBaseGameLayer, GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (!typeinfo_cast<PlayLayer*>(this)) return;
		if (button < 1 || button > 3) return;

		auto& st = iv::State::get();
		int p = isPlayer1 ? 0 : 1;

		// Solo contamos clicks del jugador 1 para no duplicar en modo dual
		if (down && !st.down[p][button] && isPlayer1) {
			st.total++;
			st.clicks.push_back(iv::now());
		}
		st.down[p][button] = down;
	}
};

class $modify(IVPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto spr = CircleButtonSprite::create(
			CCLabelBMFont::create("IV", "bigFont.fnt")
		);
		auto btn = CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(IVPauseLayer::onInputIndicator)
		);
		btn->setID("input-indicator-button"_spr);

		if (auto menu = this->getChildByID("right-button-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		} else {
			auto win = CCDirector::get()->getWinSize();
			auto menu = CCMenu::create();
			menu->setPosition({win.width - 40.f, 40.f});
			menu->addChild(btn);
			this->addChild(menu);
		}
	}

	void onInputIndicator(CCObject*) {
		if (auto p = IVSettingsPopup::create()) p->show();
	}
};
