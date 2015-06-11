#ifndef AGIF_H
#define AGIF_H

#include <QVector>
#include <QImage>
#include "quant.h"

bool gifWrite ( const QuantizeMethod method, const QVector<QImage> & images, const QVector<int>& delays, 
                const QVector< QRect >& crops, const QString& filename, bool loop = false );

#endif
