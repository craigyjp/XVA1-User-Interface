//
// Created by André Mathlener on 08/04/2021.
//

#include "ParameterController.h"
#include "Globals.h"

ParameterController::ParameterController(Synthesizer *synthesizer, Multiplexer *multiplexer, TFT_eSPI *tft,
                                         Adafruit_SSD1306 *display, LEDButton *upButton, LEDButton *downButton)
        : synthesizer(synthesizer), multiplexer(multiplexer), tft(tft), display(display),
          upButton(upButton), downButton(downButton) {
    clearParameters();
}

void ParameterController::upButtonTapped() {
    if (section == nullptr) return;

    if (currentPageNumber > 0) {
        int newPage = currentPageNumber - 1;
        setActivePage(newPage);
    }
}

void ParameterController::downButtonTapped() {
    if (section == nullptr) return;

    if (currentPageNumber < getSection()->getNumberOfPages() - 1) {
        int newPage = currentPageNumber + 1;
        setActivePage(newPage);
    }
}

void ParameterController::setDefaultSection() {
    setSection(&defaultSection);
}

void ParameterController::setSection(Section *pSection) {
    SerialUSB.print(F("Setting active section: "));
    SerialUSB.println((pSection != nullptr) ? pSection->getName().c_str() : "null");

    ParameterController::section = pSection;
    currentSubSectionNumber = 0;

    if (section == nullptr) {
        upButton->setLED(false);
        downButton->setLED(false);
        clearParameters();
        displayParameters();
    } else {
        if (section != &defaultSection) {
            displaySubSections();
        }
        if (section->getNumberOfSubSections() > 0) {
            displayCurrentSubsection();
        }

//        SerialUSB.print("Number of parameters: ");
//        SerialUSB.println(section->getParameters().size());
//        SerialUSB.print("Number of pages: ");
//        SerialUSB.println(section->getNumberOfPages());

        setActivePage(0);
    }
}

void ParameterController::clearParameters() {
    for (auto &parameter : parameterIndices) {
        parameter = -1;
    }
}

void ParameterController::setActivePage(int pageNumber) {
    if (section == nullptr) return;

    if (pageNumber < 0 && pageNumber > getSection()->getNumberOfPages()) return;

    if (pageNumber != currentPageNumber) {
        SerialUSB.print(F("Setting active page: "));
        SerialUSB.println(pageNumber);
    }

    clearParameters();
    currentPageNumber = pageNumber;

    int start = (pageNumber * 8);
    int end = start + 7;
    if (end > getSection()->getParameters().size() - 1) {
        end = getSection()->getParameters().size() - 1;
    }

    int i = start;
    for (int &parameterIndex : parameterIndices) {
        parameterIndex = i <= end ? i : -1;
        i++;
    }

//    SerialUSB.println("Active parameters:");
//
//    for (int index : parameterIndices) {
//        if (index >= 0) {
//            SynthParameter param = getSection()->getParameters()[index];
//            SerialUSB.print(" - ");
//            SerialUSB.print(param.getName().c_str());
//            SerialUSB.print(" (");
//            SerialUSB.print(param.getNumber(0));
//            SerialUSB.println(")");
//        } else {
//            SerialUSB.println(" -");
//        }
//    }

    displayActivePage();
    displayParameters();
}

void ParameterController::setActiveSubSection(int subSectionNumber) {
    clearCurrentSubsection();

    if (section->getSubSections().size() > 0 && !section->hasVirtualSubSections()) {
        subSection = section->getSubSections().at(subSectionNumber);
    }
    currentSubSectionNumber = subSectionNumber;

    displayCurrentSubsection();
    setActivePage(currentPageNumber); // This redraws all parmeters
}

void ParameterController::displayActivePage() {
    if (section == nullptr) {
        upButton->setLED(false);
        downButton->setLED(false);
        return;
    };

    int numberOfPages = getSection()->getNumberOfPages();
    upButton->setLED(numberOfPages > 1 && currentPageNumber > 0);
    downButton->setLED(numberOfPages > 1 && currentPageNumber < numberOfPages - 1);
}

bool ParameterController::rotaryEncoderChanged(int id, bool clockwise, int speed) {
    if (section == nullptr) return false;

    if (id == 0) {
        int numberOfSubSections = section->getNumberOfSubSections();
        if (numberOfSubSections > 0) {
            if (clockwise) {
                if (currentSubSectionNumber + 1 < numberOfSubSections) {
                    setActiveSubSection(currentSubSectionNumber + 1);
                }
            } else {
                if (currentSubSectionNumber > 0) {
                    setActiveSubSection(currentSubSectionNumber - 1);
                }
            }
        }

        return true;
    } else {
        id--;
        int index = parameterIndices[id];
        if (index >= 0) {
            handleParameterChange(index, clockwise, speed);

            int index1 = (id % 2 == 0) ? id : id - 1;
            int index2 = index1 + 1;
            int displayNumber = (index1 / 2);

            displayTwinParameters(parameterIndices[index1], parameterIndices[index2], displayNumber);
            return true;
        }
    }

    return false;
}

bool ParameterController::rotaryEncoderButtonChanged(int id, bool released) {
    if (section == nullptr) return false;

    return false;
}

void ParameterController::handleParameterChange(int index, bool clockwise, int speed) {
    SynthParameter parameter = getSection()->getParameters()[index];
    int currentValue;

    int subIndex = (section->hasVirtualSubSections()) ? currentSubSectionNumber : 0;

    if (parameter.getType() == PERFORMANCE_CTRL) {
        byte msb = synthesizer->getParameter(parameter.getNumber(0));
        byte lsb = synthesizer->getParameter(parameter.getNumber(1));
        int combined = (msb << 7) + lsb;
        currentValue = combined;
    } else {
        currentValue = synthesizer->getParameter(parameter.getNumber(subIndex));
    }
    int newValue = -1;

    if (clockwise) {
        if (currentValue < parameter.getMax()) {
            newValue = currentValue + speed;
            if (newValue > parameter.getMax()) {
                newValue = parameter.getMax();
            }
        }
    } else {
        if (currentValue > parameter.getMin()) {
            newValue = currentValue - speed;
            if (newValue < parameter.getMin()) {
                newValue = parameter.getMin();
            }
        }
    }

    if (newValue >= 0 && newValue != currentValue) {
        if (parameter.getType() == PERFORMANCE_CTRL) {
            int msb = newValue >> 7;
            int lsb = newValue & 127;

            synthesizer->setParameter(parameter.getNumber(0), msb);
            synthesizer->setParameter(parameter.getNumber(1), lsb);
        } else {
            synthesizer->setParameter(parameter.getNumber(subIndex), newValue);
        }
    }
}

void ParameterController::displayTwinParameters(int index1, int index2, int displayNumber) {
    string name1 = (index1 >= 0) ? getSection()->getParameters()[index1].getName() : "";
    string name2 = (index2 >= 0) ? getSection()->getParameters()[index2].getName() : "";

    displayTwinParameters(name1, getDisplayValue(index1), name2, getDisplayValue(index2), displayNumber);
}

void ParameterController::displayTwinParameters(string title1, string value1, string title2, string value2,
                                                int displayNumber) {
    multiplexer->selectChannel(displayNumber);

    display->clearDisplay();

    display->setTextSize(1);
    display->setTextColor(WHITE);
    drawCenteredText(title1, 64, 0);

    display->setTextSize(2);
    drawCenteredText(value1, 64, 12);

    if (!title1.empty() || !value1.empty() || !title2.empty() || !value2.empty()) {
        display->drawLine(0, 30, display->width() - 1, 30, WHITE);
    }

    display->setTextSize(1);
    drawCenteredText(title2, 64, 34);

    display->setTextSize(2);
    drawCenteredText(value2, 64, 47);

    display->display();
}

void ParameterController::displayParameters() {
    for (int i = 0; i < 4; ++i) {
        int index1 = i * 2;
        int index2 = index1 + 1;
        displayTwinParameters(parameterIndices[index1], parameterIndices[index2], i);
    }
}

void ParameterController::drawCenteredText(string buf, int x, int y) {
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(buf.c_str(), 0, y, &x1, &y1, &w, &h); //calc width of new string
    if (x - w / 2 <= 0) {
        // Text won't fit. Set smallest text size and recompute
        display->setTextSize(1);
        display->getTextBounds(buf.c_str(), 0, y, &x1, &y1, &w, &h);
        display->setCursor(x - w / 2, y + (h / 2));
    } else {
        display->setCursor(x - w / 2, y);
    }
    display->print(buf.c_str());
}

string ParameterController::getDisplayValue(int parameterIndex) {
    string printValue = "";

    if (parameterIndex >= 0) {
        SynthParameter parameter = getSection()->getParameters()[parameterIndex];

        int subIndex = (section->hasVirtualSubSections()) ? currentSubSectionNumber : 0;

        int value;
        switch (parameter.getType()) {
            case PERFORMANCE_CTRL: {
                byte msb = synthesizer->getParameter(parameter.getNumber(0));
                byte lsb = synthesizer->getParameter(parameter.getNumber(1));
                int combined = (msb << 7) + lsb;

                value = (int) combined;

                break;
            }
            case CENTER_128: {
                value = (int) synthesizer->getParameter(parameter.getNumber(subIndex)) - 128;
                if (value > 0) {
                    printValue = "+" + to_string(value);
                }

                break;
            }
            default: {
                value = (int) synthesizer->getParameter(parameter.getNumber(subIndex));
            }
        };

        if (printValue.empty()) {
            if (value < parameter.getDescriptions().size()) {
                printValue = parameter.getDescriptions()[value];
            } else {
                printValue = to_string(value);
            }
        }
    }

    return printValue;
}

Section *ParameterController::getSection()  {
    if (section->getSubSections().size() > 0) {
        subSection = section->getSubSections().at(currentSubSectionNumber);
        return &subSection;
    }

    return section;
}

void ParameterController::displaySubSections() {
    tft->fillScreen(TFT_BLACK);
    tft->setCursor(0, 0, 1);

    // Set the font colour to be orange with a black background, set text size multiplier to 1
    tft->setTextColor(MY_ORANGE, TFT_BLACK);
    tft->setTextSize(2);
    tft->println(section->getName().c_str());

    int lineNumber = 0;

    SerialUSB.println(F("Sub-Sections:"));
    for (auto &title : section->getSubSectionTitles()) {
        SerialUSB.print(" - ");
        SerialUSB.println(title.c_str());

        tft->setTextColor(TFT_GREY, TFT_BLACK);
        tft->drawString(title.c_str(), 20, 40 + LINE_HEIGHT * lineNumber, 1);
        lineNumber++;
    }
}

void ParameterController::clearCurrentSubsection() {
    tft->setTextColor(TFT_GREY, TFT_BLACK);
    tft->drawString(" ", 0, 40 + LINE_HEIGHT * currentSubSectionNumber, 1);
    tft->drawString(section->getSubSectionTitles().at(currentSubSectionNumber).c_str(), 20, 40 + LINE_HEIGHT * currentSubSectionNumber, 1);
}

void ParameterController::displayCurrentSubsection() {
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString(">", 0, 40 + LINE_HEIGHT * currentSubSectionNumber, 1);
    tft->drawString(section->getSubSectionTitles().at(currentSubSectionNumber).c_str(), 20, 40 + LINE_HEIGHT * currentSubSectionNumber, 1);
}





