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

#include "input_source.h"
#include "opentx.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

class SensorValue : public StaticText
{
 public:
  SensorValue(Window *parent, const rect_t &rect, ExpoData *input) :
      StaticText(parent, rect), input(input)
  {
  }

//   void paint(BitmapBuffer *dc) override
//   {
//     if (isTelemetryValue()) {
//       uint8_t sensorIndex = (input->srcRaw - MIXSRC_FIRST_TELEM) / 3;
// #if defined(SIMU)
//       if (true) {
// #else
//       TelemetryItem &telemetryItem = telemetryItems[sensorIndex];
//       if (telemetryItem.isAvailable()) {
// #endif
//         LcdFlags flags = LEFT | COLOR_THEME_PRIMARY1;
//         drawSensorCustomValue(dc, 3, 2, sensorIndex, lastSensorVal, flags);
//       } else {
//         dc->drawText(3, 2, "---", COLOR_THEME_PRIMARY1);
//       }
//     }
//   }

  bool isTelemetryValue()
  {
    return input->srcRaw >= MIXSRC_FIRST_TELEM &&
           input->srcRaw <= MIXSRC_LAST_TELEM;
  }

  void checkEvents() override
  {
    if (lv_obj_has_flag(lvobj, LV_OBJ_FLAG_HIDDEN))
      return;

    // TODO: check for telemetry available
    if (isTelemetryValue()) {
      getvalue_t sensorVal = getValue(input->srcRaw);
      if (lastSensorVal != sensorVal) {
        lastSensorVal = sensorVal;
        setText(std::to_string(lastSensorVal));
      } 
    } else {
      setText("---");
    }
  }

  LcdFlags getSensorPrec()
  {
    LcdFlags prec = 0;
    if (isTelemetryValue()) {
      uint8_t sensorIndex = (input->srcRaw - MIXSRC_FIRST_TELEM) / 3;
      TelemetrySensor sensor = g_model.telemetrySensors[sensorIndex];
      if (sensor.prec > 0) {
        prec |= (sensor.prec == 1 ? PREC1 : PREC2);
      }
    }
    return prec;
  }

 protected:
  getvalue_t lastSensorVal;
  ExpoData *input;
};

void InputSource::value_changed(lv_event_t *e)
{
  auto obj = lv_event_get_target(e);
  auto src = (InputSource*)lv_obj_get_user_data(obj);
  if (!src) return;

  src->update();
}

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

InputSource::InputSource(Window* parent, ExpoData* input) :
  Window(parent, rect_t{}), input(input)
{
  lv_obj_set_flex_flow(lvobj, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(lvobj, lv_dpx(4), 0);
  lv_obj_set_size(lvobj, lv_pct(100), LV_SIZE_CONTENT);

  new SourceChoice(
      this, rect_t{}, INPUTSRC_FIRST, INPUTSRC_LAST, GET_DEFAULT(input->srcRaw),
      [=](int32_t newValue) {
        input->srcRaw = newValue;
        lv_event_send(lvobj, LV_EVENT_VALUE_CHANGED, nullptr);
        SET_DIRTY();
      });
  lv_obj_add_event_cb(lvobj, InputSource::value_changed, LV_EVENT_VALUE_CHANGED, nullptr);

  sensor_form = new FormGroup(this, rect_t{});
  sensor_form->setFlexLayout();
  auto sensor_form_obj = sensor_form->getLvObj();

  FlexGridLayout grid(col_dsc, row_dsc);
  auto line = sensor_form->newLine(&grid);

  // Value
  Window* w = new StaticText(line, rect_t{}, STR_VALUE, 0, COLOR_THEME_PRIMARY1);
  auto sensor = new SensorValue(line, rect_t{}, input);


  // Scale
  line = sensor_form->newLine(&grid);
  w = new StaticText(line, rect_t{}, STR_SCALE, 0, COLOR_THEME_PRIMARY1);
  w = new NumberEdit(line, rect_t{}, 0,
                 maxTelemValue(input->srcRaw - MIXSRC_FIRST_TELEM + 1),
                 GET_SET_DEFAULT(input->scale), 0, sensor->getSensorPrec());

  update();
}

void InputSource::update()
{
  if (input->srcRaw > MIXSRC_Ail && input->trimSource == TRIM_ON) {
    input->trimSource = TRIM_OFF;
  }

  if (!sensor_form) return;
  auto sensor_form_obj = sensor_form->getLvObj();
  
  if (input->srcRaw >= MIXSRC_FIRST_TELEM &&
      input->srcRaw <= MIXSRC_LAST_TELEM) {  
    lv_obj_clear_flag(sensor_form_obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(sensor_form_obj, LV_OBJ_FLAG_HIDDEN);
  }
}
