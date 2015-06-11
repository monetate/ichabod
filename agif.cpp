#include <stdio.h>
#include "agif.h"
#include <gif_lib.h>
#include "quant.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <map>

#include <QColor>
#include <QRgb>
#include <QHash>
#include <QMultiMap>
#include <QPainter>
#include <QVariant>

typedef QList< QPair<QRgb, int> > QgsColorBox; //Color / number of pixels
typedef QMultiMap< int, QgsColorBox > QgsColorBoxMap; // sum of pixels / color box


bool redCompare( const QPair<QRgb, int>& c1, const QPair<QRgb, int>& c2 )
{
    return qRed( c1.first ) < qRed( c2.first );
}

bool greenCompare( const QPair<QRgb, int>& c1, const QPair<QRgb, int>& c2 )
{
    return qGreen( c1.first ) < qGreen( c2.first );
}

bool blueCompare( const QPair<QRgb, int>& c1, const QPair<QRgb, int>& c2 )
{
    return qBlue( c1.first ) < qBlue( c2.first );
}

bool alphaCompare( const QPair<QRgb, int>& c1, const QPair<QRgb, int>& c2 )
{
    return qAlpha( c1.first ) < qAlpha( c2.first );
}

bool minMaxRange( const QgsColorBox& colorBox, int& redRange, int& greenRange, int& blueRange, int& alphaRange )
{
    if ( colorBox.size() < 1 )
    {
        return false;
    }

    int rMin = INT_MAX;
    int gMin = INT_MAX;
    int bMin = INT_MAX;
    int aMin = INT_MAX;
    int rMax = INT_MIN;
    int gMax = INT_MIN;
    int bMax = INT_MIN;
    int aMax = INT_MIN;

    int currentRed = 0; int currentGreen = 0; int currentBlue = 0; int currentAlpha = 0;

    QgsColorBox::const_iterator colorBoxIt = colorBox.constBegin();
    for ( ; colorBoxIt != colorBox.constEnd(); ++colorBoxIt )
    {
        currentRed = qRed( colorBoxIt->first );
        if ( currentRed > rMax )
        {
            rMax = currentRed;
        }
        if ( currentRed < rMin )
        {
            rMin = currentRed;
        }

        currentGreen = qGreen( colorBoxIt->first );
        if ( currentGreen > gMax )
        {
            gMax = currentGreen;
        }
        if ( currentGreen < gMin )
        {
            gMin = currentGreen;
        }

        currentBlue = qBlue( colorBoxIt->first );
        if ( currentBlue > bMax )
        {
            bMax = currentBlue;
        }
        if ( currentBlue < bMin )
        {
            bMin = currentBlue;
        }

        currentAlpha = qAlpha( colorBoxIt->first );
        if ( currentAlpha > aMax )
        {
            aMax = currentAlpha;
        }
        if ( currentAlpha < aMin )
        {
            aMin = currentAlpha;
        }
    }

    redRange = rMax - rMin;
    greenRange = gMax - gMin;
    blueRange = bMax - bMin;
    alphaRange = aMax - aMin;
    return true;
}

QRgb boxColor( const QgsColorBox& box, int boxPixels )
{
    double avRed = 0;
    double avGreen = 0;
    double avBlue = 0;
    double avAlpha = 0;
    QRgb currentColor;
    int currentPixel;

    double weight;

    QgsColorBox::const_iterator colorBoxIt = box.constBegin();
    for ( ; colorBoxIt != box.constEnd(); ++colorBoxIt )
    {
        currentColor = colorBoxIt->first;
        currentPixel = colorBoxIt->second;
        weight = ( double )currentPixel / boxPixels;
        avRed += ( qRed( currentColor ) * weight );
        avGreen += ( qGreen( currentColor ) * weight );
        avBlue += ( qBlue( currentColor ) * weight );
        avAlpha += ( qAlpha( currentColor ) * weight );
    }

    return qRgba( avRed, avGreen, avBlue, avAlpha );
}



void splitColorBox( QgsColorBox& colorBox, QgsColorBoxMap& colorBoxMap,
                                           QMap<int, QgsColorBox>::iterator colorBoxMapIt )
{

    if ( colorBox.size() < 2 )
    {
        return; //need at least two colors for a split
    }

    //a,r,g,b ranges
    int redRange = 0;
    int greenRange = 0;
    int blueRange = 0;
    int alphaRange = 0;

    if ( !minMaxRange( colorBox, redRange, greenRange, blueRange, alphaRange ) )
    {
        return;
    }

    //sort color box for a/r/g/b
    if ( redRange >= greenRange && redRange >= blueRange && redRange >= alphaRange )
    {
        qSort( colorBox.begin(), colorBox.end(), redCompare );
    }
    else if ( greenRange >= redRange && greenRange >= blueRange && greenRange >= alphaRange )
    {
        qSort( colorBox.begin(), colorBox.end(), greenCompare );
    }
    else if ( blueRange >= redRange && blueRange >= greenRange && blueRange >= alphaRange )
    {
        qSort( colorBox.begin(), colorBox.end(), blueCompare );
    }
    else
    {
        qSort( colorBox.begin(), colorBox.end(), alphaCompare );
    }

    //get median
    double halfSum = colorBoxMapIt.key() / 2.0;
    int currentSum = 0;
    int currentListIndex = 0;

    QgsColorBox::iterator colorBoxIt = colorBox.begin();
    for ( ; colorBoxIt != colorBox.end(); ++colorBoxIt )
    {
        currentSum += colorBoxIt->second;
        if ( currentSum >= halfSum )
        {
            break;
        }
        ++currentListIndex;
    }

    if ( currentListIndex > ( colorBox.size() - 2 ) ) //if the median is contained in the last color, split one item before that
    {
        --currentListIndex;
        currentSum -= colorBoxIt->second;
    }
    else
    {
        ++colorBoxIt; //the iterator needs to point behind the last item to remove
    }

    //do split: replace old color box, insert new one
    QgsColorBox newColorBox1 = colorBox.mid( 0, currentListIndex + 1 );
    colorBoxMap.insert( currentSum, newColorBox1 );

    colorBox.erase( colorBox.begin(), colorBoxIt );
    QgsColorBox newColorBox2 = colorBox;
    colorBoxMap.erase( colorBoxMapIt );
    colorBoxMap.insert( halfSum * 2.0 - currentSum, newColorBox2 );
}


void imageColors( QHash<QRgb, int>& colors, const QImage& image )
{
    colors.clear();
    int width = image.width();
    int height = image.height();

    const QRgb* currentScanLine = 0;
    QHash<QRgb, int>::iterator colorIt;
    for ( int i = 0; i < height; ++i )
    {
        currentScanLine = ( const QRgb* )( image.scanLine( i ) );
        for ( int j = 0; j < width; ++j )
        {
            colorIt = colors.find( currentScanLine[j] );
            if ( colorIt == colors.end() )
            {
                colors.insert( currentScanLine[j], 1 );
            }
            else
            {
                colorIt.value()++;
            }
        }
    }
}

void medianCut( QVector<QRgb>& colorTable, int nColors, const QImage& inputImage )
{
    QHash<QRgb, int> inputColors;
    imageColors( inputColors, inputImage );

    if ( inputColors.size() <= nColors ) //all the colors in the image can be mapped to one palette color
    {
        colorTable.resize( inputColors.size() );
        int index = 0;
        QHash<QRgb, int>::const_iterator inputColorIt = inputColors.constBegin();
        for ( ; inputColorIt != inputColors.constEnd(); ++inputColorIt )
        {
            colorTable[index] = inputColorIt.key();
            ++index;
        }
        return;
    }

    //create first box
    QgsColorBox firstBox; //QList< QPair<QRgb, int> >
    int firstBoxPixelSum = 0;
    QHash<QRgb, int>::const_iterator inputColorIt = inputColors.constBegin();
    for ( ; inputColorIt != inputColors.constEnd(); ++inputColorIt )
    {
        firstBox.push_back( qMakePair( inputColorIt.key(), inputColorIt.value() ) );
        firstBoxPixelSum += inputColorIt.value();
    }

    QgsColorBoxMap colorBoxMap; //QMultiMap< int, ColorBox >
    colorBoxMap.insert( firstBoxPixelSum, firstBox );
    QMap<int, QgsColorBox>::iterator colorBoxMapIt = colorBoxMap.end();

    //split boxes until number of boxes == nColors or all the boxes have color count 1
    bool allColorsMapped = false;
    while ( colorBoxMap.size() < nColors )
    {
        //start at the end of colorBoxMap and pick the first entry with number of colors < 1
        colorBoxMapIt = colorBoxMap.end();
        while ( true )
        {
            --colorBoxMapIt;
            if ( colorBoxMapIt.value().size() > 1 )
            {
                splitColorBox( colorBoxMapIt.value(), colorBoxMap, colorBoxMapIt );
                break;
            }
            if ( colorBoxMapIt == colorBoxMap.begin() )
            {
                allColorsMapped = true;
                break;
            }
        }

        if ( allColorsMapped )
        {
            break;
        }
        else
        {
            continue;
        }
    }

    //get representative colors for the boxes
    int index = 0;
    colorTable.resize( colorBoxMap.size() );
    QgsColorBoxMap::const_iterator colorBoxIt = colorBoxMap.constBegin();
    for ( ; colorBoxIt != colorBoxMap.constEnd(); ++colorBoxIt )
    {
        colorTable[index] = boxColor( colorBoxIt.value(), colorBoxIt.key() );
        ++index;
    }
}


std::map<QRgb,int> g_nearest;

 int nearestColor( int r, int g, int b, const QColor *palette, int size )
 {
     if (palette == 0)
       return 0;

     QRgb rgb(qRgb(r, g, b ));
     if ( g_nearest.find(rgb) != g_nearest.end()  )
     {
         return g_nearest[rgb];
     }

     int dr = palette[0].red() - r;
     int dg = palette[0].green() - g;
     int db = palette[0].blue() - b;
 
     int minDist =  dr*dr + dg*dg + db*db;
     int nearest = 0;
 
     for (int i = 1; i < size; i++ )
     {
         dr = palette[i].red() - r;
         dg = palette[i].green() - g;
         db = palette[i].blue() - b;
 
         int dist = dr*dr + dg*dg + db*db;
 
         if ( dist < minDist )
         {
             minDist = dist;
             nearest = i;
         }
     }
 
     //g_nearest[rgb] = nearest;
     return nearest;
 }

 QImage& dither(QImage &img, const QColor *palette, int size)
 {
     if (img.width() == 0 || img.height() == 0 ||
         palette == 0 || img.depth() <= 8)
       return img;
 
     QImage dImage( img.width(), img.height(), QImage::Format_Indexed8 );
     int i;
 
     dImage.setNumColors( size );
     for ( i = 0; i < size; i++ )
         dImage.setColor( i, palette[ i ].rgb() );
 
     int *rerr1 = new int [ img.width() * 2 ];
     int *gerr1 = new int [ img.width() * 2 ];
     int *berr1 = new int [ img.width() * 2 ];
 
     memset( rerr1, 0, sizeof( int ) * img.width() * 2 );
     memset( gerr1, 0, sizeof( int ) * img.width() * 2 );
     memset( berr1, 0, sizeof( int ) * img.width() * 2 );
 
     int *rerr2 = rerr1 + img.width();
     int *gerr2 = gerr1 + img.width();
     int *berr2 = berr1 + img.width();
 
     for ( int j = 0; j < img.height(); j++ )
     {
         uint *ip = (uint * )img.scanLine( j );
         uchar *dp = dImage.scanLine( j );
 
         for ( i = 0; i < img.width(); i++ )
         {
             rerr1[i] = rerr2[i] + qRed( *ip );
             rerr2[i] = 0;
             gerr1[i] = gerr2[i] + qGreen( *ip );
             gerr2[i] = 0;
             berr1[i] = berr2[i] + qBlue( *ip );
             berr2[i] = 0;
             ip++;
         }
 
         *dp++ = nearestColor( rerr1[0], gerr1[0], berr1[0], palette, size );
 
         for ( i = 1; i < img.width()-1; i++ )
         {
             int indx = nearestColor( rerr1[i], gerr1[i], berr1[i], palette, size );
             *dp = indx;
 
             int rerr = rerr1[i];
             rerr -= palette[indx].red();
             int gerr = gerr1[i];
             gerr -= palette[indx].green();
             int berr = berr1[i];
             berr -= palette[indx].blue();
 
             // diffuse red error
             rerr1[ i+1 ] += ( rerr * 7 ) >> 4;
             rerr2[ i-1 ] += ( rerr * 3 ) >> 4;
             rerr2[  i  ] += ( rerr * 5 ) >> 4;
             rerr2[ i+1 ] += ( rerr ) >> 4;
 
             // diffuse green error
             gerr1[ i+1 ] += ( gerr * 7 ) >> 4;
             gerr2[ i-1 ] += ( gerr * 3 ) >> 4;
             gerr2[  i  ] += ( gerr * 5 ) >> 4;
             gerr2[ i+1 ] += ( gerr ) >> 4;
 
             // diffuse red error
             berr1[ i+1 ] += ( berr * 7 ) >> 4;
             berr2[ i-1 ] += ( berr * 3 ) >> 4;
             berr2[  i  ] += ( berr * 5 ) >> 4;
             berr2[ i+1 ] += ( berr ) >> 4;
 
             dp++;
         }
 
         *dp = nearestColor( rerr1[i], gerr1[i], berr1[i], palette, size );
     }
 
     delete [] rerr1;
     delete [] gerr1;
     delete [] berr1;
 
     img = dImage;
     g_nearest.clear();
     return img;
 }

void makeIndexedImage( const QuantizeMethod method, QImage& img, const QVector<QRgb>& current_color_table = QVector<QRgb>() )
{
    //img.save("/tmp/out_orig.png", "png", 50);
    switch (method)
    {
    case QuantizeMethod_DIFFUSE:
    {
        img = img.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
        break;
    }
    case QuantizeMethod_THRESHOLD:
        img = img.convertToFormat(QImage::Format_Indexed8, Qt::ThresholdDither);
        break;
    case QuantizeMethod_ORDERED:
        img = img.convertToFormat(QImage::Format_Indexed8, Qt::OrderedDither);
        break;        
    case QuantizeMethod_MEDIANCUT:
    case QuantizeMethod_MEDIANCUT_FLOYD:
    {
        if ( current_color_table.size() )
        {
            img = img.convertToFormat( QImage::Format_Indexed8, current_color_table );
        }
        else
        {
            img = quantize_mediancut(img, (method == QuantizeMethod_MEDIANCUT_FLOYD));
            img = img.convertToFormat( QImage::Format_Indexed8 );
        }
        break;
    }
    default:
        assert(false); // everything must be handled
        break;
    }
    if ( img.colorCount() != 256 )
    {
        //std::cerr << "WARNING: forcing color count (" << img.colorCount() << ") to 256" << std::endl;
        img.setColorCount( 256 );
    }
}

ColorMapObject makeColorMapObject( const QImage& idx8 )
{
    // color table
    QVector<QRgb> colorTable = idx8.colorTable();
    ColorMapObject cmap;
    // numColors must be a power of 2
    int numColors = 1 << GifBitSize(idx8.colorCount());
    cmap.ColorCount = numColors;
    //std::cout << "numColors:" << numColors << std::endl;
    cmap.BitsPerPixel = idx8.depth();// should always be 8
    if ( cmap.BitsPerPixel != 8 )
    {
        std::cerr << "Incorrect bit depth" << std::endl;
        return cmap;
    }
    GifColorType* colorValues = (GifColorType*)malloc(cmap.ColorCount * sizeof(GifColorType));
    cmap.Colors = colorValues;
    int c = 0;
    for(; c < idx8.colorCount(); ++c)
    {
        //std::cout << "color " << c << " has " << qRed(colorTable[c]) << "," <<  qGreen(colorTable[c]) << "," << qBlue(colorTable[c]) << std::endl;
        colorValues[c].Red = qRed(colorTable[c]);
        colorValues[c].Green = qGreen(colorTable[c]);
        colorValues[c].Blue = qBlue(colorTable[c]);
    }
    // In case we had an actual number of colors that's not a power of 2,
    // fill the rest with something (black perhaps).
    for (; c < numColors; ++c)
    {
        colorValues[c].Red = 0;
        colorValues[c].Green = 0;
        colorValues[c].Blue = 0;
    }
    return cmap;
}

bool gifWrite ( const QuantizeMethod method, const QVector<QImage> & images, const QVector<int>& delays, 
                const QVector<QRect>& crops, const QString& filename, bool loop )
{
    if ( !images.size() )
    {
        return false;
    }
    if ( images.size() != delays.size() )
    {
        std::cerr << "Internal error: images:delays mismatch" << std::endl;
        return false;
    }
    if ( images.size() != crops.size() )
    {
        std::cerr << "Internal error: images:crops mismatch" << std::endl;
        return false;
    }


    QImage initial = images.at(0);
    QImage base_indexed = initial;
    makeIndexedImage(method, base_indexed);

    QVector<QRgb> first_color_table = base_indexed.colorTable();
    /*
    for( QVector<QRgb>::iterator it = first_color_table.begin();
         it != first_color_table.end();
         ++it )
    {
        QColor c(*it);
        //std::cout << c.red() << ":" << c.blue() << ":" << c.green() << std::endl;
    }
    std::cout << "first_color_table size:" << first_color_table.size() << std::endl;
    */

    int error = 0;
    GifFileType *gif = EGifOpenFileName(filename.toLocal8Bit().constData(), false, &error);
    if (!gif) return false;
    EGifSetGifVersion(gif, true);

    // global color map
    ColorMapObject cmap = makeColorMapObject(base_indexed);
    if (EGifPutScreenDesc(gif, base_indexed.width(), base_indexed.height(), 8, 0, &cmap) == GIF_ERROR)
    {
        std::cerr << "EGifPutScreenDesc returned error" << std::endl;
        return false;
    }
    free(cmap.Colors);
    
    if ( loop )
    {
        unsigned char nsle[12] = "NETSCAPE2.0";
        unsigned char subblock[3];
        int loop_count = 0;
        subblock[0] = 1;
        subblock[1] = loop_count % 256;
        subblock[2] = loop_count / 256;
        if (EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE) == GIF_ERROR
            || EGifPutExtensionBlock(gif, 11, nsle) == GIF_ERROR
            || EGifPutExtensionBlock(gif, 3, subblock) == GIF_ERROR
            || EGifPutExtensionTrailer(gif) == GIF_ERROR) {
            std::cerr << "Error writing loop extension" << std::endl;
            return false;
        }
    }

    for ( QVector<QImage>::const_iterator it = images.begin(); it != images.end() ; ++it )
    {
        QImage sub;

        int idx = it-images.begin();
        const QRect& crop = crops[idx];
        if ( crop.isValid() )
        { 
            sub = it->copy( crop );
        }
        else
        {
            sub = *it;
        }
        makeIndexedImage(method, sub, first_color_table);

        /*
        QVector<QRgb> sub_color_table = sub.colorTable();
        for( QVector<QRgb>::iterator it = sub_color_table.begin();
             it != sub_color_table.end();
             ++it )
        {
            QColor c(*it);
            //std::cout << c.red() << ":" << c.blue() << ":" << c.green() << std::endl;
        }
        std::cout << "sub_color_table size:" << sub_color_table.size() << std::endl;

        if ( sub_color_table != first_color_table )
        {
            std::cerr << "WARNING: sub_color_table table does NOT match first_color_table!" << std::endl;
        }
        */


        // animation delay
        int msec_delay = delays.at( idx );
        static unsigned char ExtStr[4] = { 0x04, 0x00, 0x00, 0xff };
        ExtStr[0] = (false) ? 0x06 : 0x04;
        ExtStr[1] = msec_delay % 256;
        ExtStr[2] = msec_delay / 256;
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, ExtStr);

        // local color map
        ColorMapObject lcmap = makeColorMapObject(sub);

        // local image description
        int local_x = 0;
        int local_y = 0;
        int local_w = sub.width();
        int local_h = sub.height();
        if ( crop.isValid() )
        {
            local_x = crop.x();
            local_y = crop.y();
        }
        if (EGifPutImageDesc(gif, local_x, local_y, local_w, local_h, 0, &lcmap) == GIF_ERROR)
        {
            std::cerr << "EGifPutImageDesc returned error" << std::endl;
        }
        free(lcmap.Colors);

        int lc = sub.height();
        for (int l = 0; l < lc; ++l)
        {
            uchar* line = sub.scanLine(l);
            if (EGifPutLine(gif, (GifPixelType*)line, local_w) == GIF_ERROR)
            {
                std::cerr << "EGifPutLine returned error: " <<  gif->Error << std::endl;
            }
        }
    }
    EGifCloseFile(gif);
    return true;
}

