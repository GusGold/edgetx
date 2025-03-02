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
#include "theme_manager.h"
#include "view_main.h"

#define MAX_FILES 9
ThemePersistance themePersistance;

static const char *colorNames[COLOR_COUNT] = {
    "DEFAULT",    "PRIMARY1",   "PRIMARY2",   "PRIMARY3",
    "SECONDARY1", "SECONDARY2", "SECONDARY3", "FOCUS",
    "EDIT",       "ACTIVE",     "WARNING",    "DISABLED", "CUSTOM",
};

#define HEX_COLOR_VALUE_LEN 8
constexpr const char *RGBSTRING = "RGB(";
constexpr const char* SELECTED_THEME_FILE = THEMES_PATH "/selectedtheme.txt";

ThemeFile::ThemeFile(std::string themePath) :
  path(themePath)
{
  if (path.size()) {
      deSerialize();
  }

  FIL file;
  auto found = path.rfind('/');
  if (found != std::string::npos) {
    int n = 0;
    while (n < MAX_FILES) {
      auto baseFileName(path.substr(0, found + 1) + (n != 0 ? "screenshot" + std::to_string(n) : "logo") + ".png");
      FRESULT result = f_open(&file, baseFileName.c_str(), FA_OPEN_EXISTING);
      if (result == FR_OK) {
        _imageFileNames.emplace_back(baseFileName);
        f_close(&file);
      } else {
        break;
      }

      n++;
    }
  }
}

ThemeFile& ThemeFile::operator= (const ThemeFile& theme)
{
  if (this == &theme)
    return *this;

  path = theme.path;
  strncpy(name, theme.name, NAME_LENGTH);
  strncpy(author, theme.author, AUTHOR_LENGTH);
  strncpy(info, theme.info, INFO_LENGTH);
  colorList.assign(theme.colorList.begin(), theme.colorList.end());
  _imageFileNames.assign(theme._imageFileNames.begin(), theme._imageFileNames.end());

  return *this;
};

std::vector<std::string> ThemeFile::getThemeImageFileNames()
{
  return _imageFileNames;
}

void ThemeFile::serialize()
{
  FIL file;
  FRESULT result = f_open(&file, path.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
  if (result == FR_OK) {
    f_printf(&file, "---\n");
    f_printf(&file, "summary:\n");
    f_printf(&file, "  name: %s\n", name);
    f_printf(&file, "  author: %s\n", author);
    f_printf(&file, "  info: %s\n", info);
    f_printf(&file, "\n");
    f_printf(&file, "colors:\n");

    for (auto colorEntry : colorList) {
      auto r = GET_RED(colorEntry.colorValue);
      auto g = GET_GREEN(colorEntry.colorValue);
      auto b = GET_BLUE(colorEntry.colorValue);

      std::string colorName(colorNames[colorEntry.colorNumber]);
      colorName += ":";

      f_printf(&file, "  %-11s 0x%02X%02X%02X\n", colorName.c_str(), r,g,b);
    }
    f_close(&file);
  }
}

void ThemeFile::deSerialize()
{
  char line[256 + 1];
  ScanState scanState = none;

  FIL file;
  FRESULT result = f_open(&file, path.c_str(), FA_OPEN_EXISTING | FA_READ);
  if (result != FR_OK) return;

  int lineNo = 1;
  while (readNextLine(file, line, 256)) {
    int len = strlen(line);
    if (len == 0) continue;

    if (lineNo == 1 && len != 3 && strcmp(line, "---") != 0) {
      TRACE("invalid yml file at line %d", lineNo);
      return;
    } else if (lineNo != 1) {
      if (line[0] != ' ' && line[0] != '\t') {
        char *pline = trim(line);
        if (line[strlen(line) - 1] != ':' && pline[0] != '#') {
          TRACE("invalid yml file at line %d", lineNo);
          return;
        }

        if (strcmp(pline, "colors:") == 0) {
          scanState = colors;
          continue;
        } else if (strcmp(pline, "summary:") == 0) {
          scanState = summary;
          continue;
        }
      }

      const char *ptr = strchr(line, ':');
      if (!ptr) continue;

      char lvalue[64];
      char rvalue[64];
      char *plvalue;
      char *prvalue;

      strncpy(lvalue, line, min((int)(ptr - line), 63));
      lvalue[ptr - line] = '\0';
      strncpy(rvalue, ptr + 1, 63);
      plvalue = trim(lvalue);
      prvalue = trim(rvalue);

      switch (scanState) {
        case colors: {
          int colorIndex = findColorIndex(plvalue);
          if (colorIndex >= 0) {
            uint32_t color;
            if (convertRGB(prvalue, color))
              colorList.emplace_back(ColorEntry{(LcdColorIndex)colorIndex, color});
            else
              TRACE("Theme: Could not convert color value");
          }
        } break;

        case summary: {
          if (strcmp(plvalue, "name") == 0) {
            strncpy(name, prvalue, NAME_LENGTH);
            name[NAME_LENGTH] = '\0';
          } else if (strcmp(plvalue, "author") == 0) {
            strncpy(author, prvalue, AUTHOR_LENGTH);
            author[AUTHOR_LENGTH] = '\0';
          } else if (strcmp(plvalue, "info") == 0) {
            strncpy(info, prvalue, INFO_LENGTH);
            info[INFO_LENGTH] = '\0';
          }
        } break;
      }
    }

    lineNo++;
  }

  f_close(&file);
}

bool ThemeFile::convertRGB(char *pColorRGB, uint32_t &color)
{
  if (strlen(pColorRGB) < strlen(RGBSTRING)) return false;

  if (strncmp(pColorRGB, RGBSTRING, strlen(RGBSTRING)) == 0) {
    if (pColorRGB[strlen(pColorRGB) - 1] != ')') return false;

    pColorRGB[strlen(pColorRGB) - 1] = '\0';
    pColorRGB += strlen(RGBSTRING);
    if (strlen(pColorRGB)) {
      char *token = nullptr;
      int numTokens = 0;
      uint32_t tokens[3];

      token = strtok(pColorRGB, ",");
      while (token != nullptr) {
        if (numTokens < 3) {
          tokens[numTokens] = strtol(token, nullptr, 0);
          numTokens++;
        } else
          break;

        token = strtok(nullptr, ",");
      }

      if (numTokens != 3) return false;

      color = RGB(tokens[0], tokens[1], tokens[2]);
      return true;
    }
  } else if (pColorRGB[0] == '0' && (pColorRGB[1] == 'x' || pColorRGB[1] == 'X')) {
    if (strlen(pColorRGB) != HEX_COLOR_VALUE_LEN) return false;
    pColorRGB += 2;
    uint32_t tokens[3];
    char hexVal[3];
    for (int i = 0; i < 3; i++) {
      strncpy(hexVal, pColorRGB, 2);
      hexVal[2] = '\0';
      tokens[i] = strtol(hexVal, nullptr, 16);
      pColorRGB += 2;
    }

    color = RGB(tokens[0], tokens[1], tokens[2]);
    return true;
  }

  TRACE("Theme: Invalid color value");
  return false;
}

LcdColorIndex ThemeFile::findColorIndex(const char *name)
{
  int i;
  for (i = 0; i < COLOR_COUNT; i++) {
    if (strcmp(name, colorNames[i]) == 0) break;
  }

  if (i >= COLOR_COUNT) return DEFAULT_COLOR_INDEX;

  return (LcdColorIndex)i;
}

bool ThemeFile::readNextLine(FIL &file, char *line, int maxlen)
{
  if (f_gets(line, maxlen, &file) != NULL) {
    int curlen = strlen(line) - 1;
    if (line[curlen] == '\n') {  // remove unwanted chars if file was edited using windows
      if (curlen > 0 && line[curlen - 1] == '\r') {
        line[curlen - 1] = 0;
      } else {
        line[curlen] = 0;
      }
    }

    return true;
  }

  return false;
}

void ThemeFile::setColorByIndex(int index, uint32_t color)
{
  if (index >= 0 && index < (int) colorList.size()) {
    colorList[index].colorValue = color;
  }
}

void ThemeFile::setColor(LcdColorIndex colorIndex, uint32_t color)
{
  if (colorIndex >= DEFAULT_COLOR_INDEX && colorIndex < LCD_COLOR_COUNT) {
    auto colorEntry = std::find(colorList.begin(), colorList.end(), ColorEntry { colorIndex, 0});
    if (colorEntry != colorList.end())
      colorEntry->colorValue = color;
    else
      colorList.emplace_back(ColorEntry {colorIndex, color});
  }
}

void ThemeFile::applyColors()
{
  for (auto color: colorList) {
      lcdColorTable[color.colorNumber] = color.colorValue;
  }
}

void ThemeFile::applyBackground()
{
  auto instance = OpenTxTheme::instance();
  std::string backgroundImageFileName(getPath());
  auto pos = backgroundImageFileName.rfind('/');
  if (pos != std::string::npos) {
    auto rootDir = backgroundImageFileName.substr(0, pos + 1);
    rootDir = rootDir + "background_" + std::to_string(LCD_W) + "x" +
              std::to_string(LCD_H) + ".png";

    if (isFileAvailable(rootDir.c_str())) {
      instance->setBackgroundImageFileName((char *)rootDir.c_str());
    } else {
      // TODO: This needs to be made user configurable, not
      // require the file be deleted to remove global background
      std::string fileName = THEMES_PATH PATH_SEPARATOR "EdgeTX/background.png";
      if (isFileAvailable(fileName.c_str())) {
        instance->setBackgroundImageFileName(fileName.c_str());
      } else {
        instance->setBackgroundImageFileName("");
      }
    }
  } else {
    instance->setBackgroundImageFileName("");
  }
}

void ThemeFile::applyTheme()
{
  applyColors();
  applyBackground();
  OpenTxTheme::instance()->update(false);

  // Update views with new theme
  // Currently, on startup, active theme is loaded after ViewMain is created so ViewMain instance is defined
  // In case this changes, we call getInstance() here to avoid creating ViewMain
  if (ViewMain::getInstance())
    ViewMain::getInstance()->updateFromTheme();
}

// avoid leaking memory
void ThemePersistance::clearThemes()
{
  for (auto theme: themes) {
    delete theme;
  }
  themes.clear();
}

void ThemePersistance::scanThemeFolder(char *fullPath)
{
  FIL file;
  strncat(fullPath, "/theme.yml", FF_MAX_LFN);
  FRESULT result = f_open(&file, fullPath, FA_OPEN_EXISTING | FA_READ);
  if (result != FR_OK) return;

  f_close(&file);
  TRACE("scanForThemes: found file %s", fullPath);
  themes.emplace_back(new ThemeFile(fullPath));
}

void ThemePersistance::scanForThemes()
{
  clearThemes();

  DIR dir;
  FILINFO fno;

  char fullPath[FF_MAX_LFN + 1];

  strncpy(fullPath, THEMES_PATH, FF_MAX_LFN);
  fullPath[FF_MAX_LFN] = '\0';

  TRACE("opening directory: %s", fullPath);
  FRESULT res = f_opendir(&dir, fullPath);  // Open the directory
  if (res == FR_OK) {
    TRACE("scanForThemes: open successful");
    // read all entries
    bool firstTime = true;
    for (;;) {
      res = sdReadDir(&dir, &fno, firstTime);

      if (res != FR_OK || fno.fname[0] == 0)
        break;  // Break on error or end of dir

      if (strlen((const char *)fno.fname) > SD_SCREEN_FILE_LENGTH) continue;
      if (fno.fattrib & AM_DIR) {
        char themePath[FF_MAX_LFN + 1];
        strncpy(themePath, fullPath, FF_MAX_LFN);
        strncat(themePath, "/", FF_MAX_LFN);
        strncat(themePath, fno.fname, FF_MAX_LFN);
        scanThemeFolder(themePath);
      }
    }

    f_closedir(&dir);
    std::sort(themes.begin(), themes.end(),
      [](ThemeFile *a, ThemeFile *b) {
          return strcmp(a->getName(), b->getName()) < 0;
      });
  }
}

void ThemePersistance::loadDefaultTheme()
{
  refresh();

  FIL file;
  FRESULT status = f_open(&file, SELECTED_THEME_FILE, FA_READ);
  if (status != FR_OK) return;

  char line[256];
  unsigned int len;

  status = f_read(&file, line, 256, &len);
  if (status == FR_OK) {

    line[len] = '\0';

    int index = 0;
    for (auto theme : themes) {
      if (theme->getPath() == std::string(line)) {
        applyTheme(index);
        setThemeIndex(index);
      }

      index++;
    }
  }
  f_close(&file);
}

char ** ThemePersistance::getColorNames()
{
  return (char **) colorNames;
}

bool ThemePersistance::deleteThemeByIndex(int index)
{
  // greater than 0 is intentional here.  cant delete default theme.
  if (index > 0 && index < (int) themes.size()) {
    ThemeFile* theme = themes[index];
    
    char newFile[FF_MAX_LFN + 1];
    strncpy(newFile, theme->getPath().c_str(), FF_MAX_LFN);
    strcat(newFile, ".deleted");

    // for now we are just renaming the file so we don't find it
    FRESULT status = f_rename(theme->getPath().c_str(), newFile);
    refresh();
    
    // make sure currentTheme stays in bounds
    if (getThemeIndex() >= (int) themes.size())
      setThemeIndex(themes.size() - 1);

    return status == FR_OK;
  }
  return false;
}

bool ThemePersistance::createNewTheme(std::string name, ThemeFile &theme)
{
  char fullPath[FF_MAX_LFN + 1];
  strncpy(fullPath, THEMES_PATH, FF_MAX_LFN);
  fullPath[FF_MAX_LFN] = '\0';
  strncat(fullPath, "/", FF_MAX_LFN);
  strncat(fullPath, name.c_str(), FF_MAX_LFN);

  FRESULT result = f_mkdir(fullPath);
  if (result != FR_OK) return false;
  strncat(fullPath, "/", FF_MAX_LFN);
  strncat(fullPath, "theme.yml", FF_MAX_LFN);
  theme.setPath(fullPath);
  theme.serialize();
  return true;
}

void ThemePersistance::deleteDefaultTheme()
{
  FIL file;
  FRESULT status = f_open(&file, SELECTED_THEME_FILE, FA_CREATE_ALWAYS | FA_WRITE);
  if (status == FR_OK) f_close(&file);
}

void ThemePersistance::setDefaultTheme(int index)
{
  FIL file;
  if (index >= 0 && index < (int) themes.size()) {
    auto theme = themes[index];
    FRESULT status = f_open(&file, SELECTED_THEME_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (status != FR_OK) return;

    currentTheme = index;
    f_printf(&file, theme->getPath().c_str());
    f_close(&file);
  }
}

class DefaultEdgeTxTheme : public ThemeFile
{
  public:
    DefaultEdgeTxTheme() : ThemeFile("")
    {
      setName("EdgeTX Default");
      setAuthor("EdgeTX Team");
      setInfo("Default EdgeTX Color Scheme");
    
      // initializze the default color table
      colorList.emplace_back(ColorEntry { COLOR_THEME_PRIMARY1_INDEX, RGB(0, 0, 0) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_PRIMARY2_INDEX, RGB(255, 255, 255) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_PRIMARY3_INDEX, RGB(12, 63, 102) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_SECONDARY1_INDEX, RGB(18, 94, 153) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_SECONDARY2_INDEX, RGB(182, 224, 242) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_SECONDARY3_INDEX, RGB(228, 238, 242) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_FOCUS_INDEX, RGB(20, 161, 229) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_EDIT_INDEX, RGB(0, 153, 9) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_ACTIVE_INDEX, RGB(255, 222, 0) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_WARNING_INDEX, RGB(224, 0, 0) });
      colorList.emplace_back(ColorEntry { COLOR_THEME_DISABLED_INDEX, RGB(140, 140, 140) });
    }

    void applyBackground() override
    {
      auto instance = OpenTxTheme::instance();
      char fileName[FF_MAX_LFN + 1];
      fileName[FF_MAX_LFN] = '\0';
      strncpy(fileName, THEMES_PATH, FF_MAX_LFN);
      strcat(fileName, "/EdgeTX/background.png");
      instance->setBackgroundImageFileName(fileName);
    }

    std::vector<std::string> getThemeImageFileNames() override
    {
      std::vector<std::string> fileNames;
      fileNames.emplace_back("/THEMES/EdgeTX/EdgeTX.png");
      return fileNames;
    }
};

void ThemePersistance::insertDefaultTheme()
{
  auto themeFile = new DefaultEdgeTxTheme();
  themes.insert(themes.begin(), themeFile);
}

