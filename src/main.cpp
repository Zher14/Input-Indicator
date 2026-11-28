#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include "popup.hpp"

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