//
// Created by André Mathlener on 04/04/2021.
//

#ifndef XVA1USERINTERFACE_SYNTHPARAMETERBUILDER_H
#define XVA1USERINTERFACE_SYNTHPARAMETERBUILDER_H


#include "SynthParameter.h"

class SynthParameterBuilder {
    SynthParameter synthParameter;

public:
    explicit SynthParameterBuilder(std::string name) : synthParameter(name) {}

    SynthParameterBuilder &type(ParameterType type);
    SynthParameterBuilder &min(int min);
    SynthParameterBuilder &max(int max);
    SynthParameterBuilder &number(int number);
    SynthParameterBuilder &number2(int number2);
    SynthParameterBuilder &performanceControlType(int paramNumber1, int paramNumber2);

    SynthParameter create();
};


#endif //XVA1USERINTERFACE_SYNTHPARAMETERBUILDER_H
