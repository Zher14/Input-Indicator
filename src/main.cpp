#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
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

	struct Seg {
		double start;
		double end; // < 0 mientras el boton sigue pulsado
	};

	struct State {
		bool down[2][4] = {};            // [jugador][boton] 1=salto 2=izq 3=der
		std::deque<Seg> segs[2][4];      // historial para la barra
		int total = 0;
		std::deque<double> clicks;       // timestamps para el CPS

		static State& get() {
			static State s;
			return s;
		}
		void reset() {
			for (int p = 0; p < 2; p++) {
				for (int b = 0; b < 4; b++) {
					down[p][b] = false;
					segs[p][b].clear();
				}
			}
			total = 0;
			clicks.clear();
		}
		void press(int p, int b) {
			segs[p][b].push_back({now(), -1.0});
			if (segs[p][b].size() > 512) segs[p][b].pop_front();
		}
		void release(int p, int b) {
			auto& d = segs[p][b];
			if (!d.empty() && d.back().end < 0) d.back().end = now();
		}
		void purge(double window) {
			double t = now();
			for (int p = 0; p < 2; p++) {
				for (int b = 0; b < 4; b++) {
					auto& d = segs[p][b];
					while (!d.empty() && d.front().end >= 0 && t - d.front().end > window) {
						d.pop_front();
					}
				}
			}
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
// Overlay minimalista que se dibuja en el PlayLayer
// ---------------------------------------------------------------------
class InputOverlay : public CCNode {
protected:
	static constexpr float KEY = 16.f;     // tamano de la tecla
	static constexpr float BAR_X = 22.f;   // inicio de la barra
	static constexpr float BAR_W = 150.f;  // ancho de la barra
	static constexpr float ROW_H = 22.f;   // alto de cada fila

	struct Row {
		int player = 0;
		int button = 0;
		CCNode* holder = nullptr;
		CCDrawNode* key = nullptr;
		CCDrawNode* bar = nullptr;
	};
	std::vector<Row> m_rows;
	CCLabelBMFont* m_info = nullptr;

	static void box(CCDrawNode* n, float x1, float y1, float x2, float y2,
		ccColor4F fill, float bw, ccColor4F border) {
		CCPoint v[4] = {
			CCPoint(x1, y1), CCPoint(x2, y1), CCPoint(x2, y2), CCPoint(x1, y2)
		};
		n->drawPolygon(v, 4, fill, bw, border);
	}

	void addRow(int player, int button, int idx) {
		Row r;
		r.player = player;
		r.button = button;

		r.holder = CCNode::create();
		r.holder->setPosition({0.f, -ROW_H * idx});
		this->addChild(r.holder);

		r.key = CCDrawNode::create();
		r.holder->addChild(r.key);

		r.bar = CCDrawNode::create();
		r.holder->addChild(r.bar);

		m_rows.push_back(r);
	}

	bool init(bool platformer) {
		if (!CCNode::init()) return false;

		int idx = 0;
		for (int p = 0; p < 2; p++) {
			if (platformer) {
				this->addRow(p, 2, idx++);
				this->addRow(p, 1, idx++);
				this->addRow(p, 3, idx++);
			} else {
				this->addRow(p, 1, idx++);
			}
		}

		m_info = CCLabelBMFont::create("", "chatFont.fnt");
		m_info->setAnchorPoint({0.f, 0.5f});
		m_info->setScale(0.7f);
		m_info->setPosition({0.f, 20.f});
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

	// Se llama cada frame y tambien desde el menu de configuracion.
	void refresh() {
		bool enabled = iv::S<bool>("enabled");
		this->setVisible(enabled);
		if (!enabled) return;

		float op = static_cast<float>(iv::S<double>("opacity"));
		float scale = static_cast<float>(iv::S<double>("scale"));
		float px = static_cast<float>(iv::S<double>("pos-x"));
		float py = static_cast<float>(iv::S<double>("pos-y"));
		float window = static_cast<float>(iv::S<double>("window"));
		bool showTimeline = iv::S<bool>("timeline");
		if (window < 0.1f) window = 0.1f;

		auto win = CCDirector::get()->getWinSize();
		this->setScale(scale);
		this->setPosition({win.width * px / 100.f, win.height * py / 100.f});

		auto pl = PlayLayer::get();
		bool dual = pl && pl->m_gameState.m_isDualMode;
		auto& st = iv::State::get();
		st.purge(window);
		double t = iv::now();

		for (auto& r : m_rows) {
			bool visible = r.player == 0 || dual;
			r.holder->setVisible(visible);
			if (!visible) continue;

			// Tecla
			bool pressed = st.down[r.player][r.button];
			r.key->clear();
			if (pressed) {
				box(r.key, 0.f, -KEY / 2, KEY, KEY / 2,
					ccc4f(1.f, 1.f, 1.f, 0.95f * op), 0.f, ccc4f(1.f, 1.f, 1.f, 0.95f * op));
			} else {
				box(r.key, 0.f, -KEY / 2, KEY, KEY / 2,
					ccc4f(0.f, 0.f, 0.f, 0.45f * op), 1.f, ccc4f(1.f, 1.f, 1.f, 0.35f * op));
			}

			// Barra de clicks / releases
			r.bar->setVisible(showTimeline);
			if (!showTimeline) continue;
			r.bar->clear();
			box(r.bar, BAR_X, -KEY / 2, BAR_X + BAR_W, KEY / 2,
				ccc4f(0.f, 0.f, 0.f, 0.35f * op), 1.f, ccc4f(1.f, 1.f, 1.f, 0.18f * op));

			for (auto& s : st.segs[r.player][r.button]) {
				double e = s.end < 0 ? t : s.end;
				float x1 = BAR_W * (1.f - static_cast<float>((t - s.start) / window));
				float x2 = BAR_W * (1.f - static_cast<float>((t - e) / window));
				x1 = std::max(0.f, x1);
				x2 = std::min(BAR_W, x2);
				if (x2 <= 0.f || x1 >= BAR_W) continue;
				if (x2 - x1 < 1.f) x2 = std::min(BAR_W, x1 + 1.f);

				float a = (s.end < 0 ? 0.95f : 0.8f) * op;
				box(r.bar, BAR_X + x1, -KEY / 2 + 2.f, BAR_X + x2, KEY / 2 - 2.f,
					ccc4f(1.f, 1.f, 1.f, a), 0.f, ccc4f(1.f, 1.f, 1.f, a));
			}
		}

		// Texto
		bool showCps = iv::S<bool>("show-cps");
		bool showTotal = iv::S<bool>("show-total");
		std::string txt;
		if (showCps) txt += fmt::format("{} CPS", st.cps());
		if (showTotal) {
			if (!txt.empty()) txt += "  -  ";
			txt += fmt::format("{}", st.total);
		}
		m_info->setString(txt.c_str());
		m_info->setOpacity(static_cast<GLubyte>(255.f * op * 0.85f));
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
	static constexpr SliderDef SLIDERS[5] = {
		{"opacity", "Opacidad", 0.1, 1.0},
		{"scale", "Tamano", 0.5, 2.0},
		{"pos-x", "Posicion X", 0.0, 100.0},
		{"pos-y", "Posicion Y", 0.0, 100.0},
		{"window", "Duracion barra", 0.5, 5.0},
	};
	CCLabelBMFont* m_labels[5] = {};

	static std::string labelText(int i, double v) {
		return fmt::format("{}: {:.2f}", SLIDERS[i].name, v);
	}

	bool init() {
		if (!Popup::init(300.f, 300.f)) return false;
		this->setTitle("Input Indicator");

		auto size = m_mainLayer->getContentSize();
		float cx = size.width / 2.f;

		// Sliders
		for (int i = 0; i < 5; i++) {
			float y = 228.f - 34.f * i;
			double cur = iv::S<double>(SLIDERS[i].key);

			m_labels[i] = CCLabelBMFont::create(labelText(i, cur).c_str(), "bigFont.fnt");
			m_labels[i]->setScale(0.35f);
			m_labels[i]->setPosition({cx, y + 12.f});
			m_mainLayer->addChild(m_labels[i]);

			auto slider = Slider::create(this, menu_selector(IVSettingsPopup::onSlider), 0.7f);
			slider->setPosition({cx, y - 4.f});
			slider->getThumb()->setTag(i);
			slider->setValue(static_cast<float>((cur - SLIDERS[i].min) / (SLIDERS[i].max - SLIDERS[i].min)));
			m_mainLayer->addChild(slider);
		}

		// Toggles (2 columnas)
		struct ToggleDef { char const* key; char const* name; };
		ToggleDef toggles[4] = {
			{"enabled", "Activado"},
			{"show-cps", "CPS"},
			{"show-total", "Clicks"},
			{"timeline", "Barra"},
		};
		for (int i = 0; i < 4; i++) {
			float x = (i % 2 == 0) ? 45.f : 170.f;
			float y = 50.f - 26.f * (i / 2);

			auto t = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(IVSettingsPopup::onToggle), 0.55f
			);
			t->toggle(iv::S<bool>(toggles[i].key));
			t->setTag(i);
			t->setPosition({x, y});
			m_buttonMenu->addChild(t);

			auto lbl = CCLabelBMFont::create(toggles[i].name, "bigFont.fnt");
			lbl->setScale(0.35f);
			lbl->setAnchorPoint({0.f, 0.5f});
			lbl->setPosition({x + 22.f, y});
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
		if (i < 0 || i > 4) return;

		double val = SLIDERS[i].min + thumb->getValue() * (SLIDERS[i].max - SLIDERS[i].min);
		val = std::round(val * 100.0) / 100.0;
		Mod::get()->setSettingValue<double>(SLIDERS[i].key, val);
		m_labels[i]->setString(labelText(i, val).c_str());
		this->applyNow();
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool on = !t->isToggled(); // el callback se llama antes de cambiar el estado
		char const* keys[4] = {"enabled", "show-cps", "show-total", "timeline"};
		int i = t->getTag();
		if (i < 0 || i > 3) return;
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

		if (down && !st.down[p][button]) {
			st.press(p, button);
			// Solo contamos clicks del jugador 1 para no duplicar en modo dual
			if (isPlayer1) {
				st.total++;
				st.clicks.push_back(iv::now());
			}
		} else if (!down && st.down[p][button]) {
			st.release(p, button);
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
			auto fallbackMenu = CCMenu::create();
			fallbackMenu->setPosition({win.width - 40.f, 40.f});
			fallbackMenu->addChild(btn);
			this->addChild(fallbackMenu);
		}
	}

	void onInputIndicator(CCObject*) {
		if (auto p = IVSettingsPopup::create()) p->show();
	}
};