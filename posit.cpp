#include "cv.h"
#include "cxmisc.h"
#include "highgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <ctype.h>
#include <iostream>
#include "BlobResult.h"
#include <math.h>
//#include "Blob.h"
using namespace std;
#define ESC 27
#define DEV "development"
#define THR "thresholded"
#define WAIT 1

#define BLU CV_RGB(0,0,255)
#define RED CV_RGB(255,0,0)
#define GRN CV_RGB(0,255,0)
#define ORN CV_RGB(143,89,26)
#define R_BLU CV_RGB(0,0,1)
#define R_RED CV_RGB(1,0,0)
#define R_GRN CV_RGB(0,1,0)
#define R_ORN CV_RGB(140.0/255.0,115.0/255.0,0.0/*26.0/255.0*/)
#define NUM_LEDS 4


void printMat(CvMat * mat){
    cout << "[";
    for(int i = 0;i < mat->rows; i++){
	if(i) cout << " ";
	for(int j = 0; j< mat->cols;j++){
	    cout << *cvGet2D(mat,i,j).val << " ";
	}
	if(i!=mat->rows-1)cout << endl;
    }
    cout << "]" << endl;
}

void printBlobLoc(const char * name, CBlob blob){
    cout << name << " " << blob.minx << "," <<	blob.miny << " to " <<
	blob.MaxX() << "," <<	blob.MaxY() << " ";
}

CvScalar sumBlob(IplImage * img, CBlob blob){
//unsigned int a ing->imageData    y*img->widthStep+x
    CvScalar result, temp;
    for(int i = 0; i < 4; i++){
	result.val[i] = 0.0;
    }
    /*    cout << blob.minx << "," << blob.miny << " " <<
	  blob.maxx << "," << blob.maxy<<endl;*/
    for(int x = (int)blob.minx;x<(int)blob.maxx;x++){
	for(int y = (int)blob.miny;y<(int)blob.maxy;y++){
	    temp = cvGet2D(img,y,x);
	    for(int i = 0; i < 4; i++){
		result.val[i] += temp.val[i];
	    }
	}
    }
  
    double scale = result.val[1]+result.val[0]+result.val[2];
    result.val[0] = result.val[0]/scale;
    result.val[1] = result.val[1]/scale;
    result.val[2] = result.val[2]/scale;
    /*    for(int i = 0;i<3; i++)
      cout << result.val[i] << " " ;
      cout << endl;*/
    return result;
}
double myabs(double v){ return (v<0.0)?-1*v:v;}

double colorDist(CvScalar c1, CvScalar ratio){
  double result = 0.0;
  double sumc1 = c1.val[0] + c1.val[1] + c1.val[2];
  for(int i = 0; i < 3; i++)
    result += myabs((c1.val[i]/sumc1) - ratio.val[i]);
  return result;
  
}


void rectBlob(IplImage * img, CBlob blob, CvScalar color){
  CvScalar brightc = color;
  int mi = (brightc.val[0]>brightc.val[1])?0:1;
  mi = (brightc.val[mi] > brightc.val[2])?mi:2;
  int max = 255;
  double scale = max/brightc.val[mi];
  brightc.val[0] = brightc.val[0]*scale;
  brightc.val[1] = brightc.val[1]*scale;
  brightc.val[2] = brightc.val[2]*scale;
  
  
  cvRectangle(img,cvPoint((int)blob.minx,(int)blob.miny), 
	      cvPoint((int)blob.MaxX(), (int)blob.MaxY()), brightc);
}

int main(int argc, char * argv[]){

    if (argc != 2) {
		printf("usage: %s vid.avi\n", argv[0]);
		abort();
    }
	
    CvCapture*  vid = cvCreateFileCapture(argv[1]);
	
    if( !vid )
    {
        fprintf(stderr,"Error opening video\n");
        abort();
    }
	
	
    IplImage * videoFrame = NULL;
    cvNamedWindow(DEV, 1);
    cvNamedWindow(THR, 1);
    videoFrame = cvQueryFrame(vid);
    IplImage * thresholded = (IplImage*)cvClone(videoFrame);
    IplImage * greythresh = cvCreateImage(cvSize(videoFrame->width,videoFrame->height),IPL_DEPTH_8U, 1);
    int t = 0;
    // object that will contain blobs of thresholded
    IplConvKernel * element = cvCreateStructuringElementEx(5, 5, 2, 2, CV_SHAPE_ELLIPSE);
    CvPoint3D32f objectPoints[NUM_LEDS] = {cvPoint3D32f( -0.2891,-0.08,-0.125 ),\
		       cvPoint3D32f( 0.0,-0.103,0.0 ),\
		       cvPoint3D32f( 0.2858,-0.080,-0.125 ),\
		       cvPoint3D32f( 0.0,0.0,0.0 )};
    CvPOSITObject * posObj = cvCreatePOSITObject(objectPoints,NUM_LEDS);
    double focal_length = 0.003;
    
    CvMatr32f rotation_matrix = new float[9];
    CvVect32f translation_vector = new float[3];


    do{
	cvThreshold( videoFrame, thresholded, 250, 1, CV_THRESH_TOZERO );
	cvDilate(thresholded,thresholded,element);
	cvSmooth(thresholded,thresholded);
	cvCvtColor(thresholded, greythresh, CV_BGR2GRAY);
	CBlobResult blobs;
	// Extract the blobs using a threshold of 100 in the image
	blobs = CBlobResult( greythresh, NULL, 10, true );
	// object with the blob with most perimeter in the image
	// (the criteria to select can be any class derived from COperadorBlob)
	// from the filtered blobs, get the blob with biggest perimeter
	blobs.Filter( blobs, B_INCLUDE, CBlobGetPerimeter(), B_LESS, 1000 );
	blobs.Filter( blobs, B_INCLUDE, CBlobGetPerimeter(), B_GREATER, 15 );
	// get the blob with less area
	CBlob blob[4];
	int color_index[4]; //blue green red orange
	for(int i = 0; i<4;i++)
	  color_index[i]= 0;
	double bluMin= 1.0, ornMin= 1.0, grnMin= 1.0;
	if(blobs.GetNumBlobs() > 3){
	  for(int i  = 0; i< 4; i++){
	    blobs.GetNthBlob( CBlobGetArea(), i, blob[i] );
	    double t;
	    if((t=colorDist(sumBlob(thresholded,blob[i]),R_GRN))<grnMin){
	      color_index[1] = i;
	      grnMin = t;
	    }
	    if((t=colorDist(sumBlob(thresholded,blob[i]),R_BLU))<bluMin){
	      color_index[0] = i;
	      bluMin = t;
	    }
	  }
	  if(color_index[0] == color_index[1])//blue and green match blobs
	    continue;//tryagain
	  int left[2];
	  int k = 0;
	  for(int i= 0; i< 4; i++)
	    if((color_index[0] != i) && (color_index[1] != i))
	      left[k++] = i;

	  if(colorDist(sumBlob(thresholded,blob[left[0]]),R_ORN)<colorDist(sumBlob(thresholded,blob[left[1]]),R_ORN)){
	    color_index[3] = left[0];
	    color_index[2] = left[1];
	  }else{
	    color_index[3] = left[1];
	    color_index[2] = left[0];
	  }
	  CvScalar colors[4] = {BLU,GRN,RED,ORN};
	  for(int i = 0; i < 4; i++){
	    rectBlob(thresholded,blob[color_index[i]],colors[i]);
	  }
	  CvPoint2D32f imgPoints[NUM_LEDS];
	  for(int i; i < NUM_LEDS; i++){
	    imgPoints[i] = cvPoint2D32f(blob[color_index[i]].sumx,blob[color_index[i]].sumy);
	  }
	  cvPOSIT(posObj,imgPoints,focal_length,cvTermCriteria(1,10,.05),
		  rotation_matrix,translation_vector);
	  cout << "rotation_matrix" << endl;
	  for(int i=0;i<3;i++){
	    for(int j=0;j<3;j++){
	      cout << rotation_matrix[i*3+j] << "\t";
	    }
	    cout << endl;
	  }
	  cout <<  "translation_vector" << endl;
	  for(int j=0;j<3;j++){
	    cout << translation_vector[j] << endl;
	  }
	  
	}
	cvShowImage(THR, thresholded);
	cvShowImage(DEV, videoFrame);
	int k = cvWaitKey(0);
	if(k == ESC || k == 'q')
	    break;
	

	
    }while(videoFrame = cvQueryFrame(vid));
    
    
    return 0;

}
