/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "afhds3_options.h"
#include "opentx.h"

//#include "checkbox.h"

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT,
                                     LV_GRID_TEMPLATE_LAST};

#define SET_DIRTY()

static const char* _analog_outputs[] = { "PWM", "PPM" };
static const char* _bus_types[] = { "iBUS OUT", "iBUS IN", "SBUS" };
static const char* _v1_bus_types[] = { "PWM", "PPM", "SBUS", "iBUS IN", "iBUS OUT" };
static const char* _v1_pwmfreq_types[] = { STR_ANALOG_SERVO, STR_DIGITAL_SERVO, "SR833HZ", "SFR1000HZ", STR_MULTI_CUSTOM };
static const char* _v0_pwmfreq_types[] = { STR_ANALOG_SERVO, STR_DIGITAL_SERVO, STR_MULTI_CUSTOM };

static const uint16_t _v1_index2pwmvalue[] = {50, 333, 2, 1, 50};
static const uint16_t _v0_index2pwmvalue[] = {50, 333, 50};
static uint16_t _v1_pwmvalue_type[2][32] = {0xff};
static uint8_t channel_num[]={18, 10, 18, 8, 12};



static void pwmfreq_changed(lv_event_t* e)
{
  auto obj = lv_event_get_target(e);
  auto dat = (Choice*)lv_obj_get_user_data(obj);
  if (!dat) return;

  auto num_edit_obj = (lv_obj_t*)lv_event_get_user_data(e);
  if (!num_edit_obj) return;

  if (dat->getIntValue() == 4) {
    lv_obj_clear_flag(num_edit_obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(num_edit_obj, LV_OBJ_FLAG_HIDDEN);
  }
}
static void pwmfreq_changedV0(lv_event_t* e)
{
  auto obj = lv_event_get_target(e);
  auto dat = (Choice*)lv_obj_get_user_data(obj);
  if (!dat) return;

  auto num_edit_obj = (lv_obj_t*)lv_event_get_user_data(e);
  if (!num_edit_obj) return;

  if (dat->getIntValue() == 2) {
    lv_obj_clear_flag(num_edit_obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(num_edit_obj, LV_OBJ_FLAG_HIDDEN);
  }
}
PWMfrequencyChoice::PWMfrequencyChoice(Window* parent, uint8_t moduleIdx, uint8_t channelIdx) :
  FormGroup(parent, rect_t{})
{
  setFlexLayout(LV_FLEX_FLOW_ROW);
  lv_obj_set_width(lvobj, LV_SIZE_CONTENT);
  uint16_t &pwmvalue_type = _v1_pwmvalue_type[moduleIdx][channelIdx];
  auto vCfg = &afhds3::getConfig(moduleIdx)->v1;
  if( 0xff ==pwmvalue_type )
  {
    if ( 50 == vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] ) pwmvalue_type = 0;
    else if ( 333 == vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] ) pwmvalue_type = 1;
    else if ( 2 == vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] ) pwmvalue_type = 2;
    else if ( 1 == vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] ) pwmvalue_type = 3;
    else  pwmvalue_type = 4;
  }
  auto choice = new Choice(this, rect_t{}, _v1_pwmfreq_types, 0, 4,
                [=,&pwmvalue_type]{
                      return pwmvalue_type;
                  },
                [=,&pwmvalue_type](int32_t newValue) {
                      pwmvalue_type=newValue;
                      vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] = _v1_index2pwmvalue[newValue];
                    });
    auto num_edit = new NumberEdit(this, rect_t{}, 50, 400,
                   [=,&pwmvalue_type] { return (pwmvalue_type==4?vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx]:50); },
                   [=](int16_t newVal) {
                       vCfg->PWMFrequenciesV1.PWMFrequencies[channelIdx] = newVal;
                   });
  c_obj = choice->getLvObj();
  auto btn_obj = num_edit->getLvObj();
  lv_obj_add_event_cb(c_obj, pwmfreq_changed, LV_EVENT_VALUE_CHANGED, btn_obj);
  lv_event_send(c_obj, LV_EVENT_VALUE_CHANGED, nullptr);
}

PWMfrequencyChoice::PWMfrequencyChoice(Window* parent, uint8_t moduleIdx ) :
  FormGroup(parent, rect_t{})
{
  setFlexLayout(LV_FLEX_FLOW_ROW);
  lv_obj_set_width(lvobj, LV_SIZE_CONTENT);
  uint16_t &pwmvalue_type = _v1_pwmvalue_type[moduleIdx][0];
  auto vCfg = &afhds3::getConfig(moduleIdx)->v0;
  if( 0xff ==pwmvalue_type )
  {
    if ( 50 == (vCfg->PWMFrequency.Frequency&0x7fff) ) pwmvalue_type = 0;
    else if ( 333 == (vCfg->PWMFrequency.Frequency&0x7fff) ) pwmvalue_type = 1;
    else  pwmvalue_type = 2;
  }
  auto choice = new Choice(this, rect_t{}, _v0_pwmfreq_types, 0, 2,
                [=,&pwmvalue_type]{
                      return pwmvalue_type;
                  },
                [=,&pwmvalue_type](int32_t newValue) {
                      pwmvalue_type=newValue;
                      vCfg->PWMFrequency.Frequency = _v0_index2pwmvalue[newValue];
                    });
    auto num_edit = new NumberEdit(this, rect_t{}, 50, 400,
                   [=,&pwmvalue_type] { return (pwmvalue_type==2?vCfg->PWMFrequency.Frequency&0x7fff:50); },
                   [=](int16_t newVal) {
                       vCfg->PWMFrequency.Frequency = newVal;
                   });
  c_obj = choice->getLvObj();
  auto btn_obj = num_edit->getLvObj();
  lv_obj_add_event_cb(c_obj, pwmfreq_changedV0, LV_EVENT_VALUE_CHANGED, btn_obj);
  lv_event_send(c_obj, LV_EVENT_VALUE_CHANGED, nullptr);
}

void PWMfrequencyChoice::update() const
{
  lv_event_send(c_obj, LV_EVENT_VALUE_CHANGED, nullptr);
}



AFHDS3_Options::AFHDS3_Options(uint8_t moduleIdx) : Page(ICON_MODEL_SETUP)
{
  cfg = afhds3::getConfig(moduleIdx);
  std::string title =
      moduleIdx == INTERNAL_MODULE ? STR_INTERNALRF : STR_EXTERNALRF;
  title = "\nAFHDS3 (";
  title += (moduleIdx == INTERNAL_MODULE ? "INRM301" : "FRM303");
  title += ")";
  header.setTitle(title);

  auto form = new FormGroup(&body, rect_t{});
  form->setFlexLayout();
  form->padAll(lv_dpx(8));

  FlexGridLayout grid(col_dsc, row_dsc, 2);

  if (cfg->version == 0) {
    auto vCfg = &cfg->v0;

    auto line = form->newLine(&grid);
    std::string temp_str ="PWM";
    temp_str += TR_POWERMETER_FREQ;
    new StaticText(line, rect_t{}, temp_str);
    new PWMfrequencyChoice(line, moduleIdx );
    line = form->newLine(&grid);

    temp_str = "PWM";
    temp_str += " ";
    temp_str += STR_SYNC;
    new StaticText(line, rect_t{}, temp_str);
    new CheckBox(line, rect_t{}, GET_SET_DEFAULT(vCfg->PWMFrequency.Synchronized));
    line = form->newLine(&grid);

    temp_str = STR_CH;
    temp_str += " 1";
    new StaticText(line, rect_t{}, temp_str );
    new Choice(line, rect_t{}, _analog_outputs,
               afhds3::SES_ANALOG_OUTPUT_PWM, afhds3::SES_ANALOG_OUTPUT_PPM,
               GET_SET_DEFAULT(vCfg->AnalogOutput));

    line = form->newLine(&grid);
    new StaticText(line, rect_t{}, STR_SERIAL_BUS);
    new Choice(line, rect_t{}, _bus_types,
               0, 2,
               GET_SET_DEFAULT(cfg->BusType.ExternalBusType));

  } else {
    auto vCfg = &cfg->v1;
    for (uint8_t i = 0; i < channel_num[vCfg->PhyMode]; i++) {
      std::string temp_str = STR_CH;
      temp_str += " " + std::to_string(i+1);
      auto line = form->newLine(&grid);
      new StaticText(line, rect_t{}, temp_str);
      new PWMfrequencyChoice(line, moduleIdx, i);
      line = form->newLine(&grid);

      temp_str = "PWM";
      temp_str += " ";
      temp_str += STR_SYNC;
      new StaticText(line, rect_t{}, temp_str);
      new CheckBox(
          line, rect_t{}, GET_DEFAULT((vCfg->PWMFrequenciesV1.Synchronized&1<<i)>>i),
          [=](uint8_t newVal) {
            vCfg->PWMFrequenciesV1.Synchronized &= ~(1<<i);
            vCfg->PWMFrequenciesV1.Synchronized |= (newVal?1:0)<<i;
          });
    }

    for (uint8_t i = 0; i < SES_NPT_NB_MAX_PORTS; i++) {
      auto line = form->newLine(&grid);
      std::string portName = "NP";
      portName += 'A' + i;
      new StaticText(line, rect_t{}, portName.c_str());
      new Choice(line, rect_t{}, _v1_bus_types, afhds3::SES_NPT_PWM,
                 afhds3::SES_NPT_IBUS1_OUT,
                 GET_DEFAULT(vCfg->NewPortTypes[i]),
                 [=](int32_t newValue) {
                  if(!newValue)
                    vCfg->NewPortTypes[i] = newValue;
                  else {
                    uint8_t j = 0;
                    for ( j = 0; j < SES_NPT_NB_MAX_PORTS; j++) {
                      if ( vCfg->NewPortTypes[j]== newValue && i != j )
                        break;
                    }
                    //The RX does not support two or more ports to output IBUS (the same is true for PPM and SBUS).
                    if(j==SES_NPT_NB_MAX_PORTS ) vCfg->NewPortTypes[i] = newValue;
                  }
      });
    }
  }
  auto line = form->newLine(&grid);
  new StaticText(line, rect_t{}, STR_SIGNAL_OUTPUT);
  std::vector<std::string> signed_strength_ch;
  signed_strength_ch.emplace_back(STR_OFF);
  for (int i = 0; i < channel_num[cfg->v1.PhyMode]; i++) {
    std::string temstr = STR_CH;
    temstr += " " + std::to_string(i + 1);
    signed_strength_ch.emplace_back(temstr);
  }
  new Choice(line, rect_t{}, signed_strength_ch,
               0, channel_num[cfg->v1.PhyMode],
               [=] { return cfg->v1.SignalStrengthRCChannelNb==0xff?0:cfg->v1.SignalStrengthRCChannelNb+1; },
               [=](int32_t newValue) { newValue?cfg->v1.SignalStrengthRCChannelNb = newValue-1:cfg->v1.SignalStrengthRCChannelNb=0xff;});
}
