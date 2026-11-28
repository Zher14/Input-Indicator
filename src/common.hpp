#pragma once
#include <Geode/Geode.hpp>
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