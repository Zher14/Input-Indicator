#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <string>
#include <vector>

using namespace geode::prelude;

// ---------------------------------------------------------------------
// Utilidades, estilos, fuentes e idiomas
// ---------------------------------------------------------------------
namespace iv {
	static double now() {
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	static GLubyte u8(float v) {
		return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f));
	}

	template <class T>
	static T S(char const* key) {
		return Mod::get()->getSettingValue<T>(key);
	}

	// Ajustes guardados aparte (idioma, estilo, fuente)
	static int geti(char const* key, int def) {
		return Mod::get()->getSavedValue<int>(key, def);
	}
	static void seti(char const* key, int v) {
		Mod::get()->setSavedValue<int>(key, v);
	}

	// ---- Estilos de UI ----
	struct Style {
		float w;       // grosor de teclas y barra
		bool dark;     // fondo oscuro (true) o claro (false)
		float keyA;    // opacidad de la tecla sin pulsar
		float bgA;     // opacidad del fondo de la barra
		float outline; // contorno base del estilo
		float segA;    // opacidad de los bloques de la barra
		bool tint;     // tecla sin pulsar con el color de la barra
	};
	static Style const STYLES[5] = {
		{16.f, true,  0.50f, 0.45f, 1.f, 0.90f, false}, // Clasico
		{12.f, false, 0.14f, 0.07f, 0.f, 0.90f, false}, // Minimalista
		{12.f, true,  0.60f, 0.55f, 2.f, 1.00f, false}, // Neon
		{14.f, false, 0.35f, 0.00f, 0.f, 0.95f, true},  // Solido
		{ 6.f, false, 0.25f, 0.10f, 0.f, 0.90f, false}, // Delgado
	};
	static int styleIdx() { return std::clamp(geti("ui-style", 1), 0, 4); }

	// ---- Fuentes de Geometry Dash ----
	static char const* const FONTS[8] = {
		"bigFont.fnt", "chatFont.fnt", "goldFont.fnt",
		"gjFont01.fnt", "gjFont02.fnt", "gjFont03.fnt", "gjFont04.fnt", "gjFont05.fnt"
	};
	static char const* const FONT_NAMES[8] = {
		"bigFont", "chatFont", "goldFont",
		"gjFont01", "gjFont02", "gjFont03", "gjFont04", "gjFont05"
	};
	static float const FONT_SCALE[8] = {0.32f, 0.55f, 0.32f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f};
	static int fontIdx() { return std::clamp(geti("font", 1), 0, 7); }

	// ---- Idiomas: es, en, ru, zh, ja ----
	static char const* const LANG_NAMES[5] = {
		"Español", "English", "Русский", "中文", "日本語"
	};
	static int lang() { return std::clamp(geti("language", 0), 0, 4); }

	enum Tx {
		T_PAGE0, T_PAGE1, T_PAGE2,
		T_LANG, T_STYLE, T_FONT,
		T_ENABLED, T_CLICKS, T_TIMELINE, T_VERTICAL, T_FLIP, T_FADE,
		T_OPACITY, T_SCALE, T_TEXTSIZE, T_POSX, T_POSY, T_SPEED, T_LENGTH,
		T_OUTLINE, T_GAP, T_BARCOLOR, T_OUTCOLOR,
		T_COUNT
	};
	static char const* const TR[T_COUNT][5] = {
		/* T_PAGE0    */ {"Interfaz", "Interface", "Интерфейс", "界面", "インターフェース"},
		/* T_PAGE1    */ {"Barra", "Bar", "Полоса", "进度条", "バー"},
		/* T_PAGE2    */ {"Colores y texto", "Colors & text", "Цвета и текст", "颜色与文字", "色とテキスト"},
		/* T_LANG     */ {"Idioma", "Language", "Язык", "语言", "言語"},
		/* T_STYLE    */ {"Estilo", "Style", "Стиль", "样式", "スタイル"},
		/* T_FONT     */ {"Fuente", "Font", "Шрифт", "字体", "フォント"},
		/* T_ENABLED  */ {"Activo", "Enabled", "Вкл.", "启用", "有効"},
		/* T_CLICKS   */ {"Clicks", "Clicks", "Клики", "点击", "クリック"},
		/* T_TIMELINE */ {"Barra", "Bar", "Полоса", "进度条", "バー"},
		/* T_VERTICAL */ {"Vertical", "Vertical", "Вертикаль", "垂直", "縦"},
		/* T_FLIP     */ {"Invertir", "Flip", "Инверсия", "反转", "反転"},
		/* T_FADE     */ {"Desvanecer", "Fade", "Затухание", "渐隐", "フェード"},
		/* T_OPACITY  */ {"Opacidad", "Opacity", "Прозрачность", "不透明度", "不透明度"},
		/* T_SCALE    */ {"Tamaño", "Size", "Размер", "大小", "サイズ"},
		/* T_TEXTSIZE */ {"Texto", "Text", "Текст", "文字", "テキスト"},
		/* T_POSX     */ {"Posición X", "Position X", "Позиция X", "位置 X", "位置 X"},
		/* T_POSY     */ {"Posición Y", "Position Y", "Позиция Y", "位置 Y", "位置 Y"},
		/* T_SPEED    */ {"Velocidad", "Speed", "Скорость", "速度", "速度"},
		/* T_LENGTH   */ {"Largo", "Length", "Длина", "长度", "長さ"},
		/* T_OUTLINE  */ {"Contorno", "Outline", "Контур", "轮廓", "輪郭"},
		/* T_GAP      */ {"Separación", "Gap", "Отступ", "间距", "間隔"},
		/* T_BARCOLOR */ {"Color de barra", "Bar color", "Цвет полосы", "条形颜色", "バーの色"},
		/* T_OUTCOLOR */ {"Color de contorno", "Outline color", "Цвет контура", "轮廓颜色", "輪郭の色"},
	};
	static char const* tr(int k) { return TR[k][lang()]; }

	static char const* const STYLE_NAMES[5][5] = {
		{"Clásico", "Classic", "Классика", "经典", "クラシック"},
		{"Minimalista", "Minimal", "Минимализм", "极简", "ミニマル"},
		{"Neón", "Neon", "Неон", "霓虹", "ネオン"},
		{"Sólido", "Solid", "Сплошной", "实心", "ソリッド"},
		{"Delgado", "Thin", "Тонкий", "细线", "シン"},
	};

	// ---- Colores rapidos ----
	static ccColor3B const SWATCHES[8] = {
		{255, 255, 255}, {70, 235, 255}, {90, 255, 130}, {255, 225, 70},
		{255, 80, 80}, {190, 100, 255}, {80, 140, 255}, {0, 0, 0}
	};

	// ---- Estado de los inputs ----
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
}

// ---------------------------------------------------------------------
// Overlay: teclas + barra de clicks/releases (vertical u horizontal)
// ---------------------------------------------------------------------
class InputOverlay : public CCNode {
protected:
	static constexpr float PGAP = 12.f;         // espacio extra entre jugadores
	static constexpr float BASE_WINDOW = 2.f;   // segundos visibles a velocidad 1x

	struct Row {
		int player = 0;
		int button = 0;
		int col = 0;
		CCNode* holder = nullptr;
		CCLayerColor* bg = nullptr;
		CCNode* segLayer = nullptr;
		CCLayerColor* key = nullptr;
		std::array<CCLayerColor*, 4> barFrame{};
		std::array<CCLayerColor*, 4> keyFrame{};
		std::vector<CCLayerColor*> pool;
	};
	std::vector<Row> m_rows;
	CCLabelBMFont* m_info = nullptr;
	std::string m_fontName = "chatFont.fnt";

	std::array<CCLayerColor*, 4> makeFrame(CCNode* parent) {
		std::array<CCLayerColor*, 4> f{};
		for (int i = 0; i < 4; i++) {
			f[i] = CCLayerColor::create(ccc4(255, 255, 255, 255), 1.f, 1.f);
			parent->addChild(f[i]);
		}
		return f;
	}

	static void setFrame(std::array<CCLayerColor*, 4>& e, float x, float y, float w, float h,
		float t, ccColor3B c, GLubyte o, bool show) {
		for (auto n : e) n->setVisible(show);
		if (!show) return;
		float d[4][4] = {
			{x - t, y - t, w + 2 * t, t},
			{x - t, y + h, w + 2 * t, t},
			{x - t, y, t, h},
			{x + w, y, t, h},
		};
		for (int i = 0; i < 4; i++) {
			e[i]->setContentSize({d[i][2], d[i][3]});
			e[i]->setPosition({d[i][0], d[i][1]});
			e[i]->setColor(c);
			e[i]->setOpacity(o);
		}
	}

	void addRow(int player, int button, int col) {
		Row r;
		r.player = player;
		r.button = button;
		r.col = col;

		r.holder = CCNode::create();
		this->addChild(r.holder);

		r.bg = CCLayerColor::create(ccc4(255, 255, 255, 255), 10.f, 10.f);
		r.holder->addChild(r.bg);

		r.barFrame = this->makeFrame(r.holder);

		r.segLayer = CCNode::create();
		r.holder->addChild(r.segLayer);

		r.key = CCLayerColor::create(ccc4(255, 255, 255, 255), 10.f, 10.f);
		r.holder->addChild(r.key);

		r.keyFrame = this->makeFrame(r.holder);

		m_rows.push_back(r);
	}

	CCLayerColor* getSeg(Row& r, size_t i) {
		while (r.pool.size() <= i) {
			auto n = CCLayerColor::create(ccc4(255, 255, 255, 255), 10.f, 1.f);
			r.segLayer->addChild(n);
			r.pool.push_back(n);
		}
		return r.pool[i];
	}

	bool init(bool platformer) {
		if (!CCNode::init()) return false;

		int col = 0;
		for (int p = 0; p < 2; p++) {
			if (platformer) {
				this->addRow(p, 2, col++);
				this->addRow(p, 1, col++);
				this->addRow(p, 3, col++);
			} else {
				this->addRow(p, 1, col++);
			}
		}

		m_info = CCLabelBMFont::create("", "chatFont.fnt");
		m_info->setAnchorPoint({0.f, 0.5f});
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
		float gap = static_cast<float>(iv::S<double>("gap"));
		bool showTimeline = iv::S<bool>("timeline");
		bool vertical = iv::S<bool>("vertical");
		bool flip = iv::S<bool>("flip");
		bool fade = iv::S<bool>("fade");
		ccColor3B col = iv::S<ccColor3B>("bar-color");
		ccColor3B ocol = iv::S<ccColor3B>("outline-color");

		auto const& stl = iv::STYLES[iv::styleIdx()];
		float w = stl.w;
		float base = w + 3.f;
		float outl = stl.outline + static_cast<float>(iv::S<double>("outline-size"));

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

		ccColor3B black = {0, 0, 0};
		ccColor3B white = {255, 255, 255};
		ccColor3B idleCol = stl.dark ? black : (stl.tint ? col : white);

		for (auto& r : m_rows) {
			bool visible = r.player == 0 || dual;
			r.holder->setVisible(visible);
			if (!visible) continue;

			// Orientacion: vertical = filas lado a lado; horizontal = rotado 90 grados
			float off = r.col * (w + gap) + r.player * PGAP;
			if (vertical) {
				r.holder->setPosition({off, 0.f});
				r.holder->setRotation(0.f);
			} else {
				r.holder->setPosition({0.f, -off});
				r.holder->setRotation(90.f);
			}
			r.holder->setScaleY(flip ? -1.f : 1.f);

			// Tecla
			bool pressed = st.down[r.player][r.button];
			r.key->setContentSize({w, w});
			r.key->setPosition({0.f, 0.f});
			r.key->setColor(pressed ? col : idleCol);
			r.key->setOpacity(iv::u8((pressed ? 0.95f : stl.keyA) * 255.f * op));
			setFrame(r.keyFrame, 0.f, 0.f, w, w, outl, ocol, iv::u8(255.f * op), outl > 0.f);

			// Barra
			setFrame(r.barFrame, 0.f, base, w, length, outl, ocol, iv::u8(255.f * op),
				showTimeline && outl > 0.f);
			r.segLayer->setVisible(showTimeline);
			r.bg->setVisible(showTimeline && stl.bgA > 0.f);
			if (!showTimeline) continue;

			r.bg->setContentSize({w, length});
			r.bg->setPosition({0.f, base});
			r.bg->setColor(stl.dark ? black : white);
			r.bg->setOpacity(iv::u8(stl.bgA * 255.f * op));

			size_t n = 0;
			for (auto& s : st.segs[r.player][r.button]) {
				double e = s.end < 0 ? t : s.end;
				float y1 = length * static_cast<float>((t - e) / window);
				float y2 = length * static_cast<float>((t - s.start) / window);
				y1 = std::max(0.f, y1);
				y2 = std::min(length, y2);
				if (y1 >= length || y2 <= 0.f) continue;
				if (y2 - y1 < 1.f) y2 = std::min(length, y1 + 1.f);

				float a = stl.segA * (s.end < 0 ? 1.f : 0.85f);
				if (fade) {
					float frac = ((y1 + y2) * 0.5f) / length;
					a *= 1.f - 0.85f * std::clamp(frac, 0.f, 1.f);
				}

				auto node = this->getSeg(r, n++);
				node->setContentSize({w, y2 - y1});
				node->setPosition({0.f, base + y1});
				node->setColor(col);
				node->setOpacity(iv::u8(a * 255.f * op));
				node->setVisible(true);
			}
			for (; n < r.pool.size(); n++) r.pool[n]->setVisible(false);
		}

		// Texto (CPS y clicks totales) con la fuente elegida
		int fi = iv::fontIdx();
		if (m_fontName != iv::FONTS[fi]) {
			m_info->setFntFile(iv::FONTS[fi]);
			m_fontName = iv::FONTS[fi];
		}
		m_info->setScale(iv::FONT_SCALE[fi] * static_cast<float>(iv::S<double>("text-scale")));

		bool above = !vertical || flip;
		m_info->setPosition({0.f, above ? 10.f : -10.f});
		m_info->setAnchorPoint({(!vertical && flip) ? 1.f : 0.f, 0.5f});

		bool showCps = iv::S<bool>("show-cps");
		bool showTotal = iv::S<bool>("show-total");
		std::string txt;
		if (showCps) txt += fmt::format("{} CPS", st.cps());
		if (showTotal) {
			if (!txt.empty()) txt += "  ";
			txt += fmt::format("{}", st.total);
		}
		m_info->setString(txt.c_str());
		m_info->setOpacity(iv::u8(255.f * op * 0.8f));
		m_info->setVisible(!txt.empty());
	}
};

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

		m_