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
#include <GLUT/glut.h>
#include "skull.h"

//#include "Blob.h"
using namespace std;
#define ESC 27
#define DEV "development"
#define THR "thresholded"
#define WAIT 1
#define NUM_LEDS 4

#define BLU CV_RGB(0,0,255)
#define RED CV_RGB(255,0,0)
#define GRN CV_RGB(0,255,0)
#define ORN CV_RGB(143,89,26)
/*
#define R_BLU CV_RGB(0,0,1)
#define R_RED CV_RGB(1,0,0)
#define R_GRN CV_RGB(0,1,0)
#define R_ORN CV_RGB(140.0/255.0,115.0/255.0,0.0)
#define NUM_LEDS 4
*/
CvScalar normRGB(CvScalar color){
    double dist = sqrt(color.val[0]*color.val[0]+color.val[1]*color.val[1]+color.val[2]*color.val[2]);
    CvScalar result = color;
    for(int i = 0; i < 3; i++)
	result.val[i] = result.val[i]/dist;
    return result;
}

CvScalar R_BLU = normRGB(BLU);
CvScalar R_RED = normRGB(RED);
CvScalar R_ORN = normRGB(ORN);
CvScalar R_GRN = normRGB(GRN);


CvCapture*  vid;
IplImage * videoFrame;



IplImage * thresholded;
IplImage * greythresh;
int t;
IplConvKernel * element;
CvPoint3D32f objectPoints[NUM_LEDS] = {cvPoint3D32f( -0.2891,-0.08,-0.125 ),\
cvPoint3D32f( 0.0,-0.103,0.0 ),\
cvPoint3D32f( 0.2858,-0.080,-0.125 ),\
cvPoint3D32f( 0.0,0.0,0.0 )};

CvPOSITObject * posObj;
double focal_length;

CvMatr32f rotation_matrix;
CvVect32f translation_vector;

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

double colorDist(CvScalar c1, CvScalar ratio){
    CvScalar norm = normRGB(c1);
    double result = 0.0;
	//    double sumc1 = c1.val[0] + c1.val[1] + c1.val[2];
    for(int i = 0; i < 3; i++){
		double t = ((norm.val[i]) - ratio.val[i]);
		result += t*t;
    }
    result = sqrt(result);	
    cout << result << "r ";
    return result;
}

//colors always go in this order: blue green red orange
// match_index: which color goes to which blob, so the blue blob is 
// match_index[0]
double optimalLEDMatch(CvScalar colors[NUM_LEDS], CvScalar leds[NUM_LEDS], 
		       int match_index[NUM_LEDS])
{
  double result = 50000.0;
  
  for(int i = 0; i< NUM_LEDS; i++){
    double test = colorDist(leds[i],colors[0]);
    if(test > result)
      continue;
    for(int j = 0; j < NUM_LEDS; j++){
      if(j == i) continue;
      test += colorDist(leds[j],colors[1]);
      if(test > result)
	continue;
      for(int k = 0; k < NUM_LEDS; k++){
	if(k == j || k == i) continue;
	test += colorDist(leds[k],colors[2]);
	if(test > result)
	  continue;
	for(int m = 0; m < NUM_LEDS; m++){
	  if(m == j || m == k || m == i) continue;
	  test += colorDist(leds[m],colors[3]);
	  if(test < result){
	    result = test;
	    match_index[0] = i;
	    match_index[1] = j;
	    match_index[2] = k;
	    match_index[3] = m;
	  }	  
	}
      }
    }
  }
  cout << "colors";
  for(int i = 0; i < NUM_LEDS; i++)
    cout << match_index[i] << " ";
  cout << result << endl;
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


void idle(void)
{
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
	CBlob blob[NUM_LEDS];
	CvScalar blobColors[NUM_LEDS];
	for(int i  = 0; i< NUM_LEDS; i++){
	    blobs.GetNthBlob( CBlobGetArea(), i, blob[i] );
	    blobColors[i] = sumBlob(thresholded,blob[i]);
	}
	int color_index[NUM_LEDS]; //blue green red orange
	CvScalar colors[NUM_LEDS] = {R_BLU,R_GRN,R_RED,R_ORN};

	if(blobs.GetNumBlobs() >= NUM_LEDS){
	    optimalLEDMatch(colors,blobColors,color_index);
	    for(int i = 0; i < 4; i++){
		rectBlob(thresholded,blob[color_index[i]],colors[i]);
	    }
			
			
	    CvPoint2D32f imgPoints[NUM_LEDS];
	    for(int i = 0; i < NUM_LEDS; i++){
		imgPoints[i] = cvPoint2D32f(blob[color_index[i]].sumx,blob[color_index[i]].sumy);
		cout << "imgPoint " << i << ": " << imgPoints[i].x << ", " << imgPoints[i].y << endl;
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
	//	cvShowImage(DEV, videoFrame);
	int k = cvWaitKey(0);
	if(k == 'q' || k == ESC)
	  exit(0);
	videoFrame = cvQueryFrame(vid);
	glutPostRedisplay();
}

void
visible(int state)
{
	if (state == GLUT_VISIBLE) {
		glutIdleFunc(idle);
	} else {
		glutIdleFunc(NULL);
	}
}


void display(void)
{
	unsigned int i;
	glMatrixMode(GL_PROJECTION);
	//glLoadIdentity();
	GLfloat projection[16];
	static GLfloat xp = .1;
	static GLfloat yp = .1;
	static GLfloat zp = -.07;
	zp -= 0;
	
	//yp += .1;
	//xp += .1;
	projection[0] = 50;
	projection[1] = 0;
	projection[2] = 0;
	projection[3] = 0;
	
	projection[4] = 0;
	projection[5] = 50;
	projection[6] = 0;
	projection[7] = 0;
	
	projection[8] = 0;
	projection[9] = 0;
	projection[10] = 1;
	projection[11] = 0;
	
	projection[12] = 0;
	projection[13] = 0;
	projection[14] = 0;
	projection[15] = 1;
	
	//glMultMatrixf(projection);
	
	glMatrixMode(GL_MODELVIEW);
	
	
	glPushMatrix(); /* Make sure the matrix stack is cleared at the end of this function. */
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	
	/*@@@@@@ Rotation and Translation of Entire Scene @@@@@*/
	//
	//	if(mvt_x < 0 && mvt_y < 0){
	//		glTranslatef(mvt_x ,mvt_y ,mvt_z );
	//		mvt_x = mvt_x - Tx;
	//		mvt_y = mvt_y - Ty;
	//		mvt_z = mvt_z - Tz;
	//		
	//		glRotatef(mvr_d, mvr_x, mvr_y, mvr_z);
	//		mvr_d = mvr_d - Rx;
	//	}
	
	//else{
	glTranslatef(0.0, 0.0 , -8.42);
	//}
	
	
	
	
	/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
	
	//	glPushMatrix();
	//	glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
	//	glPopMatrix();
	
	glScalef(8.0, 8.0, 8.0);
	glRotatef(180, 0.0, 1.0, 0.0);
	
	GLfloat transform[16];
	transform[0] = rotation_matrix[0];
	transform[1] = rotation_matrix[3];
	transform[2] = rotation_matrix[6];
	transform[3] = 0;
	transform[4] = rotation_matrix[1];
	transform[5] = rotation_matrix[4];
	transform[6] = rotation_matrix[7];
	transform[7] = 0;
	transform[8] = rotation_matrix[2];
	transform[9] = rotation_matrix[5];
	transform[10] = rotation_matrix[8];
	transform[11] = 0;
	transform[12] = translation_vector[0];
	transform[13] = translation_vector[1];
	transform[14] = translation_vector[2];
	transform[15] = 1;
	
	glMultMatrixf(transform);




	
	const GLfloat			lightAmbient[] = {0.2, 0.2, 0.2, 1.0};
	const GLfloat			lightDiffuse[] = {1.0, 1.0, 1.0, 1.0};
	const GLfloat			matAmbient[] = {0.6, 0.6, 0.6, 1.0};
	const GLfloat			matDiffuse[] = {1.0, 1.0, 1.0, 1.0};	
	const GLfloat			matSpecular[] = {1.0, 1.0, 1.0, 1.0};
	const GLfloat			lightPosition[] = {0.0, 0.0, 1.0, 0.0}; 
	const GLfloat			lightShininess = 100.0;
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, lightShininess);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition); 			
	glShadeModel(GL_SMOOTH);
	
	
	glVertexPointer(3, GL_FLOAT, 0, skullVertices);
	glNormalPointer(GL_FLOAT, 0, skullNormals);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnable(GL_COLOR_MATERIAL);
	glColor4f(1, 1, 1, 1);
	for (i = 0; i < skullIndexCount/3; i++) {
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, &skullIndices[i * 3]);
	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	
	glutSwapBuffers();
	

	
	glPopMatrix(); /* Clear the matrix stack */
	
}

void
keyboard(unsigned char ch, int x, int y)
{
	switch (ch) {
		case 27:             /* escape */
			exit(0);
			break;
	}
}



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

double myabs(double v){ return (v<0.0)?-1*v:v;}


int main(int argc, char * argv[]){
	
	
    if (argc == 1) {
		vid = cvCreateCameraCapture(0);
    }else if(argc == 2){
		vid = cvCreateFileCapture(argv[1]);
    }else{
		printf("usage: %s vid.avi\n", argv[0]);
		abort();
    }
	
	
	
    if( !vid )
    {
        fprintf(stderr,"Error opening video\n");
        abort();
    }
	
	
    videoFrame = NULL;
    cvNamedWindow(DEV, 1);
    cvNamedWindow(THR, 1);
    videoFrame = cvQueryFrame(vid);
    thresholded = (IplImage*)cvClone(videoFrame);
    greythresh = cvCreateImage(cvSize(videoFrame->width,videoFrame->height),IPL_DEPTH_8U, 1);
    t = 0;
    // object that will contain blobs of thresholded
    element = cvCreateStructuringElementEx(5, 5, 2, 2, CV_SHAPE_ELLIPSE);
    //CvPoint3D32f objectPoints[NUM_LEDS] = {cvPoint3D32f( -0.2891,-0.08,-0.125 ),\
//		cvPoint3D32f( 0.0,-0.103,0.0 ),\
//		cvPoint3D32f( 0.2858,-0.080,-0.125 ),\
//	cvPoint3D32f( 0.0,0.0,0.0 )};
    posObj = cvCreatePOSITObject(objectPoints,NUM_LEDS);
    focal_length = 875;
    
    rotation_matrix = new float[9];
    translation_vector = new float[3];
	
	
	
	/** Initialize openGL window **/
	glutInitWindowSize(320, 200);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("head");
	
	glutKeyboardFunc(keyboard);
	glutDisplayFunc(display); 
	glutVisibilityFunc(visible);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	glFrustum(-.9, .9, -.9, .9, 1.0, 35.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glShadeModel(GL_SMOOTH);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	
	glutMainLoop();	
	
	

    
    
    return 0;
	
}
