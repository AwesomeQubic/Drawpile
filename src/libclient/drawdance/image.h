#ifndef DRAWDANCE_IMAGE_H
#define DRAWDANCE_IMAGE_H
#include <QImage>

typedef struct DP_Image DP_Image;
typedef union DP_Pixel8 DP_Pixel8;

namespace drawdance {

QImage wrapImage(DP_Image *img);

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels);

}

#endif
