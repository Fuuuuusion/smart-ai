#pragma once

#include <QString>

class MathParser
{
public:
    static bool evaluate(const QString &expression, double *result, QString *error = nullptr);
};

