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
#include "box.h"

//#include "Blob.h"
using namespace std;
#define ESC 27
#define DEV "development"
#define THR "thresholded"
#define WAIT 1
#define NUM_LEDS 4

#define BLU CV_RGB(0,0,255)
#define RED CV_RGB(142, 16, 79)
#define GRN CV_RGB(31,207,0)
#define ORN CV_RGB(191,64,64)

//#define R_BLU CV_RGB(0,0,1)
//#define R_RED CV_RGB(1,0,0)
//#define R_GRN CV_RGB(0,1,0)
//#define R_ORN CV_RGB(140.0/255.0,115.0/255.0,0.0)
//#define NUM_LEDS 4

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
CvPoint3D32f objectPoints[NUM_LEDS] = {cvPoint3D32f( 0.2891,0.08,0.125 ),\
cvPoint3D32f( 0.0,0.103,0.0 ),\
cvPoint3D32f( -0.2858,0.080,0.125 ),\
cvPoint3D32f( 0.0,0.0,0.0 )};

CvPOSITObject * posObj;
double focal_length;

CvMatr32f rotation_matrix;
CvVect32f translation_vector;

CvMatr32f rotation_matrix_opt1;
CvMatr32f rotation_matrix_opt2;
CvVect32f translation_vector_opt1;
CvVect32f translation_vector_opt2;


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
			bool isBlack = false;
			bool isWhite = false;
			if (temp.val[0] == 0 && temp.val[1] == 0 && temp.val[2] == 0) {
				isBlack = true;
			}
			if (temp.val[0] > 250 && temp.val[1] > 250 && temp.val[2] > 250) {
				isWhite = true;
			}
			if (!isBlack && !isWhite) {
				for(int i = 0; i < 4; i++){
					result.val[i] += temp.val[i];
				}
			}
		}
    }
	return normRGB(result);
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
    //    cout << result << "r ";
    return result;
}

double colorDist2(CvScalar c1, CvScalar c2) {
	CvScalar norm1 = normRGB(c1);
	CvScalar norm2 = normRGB(c2);
	double result = 0;
	for (int i = 0; i < 3; i++) {
		double diff = norm1.val[i] - norm2.val[i];
		if (diff < 0)
			diff = -diff;
		result += diff;
	}
	return result;
}

void otherLEDMatch(CvScalar colors[NUM_LEDS], CvScalar leds[NUM_LEDS], int * match_index) {
	bool taken[4];
	for (int i = 0; i < 3; i++) {
		taken[i] = false;
	}
	
	for (int i = 0; i < NUM_LEDS; i++) {
		double lowestCost = 100000;
		int matchForI = 0;
		for (int j = 0; j < NUM_LEDS; j++) {
			//if (taken[j])
			//	continue;
			double dist = colorDist2(colors[i], leds[j]);
			if (dist < lowestCost) {
				lowestCost = dist;
				matchForI = j;
			}
		}
		match_index[i] = matchForI;
		taken[matchForI] = true;
	}
}

//colors always go in this order: blue green red orange
// match_index: which color goes to which blob, so the blue blob is 
// match_index[0]
double optimalLEDMatch(CvScalar colors[NUM_LEDS], CvScalar leds[NUM_LEDS], 
		       int match_index[NUM_LEDS])
{
  double test[NUM_LEDS][NUM_LEDS];//leds X colors
  for(int i = 0; i < NUM_LEDS; i++)
    for(int j = 0; j < NUM_LEDS; j++)
      test[i][j] = colorDist(leds[i],colors[j]);
  double result = 50000.0;
  //  double test[4];
  
  for(int i = 0; i< NUM_LEDS; i++){
    if(test[i][0] > result)
      continue;
    for(int j = 0; j < NUM_LEDS; j++){
      if(j == i) continue;
      if(test[i][0]+test[j][1] > result)
	continue;
      for(int k = 0; k < NUM_LEDS; k++){
	if(k == j || k == i) continue;
	if(test[i][0]+test[j][1]+test[k][2] > result)
	  continue;
	for(int m = 0; m < NUM_LEDS; m++){
	  if(m == j || m == k || m == i) continue;
	  double r;
	  if((r = test[i][0]+test[j][1]+test[k][2]+test[m][3]) < result){
	    result = r;
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
	//cvSmooth(thresholded,thresholded);
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
	CvScalar vibColors[NUM_LEDS] = {CV_RGB(0,0,255),CV_RGB(0,255,0),
	CV_RGB(255,0,0),CV_RGB(255,255,0)};
	
	if(blobs.GetNumBlobs() >= NUM_LEDS){
	    
		bool alreadyPicked[NUM_LEDS];
		for (int i = 0; i < NUM_LEDS; i++) {
			alreadyPicked[i] = false;
		}
		//optimalLEDMatch(colors,blobColors,color_index);		
		// find leftmost led (red)
		double temp = blob[0].sumx; 
		int ti = 0;
		for(int i = 1; i < NUM_LEDS; i++){
			if(blob[i].sumx < temp){
				ti = i;
				temp = blob[i].sumx;
			}
		}
		color_index[2] = ti;
		alreadyPicked[ti] = true;
		// find rightmost led (blue)
		temp = blob[0].sumx; 
		ti = 0;
		for(int i = 1; i < NUM_LEDS; i++){
			if(!alreadyPicked[i] && (blob[i].sumx > temp)){
				ti = i;
				temp = blob[i].sumx;
			}
		}
		color_index[0] = ti;
		alreadyPicked[ti] = true;
		// find top led (orange)
		temp = 2000; 
		ti = 0;
		for(int i = 0; i < NUM_LEDS; i++){
			if(!alreadyPicked[i] && (blob[i].sumy < temp)){
				ti = i;
				temp = blob[i].sumy;
			}
		}
		color_index[3] = ti;
		alreadyPicked[ti] = true;
		// find bottom led (green)
		ti = 0;
		for(int i = 0; i < NUM_LEDS; i++){
			if(!alreadyPicked[i]){
				ti = i;
			}
		}
		color_index[1] = ti;
		
		
		
		/*		
		 if (blob[color_index[2]].sumx > blob[color_index[3]].sumx){
		 int tmp = color_index[2];
		 color_index[2] = color_index[3];
		 color_index[3] = tmp;
		 }
		 if (blob[color_index[1]].sumy < blob[color_index[3]].sumy) {
		 int tmp = color_index[1];
		 color_index[1] = color_index[3];
		 color_index[3] = tmp;
		 }
		 */
	    CvPoint2D32f imgPoints[NUM_LEDS];
	    for(int i = 0; i < NUM_LEDS; i++){
			imgPoints[i] = cvPoint2D32f(blob[color_index[i]].sumx,blob[color_index[i]].sumy);
			cout << "imgPoint " << i << ": " << imgPoints[i].x << ", " << imgPoints[i].y << endl;
	    }
		
		
	    cvPOSIT(posObj,imgPoints,focal_length,cvTermCriteria(1,10,.05), rotation_matrix_opt1,translation_vector_opt1);
		CvPoint2D32f tmp = imgPoints[2];
		imgPoints[2] = imgPoints[3];
		imgPoints[3] = tmp;
		cvPOSIT(posObj,imgPoints,focal_length,cvTermCriteria(1,10,.05), rotation_matrix_opt2, translation_vector_opt2);
		
		
		float unit_vector[3];
		CvMat unit_vector_m = cvMat(3, 1, CV_32F, unit_vector);
		unit_vector[0] = 0;
		unit_vector[1] = 0;
		unit_vector[2] = 1;
		float rotated_old[3];
		float rotated_opt1[3];
		float rotated_opt2[3];
		float dot_opt1 = 0;
		float dot_opt2 = 0;
		CvMat rotated_old_m = cvMat(3, 1, CV_32F, rotated_old);
		CvMat rotated_opt1_m = cvMat(3, 1, CV_32F, rotated_opt1);
		CvMat rotated_opt2_m = cvMat(3, 1, CV_32F, rotated_opt2);
		
		CvMat rotation_matrix_m = cvMat(3, 3, CV_32F, rotation_matrix);
		CvMat rotation_matrix_opt1_m = cvMat(3, 3, CV_32F, rotation_matrix_opt1);
		CvMat rotation_matrix_opt2_m = cvMat(3, 3, CV_32F, rotation_matrix_opt2);
		
		cvMatMul(&rotation_matrix_m, &unit_vector_m, &rotated_old_m);
		cvMatMul(&rotation_matrix_opt1_m, &unit_vector_m, &rotated_opt1_m);
		cvMatMul(&rotation_matrix_opt2_m, &unit_vector_m, &rotated_opt2_m);
		
		dot_opt1 = rotated_old[0] * rotated_opt1[0] + rotated_old[1] * rotated_opt1[1] + rotated_old[2] * rotated_opt1[2];
		dot_opt2 = rotated_old[0] * rotated_opt2[0] + rotated_old[1] * rotated_opt2[1] + rotated_old[2] * rotated_opt2[2];
		cout << "dot 1 = " << dot_opt1 << ", dot 2 = " << dot_opt2 << endl;
		if (dot_opt1 < 0)
			dot_opt1 = -dot_opt1;
		if (dot_opt1 > 1)
			dot_opt1 = 1;
		if (dot_opt2 < 0)
			dot_opt2 = -dot_opt2;
		if (dot_opt2 > 1)
			dot_opt2 = 1;
		float theta_opt1 = acos(dot_opt1);
		float theta_opt2 = acos(dot_opt2);
		if (theta_opt1 < 0)
			theta_opt1 = -theta_opt1;
		if (theta_opt2 < 0)
			theta_opt2 = -theta_opt2;
		cout << "theta 1 = " << theta_opt1 << ", theta 2 = " << theta_opt2 << endl;
		if (/*theta_opt1 <= theta_opt2*/ true) {
			cout << "picking theta1" << endl;
			for (int i = 0; i < 9; i++) {
				rotation_matrix[i] = rotation_matrix_opt1[i];
			}
			for (int i = 0; i < 3; i++) {
				translation_vector[i] = translation_vector_opt1[i];
			}
			
		} else {
			cout << "picking theta2" << endl;
			for (int i = 0; i < 9; i++) {
				rotation_matrix[i] = rotation_matrix_opt2[i];
			}
			for (int i = 0; i < 3; i++) {
				translation_vector[i] = translation_vector_opt2[i];
			}
			int tmp = color_index[2];
			color_index[2] = color_index[3];
			color_index[3] = tmp;
		}
		for(int i = 0; i < 4; i++){
			rectBlob(thresholded,blob[color_index[i]],colors[i]);
	    }
		// rotation =, translation =
		
		
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
	int k = cvWaitKey(WAIT);
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
	glLoadIdentity();
	GLfloat size = .01 * tanf(45.0f*3.1415f/360.0f);
	glFrustum(-size, size, -size, size, 0.01, 100);


	GLfloat transform[16];
	transform[0] = 1;
	transform[1] = 0;
	transform[2] = 0;
	transform[3] = 0;
	
	transform[4] = 0;
	transform[5] = 1;
	transform[6] = 0;
	transform[7] = 0;
	
	transform[8] = 0;
	transform[9] = 0;
	transform[10] = translation_vector[2];
	transform[11] = 0;
	transform[12] = -(translation_vector[0]-.9);
	transform[13] = translation_vector[1]-.4;
	transform[14] = 0;
	transform[15] = 1;
	
	glMultMatrixf(transform);
	
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
	
	glTranslatef(0.0, 0.0 , -1);
	//}
	
	
	
	
	/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/
	
	//	glPushMatrix();
	//	glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
	//	glPopMatrix();
	
	
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
	
	
	glEnable(GL_COLOR_MATERIAL);
	// back wall
	glColor4f(1, 1, 1, 1);
	glNormal3f(0, 0, -1);
	glBegin(GL_QUADS);
	glVertex3f(-.5, -.5, -1);
	glVertex3f(.5, -.5, -1);
	glVertex3f(.5, .5, -1);
	glVertex3f(-.5, .5, -1);
	glEnd();
	
	// left wall
	glColor4f(1, 0, 0, 1);
	glNormal3f(1, 0, 0);
	glBegin(GL_QUADS);
	glVertex3f(-.5, -.5, 0);
	glVertex3f(-.5, -.5, -1);
	glVertex3f(-.5, .5, -1);
	glVertex3f(-.5, .5, 0);
	glEnd();
	
	// right wall
	glColor4f(0, 0, 1, 1);
	glNormal3f(-1, 0, 0);
	glBegin(GL_QUADS);
	glVertex3f(.5, -.5, -1);
	glVertex3f(.5, -.5, 0);
	glVertex3f(.5, .5, 0);
	glVertex3f(.5, .5, -1);
	glEnd();
	
	// bottom wall
	glColor4f(0, 1, 1, 1);
	glNormal3f(0, 1, 0);
	glBegin(GL_QUADS);
	glVertex3f(-.5, -.5, 0);
	glVertex3f(.5, -.5, 0);
	glVertex3f(.5, -.5, -1);
	glVertex3f(-.5, -.5, -1);
	glEnd();
	
	// top wall
	glColor4f(0, 1, 0, 1);
	glNormal3f(0, -1, 0);
	glBegin(GL_QUADS);
	glVertex3f(-.5, .5, 0);
	glVertex3f(-.5, .5, -1);
	glVertex3f(.5, .5, -1);
	glVertex3f(.5, .5, 0);
	glEnd();
	
	
	
	
//	glTranslatef(0, 0, 40);
//	glVertexPointer(3, GL_FLOAT, 0, CubeVertices);
//	glNormalPointer(GL_FLOAT, 0, CubeNormals);
//	glEnableClientState(GL_VERTEX_ARRAY);
//	glEnableClientState(GL_NORMAL_ARRAY);
//	glEnable(GL_COLOR_MATERIAL);
//	glColor4f(1, 1, 1, 1);
//	for (i = 0; i < CubeIndexCount/3; i++) {
//		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, &CubeIndices[i * 3]);
//	}
//	glDisableClientState(GL_VERTEX_ARRAY);
//	glDisableClientState(GL_NORMAL_ARRAY);
	
	
	
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
    //    cvNamedWindow(DEV, 1);
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
	
	rotation_matrix_opt1 = new float[9];
	rotation_matrix_opt2 = new float[9];
	translation_vector_opt1 = new float[3];
	translation_vector_opt2 = new float[2];
	
	
	/** Initialize openGL window **/
	glutInitWindowSize(1280, 800);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("head");
	
	glutKeyboardFunc(keyboard);
	glutDisplayFunc(display); 
	glutVisibilityFunc(visible);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
//	glFrustum(-.9, .9, -0.9f, 0.9f, 1.0, 40.0);
	GLfloat size = .01 * tanf(45.0f*3.1415f/360.0f);
	glFrustum(-size, size, -size, size, 0.01, 100);

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
