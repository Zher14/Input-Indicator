#pragma once
#include "common.hpp"

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