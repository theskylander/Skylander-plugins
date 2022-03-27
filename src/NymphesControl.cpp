#include "Skylander.hpp"
#include "UI.hpp"
#include <climits>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <deque>
#include <vector>

#include "osdialog.h" // needed for filename dialogue
#include <iomanip> // for setw

struct CCMidiOutput : midi::Output {
	int lastValues[128];

	CCMidiOutput() {
		reset();
	}

	void reset() {
		for (int n = 0; n < 128; n++) {
			lastValues[n] = -1;
		}
	}

	void setValue(int value, int cc) {
		if (value == lastValues[cc])
			return;
		lastValues[cc] = value;
		// CC
		midi::Message m;
		m.setStatus(0xb);
		m.setNote(cc);
		m.setValue(value);
		sendMessage(m);
	}

	void sendProgram(uint8_t program) {
		// setProgram(program);
		midi::Message m;
		m.setStatus(0xC);
		m.setNote(program);
		m.setValue(0x0);
		sendMessage(m);
	}
  
  
};


struct NymphesControl : Module {
	enum ParamIds {
		ENUMS(CONTROLLERS, 74),
		LFO1_TYPE,
		LFO2_TYPE,
		MOD_SOURCE1,
		LFO1_SYNC,
		LFO2_SYNC,
		SUSTAIN,
		LEGATO,
		PLAYMODE,
		LOAD,
		SAVE,
		ENUMS(MOD_TYPE, 4),
		PROGRAM_BANK,
		PROGRAM_KNOB,
		PROGRAM_SEND,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(CC_INPUTS, 74),
		CV_PC,
		CV_PC_SEND,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CTRL_LIGHTS, 74),
		ENUMS(LFO1_TYPE_LIGHTS, 4),
		ENUMS(LFO2_TYPE_LIGHTS, 4),
		ENUMS(MOD_SOURCE_LIGHTS, 4),
		LFO1_SYNC_LIGHT,
		LFO2_SYNC_LIGHT,
		SUSTAIN_LIGHT,
		LEGATO_LIGHT,
		ENUMS(PLAYMODE_LIGHTS, 6),
		ENUMS(PC_BANK_LIGHTS,2),
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;
	int8_t values_in[128];
	int learnedCcs[82];
	dsp::ExponentialFilter valueFilters[38];
	dsp::ExponentialFilter button_valueFilters[8];
	dsp::ExponentialFilter mod_valueFilters[4][36];
        int valueFilters_last[38];
        int button_valueFilters_last[8];
        int mod_valueFilters_last[4][36];
        int cc_values_last[74];
        int controller_values_last[38];
        int button_controller_value_last[8];
        int mod_controller_values_last[4][36];
        int last_value_out[38];
        int last_button_value_out[8];
        int mod_value[4][36];
        int last_mod_value[4][36];

	CCMidiOutput midiOutput;
	float rateLimiterPhase = 0.f;
        int value_out = 0;
        bool value_changed = false;

        int button_settings[7] = {0,0,0,0,0,0,0};
        int button_last[7] = {0,0,0,0,0,0,0};
        int mod_button_last[4] = {0,0,0,0};
  
        int number_lights_per_button[7] = {3,3,3,1,1,1,1};
        bool load_patch = false;
        bool save_patch = false;
        int load_last_value = 0;
        int save_last_value = 0;
        std::string lastPath;
        std::string NYM_FILTERS_load = "Nymphes Patch file load (.nym):nym";
        std::string NYM_FILTERS_save = "Nymphes Patch file save (.nym):nym";

        int target_program;
        char current_bank;
        int current_program;
        int current_values[38];
        int mod_current_values[4][36];
        int mod_display_values[36];
        int mod_src = 0;
        int mod_src_last = 4;
        int count1 = 0;
        int count2 = 0;
        int slow_control = 20;

        bool button_pressed = false;

        bool factory;
        bool factory_last;
        int pc_bank_last = 0;
        int program_change_last = 0;
        dsp::SchmittTrigger sendPCTrigger;
  
	NymphesControl() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 74; i++) {
		        configParam(NymphesControl::CONTROLLERS+i, 0, 128, 9*(i%14), "");
		}
		for (int i = 0; i < 38; i++) {
		  valueFilters[i].setTau(1 / 500.f); //x
		}
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 36; j++) {
			  mod_valueFilters[i][j].setTau(1 / 500.f); //x controllers and cc_inputs: (28-55, 64-67)
			}
		}
		for (int i = 0; i < 8; i++) {
		  button_valueFilters[i].setTau(1 / 500.f); //x
		}
		for (int i = 0; i < 4; i++) {
		  configParam(NymphesControl::MOD_TYPE+i, 0, 1, 0, "");
		}
		  
		configParam(NymphesControl::LFO1_TYPE, 0, 3, 0, "");		
		configParam(NymphesControl::LFO1_SYNC, 0, 1, 0, "");		
		configParam(NymphesControl::LFO2_TYPE, 0, 3, 0, "");		
		configParam(NymphesControl::LFO2_SYNC, 0, 1, 0, "");		
		configParam(NymphesControl::MOD_SOURCE1, 0, 1, 0, "");		
		configParam(NymphesControl::SUSTAIN, 0, 1, 0, "");		
		configParam(NymphesControl::LEGATO, 0, 1, 0, "");		
		configParam(NymphesControl::PLAYMODE, 0, 5, 0, "");		
		configParam(NymphesControl::LOAD, 0.0, 1.0, 0.0, "");
		configParam(NymphesControl::SAVE, 0.0, 1.0, 0.0, "");

		configParam(NymphesControl::PROGRAM_BANK, 0.0, 1.0, 0.0, "");
		configParam(NymphesControl::PROGRAM_KNOB, 0.0, 48.0, 0.0, "");
		configParam(NymphesControl::PROGRAM_SEND, 0.0, 1.0, 0.0, "");
		
		onReset();
	}

        // void load(std::string);
        // void save(std::string);
  
	void onReset() override {
		for (int i = 0; i < 128; i++) {
			values_in[i] = -10;
		}
		current_program = 0x00;
		target_program = 0x00;
		current_bank = '0';
		factory = false;
		factory_last = false;
		for (int i = 0; i < 38; i++) {
			current_values[i] = 0;
			valueFilters_last[i] = -10;
			last_value_out[i] = -10;
		        controller_values_last[i] = -10;
		}
		for (int i = 0; i < 74; i++) {
		        cc_values_last[i] = -10;
		}
		for (int i = 0; i < 4; i++) {
		        for (int j = 0; j < 36; j++) {
			  mod_valueFilters_last[i][j] = -10;
			  last_mod_value[i][j] = -10;
			  mod_controller_values_last[i][j] = -10;
			  mod_current_values[i][j] = 0;
			}
		}
		
		for (int i = 0; i < 8; i++) {
 		        button_valueFilters_last[i] = -10;
		}

		int nymphes_cc_map[82] = {
					  22, // 0-3 lfo1 type 70
					  28, // 0-3 lfo2 type 72
					  30, // 0-3 mod source selector 74
					  23, // 0-1 lfo1 sync 71
					  29, // 0-1 lfo2 sync 73
					  64, // 0-1 sustain pedal 75
					  68, // 0-1 legato 76
					  17, // 0-5 playmode 77
					  36,37,39,40,41,45,46,47,52,53,54,55,58,59, // mod shift 28-41
					  31,32,33,34,35,42,43,44,48,49,50,51,56,57, // mod normal 42-55
					  86,87,88,89, // reverb mod 78-81
					  60,61,62,63, // lfo2 mod 64-67
					  12,5,15,16,14,81,4,8,73,84,85,72,20,21, // shift 0-13
					  70,9,10,11,13,74,71,3,79,80,82,83,18,19, // normal 14-27
					  75,76,77,78, // reverb 56-59
					  24,25,26,27, // lfo2 60-63
					  1, // mod wheel 68
					  7 // volume 69
		};
		for (int i = 0; i < 82; i++) {
			// learnedCcs[i] = i;
			learnedCcs[i] = nymphes_cc_map[i];
		}
		mod_src = 0;
		mod_src_last = 4;
		midiInput.reset();
		midiOutput.reset();
	}

	void process(const ProcessArgs& args) override {
		midi::Message msg;
		while (midiInput.tryPop(&msg, args.frame)) {
			processMessage(msg);
		}

		//---------------------------------------------------------------------------
		// buttons:

		for (int i = 0; i < 8; i++) {
		  
		  int cc = learnedCcs[i];
		  float value_in = values_in[cc] / 127.f;
		  
		  //x if (std::fabs(button_valueFilters[i].out - value_in) >= 1.f) {
		    // Jump value
		    button_valueFilters[i].out = value_in;
		  // }
		  // else {
		  //   // Smooth value with filter
		  //   button_valueFilters[i].process(args.sampleTime, value_in);
		  // }
		}

		for (int i = 0; i < 4; i++) {
		  if ((params[MOD_TYPE+i].getValue() != mod_button_last[i]) && mod_button_last[i] == 0) {
		      button_settings[2] = i;
		      button_pressed = true;
		  }
		  mod_button_last[i] = params[MOD_TYPE+i].getValue();
		}		

		
		for (int j = 0; j < 7; j++) {
		  if (j != 2) {
		    if ((params[LFO1_TYPE+j].getValue() != button_last[j]) && button_last[j] == 0) {
		      button_settings[j]++;
		      if (button_settings[j] > number_lights_per_button[j]) button_settings[j] = 0;
		      // if (j == 2) button_pressed = true;
		    }
		  }

		  if (button_valueFilters_last[j] != (int) std::round(button_valueFilters[j].out * 127)) {
		    value_out = (int) std::round(button_valueFilters[j].out * 127);
		    button_valueFilters_last[j] = (int) std::round(button_valueFilters[j].out * 127);
		    if (j == 2) button_pressed = false;
		  } else {
		    value_out = button_settings[j];
		  }		  
		  value_out = clamp(value_out, 0, 127);
		  if (last_button_value_out[j] != value_out) {
		    midiOutput.setValue(value_out, learnedCcs[j]);
		    last_button_value_out[j] = value_out;
		  }
		  // params[LFO1_TYPE].setValue(lfo1_type);
		  button_settings[j] = value_out;
		  if (j == 2) {
		    mod_src = value_out;
		    if ( mod_src != mod_src_last ) {
		      mod_src_last = mod_src;
		      for (int i = 0; i < 36; i++) {
			if ( i < 28 ) {
			  params[CONTROLLERS+i+28].setValue(mod_current_values[mod_src][i]);
			  lights[CTRL_LIGHTS + i + 28].setBrightness((mod_current_values[mod_src][i]+1.)/128.);
			} else if ( i >= 28 && i < 32 ) {
			  params[CONTROLLERS+i+32].setValue(mod_current_values[mod_src][i]);
			  lights[CTRL_LIGHTS + i + 32].setBrightness((mod_current_values[mod_src][i]+1.)/128.);
			} else {
			  params[CONTROLLERS+i+36].setValue(mod_current_values[mod_src][i]);
			  lights[CTRL_LIGHTS + i + 36].setBrightness((mod_current_values[mod_src][i]+1.)/128.);
			}		      
		      }
		    }
		  }
			      
		  if (j < 3) {
		    if (button_settings[j] == 0) {
		      lights[LFO1_TYPE_LIGHTS+4*j].value = 1.0;
		      lights[LFO1_TYPE_LIGHTS+1+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+2+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+3+4*j].value = 0.0;
		      // } else if (lfo1_type == 1) {
		    } else if (button_settings[j] == 1) {
		      lights[LFO1_TYPE_LIGHTS+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+1+4*j].value = 1.0;
		      lights[LFO1_TYPE_LIGHTS+2+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+3+4*j].value = 0.0;
		    } else if (button_settings[j] == 2) {
		      lights[LFO1_TYPE_LIGHTS+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+1+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+2+4*j].value = 1.0;
		      lights[LFO1_TYPE_LIGHTS+3+4*j].value = 0.0;
		    } else if (button_settings[j] == 3) {
		      lights[LFO1_TYPE_LIGHTS+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+1+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+2+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+3+4*j].value = 1.0;
		    } else if (button_settings[j] == 4) {
		      lights[LFO1_TYPE_LIGHTS+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+1+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+2+4*j].value = 0.0;
		      lights[LFO1_TYPE_LIGHTS+3+4*j].value = 0.0;
		    }
		  } else if (j > 2 && j < 7) {
		    lights[LFO1_SYNC_LIGHT+j-3].value = button_settings[j];
		  }
		  // lfo1_type_last = params[LFO1_TYPE].getValue();
		  button_last[j] = params[LFO1_TYPE+j].getValue();
		
		}

		if (button_valueFilters_last[7] != (int) std::round(button_valueFilters[7].out * 127)) {
		  value_out = (int) std::round(button_valueFilters[7].out * 127);
		  button_valueFilters_last[7] = (int) std::round(button_valueFilters[7].out * 127);
		} else {
		  value_out = params[PLAYMODE].getValue();
		}		  		
		value_out = clamp(value_out, 0, 127);
		if (last_button_value_out[7] != value_out) {
		  midiOutput.setValue(value_out, learnedCcs[7]);
		  last_button_value_out[7] = value_out;
		}
		for (int k=0; k<6; k++) {
		  if (k == params[PLAYMODE].getValue()) lights[PLAYMODE_LIGHTS+k].value = 1.0;
		  else lights[PLAYMODE_LIGHTS+k].value = 0.0;
		}
		params[PLAYMODE].setValue(value_out);
		
		//---------------------------------------------------------------------------
		// mod controllers:		
		
		if (button_pressed == false) {
		  for (int i = 0; i < 36; i++) {

		    int cc = learnedCcs[i+8];
		    float value_in = values_in[cc] / 127.f;
		    
		    if (std::fabs(mod_valueFilters[mod_src][i].out - value_in) >= 1.f) {
		      // Jump value
		      mod_valueFilters[mod_src][i].out = value_in;
		    }
		    else {
		      // Smooth value with filter
		      mod_valueFilters[mod_src][i].process(args.sampleTime, value_in);
		    }
		  }
		}

		//------------------
		//x
		const float rateLimiterPeriod = 0.0005f;
		rateLimiterPhase += args.sampleTime / rateLimiterPeriod;
		if (rateLimiterPhase >= 1.f) {
			rateLimiterPhase -= 1.f;
		}
		else {
			return;
		}

		// std::cout << " args.sampleTime: " << args.sampleTime << std::endl;

		count1++;
			
		for (int i = 0; i < 36; i++) {
		  value_out = 0;
		  value_changed = false;
		  if (mod_valueFilters_last[mod_src][i] != (int) std::round(mod_valueFilters[mod_src][i].out * 127)) {
		    value_out = (int) std::round(mod_valueFilters[mod_src][i].out * 127);
		    if ( i < 28 ) {
		      params[CONTROLLERS+i+28].setValue(value_out);
		      mod_controller_values_last[mod_src][i] = value_out;
		    } else if ( i >= 28 && i < 32 ) {
		      params[CONTROLLERS+i+32].setValue(value_out);
		      mod_controller_values_last[mod_src][i] = value_out;
		    } else {
		      params[CONTROLLERS+i+36].setValue(value_out);
		      mod_controller_values_last[mod_src][i] = value_out;
		    }
		    //x
		    mod_valueFilters_last[mod_src][i] = (int) std::round(mod_valueFilters[mod_src][i].out * 127);
		    value_changed = true;
		  }
		  if ( i < 28 ) {
		    if (cc_values_last[i+28] != (int) std::round(inputs[CC_INPUTS + i + 28].getVoltage() / 10.f * 127)) {
		      value_out = (int) std::round(mod_valueFilters[mod_src][i].out * 127) + (int) std::round(inputs[CC_INPUTS + i + 28].getVoltage() / 10.f * 127);
		      cc_values_last[i+28] = (int) std::round(inputs[CC_INPUTS + i + 28].getVoltage() / 10.f * 127);
		      value_changed = true;
		    }
		    if (mod_controller_values_last[mod_src][i] != (int) params[CONTROLLERS+i+28].getValue()) {
		      value_out = value_out + (int) params[CONTROLLERS+i+28].getValue();
		      mod_controller_values_last[mod_src][i] = (int) params[CONTROLLERS+i+28].getValue();
		      value_changed = true;
		    }
		  } else if ( i >= 28 && i < 32 ) {
		    if (cc_values_last[i+32] != (int) std::round(inputs[CC_INPUTS + i + 32].getVoltage() / 10.f * 127)) {
		      value_out = (int) std::round(mod_valueFilters[mod_src][i].out * 127) + (int) std::round(inputs[CC_INPUTS + i + 32].getVoltage() / 10.f * 127);
		      cc_values_last[i+32] = (int) std::round(inputs[CC_INPUTS + i + 32].getVoltage() / 10.f * 127);
		      value_changed = true;
		    }
		    if (mod_controller_values_last[mod_src][i] != (int) params[CONTROLLERS+i+32].getValue()) {
		      value_out = value_out + (int) params[CONTROLLERS+i+32].getValue();
		      mod_controller_values_last[mod_src][i] = (int) params[CONTROLLERS+i+32].getValue();
		      value_changed = true;
		    }
		  } else {
		    if (cc_values_last[i+36] != (int) std::round(inputs[CC_INPUTS + i + 36].getVoltage() / 10.f * 127)) {
		      value_out = (int) std::round(mod_valueFilters[mod_src][i].out * 127) + (int) std::round(inputs[CC_INPUTS + i + 36].getVoltage() / 10.f * 127);
		      cc_values_last[i+36] = (int) std::round(inputs[CC_INPUTS + i + 36].getVoltage() / 10.f * 127);
		      value_changed = true;
		    }
		    if (mod_controller_values_last[mod_src][i] != (int) params[CONTROLLERS+i+36].getValue()) {
		      value_out = value_out + (int) params[CONTROLLERS+i+36].getValue();
		      mod_controller_values_last[mod_src][i] = (int) params[CONTROLLERS+i+36].getValue();
		      value_changed = true;
		    }
		  }			
		  value_out = clamp(value_out, 0, 127);
		  if (last_mod_value[mod_src][i] != value_out && value_changed) {
		    if (count1 > slow_control) {
		      count1 = 0;
		      midiOutput.setValue(value_out, learnedCcs[i+8]);
		      last_mod_value[mod_src][i] = value_out;
		    }
		  }
		  if (value_changed) {
		    if ( i < 28 ) {
		      params[CONTROLLERS+i+28].setValue(value_out);
		      lights[CTRL_LIGHTS + i + 28].setBrightness((value_out+1.)/128.);
		    } else if ( i >= 28 && i < 32 ) {
		      params[CONTROLLERS+i+32].setValue(value_out);
		      lights[CTRL_LIGHTS + i + 32].setBrightness((value_out+1.)/128.);
		    } else {
		      params[CONTROLLERS+i+36].setValue(value_out);
		      lights[CTRL_LIGHTS + i + 36].setBrightness((value_out+1.)/128.);
		    }
		    mod_current_values[mod_src][i] = value_out;
		  }
		  mod_display_values[i] = mod_current_values[mod_src][i];
		}
		
		//---------------------------------------------------------------------------
		// normal controllers:
		
		for (int i = 0; i < 38; i++) {
		  // if (!outputs[CC_OUTPUT + i].isConnected())
		  // 	continue;
		  
		  int cc = learnedCcs[i+44];
		  float value_in = values_in[cc] / 127.f;
		  
		  // if (value_in != params[i].getValue() && !skip[i]) {
		  // Detect behavior from MIDI buttons.
		  if (std::fabs(valueFilters[i].out - value_in) >= 1.f) {
		    // Jump value
		    valueFilters[i].out = value_in;
		  }
		  else {
		    // Smooth value with filter
		    valueFilters[i].process(args.sampleTime, value_in);
		  }
		  // outputs[CC_OUTPUT + i].setVoltage(valueFilters[i].out * 10.f);
		    // }
		}

		
		count2++;

		for (int j = 0; j < 38; j++) {
		  value_out = 0;
		  value_changed = false;
		  int value_idx = j;
		  int slider_idx = j;
		  int cc_idx = j + 44;
		  if ( j >= 28 && j < 32 ) {
		    slider_idx += 28;
		  } else if ( j >= 32 && j < 36 ) {
		    slider_idx += 32;
		  } else if ( j >= 36) {
		    slider_idx += 36;
		  }
		  // if (!inputs[CC_INPUTS + i].isConnected()) {
		  if (valueFilters_last[value_idx] != (int) std::round(valueFilters[value_idx].out * 127)) {
		    value_out = (int) std::round(valueFilters[value_idx].out * 127);
		    params[CONTROLLERS+slider_idx].setValue(value_out);
		    //bug controller_values_last[slider_idx] = value_out;		      
		    controller_values_last[value_idx] = value_out;		      
		    //x
		    valueFilters_last[value_idx] = (int) std::round(valueFilters[value_idx].out * 127);
		    value_changed = true;
		  }
		  if (cc_values_last[slider_idx] != (int) std::round(inputs[CC_INPUTS + slider_idx].getVoltage() / 10.f * 127)) {
		    value_out = (int) std::round(valueFilters[value_idx].out * 127) + (int) std::round(inputs[CC_INPUTS + slider_idx].getVoltage() / 10.f * 127);
		    cc_values_last[slider_idx] = (int) std::round(inputs[CC_INPUTS + slider_idx].getVoltage() / 10.f * 127);
		    value_changed = true;
		  }
		  //bug if (controller_values_last[slider_idx] != (int) params[CONTROLLERS+slider_idx].getValue()) {
		  if (controller_values_last[value_idx] != (int) params[CONTROLLERS+slider_idx].getValue()) {
		    value_out = value_out + (int) params[CONTROLLERS+slider_idx].getValue();
		    //bug controller_values_last[slider_idx] = (int) params[CONTROLLERS+slider_idx].getValue();
		    controller_values_last[value_idx] = (int) params[CONTROLLERS+slider_idx].getValue();
		    value_changed = true;
		  }
		  value_out = clamp(value_out, 0, 127);
		  if (last_value_out[value_idx] != value_out && value_changed) {
		    if (count2 > slow_control) {
		      count2 = 0;
		      midiOutput.setValue(value_out, learnedCcs[cc_idx]);
		      last_value_out[value_idx] = value_out;
		    }
		  }
		  if (value_changed) {
		    params[CONTROLLERS+slider_idx].setValue(value_out);
		    lights[CTRL_LIGHTS + slider_idx].setBrightness((value_out+1.)/128.);
		    current_values[value_idx] = value_out;
		  }
		}		

		//------

		if(inputs[CV_PC].isConnected()) {
		  float inputValue = clamp(inputs[CV_PC].getVoltage(), 0.f, 10.f);
		  target_program = (uint8_t) rescale(inputValue, 0.f, 10.f, 0.f, 127.f);
		  if(target_program > 48) {
		    target_program -= 48;
		    if(target_program > 48) target_program = 48;
		    factory = true;
		  } else {
		    factory = false;
		  }		  
		}
		else {
		  target_program = (uint8_t) params[PROGRAM_KNOB].getValue();
		}
		
		if (((params[PROGRAM_BANK].getValue() != pc_bank_last) && (pc_bank_last == 0.0)))
		  {
		    factory = !factory;	
		  }

		if (factory_last != factory) {
		  factory_last = factory;
		  if(factory) {
		    midiOutput.setValue(1, 0);
		    midiOutput.setValue(0, 32);
		  } else {
		    midiOutput.setValue(0, 0);
		    midiOutput.setValue(0, 32);
		  }
		}
		
		if (values_in[0] == 1 && values_in[32] == 0) {
		  factory = true;
		  values_in[0] = -10;
		  values_in[32] = -10;
		} else if (values_in[0] == 0 && values_in[32] == 0) {
		  factory = false;
		  values_in[0] = -10;
		  values_in[32] = -10;
		}
		
		if (factory) {
		  lights[PC_BANK_LIGHTS].value = 0.0;
		  lights[PC_BANK_LIGHTS+1].value = 1.0;
		} else {
		  lights[PC_BANK_LIGHTS].value = 1.0;
		  lights[PC_BANK_LIGHTS+1].value = 0.0;
		}
		
		// pc_bank_last[0] = inputs[CV_DIRECTION].getVoltage();
		pc_bank_last = params[PROGRAM_BANK].getValue();
		

		current_program = target_program%7 + 1;
		if(target_program/7 == 0) current_bank = 'A';
		if(target_program/7 == 1) current_bank = 'B';
		if(target_program/7 == 2) current_bank = 'C';
		if(target_program/7 == 3) current_bank = 'D';
		if(target_program/7 == 4) current_bank = 'E';
		if(target_program/7 == 5) current_bank = 'F';
		if(target_program/7 == 6) current_bank = 'G';

		if(sendPCTrigger.process(fmax(params[PROGRAM_SEND].getValue(), inputs[CV_PC_SEND].getVoltage()))) {
		  midiOutput.sendProgram(target_program);
		}

		//--------

		

		if ((params[LOAD].getValue() != load_last_value) && (load_last_value == 0.0)) {
		  load_patch = !load_patch;

		  std::string dir;
		  if (lastPath.empty()) {
		    dir = asset::user("patches");
		    system::createDirectory(dir);
		  }
		  else {
		    dir = system::getDirectory(this->lastPath);
		  }
		  osdialog_filters *filters = osdialog_filters_parse(NYM_FILTERS_load.c_str());
		  char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		  if (path) {
		    load(path);
		    lastPath = path;
		    free(path);
		  }

		  osdialog_filters_free(filters);
		  
		}
		if ((params[SAVE].getValue() != save_last_value) && (save_last_value == 0.0)) {
		  save_patch = !save_patch;

		  std::string dir;
		  if (lastPath.empty()) {
		    dir = asset::user("patches");
		    system::createDirectory(dir);
		  }
		  else {
		    dir = system::getDirectory(this->lastPath);
		  }
		  osdialog_filters *filters = osdialog_filters_parse(NYM_FILTERS_save.c_str());
		  char *path = osdialog_file(OSDIALOG_SAVE, dir.c_str(), NULL, filters);
		  // Append .nym extension if no extension was given.
		  std::string pathStr = path;
		  if (system::getExtension(pathStr) != ".nym") {
		  // if (string::filenameExtension(string::filename(pathStr)) == "") {
		    pathStr += ".nym";
		  }
		  if (path) {
		    save(pathStr);
		    lastPath = pathStr;
		    free(path);
		  }

		  osdialog_filters_free(filters);
		  
		}

		load_last_value = params[LOAD].getValue();
		save_last_value = params[SAVE].getValue();
		
	}

        void load(std::string filename) {

	  FILE *patchFile = fopen(filename.c_str(), "r");
	  if (!patchFile) {
	    // Exit silently
	    return;
	  }

	  int value_in = 0;
	  // char line[1000], colname[1000];
	  char line[1000];
	  std::vector<int> result;
	  int val;
	  // Read data, line by line
	  while(fgets(line,1000,patchFile))
	    {
	      // Create a stringstream of the current line
	      std::stringstream ss(line);
	      
	      // Keep track of the current column index
	      int colIdx = 0;
	      
	      // Extract each integer
	      while(ss >> val){
		
	     	// Add the current integer to the 'colIdx' column's values vector
		result.push_back(val);
		
	     	// If the next token is a comma, ignore it and move on
	     	if(ss.peek() == ',') ss.ignore();
		
	     	// Increment the column index
	     	colIdx++;
	      }
	    }
	  
	  int indx = 0;
	  for (int i = 0; i < 38; i++) {
	    value_in = result.at(i);
	    if ( i < 28 ) {
	      params[CONTROLLERS+i].setValue(value_in);
	    } else if ( i >= 28 && i < 32 ) {
	      params[CONTROLLERS+ i + 28].setValue(value_in);
	    } else if ( i >= 32 && i < 36 ) {
	      params[CONTROLLERS+ i + 32].setValue(value_in);
	    } else {
	      params[CONTROLLERS+ i + 36].setValue(value_in);
	    }
	    indx++;
	  }

	  for (int i = 0; i < 4; i++) {
	    for (int j = 0; j < 36; j++) {
	      value_in = result.at(indx);
	      mod_current_values[i][j] = value_in;
	      indx++;
	    }
	  }

	  for (int j = 0; j < 7; j++) {
	    value_in = result.at(indx);
	    button_settings[j] = value_in;
	    if (j == 2) mod_src = value_in;
	    indx++;
	  }

	  for (int i = 0; i < 36; i++) {
	    if ( i < 28 ) {
	      params[CONTROLLERS+i+28].setValue(mod_current_values[mod_src][i]);
	    } else if ( i >= 28 && i < 32 ) {
	      params[CONTROLLERS+i+32].setValue(mod_current_values[mod_src][i]);
	    } else {
	      params[CONTROLLERS+i+36].setValue(mod_current_values[mod_src][i]);		
	    }
	  }

	  value_in = result.at(indx);
	  params[PLAYMODE].setValue(value_in);
	}

        void save(std::string savefilename) {

	  FILE *patchFile = fopen(savefilename.c_str(), "w");
	  if (!patchFile) {
	    // Exit silently
	    return;
	  }

	  
	  for (int i = 0; i < 28; i++) {
	    std::string value = std::to_string(int(params[CONTROLLERS+i].getValue())) + ", ";
	    fputs(value.c_str(),patchFile);
	  }

	  for (int i = 56; i < 60; i++) {
	    std::string value = std::to_string(int(params[CONTROLLERS+i].getValue())) + ", ";
	    fputs(value.c_str(),patchFile);
	  }

	  for (int i = 64; i < 68; i++) {
	    std::string value = std::to_string(int(params[CONTROLLERS+i].getValue())) + ", ";
	    fputs(value.c_str(),patchFile);
	  }
	  
	  for (int i = 72; i < 74; i++) {
	    std::string value = std::to_string(int(params[CONTROLLERS+i].getValue())) + ", ";
	    fputs(value.c_str(),patchFile);
	  }

	  for (int i = 0; i < 4; i++) {
	    for (int j = 0; j < 36; j++) {
	      std::string value = std::to_string(mod_current_values[i][j]) + ", ";
	      fputs(value.c_str(),patchFile);
	    }
	  }
	    	  
	  for (int j = 0; j < 7; j++) {
	    std::string value = std::to_string(button_settings[j]) + ", ";
	    fputs(value.c_str(),patchFile);
	  }
	  std::string value = std::to_string(int(params[PLAYMODE].getValue())) + "\n";
	  fputs(value.c_str(),patchFile);

	  fclose(patchFile);

	}

  
	void processMessage(midi::Message msg) {
		switch (msg.getStatus()) {
			// cc
			case 0xb: {
				processCC(msg);
			} break;
		        case 0xc: {
 			        processPC(msg);
			} break;
			default: break;
		}
	}

	void processCC(midi::Message msg) {
		uint8_t cc = msg.getNote();
		// Allow CC to be negative if the 8th bit is set.
		// The gamepad driver abuses this, for example.
		// Cast uint8_t to int8_t
		int8_t value_in = msg.bytes[2];
		value_in = clamp(value_in, -127, 127);
		// Learn
		values_in[cc] = value_in;
	}

	void processPC(midi::Message msg) {
	        uint8_t program = msg.getNote();
		setProgram(program);
	}

	void setProgram(uint8_t program) {
		// current_program = program;
		params[PROGRAM_KNOB].setValue(program);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* ccsJ = json_array();
		for (int i = 0; i < 82; i++) {
			json_array_append_new(ccsJ, json_integer(learnedCcs[i]));
		}
		json_object_set_new(rootJ, "ccs", ccsJ);

		// Remember values so users don't have to touch MIDI controller knobs when restarting Rack
		json_t* values_inJ = json_array();
		for (int i = 0; i < 128; i++) {
			json_array_append_new(values_inJ, json_integer(values_in[i]));
		}
		json_object_set_new(rootJ, "values_in", values_inJ);

		json_object_set_new(rootJ, "midi", midiInput.toJson());
		json_object_set_new(rootJ, "midiOut", midiOutput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* ccsJ = json_object_get(rootJ, "ccs");
		if (ccsJ) {
			for (int i = 0; i < 82; i++) {
				json_t* ccJ = json_array_get(ccsJ, i);
				if (ccJ)
					learnedCcs[i] = json_integer_value(ccJ);
			}
		}

		json_t* values_inJ = json_object_get(rootJ, "values_in");
		if (values_inJ) {
			for (int i = 0; i < 128; i++) {
				json_t* value_inJ = json_array_get(values_inJ, i);
				if (value_inJ) {
					values_in[i] = json_integer_value(value_inJ);
					midiOutput.setValue(json_integer_value(value_inJ),i);
				}
			}
		}
		
		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ) {
			midiInput.fromJson(midiJ);
		}
		json_t* midiOutJ = json_object_get(rootJ, "midiOut");
		if (midiOutJ)
		  midiOutput.fromJson(midiOutJ);
	}
};

////////////////////////////////////
struct VerySmallDisplayWidget : TransparentWidget {
  int *value = NULL;
  std::shared_ptr<Font> font;

  VerySmallDisplayWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Segment7Standard.ttf"));
  };

  void draw(const DrawArgs& args) override
  {
    if (!value) {
      return;
    }
    // Background
    //NVGcolor backgroundColor = nvgRGB(0x20, 0x20, 0x20);
    NVGcolor backgroundColor = nvgRGB(0x20, 0x10, 0x10);
    NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 2.0);
    nvgFillColor(args.vg, backgroundColor);
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 1.0);
    nvgStrokeColor(args.vg, borderColor);
    nvgStroke(args.vg);    
    // text 
    nvgFontSize(args.vg, 7);
    nvgFontFaceId(args.vg, font->handle);
    // nvgTextLetterSpacing(args.vg, 1.1111);
    nvgTextLetterSpacing(args.vg, 1);

    std::stringstream to_display;   
    to_display << std::setw(3) << *value;

    Vec textPos = Vec(0.5f, 6.33333f); 

    NVGcolor textColor = nvgRGB(0xdf, 0xd2, 0x2c);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);

    textColor = nvgRGB(0xda, 0xe9, 0x29);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "\\\\\\", NULL);

    // textColor = nvgRGB(0xf0, 0x00, 0x00);
    textColor = nvgRGB(0xff, 0xd5, 0xd5);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
  }
};
////////////////////////////////////

////////////////////////////////////
struct DisplayWidget : TransparentWidget {
  int *value = NULL;
  char *bank = NULL;
  std::shared_ptr<Font> font;

  DisplayWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Segment7Standard.ttf"));
  };

  void draw(const DrawArgs& args) override
  {
    if (!value) {
      return;
    }
    // Background
    //NVGcolor backgroundColor = nvgRGB(0x20, 0x20, 0x20);
    NVGcolor backgroundColor = nvgRGB(0x20, 0x10, 0x10);
    NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 2.0);
    nvgFillColor(args.vg, backgroundColor);
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 1.0);
    nvgStrokeColor(args.vg, borderColor);
    nvgStroke(args.vg);    
    // text 
    nvgFontSize(args.vg, 11);
    nvgFontFaceId(args.vg, font->handle);
    // nvgTextLetterSpacing(args.vg, 1.1111);
    nvgTextLetterSpacing(args.vg, 1);

    std::stringstream to_display;   
    to_display << *bank << std::setw(2) << *value;

    Vec textPos = Vec(0.7857f, 9.99523f); 

    NVGcolor textColor = nvgRGB(0xdf, 0xd2, 0x2c);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);

    textColor = nvgRGB(0xda, 0xe9, 0x29);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "\\\\\\", NULL);

    // textColor = nvgRGB(0xf0, 0x00, 0x00);
    textColor = nvgRGB(0xff, 0xd5, 0xd5);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
  }
};
////////////////////////////////////


struct NymphesControlWidget : ModuleWidget {
  NymphesControlWidget(NymphesControl* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NymphesControl.svg")));
    // setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/nymphes-top-W.svg")));
    
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    
    typedef Grid2MidiWidget<CcChoice<NymphesControl>> TMidiWidget;
    TMidiWidget* midiWidget = createWidget<TMidiWidget>(mm2px(Vec(3.399621, 11.337339)));
    midiWidget->box.size = mm2px(Vec(40, 27.667)); // 44->68
    midiWidget->setMidiPort(module ? &module->midiInput : NULL);
    // midiWidget->setModule(module);
    addChild(midiWidget);
    
    typedef Grid2MidiWidget<CcChoice<NymphesControl>> TMidiWidget2;
    // TMidiWidget2* midiWidget2 = createWidget<TMidiWidget2>(mm2px(Vec(87.399621, 14.837339)));
    TMidiWidget2* midiWidget2 = createWidget<TMidiWidget2>(mm2px(Vec(3.399621, 40.837339)));
    midiWidget2->box.size = mm2px(Vec(40, 27.667));
    midiWidget2->setMidiPort(module ? &module->midiOutput : NULL);
    addChild(midiWidget2);
    
    //Red, Green, Yellow, Blue, White - componentlibrary.hpp
    
    float slider_x[14] = {179.628, 185.508, 190.947, 196.610, 202.324, 212.956, 218.500, 224.200, 234.887, 240.370, 246.114, 251.808, 262.310, 267.731};
    for (int i = 0; i < 14; i++) {
      addParam(createLightParam<LEDLightSliderFixed<YellowLight>>(mm2px(Vec(slider_x[i]-0.9-4.588-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addParam(createLightParam<LEDLightSliderFixed<BlueLight>>(mm2px(Vec(slider_x[i]-0.9-4.588-120, 94.18849).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i+14, NymphesControl::CTRL_LIGHTS + i + 14));
      
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(slider_x[i]+0.5-4.588-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(slider_x[i]+0.5-4.588-120, 79 + (i%2)*6)), module, NymphesControl::CC_INPUTS + i + 14));		  
    }
    for (int i = 28; i < 42; i++) {
      //123.709
      addParam(createLightParam<LEDLightSliderFixed<RedLight>>(mm2px(Vec(3.709+slider_x[i-28]-0.9-4.588, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addParam(createLightParam<LEDLightSliderFixed<RedLight>>(mm2px(Vec(3.709+slider_x[i-28]-0.9-4.588, 94.18849).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i+14, NymphesControl::CTRL_LIGHTS + i + 14));
      
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(3.709+slider_x[i-28]+0.5-4.588, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(3.709+slider_x[i-28]+0.5-4.588, 79 + (i%2)*6)), module, NymphesControl::CC_INPUTS + i + 14));		  
    }
    
    
    // reverb
    for (int i = 56; i < 60; i++) {
      addParam(createLightParam<LEDLightSliderFixed<WhiteLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    // reverb mod
    for (int i = 60; i < 64; i++) {
      addParam(createLightParam<LEDLightSliderFixed<RedLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    // lfo2 control
    for (int i = 64; i < 68; i++) {
      addParam(createLightParam<LEDLightSliderFixed<GreenLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    // lfo2 mod
    for (int i = 68; i < 72; i++) {
      addParam(createLightParam<LEDLightSliderFixed<RedLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    for (int i = 72; i < 73; i++) {
      addParam(createLightParam<LEDLightSliderFixed<BlueLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    for (int i = 73; i < 74; i++) {
      addParam(createLightParam<LEDLightSliderFixed<YellowLight>>(mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-2, 0))), module, NymphesControl::CONTROLLERS + i, NymphesControl::CTRL_LIGHTS + i));
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(85.351+6*i+0.5-120, 65 + (i%2)*6 - 1.616)), module, NymphesControl::CC_INPUTS + i));
    }
    
    // addParam(createParam<SonusBigSnapKnob>(mm2px(Vec(420,106)), module, NymphesControl::LFO_CTRL));
    float extra_shift = 5.5;
    addParam(createParam<CKD6>(mm2px(Vec(300.6+10.5+extra_shift,104.5)), module, NymphesControl::LFO1_TYPE));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(299.1+11+extra_shift, 101)), module, NymphesControl::LFO1_TYPE_LIGHTS));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(302.1+11+extra_shift, 101)), module, NymphesControl::LFO1_TYPE_LIGHTS + 1));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(305.1+11+extra_shift, 101)), module, NymphesControl::LFO1_TYPE_LIGHTS + 2));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(308.1+11+extra_shift, 101)), module, NymphesControl::LFO1_TYPE_LIGHTS + 3));
    addParam(createParam<CKD6>(mm2px(Vec(312.6+10.5+extra_shift,104.5)), module, NymphesControl::LFO1_SYNC));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(315.6+11+extra_shift, 101)), module, NymphesControl::LFO1_SYNC_LIGHT));

    addParam(createParam<CKD6>(mm2px(Vec(324.6+10.5+extra_shift,104.5)), module, NymphesControl::LFO2_TYPE));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(323.1+11+extra_shift, 101)), module, NymphesControl::LFO2_TYPE_LIGHTS));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(326.1+11+extra_shift, 101)), module, NymphesControl::LFO2_TYPE_LIGHTS + 1));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(329.1+11+extra_shift, 101)), module, NymphesControl::LFO2_TYPE_LIGHTS + 2));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(332.1+11+extra_shift, 101)), module, NymphesControl::LFO2_TYPE_LIGHTS + 3));
    addParam(createParam<CKD6>(mm2px(Vec(336.6+10.5+extra_shift,104.5)), module, NymphesControl::LFO2_SYNC));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(339.6+11+extra_shift, 101)), module, NymphesControl::LFO2_SYNC_LIGHT));
    
    // addParam(createParam<CKD6>(mm2px(Vec(348.6-3.5,104.5)), module, NymphesControl::MOD_SOURCE));
    addParam(createParam<LEDButton>(mm2px(Vec(274,61)), module, NymphesControl::MOD_TYPE));
    addParam(createParam<LEDButton>(mm2px(Vec(274,68)), module, NymphesControl::MOD_TYPE + 1));
    addParam(createParam<LEDButton>(mm2px(Vec(274,75)), module, NymphesControl::MOD_TYPE + 2));
    addParam(createParam<LEDButton>(mm2px(Vec(274,82)), module, NymphesControl::MOD_TYPE + 3));
    addChild(createLight<MediumLight<BlueLight>>(mm2px(Vec(275.5, 62.5)), module, NymphesControl::MOD_SOURCE_LIGHTS));
    addChild(createLight<MediumLight<BlueLight>>(mm2px(Vec(275.5, 69.5)), module, NymphesControl::MOD_SOURCE_LIGHTS + 1));
    addChild(createLight<MediumLight<BlueLight>>(mm2px(Vec(275.5, 76.5)), module, NymphesControl::MOD_SOURCE_LIGHTS + 2));
    addChild(createLight<MediumLight<BlueLight>>(mm2px(Vec(275.5, 83.5)), module, NymphesControl::MOD_SOURCE_LIGHTS + 3));
    
    addParam(createParam<CKD6>(mm2px(Vec(380.6-3.5-extra_shift,104.5)), module, NymphesControl::SUSTAIN));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(383.6-3-extra_shift, 101)), module, NymphesControl::SUSTAIN_LIGHT));
    
    addParam(createParam<CKD6>(mm2px(Vec(392.6-3.5-extra_shift,104.5)), module, NymphesControl::LEGATO));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(395.6-3-extra_shift, 101)), module, NymphesControl::LEGATO_LIGHT));
    
    addParam(createParam<SonusBigSnapKnob>(mm2px(Vec(329, 75)), module, NymphesControl::PLAYMODE));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(350, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(355, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS + 1));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(360, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS + 2));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(365, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS + 3));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(370, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS + 4));
    addChild(createLight<SmallLight<BlueLight>>(mm2px(Vec(375, 82.5)), module, NymphesControl::PLAYMODE_LIGHTS + 5));
    
    
    addParam(createParam<CKD6>(mm2px(Vec(9.75, 106.5)), module, NymphesControl::LOAD));
    addParam(createParam<CKD6>(mm2px(Vec(27.0, 106.5)), module, NymphesControl::SAVE));
    
    
    addParam(createParam<CKD6>(mm2px(Vec(3.75, 77.5)), module, NymphesControl::PROGRAM_BANK));
    addParam(createParam<SonusBigSnapKnob>(mm2px(Vec(14, 77.5)), module, NymphesControl::PROGRAM_KNOB));
    addParam(createParam<CKD6>(mm2px(Vec(33.0, 77.5)), module, NymphesControl::PROGRAM_SEND));
    addChild(createLight<SmallLight<RedLight>>(mm2px(Vec(4.5, 74.5)), module, NymphesControl::PC_BANK_LIGHTS));
    addChild(createLight<SmallLight<RedLight>>(mm2px(Vec(10.5, 74.5)), module, NymphesControl::PC_BANK_LIGHTS+1));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.5, 94)), module, NymphesControl::CV_PC));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38, 94)), module, NymphesControl::CV_PC_SEND));
    
    
    // 		70  // 0-3 lfo1 type
    // 		71  // 0-1 lfo1 sync
    // 		72  // 0-3 lfo2 type
    // 		73  // 0-1 lfo2 sync
    // 		74  // 0-3 mod source selector
    // 		75  // 0-1 sustain pedal
    // 		76  // 0-1 legato
    // 		77  // 0-5 playmode
    
    //VALUE DISPLAY PROGRAM CHANGE
    DisplayWidget *value_display_pc;
    value_display_pc = new DisplayWidget();
    value_display_pc->box.pos = mm2px(Vec(19, 84.5));
    value_display_pc->box.size = Vec(24.357, 12.2221);
    if (module) {
      value_display_pc->bank = &module->current_bank;
      value_display_pc->value = &module->current_program;
    }
    addChild(value_display_pc); 
      
    //VALUE DISPLAY
    VerySmallDisplayWidget *value_display[74];
    for (int i = 0; i<14; i++) {
      value_display[i] = new VerySmallDisplayWidget();
      value_display[i]->box.pos = mm2px(Vec(slider_x[i]-0.9-4.588-120, 30.73149 - 3.516).plus(Vec(-1.2, 26.9)));
      value_display[i]->box.size = Vec(15.5, 7.7777);
      if (module) {
	value_display[i]->value = &module->current_values[i];
      }
    addChild(value_display[i]); 
    
    value_display[i+14] = new VerySmallDisplayWidget();
    value_display[i+14]->box.pos = mm2px(Vec(slider_x[i]-0.9-4.588-120, 93.18849).plus(Vec(-1.2, -1.9)));
    value_display[i+14]->box.size = Vec(15.5, 7.7777);
    if (module) {
      value_display[i+14]->value = &module->current_values[i+14];
    }
    addChild(value_display[i+14]); 		  
  }
  
  for (int i = 28; i<42; i++) {
    value_display[i] = new VerySmallDisplayWidget();
    value_display[i]->box.pos = mm2px(Vec(3.709+slider_x[i-28]-0.9-4.588, 30.73149 - 3.516).plus(Vec(-1.2, 26.9)));
    value_display[i]->box.size = Vec(15.5, 7.7777);
    if (module) {
      value_display[i]->value = &module->mod_display_values[i-28];
    }
    addChild(value_display[i]); 
    
    value_display[i+14] = new VerySmallDisplayWidget();
    value_display[i+14]->box.pos = mm2px(Vec(3.709+slider_x[i-28]-0.9-4.588, 93.18849).plus(Vec(-1.2, -1.9)));
    value_display[i+14]->box.size = Vec(15.5, 7.7777);
    if (module) {
      // value_display[i+14]->value = &module->current_values[i+14];
      value_display[i+14]->value = &module->mod_display_values[i-14];
    }
    addChild(value_display[i+14]); 		  
  }
  
  
  for (int i = 56; i<74; i++) {
    value_display[i] = new VerySmallDisplayWidget();
    value_display[i]->box.pos = mm2px(Vec(85.351+6*i-0.9-120, 30.73149 - 3.516).plus(Vec(-1.2, 26.9)));
    value_display[i]->box.size = Vec(15.5, 7.7777);
    if (module) {
      if( i < 60 ) {
	value_display[i]->value = &module->current_values[i-28];
      } else if ( i >= 64 && i < 68 ) {
	value_display[i]->value = &module->current_values[i-32];
      } else if ( i > 71) {
	value_display[i]->value = &module->current_values[i-36];
      } else if ( i >= 60 && i < 64 ) {
	value_display[i]->value = &module->mod_display_values[i-32];
      } else {
	value_display[i]->value = &module->mod_display_values[i-36];
      }
    }
    addChild(value_display[i]); 
		}
  
  }
  
};

// 385.264

// Use legacy slug for compatibility
Model* modelNymphesControl = createModel<NymphesControl, NymphesControlWidget>("NymphesControl");

// 86.495
// 171.083-84.588
