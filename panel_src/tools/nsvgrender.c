// nsvgrender: rasterize an SVG with nanosvg (the parser Rack uses) -> raw RGBA file
// usage: nsvgrender in.svg out.rgba scale   (prints W H)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
int main(int argc, char** argv){
  if(argc<4){fprintf(stderr,"usage\n");return 1;}
  float sc=atof(argv[3]);
  NSVGimage* img=nsvgParseFromFile(argv[1],"px",96.0f);
  if(!img){fprintf(stderr,"parse fail\n");return 1;}
  int w=(int)(img->width*sc+0.5f), h=(int)(img->height*sc+0.5f);
  unsigned char* buf=calloc(w*h*4,1);
  NSVGrasterizer* r=nsvgCreateRasterizer();
  nsvgRasterize(r,img,0,0,sc,buf,w,h,w*4);
  FILE* f=fopen(argv[2],"wb"); fwrite(buf,1,w*h*4,f); fclose(f);
  printf("%d %d\n",w,h); return 0;
}
