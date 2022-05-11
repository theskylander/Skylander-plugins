#pragma once

#include <iostream>

#include "Skylander.hpp"
#include "componentlibrary.hpp"

// taken from UI.hpp which became AHCommon.hpp from Amalgamated Harmonics

struct ParamEvent {
	
	ParamEvent(int t, int i, float v) : pType(t), pId(i), value(v) {}
	
	int pType;
	int pId;
	float value;

};

struct SkylanderModule : rack::Module {

  SkylanderModule(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	int stepX = 0;
	
	bool debugFlag = false;
	
	inline bool debugEnabled() {
		return debugFlag;
	}
	
	bool receiveEvents = false;
	int keepStateDisplay = 0;
	std::string paramState = ">";
	
	virtual void receiveEvent(ParamEvent e) {
		paramState = ">";
		keepStateDisplay = 0;
	}
	
	void process(const ProcessArgs& args) override {
		
		stepX++;
	
		// Once we start stepping, we can process events
		receiveEvents = true;
		// Timeout for display
		keepStateDisplay++;
		if (keepStateDisplay > 50000) {
			paramState = ">"; 
		}
		
	}
	
};

struct StateDisplay : TransparentWidget {
	
	SkylanderModule *module;
	int frame = 0;
        std::string _fontPath;

        StateDisplay() : 
	  _fontPath(asset::plugin(pluginInstance, "res/EurostileBold.ttf"))
  {
  }

	void draw(const DrawArgs& args) override {
	
	  std::shared_ptr<Font> font = APP->window->loadFont(_fontPath);
	  if(font) {
	    Vec pos = Vec(0, 15);

	    nvgFontSize(args.vg, 16);
	    nvgFontFaceId(args.vg, font->handle);
	    nvgTextLetterSpacing(args.vg, -1);
	    
	    nvgFillColor(args.vg, nvgRGBA(255, 0, 0, 0xff));
	    
	    char text[128];
	    snprintf(text, sizeof(text), "%s", module->paramState.c_str());
	    nvgText(args.vg, pos.x + 10, pos.y + 5, text, NULL);

	  }

	}
	
};

struct SkylanderParamWidget { // it's a mix-in

	int pType = -1; // Should be set by ste<>(), but if not this allows us to catch pwidgers we are not interested in
	int pId;
	SkylanderModule *mod = NULL;
	
	virtual ParamEvent generateEvent(float value) {
		return ParamEvent(pType,pId,value);
	};
		
	template <typename T = SkylanderParamWidget>
	static void set(T *param, int pType, int pId) {
		param->pType = pType;
		param->pId = pId;
	}

};

// Not going to monitor buttons
struct SkylanderButton : SVGSwitch {
	SkylanderButton() {
  	        momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/AHButton.svg")));
	}	
};

struct SkylanderKnob : RoundKnob, SkylanderParamWidget {
                void onChange(const rack::event::Change &e) override { 
		// One off cast, don't want to subclass from ParamWidget, so have to grab it here
		if (!SkylanderParamWidget::mod) {
		  SkylanderParamWidget::mod = static_cast<SkylanderModule *>(getParamQuantity()->module);
		}
		SkylanderParamWidget::mod->receiveEvent(generateEvent(getParamQuantity()->getValue()));
		RoundKnob::onChange(e);
	}
};

struct SkylanderKnobSnap : SkylanderKnob {
	SkylanderKnobSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/AHKnob.svg")));
	}
};

struct SkylanderKnobNoSnap : SkylanderKnob {
	SkylanderKnobNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/AHKnob.svg")));
	}
};


struct SkylanderTrimpotSnap : SkylanderKnob {
	SkylanderTrimpotSnap() {
		snap = true;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/AHTrimpot.svg")));
	}
};

struct SkylanderTrimpotNoSnap : SkylanderKnob {
	SkylanderTrimpotNoSnap() {
		snap = false;
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ComponentLibrary/AHTrimpot.svg")));
	}
};


struct SonusBigKnob : SVGKnob
{
    SonusBigKnob()
    {
        box.size = Vec(54, 54);
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/bigknob.svg")));
    }
};

struct SonusBigSnapKnob : SonusBigKnob
{
    SonusBigSnapKnob()
    {
        snap = true;
    }
};

struct SonusBigKnob26 : SVGKnob
{
    SonusBigKnob26()
    {
        box.size = Vec(26, 26);
        minAngle = -0.75 * M_PI;
        maxAngle = 0.75 * M_PI;
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/knob_26px.svg")));
    }
};

struct SonusBigSnapKnob26 : SonusBigKnob26
{
    SonusBigSnapKnob26()
    {
        snap = true;
    }
};

//BogAudio knob16:
struct BGKnob : RoundKnob {
	std::string _svgBase;

	BGKnob(const char* svgBase, int dim);

	void redraw();
};

struct Knob16 : BGKnob {
	Knob16();
};


struct PJ301RPort : SVGPort {
	PJ301RPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301R.svg")));
		// setSVG(APP->window->loadSvg(asset::system("res/ComponentLibrary/PJ301R.svg")));
	}
};

struct PJ301GPort : SVGPort {
	PJ301GPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301G.svg")));
	}
};

struct PJ301BPort : SVGPort {
	PJ301BPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301B.svg")));
	}
};

struct PJ301YPort : SVGPort {
	PJ301YPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301Y.svg")));
	}
};

struct PJ301BBPort : SVGPort {
	PJ301BBPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301BB.svg")));
	}
};

struct PJ301WPort : SVGPort {
	PJ301WPort() {
	  setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComponentLibrary/PJ301W.svg")));
	}
};

template <class TChoice>
struct Grid84MidiWidget : MidiDisplay {
	LedDisplaySeparator* hSeparators[6];
	LedDisplaySeparator* vSeparators[14];
	TChoice* choices[14][6];

	template <class TModule>
	void setModule(TModule* module) {
		Vec pos = channelChoice->box.getBottomLeft();
		// Add vSeparators
		for (int x = 1; x < 14; x++) {
			vSeparators[x] = createWidget<LedDisplaySeparator>(pos);
			vSeparators[x]->box.pos.x = box.size.x / 14 * x;
			addChild(vSeparators[x]);
		}
		// Add hSeparators and choice widgets
		for (int y = 0; y < 6; y++) {
			hSeparators[y] = createWidget<LedDisplaySeparator>(pos);
			hSeparators[y]->box.size.x = box.size.x;
			addChild(hSeparators[y]);
			for (int x = 0; x < 14; x++) {
				choices[x][y] = new TChoice;
				choices[x][y]->box.pos = pos;
				choices[x][y]->setId(14 * y + x);
				choices[x][y]->box.size.x = box.size.x / 6;
				choices[x][y]->box.pos.x = box.size.x / 14 * x;
				choices[x][y]->setModule(module);
				addChild(choices[x][y]);
			}
			pos = choices[0][y]->box.getBottomLeft();
		}
		for (int x = 1; x < 14; x++) {
			vSeparators[x]->box.size.y = pos.y - vSeparators[x]->box.pos.y;
		}
	}
};

template <class TModule>
struct CcChoice : LedDisplayChoice {
	TModule* module;
	int id;
	int focusCc;

	CcChoice() {
		box.size.y = mm2px(6.666);
		textOffset.y -= 4;
	}

	void setModule(TModule* module) {
		this->module = module;
	}

	void setId(int id) {
		this->id = id;
	}

	void step() override {
		int cc;
		if (!module) {
			cc = id;
		}
		else if (module->learningId == id) {
			cc = focusCc;
			color.a = 0.5;
		}
		else {
			cc = module->learnedCcs[id];
			color.a = 1.0;

			// Cancel focus if no longer learning
			if (APP->event->getSelectedWidget() == this)
				APP->event->setSelected(NULL);
		}

		// Set text
		if (cc < 0)
			text = "--";
		else
			text = string::f("%d", cc);
	}

	void onSelect(const event::Select& e) override {
		if (!module)
			return;
		module->learningId = id;
		focusCc = -1;
		e.consume(this);
	}

	void onDeselect(const event::Deselect& e) override {
		if (!module)
			return;
		if (module->learningId == id) {
			if (0 <= focusCc && focusCc < 128) {
				module->learnedCcs[id] = focusCc;
			}
			module->learningId = -1;
		}
	}

	void onSelectText(const event::SelectText& e) override {
		int c = e.codepoint;
		if ('0' <= c && c <= '9') {
			if (focusCc < 0)
				focusCc = 0;
			focusCc = focusCc * 10 + (c - '0');
		}
		if (focusCc >= 128)
			focusCc = -1;
		e.consume(this);
	}

	void onSelectKey(const event::SelectKey& e) override {
		if ((e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) && e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0) {
			event::Deselect eDeselect;
			onDeselect(eDeselect);
			APP->event->selectedWidget = NULL;
			e.consume(this);
		}
	}
};

//--------------------------------------------------------------------------------------------------------

template <class TChoice>
struct Grid2MidiWidget : MidiDisplay {
};

//-------------------------------------------------------------------------------------


template <typename TLightBase = RedLight>
struct LEDLightSliderFixed : LEDLightSlider<TLightBase> {
	LEDLightSliderFixed() {
		this->setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderHandle.svg")));
	}
};


struct UI {

	enum UIElement {
		KNOB = 0,
		PORT,
		BUTTON,
		LIGHT,
		TRIMPOT
	};
	
	float Y_KNOB[2]    = {50.8, 56.0}; // w.r.t 22 = 28.8 from bottom
	float Y_PORT[2]    = {49.7, 56.0}; // 27.7
	float Y_BUTTON[2]  = {53.3, 56.0}; // 31.3 
	float Y_LIGHT[2]   = {57.7, 56.0}; // 35.7
	float Y_TRIMPOT[2] = {52.8, 56.0}; // 30.8

	float Y_KNOB_COMPACT[2]     = {30.1, 35.0}; // Calculated relative to  PORT=29 and the deltas above
	float Y_PORT_COMPACT[2]     = {29.0, 35.0};
	float Y_BUTTON_COMPACT[2]   = {32.6, 35.0};
	float Y_LIGHT_COMPACT[2]    = {37.0, 35.0};
	float Y_TRIMPOT_COMPACT[2]  = {32.1, 35.0};
	
	float X_KNOB[2]     = {12.5, 48.0}; // w.r.t 6.5 = 6 from left
	float X_PORT[2]     = {11.5, 48.0}; // 5
	float X_BUTTON[2]   = {14.7, 48.0}; // 8.2
	float X_LIGHT[2]    = {19.1, 48.0}; // 12.6
	float X_TRIMPOT[2]  = {14.7, 48.0}; // 8.2

	float X_KNOB_COMPACT[2]     = {21.0, 35.0}; // 15 + 6, see calc above
	float X_PORT_COMPACT[2]     = {20.0, 35.0}; // 15 + 5
	float X_BUTTON_COMPACT[2]   = {23.2, 35.0}; // 15 + 8.2
	float X_LIGHT_COMPACT[2]    = {27.6, 35.0}; // 15 + 12.6
	float X_TRIMPOT_COMPACT[2]  = {23.2, 35.0}; // 15 + 8.2
	

	Vec getPosition(int type, int xSlot, int ySlot, bool xDense, bool yDense);
	
	/* From the numerical key on a keyboard (0 = C, 11 = B), spacing in px between white keys and a starting x and Y coordinate for the C key (in px)
	* calculate the actual X and Y coordinate for a key, and the scale note to which that key belongs (see Midi note mapping)
	* http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html */
	void calculateKeyboard(int inKey, float spacing, float xOff, float yOff, float *x, float *y, int *scale);
	
};


