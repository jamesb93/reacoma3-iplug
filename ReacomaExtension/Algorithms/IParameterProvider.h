#pragma once

class IParameterProvider {
public:
    virtual ~IParameterProvider() = default;
    virtual double GetParameterValue(int globalIdx) const = 0;
};
