#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ADC.h"
#include "util/util_math.h"
#include "util/util_settings.h"

OC::DigitalInput gate_inputs[3] = {OC::DIGITAL_INPUT_1,OC::DIGITAL_INPUT_2,OC::DIGITAL_INPUT_3};
ADC_CHANNEL voct_inputs[3] = {ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3};
ADC_CHANNEL velocity_input = ADC_CHANNEL_4;

enum CVMSettings {
  CVM_SETTING_OCTAVE,
  CVM_SETTING_VELOCITY,
  CVM_SETTING_CHANNEL,
  CVM_SETTING_LAST
};


class CVMIDI : public settings::SettingsBase<CVMIDI, CVM_SETTING_LAST> {
public:

  //static constexpr int x = 4;

  void Init() {
    settings_[0] = CVM_SETTING_OCTAVE;
    settings_[1] = CVM_SETTING_VELOCITY;
    settings_[2] = CVM_SETTING_CHANNEL;
  }

  int get_octave() const {
    return values_[CVM_SETTING_OCTAVE];
  }

  int get_velocity() const {
    return values_[CVM_SETTING_VELOCITY];
  }

  int get_channel() const {
    return values_[CVM_SETTING_CHANNEL];
  }

  CVMSettings setting_at(int index) const {
    return settings_[index];
  }

private:
  CVMSettings settings_[CVM_SETTING_LAST];

};


SETTINGS_DECLARE(CVMIDI, CVM_SETTING_LAST) {
  {0, -2, 1, "Octave", NULL, settings::STORAGE_TYPE_I8},
  {0, -127, 127, "Velocity", NULL, settings::STORAGE_TYPE_I8},
  {1, 1, 16, "Channel", NULL, settings::STORAGE_TYPE_U8},
};



class CVMState {
public:

  void Init() {
    cursor.Init(CVM_SETTING_OCTAVE, CVM_SETTING_LAST - 1);
    quantizer.Init();
    velocity = 0;
    notes[0] = -1;
    notes[1] = -1;
    notes[2] = -1;
    gate_raised[0] = 0;
    gate_raised[1] = 0;
    gate_raised[2] = 0;
  }

  inline bool editing() const { return cursor.editing(); }

  inline int cursor_pos() const { return cursor.cursor_pos(); }

  menu::ScreenCursor<3> cursor;
  int8_t velocity;
  int8_t notes[3];
  int8_t gate_raised[3];
  OC::SemitoneQuantizer quantizer;

};

CVMIDI cvmsettings;
CVMState cvmstate;

void CVMIDI_init() {
  cvmsettings.InitDefaults();
  cvmsettings.Init();
  cvmstate.Init();
  //cvmstate.cursor.AdjustEnd(CVM_SETTING_LAST - 1);
}

size_t CVMIDI_storageSize() {
  return CVMIDI::storageSize();
}

size_t CVMIDI_save(void *storage) {
  return cvmsettings.Save(storage);
}

size_t CVMIDI_restore(const void *storage) {
  //cvmstate.cursor.AdjustEnd(CVM_SETTING_LAST - 1);
  return cvmsettings.Restore(storage);
}

void CVMIDI_handleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      cvmstate.cursor.set_editing(false);
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      //cvmstate.cursor.AdjustEnd(CVM_SETTING_LAST - 1);
      break;
  }
}

void CVMIDI_loop() {
}


void CVMIDI_menu() {

  menu::DefaultTitleBar::Draw();
  graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
  graphics.print("CV/Gate to USB MIDI");
  menu::SettingsList<3, 0, menu::kDefaultValueX> settings_list(cvmstate.cursor);
  menu::SettingsListItem list_item;
  while (settings_list.available()) {
    const int setting = cvmsettings.setting_at(settings_list.Next(list_item));
    const int value = cvmsettings.get_value(setting);
    const settings::value_attr &attr = CVMIDI::value_attr(setting); 
    list_item.DrawDefault(value, attr);
  }
}

int8_t calc_pitch(int voct_input) {
  int32_t pitch = cvmsettings.get_octave()*12 + cvmstate.quantizer.Process(OC::ADC::raw_pitch_value(voct_inputs[voct_input])) + 60;
  CONSTRAIN(pitch,0,127);
  return pitch;
}

uint8_t calc_velocity() {
  int32_t cvpart = cvmstate.quantizer.Process(OC::ADC::raw_pitch_value(velocity_input)) + 64 ;
  int32_t soffset = cvmsettings.get_velocity();
  int32_t velocity = cvpart+soffset;
  CONSTRAIN(velocity, 0, 127);
  return velocity;
}

void CVMIDI_screensaver() {
  int any_notes = 0;
  for (int i = 0; i < 3; i++) {
    if (cvmstate.gate_raised[i]) {
      graphics.setPrintPos(0, i*10);
      graphics.print("MIDI Note #");
      graphics.print(i+1);
      graphics.print(": ");
      graphics.print(calc_pitch(i));
      any_notes = 1;
    }
  }
  if (any_notes) {
    graphics.setPrintPos(0, 30);
    graphics.print("Velocity: ");
    graphics.print(calc_velocity());
  } else {
    graphics.setPrintPos(103, 10);
    graphics.print("need");
    graphics.setPrintPos(98, 20);
    graphics.print("sweet");
    graphics.setPrintPos(103, 30);
    graphics.print("gate");
    graphics.setPrintPos(103, 40);
    graphics.print("love");
    graphics.setPrintPos(109, 50);
    graphics.print(";-");
    graphics.setPrintPos(121, 48);
    graphics.print("*");
  }
}

void CVMIDI_handleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        break;
      case OC::CONTROL_BUTTON_DOWN:
        break;
      case OC::CONTROL_BUTTON_L:
        break;
      case OC::CONTROL_BUTTON_R:
        cvmstate.cursor.toggle_editing();
        break;
    }
  }
}

void CVMIDI_handleEncoderEvent(const UI::Event &event) {
  if (OC::CONTROL_ENCODER_R == event.control) {
    if (cvmstate.editing()) {
      CVMSettings setting = cvmsettings.setting_at(cvmstate.cursor_pos());
      if (cvmsettings.change_value(setting, event.value)) { }// force update
    } else {
      cvmstate.cursor.Scroll(event.value);
    }
  }
}



void FASTRUN CVMIDI_isr() {
    uint32_t triggers = OC::DigitalInputs::clocked();

    for (int trig_num = 0; trig_num < 3; trig_num++) {
      if (triggers & DIGITAL_INPUT_MASK(gate_inputs[trig_num])) {
        cvmstate.notes[trig_num] = calc_pitch(trig_num);
        usbMIDI.sendNoteOn(cvmstate.notes[trig_num], calc_velocity(), cvmsettings.get_channel());
        usbMIDI.send_now(); 
      }
      bool gate_raised = OC::DigitalInputs::read_immediate(gate_inputs[trig_num]);
      if (!gate_raised && cvmstate.gate_raised[trig_num]) {
        usbMIDI.sendNoteOff(cvmstate.notes[trig_num], 0, cvmsettings.get_channel());
        usbMIDI.send_now(); 
      }
      cvmstate.gate_raised[trig_num] = gate_raised;
    }
}





