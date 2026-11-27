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

	static GLubyte u8(float v) {
		return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f));
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
// Overlay: teclas + barra de clicks/releases (vertical u horizontal)
// ---------------------------------------------------------------------
class InputOverlay : public CCNode {
protected:
	static constexpr float W = 12.f;            // grosor de tecla / barra
	static constexpr float GAP = 5.f;           // espacio entre filas
	static constexpr float PGAP = 12.f;         // espacio extra entre jugadores
	static constexpr float BASE = W + 3.f;      // donde empieza la barra
	static constexpr float BASE_WINDOW = 2.f;   // segundos visibles a velocidad 1x

	struct Row {
		int player = 0;
		int button = 0;
		float off = 0.f;
		CCNode* holder = nullptr;
		CCLayerColor* bg = nullptr;
		CCNode* segLayer = nullptr;
		CCLayerColor* key = nullptr;
		std::vector<CCLayerColor*> pool;
	};
	std::vector<Row> m_rows;
	CCLabelBMFont* m_info = nullptr;

	void addRow(int player, int button, float off) {
		Row r;
		r.player = player;
		r.button = button;
		r.off = off;

		r.holder = CCNode::create();
		this->addChild(r.holder);

		r.bg = CCLayerColor::create(ccc4(255, 255, 255, 255), W, 10.f);
		r.holder->addChild(r.bg);

		r.segLayer = CCNode::create();
		r.holder->addChild(r.segLayer);

		r.key = CCLayerColor::create(ccc4(255, 255, 255, 255), W, W);
		r.holder->addChild(r.key);

		m_rows.push_back(r);
	}

	CCLayerColor* getSeg(Row& r, size_t i) {
		while (r.pool.size() <= i) {
			auto n = CCLayerColor::create(ccc4(255, 255, 255, 255), W, 1.f);
			r.segLayer->addChild(n);
			r.pool.push_back(n);
		}
		return r.pool[i];
	}

	bool init(bool platformer) {
		if (!CCNode::init()) return false;

		float x = 0.f;
		for (int p = 0; p < 2; p++) {
			if (platformer) {
				this->addRow(p, 2, x); x += W + GAP;
				this->addRow(p, 1, x); x += W + GAP;
				this->addRow(p, 3, x); x += W + GAP;
			} else {
				this->addRow(p, 1, x); x += W + GAP;
			}
			x += PGAP;
		}

		m_info = CCLabelBMFont::create("", "chatFont.fnt");
		m_info->setAnchorPoint({0.f, 0.5f});
		m_info->setScale(0.55f);
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
		float speed = static_cast<float>(iv::S<double>("speed"));
		float length = static_cast<float>(iv::S<double>("length"));
		bool showTimeline = iv::S<bool>("timeline");
		bool vertical = iv::S<bool>("vertical");
		bool flip = iv::S<bool>("flip");
		ccColor3B col = iv::S<ccColor3B>("bar-color");

		if (speed < 0.05f) speed = 0.05f;
		if (length < 20.f) length = 20.f;
		float window = BASE_WINDOW / speed;

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

			// Orientacion: en vertical las filas van de lado a lado;
			// en horizontal se rota 90 grados y van hacia abajo.
			if (vertical) {
				r.holder->setPosition({r.off, 0.f});
				r.holder->setRotation(0.f);
			} else {
				r.holder->setPosition({0.f, -r.off});
				r.holder->setRotation(90.f);
			}
			r.holder->setScaleY(flip ? -1.f : 1.f);

			// Tecla
			bool pressed = st.down[r.player][r.button];
			r.key->setColor(pressed ? col : ccColor3B{255, 255, 255});
			r.key->setOpacity(iv::u8((pressed ? 0.95f : 0.14f) * 255.f * op));

			// Barra
			r.segLayer->setVisible(showTimeline);
			r.bg->setVisible(showTimeline);
			if (!showTimeline) continue;

			r.bg->setContentSize({W, length});
			r.bg->setPosition({0.f, BASE});
			r.bg->setColor({255, 255, 255});
			r.bg->setOpacity(iv::u8(0.07f * 255.f * op));

			size_t n = 0;
			for (auto& s : st.segs[r.player][r.button]) {
				double e = s.end < 0 ? t : s.end;
				float y1 = length * static_cast<float>((t - e) / window);
				float y2 = length * static_cast<float>((t - s.start) / window);
				y1 = std::max(0.f, y1);
				y2 = std::min(length, y2);
				if (y1 >= length || y2 <= 0.f) continue;
				if (y2 - y1 < 1.f) y2 = std::min(length, y1 + 1.f);

				auto node = this->getSeg(r, n++);
				node->setContentSize({W, y2 - y1});
				node->setPosition({0.f, BASE + y1});
				node->setColor(col);
				node->setOpacity(iv::u8((s.end < 0 ? 0.95f : 0.8f) * 255.f * op));
				node->setVisible(true);
			}
			for (; n < r.pool.size(); n++) r.pool[n]->setVisible(false);
		}

		// Texto pequeno (arriba o abajo segun la orientacion)
		bool above = !vertical || flip;
		m_info->setPosition({0.f, above ? 9.f : -9.f});
		m_info->setAnchorPoint({(!vertical && flip) ? 1.f : 0.f, 0.5f});

		bool showCps = iv::S<bool>("show-cps");
		bool showTotal = iv::S<bool>("show-total");
		std::string txt;
		if (showCps) txt += fmt::format("{} cps", st.cps());
		if (showTotal) {
			if (!txt.empty()) txt += "  ";
			txt += fmt::format("{}", st.total);
		}
		m_info->setString(txt.c_str());
		m_info->setOpacity(iv::u8(255.f * op * 0.7f));
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
	static constexpr SliderDef SLIDERS[6] = {
		{"opacity", "Opacidad", 0.1, 1.0},
		{"scale", "Tamano", 0.5, 2.0},
		{"pos-x", "Posicion X", 0.0, 100.0},
		{"pos-y", "Posicion Y", 0.0, 100.0},
		{"speed", "Velocidad barra", 0.25, 4.0},
		{"length", "Largo barra", 60.0, 240.0},
	};
	CCLabelBMFont* m_labels[6] = {};

	static std::string labelText(int i, double v) {
		return fmt::format("{}: {:.2f}", SLIDERS[i].name, v);
	}

	static ccColor3B swatchColor(int i) {
		static const ccColor3B cols[7] = {
			{255, 255, 255}, {70, 235, 255}, {90, 255, 130}, {255, 225, 70},
			{255, 80, 80}, {190, 100, 255}, {80, 140, 255}
		};
		return cols[i];
	}

	bool init() {
		if (!Popup::init(300.f, 310.f)) return false;
		this->setTitle("Input Indicator");

		auto size = m_mainLayer->getContentSize();
		float cx = size.width / 2.f;

		// Sliders
		for (int i = 0; i < 6; i++) {
			float y = 258.f - 30.f * i;
			double cur = iv::S<double>(SLIDERS[i].key);

			m_labels[i] = CCLabelBMFont::create(labelText(i, cur).c_str(), "bigFont.fnt");
			m_labels[i]->setScale(0.3f);
			m_labels[i]->setPosition({cx, y + 10.f});
			m_mainLayer->addChild(m_labels[i]);

			auto slider = Slider::create(this, menu_selector(IVSettingsPopup::onSlider), 0.7f);
			slider->setPosition({cx, y - 4.f});
			slider->getThumb()->setTag(i);
			slider->setValue(static_cast<float>((cur - SLIDERS[i].min) / (SLIDERS[i].max - SLIDERS[i].min)));
			m_mainLayer->addChild(slider);
		}

		// Colores rapidos
		auto colorLbl = CCLabelBMFont::create("Color", "bigFont.fnt");
		colorLbl->setScale(0.3f);
		colorLbl->setAnchorPoint({0.f, 0.5f});
		colorLbl->setPosition({24.f, 82.f});
		m_mainLayer->addChild(colorLbl);

		for (int i = 0; i < 7; i++) {
			auto holder = CCNode::create();
			holder->setContentSize({20.f, 20.f});
			auto sq = CCLayerColor::create(ccc4(255, 255, 255, 255), 20.f, 20.f);
			sq->setColor(swatchColor(i));
			holder->addChild(sq);

			auto btn = CCMenuItemSpriteExtra::create(
				holder, this, menu_selector(IVSettingsPopup::onSwatch)
			);
			btn->setTag(i);
			btn->setPosition({96.f + 28.f * i, 82.f});
			m_buttonMenu->addChild(btn);
		}

		// Toggles (3 columnas)
		struct ToggleDef { char const* key; char const* name; };
		ToggleDef toggles[6] = {
			{"enabled", "Activo"},
			{"show-cps", "CPS"},
			{"show-total", "Clicks"},
			{"timeline", "Barra"},
			{"vertical", "Vertical"},
			{"flip", "Invertir"},
		};
		for (int i = 0; i < 6; i++) {
			float x = 28.f + 84.f * (i % 3);
			float y = 52.f - 26.f * (i / 3);

			auto t = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(IVSettingsPopup::onToggle), 0.5f
			);
			t->toggle(iv::S<bool>(toggles[i].key));
			t->setTag(i);
			t->setPosition({x, y});
			m_buttonMenu->addChild(t);

			auto lbl = CCLabelBMFont::create(toggles[i].name, "bigFont.fnt");
			lbl->setScale(0.3f);
			lbl->setAnchorPoint({0.f, 0.5f});
			lbl->setPosition({x + 18.f, y});
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
		if (i < 0 || i > 5) return;

		double val = SLIDERS[i].min + thumb->getValue() * (SLIDERS[i].max - SLIDERS[i].min);
		val = std::round(val * 100.0) / 100.0;
		Mod::get()->setSettingValue<double>(SLIDERS[i].key, val);
		m_labels[i]->setString(labelText(i, val).c_str());
		this->applyNow();
	}

	void onSwatch(CCObject* sender) {
		int i = static_cast<CCNode*>(sender)->getTag();
		if (i < 0 || i > 6) return;
		Mod::get()->setSettingValue<ccColor3B>("bar-color", swatchColor(i));
		this->applyNow();
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool on = !t->isToggled(); // el callback se llama antes de cambiar el estado
		char const* keys[6] = {"enabled", "show-cps", "show-total", "timeline", "vertical", "flip"};
		int i = t->getTag();
		if (i < 0 || i > 5) return;
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