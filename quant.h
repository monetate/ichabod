#ifndef QUANT_H
#define QUANT_H

#include <QImage>

enum QuantizeMethod
{
    QuantizeMethod_THRESHOLD,
    QuantizeMethod_DIFFUSE,
    QuantizeMethod_ORDERED,
    QuantizeMethod_MEDIANCUT,
    QuantizeMethod_MEDIANCUT_FLOYD
};

QuantizeMethod toQuantizeMethod( const QString& s );
QImage quantize_mediancut( const QImage& src, bool use_floyd );

#endif
