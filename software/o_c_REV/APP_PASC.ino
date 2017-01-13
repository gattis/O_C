// Copyright (c) 2016 Patrick Dowling, Tim Churches
//
// Initial app implementation: Patrick Dowling (pld@gurkenkiste.com)
// Modifications by: Tim Churches (tim.churches@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "util/util_math.h"
#include "util/util_settings.h"
#include "pascaline/pascaline.h"

#define GRD_ROWS 5 
#define GRD_COLS 20
#define EQN_LEN 58
#define WAVETABLE_SIZE 128

static const weegfx::coord_t kGrdXStart = 2;
static const weegfx::coord_t kGrdYStart = 14;
static const weegfx::coord_t kGrdH = 9;
static const weegfx::coord_t kGrdW = 6;

const char kbd_keys[] = "<>0123456789.-+*/^()tsinqrcoexpla @";
const char initial_eqn[EQN_LEN+1] = "sin(t)                                                    ";

enum PascSettings {
  PASC_SETTING_FREQ,
  PASC_SETTING_TRIGGER_INPUT,
  PASC_SETTING_AMP,
  PASC_SETTING_OFFSET,
  PASC_SETTING_PHASE,
  PASC_SETTING_CYCLES_PER_TRIG,
  PASC_SETTING_LAST
};

class PascGenerator : public settings::SettingsBase<PascGenerator, PASC_SETTING_LAST> {
public:

  void Init(OC::DigitalInput default_trigger);
  
  uint16_t get_freq() const {
    return values_[PASC_SETTING_FREQ];
  }

  int8_t get_phase() const {
    return values_[PASC_SETTING_PHASE];
  }

  OC::DigitalInput get_trigger_input() const {
    return static_cast<OC::DigitalInput>(values_[PASC_SETTING_TRIGGER_INPUT]);
  }

  template <DAC_CHANNEL dac_channel>
  void Update(uint32_t triggers) {

    OC::DigitalInput trigger_input = get_trigger_input();
    if (trigger_input != OC::DigitalInput::DIGITAL_INPUT_LAST) {
       bool triggered = triggers & DIGITAL_INPUT_MASK(trigger_input);
       trigger_display_.Update(1, triggered);
       // TODO: reset oscillator
    }

  }

  uint8_t getTriggerState() const {
    return trigger_display_.getState();
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  PascSettings enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  void update_enabled_settings() {
    PascSettings *settings = enabled_settings_;
    *settings++ = PASC_SETTING_FREQ;
    *settings++ = PASC_SETTING_TRIGGER_INPUT;
    *settings++ = PASC_SETTING_AMP;
    *settings++ = PASC_SETTING_OFFSET;
    *settings++ = PASC_SETTING_PHASE;
    *settings++ = PASC_SETTING_CYCLES_PER_TRIG;
    num_enabled_settings_ = settings - enabled_settings_;
  }

  int equation_editing_pos;
  char equation[EQN_LEN+1];
  uint16_t wavetable[WAVETABLE_SIZE];

private:
  OC::DigitalInputDisplay trigger_display_;
  int num_enabled_settings_;
  PascSettings enabled_settings_[PASC_SETTING_LAST];

};

void PascGenerator::Init(OC::DigitalInput default_trigger) {
  InitDefaults();
  apply_value(PASC_SETTING_TRIGGER_INPUT, default_trigger);
  for (int i = 0; i < WAVETABLE_SIZE; i++) wavetable[i] = 0;
  strcpy(equation,initial_eqn);
  trigger_display_.Init();
  update_enabled_settings();
  equation_editing_pos = 0;
}

const char* const my_trig_names[5] = {
  "TR1", "TR2", "TR3", "TR4", "Free"
};

SETTINGS_DECLARE(PascGenerator, PASC_SETTING_LAST) {
  { 128, 1, 65535, "Frequency", NULL, settings::STORAGE_TYPE_U16},
  { OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_LAST, "Trigger input", my_trig_names, settings::STORAGE_TYPE_U4 },
  { 100, 100, 100, "Amplitude", NULL, settings::STORAGE_TYPE_U8},
  { 0, 0, 0, "DC Offset", NULL, settings::STORAGE_TYPE_U8},
  { 0, -127, 128, "Phase", NULL, settings::STORAGE_TYPE_I8},
  { 1, 1, 1, "Cycles / trig", NULL, settings::STORAGE_TYPE_U8},
};

class QuadPascGenerator {
public:

  void Init() {
    int input = OC::DIGITAL_INPUT_1;
    for (auto &pasc : pascs_) {
      pasc.Init(static_cast<OC::DigitalInput>(input));
      ++input;
    }
    strcpy(pascs_[0].equation,"sin(t)^3");
    pascs_[0].equation[8] = ' ';
    strcpy(pascs_[1].equation,"0.75*cos(t)-0.325*cos(2*t)-0.15*cos(3*t)-0.05*cos(4*t)");
    pascs_[1].equation[54] = ' ';


    ui.edit_mode = MODE_EDIT_EQNS;
    ui.selected_channel = 0;
    ui.eqn_editing = false;
    ui.cursor.Init(0, pascs_[0].num_enabled_settings() - 1);
    ui.kbd_position = 0;
  }

  void ISR() {
    uint32_t triggers = OC::DigitalInputs::clocked();
    pascs_[0].Update<DAC_CHANNEL_A>(triggers);
    pascs_[1].Update<DAC_CHANNEL_B>(triggers);
    pascs_[2].Update<DAC_CHANNEL_C>(triggers);
    pascs_[3].Update<DAC_CHANNEL_D>(triggers);
  }

  enum PascEditMode {
    MODE_EDIT_EQNS,
    MODE_EDIT_SETTINGS
  };

  struct {
    PascEditMode edit_mode;
    int selected_channel;
    bool eqn_editing;
    menu::ScreenCursor<menu::kScreenLines> cursor;
    int kbd_position;
  } ui;

  PascGenerator &selected() {
    return pascs_[ui.selected_channel];
  }

  PascGenerator pascs_[4];

};

QuadPascGenerator pascgen;

void PASC_init() {
  pascgen.Init();
}

size_t PASC_storageSize() {
  return 4 * PascGenerator::storageSize();
}

size_t PASC_save(void *storage) {
  size_t s = 0;
  for (auto &pasc : pascgen.pascs_)
    s += pasc.Save(static_cast<byte *>(storage) + s);
  return s;
}

size_t PASC_restore(const void *storage) {
  size_t s = 0;
  for (auto &pasc : pascgen.pascs_) {
    s += pasc.Restore(static_cast<const byte *>(storage) + s);
    pasc.update_enabled_settings();
  }
  pascgen.ui.cursor.AdjustEnd(pascgen.pascs_[0].num_enabled_settings() - 1);
  return s;
}

void PASC_handleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      break;
  }
}

void PASC_loop() {
  
}

void PASC_menu_preview() {

  auto const &pasc = pascgen.selected();

  graphics.drawHLine(0, kGrdYStart + 3*kGrdH-2, menu::kDisplayWidth);

  for (int row = 0; row < GRD_ROWS; row++) {
    for (int col = 0; col < GRD_COLS; col++) {
      weegfx::coord_t x = kGrdXStart + col * kGrdW;
      weegfx::coord_t y = kGrdYStart + row * kGrdH;
      graphics.setPrintPos(x,y);
      if (row == 0 && col == 0) graphics.print("y");
      else if (row == 0 && col == 1) graphics.print("=");
      else if (row < 3) {
        int eqn_pos = row*GRD_COLS+col-2;
        if (eqn_pos < EQN_LEN)
          graphics.print(pasc.equation[eqn_pos]);
        if (pasc.equation_editing_pos == eqn_pos) graphics.drawFrame(x-1, y-2, kGrdW+3, kGrdH+2);
      } else {
        int kbd_pos = (row-3)*GRD_COLS+col;
        if (kbd_pos < (int)strlen(kbd_keys)) {
          graphics.print(kbd_keys[kbd_pos]);
          if (kbd_pos == pascgen.ui.kbd_position) graphics.drawFrame(x-1, y-2, kGrdW+3, kGrdH+2);
        } else if (kbd_pos == (int)strlen(kbd_keys)){
          //graphics.printf("%d",pasc.wavetable[0]);
        }
      }
    }
  }
}

void PASC_menu_settings() {
  auto const &pasc = pascgen.selected();

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(pascgen.ui.cursor);
  menu::SettingsListItem list_item;
  while (settings_list.available()) {
    const int setting = pasc.enabled_setting_at(settings_list.Next(list_item));
    const int value = pasc.get_value(setting);
    const settings::value_attr &attr = PascGenerator::value_attr(setting);
    list_item.DrawDefault(value, attr);
  }
}

void PASC_menu() {

  menu::QuadTitleBar::Draw();
  for (uint_fast8_t i = 0; i < 4; ++i) {
    menu::QuadTitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
    menu::QuadTitleBar::DrawGateIndicator(i, pascgen.pascs_[i].getTriggerState());

  }

  menu::QuadTitleBar::Selected(pascgen.ui.selected_channel);

  if (QuadPascGenerator::MODE_EDIT_EQNS == pascgen.ui.edit_mode)
    PASC_menu_preview();
  else
    PASC_menu_settings();
}

void PASC_topButton() {
  auto &selected_pasc = pascgen.selected();
  selected_pasc.change_value(PASC_SETTING_FREQ, 8);
}

void PASC_lowerButton() {
  auto &selected_pasc = pascgen.selected();
  selected_pasc.change_value(PASC_SETTING_FREQ, -8); 
}


double dubpi = PI*2;
double wtsize = WAVETABLE_SIZE;

void PASC_rightButton() {

  if (QuadPascGenerator::MODE_EDIT_EQNS == pascgen.ui.edit_mode) {
    PascGenerator &pasc = pascgen.selected();
    char key = kbd_keys[pascgen.ui.kbd_position];
    if (key == '<') {
      pasc.equation_editing_pos -= 1;
      CONSTRAIN(pasc.equation_editing_pos, 0, EQN_LEN - 1);
    } else if (key == '>') {
      pasc.equation_editing_pos += 1;
      CONSTRAIN(pasc.equation_editing_pos, 0, EQN_LEN - 1);
    } else if (key == '@') {
      for (int i = 0; i < WAVETABLE_SIZE; i++) {
        double phase = i / wtsize * dubpi;
        yysett(phase);
        yysetbuf(pasc.equation);
        yyparse();
        double retval = round(32767.5*(yygetretval()+1.0));
        CONSTRAIN(retval,0,65535);
        pasc.wavetable[i] = retval;
      }

    } else {
      pasc.equation[pasc.equation_editing_pos] = key;
      pasc.equation_editing_pos += 1;
      CONSTRAIN(pasc.equation_editing_pos, 0, EQN_LEN - 1);
    }

  } else {
    pascgen.ui.cursor.toggle_editing();
  }
}

void PASC_leftButton() {
  if (QuadPascGenerator::MODE_EDIT_SETTINGS == pascgen.ui.edit_mode) {
    pascgen.ui.edit_mode = QuadPascGenerator::MODE_EDIT_EQNS;
    pascgen.ui.cursor.set_editing(false);
  } else {
    pascgen.ui.edit_mode = QuadPascGenerator::MODE_EDIT_SETTINGS;
  }
}

void PASC_handleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        PASC_topButton();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        PASC_lowerButton();
        break;
      case OC::CONTROL_BUTTON_L:
        PASC_leftButton();
        break;
      case OC::CONTROL_BUTTON_R:
        PASC_rightButton();
        break;
    }
  }
}

void PASC_handleEncoderEvent(const UI::Event &event) {

  if (OC::CONTROL_ENCODER_L == event.control) {
    int left_value = pascgen.ui.selected_channel + event.value;
    CONSTRAIN(left_value, 0, 3);
    pascgen.ui.selected_channel = left_value;
    auto &selected_pasc = pascgen.selected();
    pascgen.ui.cursor.AdjustEnd(selected_pasc.num_enabled_settings() - 1);
    // TODO: update eqn display for new channel
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (QuadPascGenerator::MODE_EDIT_EQNS == pascgen.ui.edit_mode) {
      pascgen.ui.kbd_position += event.value;
      CONSTRAIN(pascgen.ui.kbd_position,0,(int)strlen(kbd_keys)-1);
    } else {
      if (pascgen.ui.cursor.editing()) {
        auto &selected_pasc = pascgen.selected();
        PascSettings setting = selected_pasc.enabled_setting_at(pascgen.ui.cursor.cursor_pos());
        selected_pasc.change_value(setting, event.value);
      } else {
        pascgen.ui.cursor.Scroll(event.value);
      }
    }
  }
}

double screenmax = 63.0;
double maxwavetablelev = 65535.0;
double screenscale = screenmax/maxwavetablelev;
void PASC_screensaver() {
  weegfx::coord_t x,y;
  for (auto &pasc : pascgen.pascs_) {
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
      x = i;
      uint8_t pos = (i + 128 + pasc.get_phase()) % 128;
      y = 63 - (int) round(pasc.wavetable[pos]*screenscale);
      graphics.setPixel(x,y);
    }
    return;
  }
}

int ticks = 0;
void FASTRUN PASC_isr() {
     uint8_t pos = (ticks/3 + 128 + pascgen.pascs_[0].get_phase()) % 128;
     OC::DAC::set<DAC_CHANNEL_A>(pascgen.pascs_[0].wavetable[pos]);
     pos = (ticks/3 + 128 + pascgen.pascs_[1].get_phase()) % 128;
     OC::DAC::set<DAC_CHANNEL_B>(pascgen.pascs_[1].wavetable[pos]);
     ticks = (ticks + 1) % 384;
     pascgen.ISR();
}
