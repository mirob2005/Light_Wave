/* Michael Robertson
 * mirob2005@gmail.com, miro2005@csu.fullerton.edu
 * SID: 892-32-2629
 * Thesis Test Implementation
 * Start Date: 5/23/12
 *
 * $Id: raytrace.cpp 1961 2010-02-24 08:46:53Z mshafae $
 *
 */


#include <time.h>
//#include <iostream>
#include <string>
#include <math.h>
#include "getopt.h"
#include "Scene.h"
#include "FaceList.h"
#include "PlyModel.h"

using namespace std;

FaceList *gInputModel;

Image *color_image;
Image *depth_image;
Image *normal_image;
Image *coord_image;
Image *flux_image;
Image *indirect_image;

Image *cam_normal_image;
Image *cam_coord_image;

Scene *gTheScene;
string gProgramName;
int gResolution;

float* tArray;
float* dArray;
float* nArray;
float* cArray;
float* fArray;

float* cnArray;
float* ccArray;
int gNumDLights = 6485;

float diff = 0;
float midpt[3] = {0,0,0};
bool gVerbose = false;

int sampleRate = 100;

float W = 0;
float H = 0;

void usage( string message = "" ){
	cerr << message << endl;
	cerr << gProgramName << " -i <inputfile>" << endl;
	cerr << "          -or-" << endl;
	cerr << gProgramName << " --input <inputfile>" << endl;
}

std::ostream& operator <<( std::ostream &out, const Pixel &p ){
	p.write( out );
	return( out );
}

void parseCommandLine( int argc, char **argv ){
	int ch;
	string inputFile( "" ), outputFile( "" ), depthFile( "" );

	static struct option longopts[] = {
		{ "input", required_argument, NULL, 'i' },
		{ "resolution", required_argument, NULL, 'r' },
		{ "verbose", required_argument, NULL, 'v' },
		{ "help", required_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	while( (ch = getopt_long(argc, argv, "i:r:vh", longopts, NULL)) != -1 ){
		switch( ch ){
			case 'i':
				// input file
				inputFile = string(optarg);
				break;
			case 'r':
				gResolution = atoi(optarg);
				break;
			case 'v':
				gVerbose = true;
				break;
			case 'h':
				usage( );
				break;
			default:
				// do nothing
				break;
		}
	}
	// Generate File Names for color image and depth image output files from the scene input file
	string temp = inputFile;
	string temp2 = inputFile;
	outputFile = temp.replace(inputFile.length()-4,4,"").append("_color_image.ppm");
	depthFile = temp2.replace(inputFile.length()-4,4,"").append("_depth_image.ppm");
	gTheScene = new Scene( inputFile, outputFile, depthFile );
}

float* normalize3D(float* vector) {
	float vectorSquared = 0;
	float magnitude = 0;
	vectorSquared = vector[0]*vector[0]+vector[1]*vector[1]+vector[2]*vector[2];
	magnitude = sqrt(vectorSquared);
	float normal[3];
	normal[0] = vector[0]/magnitude;
	normal[1] = vector[1]/magnitude;
	normal[2] = vector[2]/magnitude;

	return normal;
}

float max(float value1, float value2)
{
	if(value1 >=value2)
		return value1;
	else
		return value2;
}

float intersectSphere(int i, float* eye, float* d){
	////A = dNorm DOT dNorm
	float A =normalize3D(d)[0]*normalize3D(d)[0]+
			 normalize3D(d)[1]*normalize3D(d)[1]+
			 normalize3D(d)[2]*normalize3D(d)[2];
	float center[3] = {gTheScene->allSpheres()[1+5*i],gTheScene->allSpheres()[2+5*i],gTheScene->allSpheres()[3+5*i]};
	float radius = gTheScene->allSpheres()[4+5*i];

	////B = 2(eye-center) DOT dNorm
	float eyeMinusCenter[3] = {eye[0]-center[0],eye[1]-center[1],eye[2]-center[2]};

	float B = 2*(eyeMinusCenter[0]*(normalize3D(d)[0])+
						eyeMinusCenter[1]*(normalize3D(d)[1])+
						eyeMinusCenter[2]*(normalize3D(d)[2]));

	//C = (eye-center) DOT (eye-center) - radius^2
	float C = (eyeMinusCenter[0]*eyeMinusCenter[0]+
					eyeMinusCenter[1]*eyeMinusCenter[1]+
					eyeMinusCenter[2]*eyeMinusCenter[2]) - (radius*radius);

	//B^2 - 4AC
	float Q = (B*B) - 4*(A*C);

	if(Q >= 0){
		float tMinus = (-B-sqrt(Q))/(2*A);
		return tMinus;
	}
	else {
		return 0;
	}
}

float intersectPlane(int i, float* eye, float* d){
	float normal[3] = { gTheScene->allPlanes()[1+5*i],
									gTheScene->allPlanes()[2+5*i],
									gTheScene->allPlanes()[3+5*i]};
	float normalDOTd = (normalize3D(normal)[0]) * (normalize3D(d)[0]) +
									 (normalize3D(normal)[1]) * (normalize3D(d)[1]) +
									 (normalize3D(normal)[2]) * (normalize3D(d)[2]);

	if(normalDOTd >=0.0001 || normalDOTd <= -0.0001) {
		float a[3] = {(normalize3D(normal)[0])*gTheScene->allPlanes()[4+5*i],
							 (normalize3D(normal)[1])*gTheScene->allPlanes()[4+5*i],
							 (normalize3D(normal)[2])*gTheScene->allPlanes()[4+5*i]};

		float aMinusO[3] = {a[0] - eye[0],a[1] - eye[1],a[2] - eye[2]};

		float t = ( aMinusO[0]*normal[0]+
						aMinusO[1]*normal[1]+
						aMinusO[2]*normal[2])/normalDOTd;
		return t;
	}
	return 0;
}

float intersectTriangle(int i, float* eye, float* d, bool mesh){
	float Aa,Ab,Ac,Ad,Ae,Af,Ag,Ah,Ai,Aj,Ak,Al;
	if(!mesh){
		Aa = (gTheScene->allTriangles()[1+10*i])-(gTheScene->allTriangles()[4+10*i]);
		Ab = (gTheScene->allTriangles()[2+10*i])-(gTheScene->allTriangles()[5+10*i]);
		Ac = (gTheScene->allTriangles()[3+10*i])-(gTheScene->allTriangles()[6+10*i]);

		Ad = (gTheScene->allTriangles()[1+10*i])-(gTheScene->allTriangles()[7+10*i]);
		Ae = (gTheScene->allTriangles()[2+10*i])-(gTheScene->allTriangles()[8+10*i]);
		Af = (gTheScene->allTriangles()[3+10*i])-(gTheScene->allTriangles()[9+10*i]);

		Ag = d[0];
		Ah = d[1];
		Ai = d[2];

		Aj = (gTheScene->allTriangles()[1+10*i])-eye[0];
		Ak = (gTheScene->allTriangles()[2+10*i])-eye[1];
		Al = (gTheScene->allTriangles()[3+10*i])-eye[2];
	}
	else{
		Aa = (gInputModel->vertices[gInputModel->faces[i][0]][0])-(gInputModel->vertices[gInputModel->faces[i][1]][0]);
		Ab = (gInputModel->vertices[gInputModel->faces[i][0]][1])-(gInputModel->vertices[gInputModel->faces[i][1]][1]);
		Ac = (gInputModel->vertices[gInputModel->faces[i][0]][2])-(gInputModel->vertices[gInputModel->faces[i][1]][2]);

		Ad = (gInputModel->vertices[gInputModel->faces[i][0]][0])-(gInputModel->vertices[gInputModel->faces[i][2]][0]);
		Ae = (gInputModel->vertices[gInputModel->faces[i][0]][1])-(gInputModel->vertices[gInputModel->faces[i][2]][1]);
		Af = (gInputModel->vertices[gInputModel->faces[i][0]][2])-(gInputModel->vertices[gInputModel->faces[i][2]][2]);

		Ag = d[0];
		Ah = d[1];
		Ai = d[2];

		Aj = (gInputModel->vertices[gInputModel->faces[i][0]][0])-eye[0];
		Ak = (gInputModel->vertices[gInputModel->faces[i][0]][1])-eye[1];
		Al = (gInputModel->vertices[gInputModel->faces[i][0]][2])-eye[2];
	}

	float M = (Aa*((Ae*Ai)-(Ah*Af))) + (Ab*((Ag*Af)-(Ad*Ai))) + (Ac*((Ad*Ah)-(Ae*Ag)));
	float t = 0;
	float Beta = 0;
	float Gamma = 0;
	if(M >= 0.0001 || M <= -0.0001){
		t = ((Af*((Aa*Ak)-(Aj*Ab))) + (Ae*((Aj*Ac)-(Aa*Al))) + (Ad*((Ab*Al)-(Ak*Ac))))/M;
		Beta = ((Aj*((Ae*Ai)-(Ah*Af))) + (Ak*((Ag*Af)-(Ad*Ai))) + (Al*((Ad*Ah)-(Ae*Ag))))/M;
		Gamma = ((Ai*((Aa*Ak)-(Aj*Ab))) + (Ah*((Aj*Ac)-(Aa*Al))) + (Ag*((Ab*Al)-(Ak*Ac))))/M;
		t = d[2]*t;

		if(Beta >-0.0001 && Beta <1.0001){
			if(Gamma > -0.0001 && Gamma < 1.0001){
				if(Beta+Gamma < 1.0001){
					return t;
				}
			}
		}
	}
	return 0;
}

void lightObject( int currentPixel, float* point, float* normal, Pixel *cp, Pixel *ip, float* d, 
				float* diffuseColor, float* specularColor, float exponent, int object, int num){
							
	float sumLightsR = 0;
	float sumLightsG = 0;
	float sumLightsB = 0;
	float irradiance = 0;
	float NdotL = 0;
	bool blocked = false;

	float normalizednormal[3] = {normalize3D(normal)[0],normalize3D(normal)[1],normalize3D(normal)[2]};
	int j =0;
	//for(int j = 0; j < gTheScene->numLights(); j++) {
	float lightVector[3] = {gTheScene->lights()[0+9*j]-point[0], 
							gTheScene->lights()[1+9*j] - point[1], 
							gTheScene->lights()[2+9*j] - point[2]};

	//Calculate Shadows with Shadow Maps
	float lightpoint[3] = {point[0], -point[2], point[1]};

	int column = int((lightpoint[0] + (W/2))/(W/(gResolution-1))+0.5);

	int row = int(-(lightpoint[1] -(H/2))/(H/(gResolution-1))+0.5);

	int lightSpacePixel = row*gResolution+column;

	if(row >=0 && column >=0) 
	{
		float depth = gTheScene->lights()[1+9*j] - lightpoint[2];

		if(depth > dArray[lightSpacePixel] +0.5)
			blocked = true;
	}

	if(!blocked){
		//Calculate Attenuation: if parsed attenuation value = 0 then fatt = 1 ( 1/(att* (mag(lightVector)+1))
		float fatt[3] = {1/(gTheScene->lights()[6+9*j]*sqrt( (lightVector[0]*lightVector[0])+
						(lightVector[1]*lightVector[1])+(lightVector[2]*lightVector[2]) )+1),
						1/(gTheScene->lights()[7+9*j]*sqrt( (lightVector[0]*lightVector[0])+
						(lightVector[1]*lightVector[1])+(lightVector[2]*lightVector[2]) )+1),
						1/(gTheScene->lights()[8+9*j]*sqrt( (lightVector[0]*lightVector[0])+
						(lightVector[1]*lightVector[1])+(lightVector[2]*lightVector[2]) )+1)};
		
		NdotL =(normalize3D(normal)[0])*(normalize3D(lightVector)[0])+
							 (normalize3D(normal)[1])*(normalize3D(lightVector)[1])+
							 (normalize3D(normal)[2])*(normalize3D(lightVector)[2]);
		if(NdotL < 0.0001){
			NdotL = 0;
		}
		//Directional Lights
		if(gTheScene->lightType()[j] == 0) {
			float direction[3] = {gTheScene->dLight()[0+4*j],gTheScene->dLight()[1+4*j],gTheScene->dLight()[2+4*j]};

			//Check to see if the light direction is facing towards the object
			if(NdotL == 0){
				//DO Nothing
			}
			else {
				float angleRad = (normalize3D(direction)[0])*-(normalize3D(lightVector)[0])+
											(normalize3D(direction)[1])*-(normalize3D(lightVector)[1])+
											(normalize3D(direction)[2])*-(normalize3D(lightVector)[2]);
				float angleDeg = (180*acos(angleRad))/3.1416;

				float maxHalfAngle = (gTheScene->dLight()[3+4*j])/2;

				if(maxHalfAngle > angleDeg) {
					//cout << "Light!" << endl;
					float reflectionVector[3] = {2*(normalize3D(normal)[0])*NdotL-(normalize3D(lightVector)[0]),
																 2*(normalize3D(normal)[1])*NdotL-(normalize3D(lightVector)[1]),
																 2*(normalize3D(normal)[2])*NdotL-(normalize3D(lightVector)[2])};
					float viewingVector[3] = {d[0], d[1], -d[2]};

					float RdotV =(normalize3D(reflectionVector)[0])*(normalize3D(viewingVector)[0])+
										 (normalize3D(reflectionVector)[1])*(normalize3D(viewingVector)[1])+
										 (normalize3D(reflectionVector)[2])*(normalize3D(viewingVector)[2]);

					//DIFFUSE COLOR = fatt*DLight*(lightColor*NdotL*diffuseColor)
					sumLightsR = sumLightsR+(fatt[0]*angleRad* (gTheScene->lights()[3+9*j]*NdotL*diffuseColor[0]) );
					sumLightsG = sumLightsG+(fatt[1]*angleRad* (gTheScene->lights()[4+9*j]*NdotL*diffuseColor[1]) );
					sumLightsB = sumLightsB+(fatt[2]*angleRad* (gTheScene->lights()[5+9*j]*NdotL*diffuseColor[2]) );

					if(RdotV < 0.00){
						RdotV = 0;
					}
					//SPECULAR COLOR = fatt*DLight*(lightColor*specularColor*(RdotV)^exponent)
					sumLightsR = sumLightsR+(fatt[0]*angleRad* (gTheScene->lights()[3+9*j]*specularColor[0]*pow(RdotV,exponent)) );
					sumLightsG = sumLightsG+(fatt[1]*angleRad* (gTheScene->lights()[4+9*j]*specularColor[1]*pow(RdotV,exponent)) );
					sumLightsB = sumLightsB+(fatt[2]*angleRad* (gTheScene->lights()[5+9*j]*specularColor[2]*pow(RdotV,exponent)) );  
				}
				else {
					//cout << "No Light" << endl;
					//Beyond Directional Light Angle, no light visible here!
				}
			}
		}
		//Point Lights
		else if (gTheScene->lightType()[j] == 1) {

			float reflectionVector[3] = {2*(normalize3D(normal)[0])*NdotL-(normalize3D(lightVector)[0]),
									 2*(normalize3D(normal)[1])*NdotL-(normalize3D(lightVector)[1]),
									 2*(normalize3D(normal)[2])*NdotL-(normalize3D(lightVector)[2])};
			float viewingVector[3] = {d[0], d[1], -d[2]};

			float RdotV =(normalize3D(reflectionVector)[0])*(normalize3D(viewingVector)[0])+
								 (normalize3D(reflectionVector)[1])*(normalize3D(viewingVector)[1])+
								 (normalize3D(reflectionVector)[2])*(normalize3D(viewingVector)[2]);

			//DIFFUSE COLOR = fatt*lightColor*NdotL*diffuseColor
			sumLightsR = sumLightsR+(fatt[0]*(gTheScene->lights()[3+9*j]*NdotL*diffuseColor[0]));
			sumLightsG = sumLightsG+(fatt[1]*(gTheScene->lights()[4+9*j]*NdotL*diffuseColor[1]));
			sumLightsB = sumLightsB+(fatt[2]*(gTheScene->lights()[5+9*j]*NdotL*diffuseColor[2]));

			if(RdotV < 0.00){
				RdotV = 0;
			}
			//SPECULAR COLOR = fatt*lightColor*specularColor*(RdotV)^exponent
			sumLightsR = sumLightsR+(fatt[0]*(gTheScene->lights()[3+9*j]*specularColor[0]*pow(RdotV,exponent)));
			sumLightsG = sumLightsG+(fatt[1]*(gTheScene->lights()[4+9*j]*specularColor[1]*pow(RdotV,exponent)));
			sumLightsB = sumLightsB+(fatt[2]*(gTheScene->lights()[5+9*j]*specularColor[2]*pow(RdotV,exponent)));
		}
		else exit(0);
	}
	//}
	/*****************************************************************************************************/
	// CALCULATE INDIRECT LIGHTING
	/*****************************************************************************************************/
	j = 0;
	float dist = sqrt( (lightVector[0]*lightVector[0])+(lightVector[1]*lightVector[1])+(lightVector[2]*lightVector[2]) );

	for(int i = 0; i < gNumDLights; i++)
	//for(int i = 0; i < 6; i++)
	{
		//VECTOR FROM POINT TO DIRECTIONAL LIGHT
		float DLmx[3] = {cnArray[7*i+0] - point[0], cnArray[7*i+1] - point[1], cnArray[7*i+2] - point[2]};
		float DLmxMAG = sqrt(DLmx[0]*DLmx[0]+DLmx[1]*DLmx[1]+DLmx[2]*DLmx[2]);
				
		float NdotDLmx = normalizednormal[0]*DLmx[0]/DLmxMAG+
							 normalizednormal[1]*DLmx[1]/DLmxMAG+
							 normalizednormal[2]*DLmx[2]/DLmxMAG;

		float direction[3] = {cnArray[7*i+3],cnArray[7*i+4],cnArray[7*i+5]};

		float LdotXmDL = (direction[0]*(-DLmx[0]/DLmxMAG)) + 
						(direction[1]*(-DLmx[1]/DLmxMAG)) +
						(direction[2]*(-DLmx[2]/DLmxMAG));


		//Check to see if the light direction is facing towards the object
		if(NdotDLmx < 0.00001 || LdotXmDL < 0.00001){
			//DO Nothing
		}
		else {
			float fatt = 1/(cnArray[7*i+6]*dist+1);

			//DIFFUSE COLOR = fatt*NdotL(for point then for DLight)
			irradiance = irradiance +(fatt* NdotDLmx*LdotXmDL);

		}
	}
/*****************************************************************************************************/

	int totalLightR = (255*sumLightsR) + (irradiance*diffuseColor[0]*gTheScene->lights()[3+9*j]);
	int totalLightG = (255*sumLightsG) + (irradiance*diffuseColor[1]*gTheScene->lights()[4+9*j]);
	int totalLightB = (255*sumLightsB) + (irradiance*diffuseColor[2]*gTheScene->lights()[5+9*j]);

	if (totalLightR >255) totalLightR = 255;
	if (totalLightG >255) totalLightG = 255;
	if (totalLightB >255) totalLightB = 255;

	*cp = Pixel( totalLightR,totalLightG,totalLightB);
	*ip = Pixel( (irradiance*diffuseColor[0]*gTheScene->lights()[3+9*j]),
		(irradiance*diffuseColor[1]*gTheScene->lights()[4+9*j]),
		(irradiance*diffuseColor[2]*gTheScene->lights()[5+9*j]));
}

void color_buffer() {
	
	const char * ply_model = gTheScene->meshFile().c_str();

	gInputModel = readPlyModel( ply_model );
	if(gInputModel==0){
		if(gVerbose) cout << "No Ply Model Loaded" << endl;
	}
	else {
		float maxX = -100;
		float maxY = -100;
		float maxZ = -100;
		float minX = 100;
		float minY = 100;
		float minZ = 100;
		//COMPUTING BOUNDING SPHERE DIMENSIONS
		for( unsigned int i = 0; i < gInputModel->vertexCount(); i++ ){
			if(maxX < gInputModel->vertices[i][0]) maxX = gInputModel->vertices[i][0];
			if(minX > gInputModel->vertices[i][0]) minX = gInputModel->vertices[i][0];
			if(maxY < gInputModel->vertices[i][1]) maxY = gInputModel->vertices[i][1];
			if(minY > gInputModel->vertices[i][1]) minY = gInputModel->vertices[i][1];
			if(maxZ < gInputModel->vertices[i][2]) maxZ = gInputModel->vertices[i][2];
			if(minZ > gInputModel->vertices[i][2]) minZ = gInputModel->vertices[i][2];
		}
		diff = sqrt( (maxX-minX)*(maxX-minX)+(maxY-minY)*(maxY-minY)+(maxZ-minZ)*(maxZ-minZ) );
		midpt[0] = maxX-((maxX-minX)/2);
		midpt[1] = maxY-((maxY-minY)/2);
		midpt[2] = maxZ-((maxZ-minZ)/2);
		if(gVerbose) printf("Ply Model %s Loaded\n", ply_model);
	}


	//35mm
	float baseAngle = 54.4322;
	float newAngle = 35;
	float viewPercentage = 1;
	//Calculate Viewing Angle from focal distance
	if(gTheScene->camType() == 2){
		newAngle = (180*2*atan(36/(2*gTheScene->camDistance())))/3.1416;
		viewPercentage = baseAngle/newAngle;
	}
	//Use Given Viewing Angle
	if(gTheScene->camType() == 1){
		newAngle = gTheScene->camAngle();
		viewPercentage = baseAngle/newAngle;
	}

	//Create new Image with x_res, y_res, and background color
	color_image = new Image(gResolution,gResolution,gTheScene->backgroundColor());
	Pixel *cp = color_image->pixels;

	//INDIRECT LIGHTING ONLY
	indirect_image = new Image(gResolution,gResolution,gTheScene->backgroundColor());
	Pixel *ip = indirect_image->pixels;


	W = 5/viewPercentage;
	H = 5/viewPercentage;
	float resX = gResolution;
	float resY = gResolution;

	//Check for camera up vector
	bool zRotate = false;
	float result = 0;
	float mag = sqrt(gTheScene->camUp()[0]*gTheScene->camUp()[0]+gTheScene->camUp()[1]*gTheScene->camUp()[1]);

	if(gTheScene->camUp()[0]>0.0001 || gTheScene->camUp()[0]< -0.0001 ){
		zRotate = true;
		result = acos(gTheScene->camUp()[1]/mag);
		if(gTheScene->camUp()[0]>0.0001){
			result = -result;
		}
	}

	tArray = new float[gResolution*gResolution];
	for(int i = 0; i < resX*resY; i++) {
		tArray[i] = 20;
	}

	int currentPixel = 0;
	while(currentPixel<resX*resY){

		float coordX = -(W/2)+(W/(resX-1))*(currentPixel%int(resX));
		float coordY = (H/2)-(H/(resY-1))*floor(currentPixel/resY);
		float coordZ = 0;

		if(zRotate) {
			float tempX = coordX;
			float tempY = coordY;
			coordX = cos(result)*tempX - sin(result)*tempY;
			coordY = sin(result)*tempX + cos(result)*tempY;
		}

		float eye[3] = {0,0,0};
		float d[3] = {0,0,0};

		if(gTheScene->camDirection()[0] == 0 && gTheScene->camDirection()[1] == -1 && gTheScene->camDirection()[2] == 0)
		{
			if(gTheScene->camUp()[2] == 1)
			{
				coordZ = coordY;
				coordY = 0;
			}
			else
			{
				coordZ = -coordY;
				coordY = 0;
			}
		}


		//Perspective Camera OR Simple Perspective Camera
		if( (gTheScene->camType() == 1) || (gTheScene->camType() == 2) ){
			eye[0] = gTheScene->camCenter()[0];
			eye[1] = gTheScene->camCenter()[1];
			eye[2] = gTheScene->camCenter()[2];
			d[0] = coordX - gTheScene->camCenter()[0];
			d[1] = coordY - gTheScene->camCenter()[1];
			d[2] = coordZ - gTheScene->camCenter()[2];
		}
		//Orthographic Projection
		else {
			eye[0] = coordX+gTheScene->camCenter()[0];
			eye[1] = coordY+gTheScene->camCenter()[1];
			eye[2] = gTheScene->camCenter()[2];
			d[0] = gTheScene->camDirection()[0];
			d[1] = gTheScene->camDirection()[1];
			d[2] = gTheScene->camDirection()[2];
		}

		//SPHERES
		for(int i = 0; i < gTheScene->numSpheres(); i++) {
			//Compute Intersection
			float tMinus = intersectSphere(i, eye, d);
			if((tMinus >= 1) && (tMinus < tArray[currentPixel])){
				tArray[currentPixel] = tMinus;

				float point[3] = {0,0,0};
				//Perspective Camera OR Simple Perspective Camera
				if( (gTheScene->camType() == 1) || (gTheScene->camType() == 2) ){
					point[0] = gTheScene->camCenter()[0] + tMinus*(normalize3D(d)[0]);
					point[1] = gTheScene->camCenter()[1] + tMinus*(normalize3D(d)[1]);
					point[2] = gTheScene->camCenter()[2] + tMinus*(normalize3D(d)[2]);
				}
				//Orthographic Projection
				else {
					point[0] = coordX+gTheScene->camCenter()[0]+tMinus*(normalize3D(d)[0]);
					point[1] = coordY+gTheScene->camCenter()[1]+tMinus*(normalize3D(d)[1]);
					point[2] = gTheScene->camCenter()[2]+tMinus*(normalize3D(d)[2]);
				}

				float center[3] = {gTheScene->allSpheres()[1+5*i],gTheScene->allSpheres()[2+5*i],gTheScene->allSpheres()[3+5*i]};
				float normal[3] = {point[0] - center[0], point[1] - center[1], point[2] - center[2]};

				//Compute Lighting and Color Pixels
				float diffuseColor[3] = { gTheScene->materials()[0+7*int(gTheScene->allSpheres()[0+5*i])],
														gTheScene->materials()[1+7*int(gTheScene->allSpheres()[0+5*i])],
														gTheScene->materials()[2+7*int(gTheScene->allSpheres()[0+5*i])]};
				float specularColor[3] = {  gTheScene->materials()[3+7*int(gTheScene->allSpheres()[0+5*i])],
															gTheScene->materials()[4+7*int(gTheScene->allSpheres()[0+5*i])],
															gTheScene->materials()[5+7*int(gTheScene->allSpheres()[0+5*i])]};
				float exponent = gTheScene->materials()[6+7*int(gTheScene->allSpheres()[0+5*i])];

				lightObject(currentPixel, point, normal, cp, ip, d, diffuseColor, specularColor, exponent,0,i);
				
			}
		}

		//PLANES
		for(int i = 0; i < gTheScene->numPlanes(); i++) {
			float t = intersectPlane(i, eye, d);

			if((t >= 1) && (t < tArray[currentPixel])){
				float point[3] = {0,0,0};
				float normal[3] = {gTheScene->allPlanes()[1+5*i],
				gTheScene->allPlanes()[2+5*i],
				gTheScene->allPlanes()[3+5*i]};
				//Perspective Camera OR Simple Perspective Camera
				if( (gTheScene->camType() == 1) || (gTheScene->camType() == 2) ){
					point[0] = gTheScene->camCenter()[0] + t*(normalize3D(d)[0]);
					point[1] = gTheScene->camCenter()[1] + t*(normalize3D(d)[1]);
					point[2] = gTheScene->camCenter()[2] + t*(normalize3D(d)[2]);
				}
				//Orthographic Projection
				else {
					point[0] = coordX+gTheScene->camCenter()[0]+t*(normalize3D(d)[0]);
					point[1] = coordY+gTheScene->camCenter()[1]+t*(normalize3D(d)[1]);
					point[2] = gTheScene->camCenter()[2]+t*(normalize3D(d)[2]);
				}

				//Compute Lighting and Color Pixels
				float diffuseColor[3] = { gTheScene->materials()[0+7*int(gTheScene->allPlanes()[0+5*i])],
														gTheScene->materials()[1+7*int(gTheScene->allPlanes()[0+5*i])],
														gTheScene->materials()[2+7*int(gTheScene->allPlanes()[0+5*i])]};
				float specularColor[3] = {  gTheScene->materials()[3+7*int(gTheScene->allPlanes()[0+5*i])],
															gTheScene->materials()[4+7*int(gTheScene->allPlanes()[0+5*i])],
															gTheScene->materials()[5+7*int(gTheScene->allPlanes()[0+5*i])]};
				float exponent = gTheScene->materials()[6+7*int(gTheScene->allPlanes()[0+5*i])];

				lightObject(currentPixel, point, normal, cp, ip, d, diffuseColor, specularColor, exponent,1,i);
				tArray[currentPixel] = t;
			}
		}

		//TRIANGLE
		for(int i = 0; i < gTheScene->numTriangles(); i++) {
			float t = intersectTriangle(i, eye, d, false);
			if(t >= 1){
				if(t < tArray[currentPixel]){
					float point[3] = {0,0,0};
					//B-A
					float BMA[3] = { (gTheScene->allTriangles()[4+10*i])-(gTheScene->allTriangles()[1+10*i]),
												(gTheScene->allTriangles()[5+10*i])-(gTheScene->allTriangles()[2+10*i]),
												(gTheScene->allTriangles()[6+10*i])-(gTheScene->allTriangles()[3+10*i])};
					//C-A
					float CMA[3] = { (gTheScene->allTriangles()[7+10*i])-(gTheScene->allTriangles()[1+10*i]),
												(gTheScene->allTriangles()[8+10*i])-(gTheScene->allTriangles()[2+10*i]),
												(gTheScene->allTriangles()[9+10*i])-(gTheScene->allTriangles()[3+10*i])};
					//(B-A)X(C-A)
					float normal[3] = {BMA[1]*CMA[2]-BMA[2]*CMA[1],-BMA[0]*CMA[2]+BMA[2]*CMA[0],BMA[0]*CMA[1]-BMA[1]*CMA[0]};

					//Perspective Camera OR Simple Perspective Camera
					if( (gTheScene->camType() == 1) || (gTheScene->camType() == 2) ){
						point[0] = gTheScene->camCenter()[0] + t*(normalize3D(d)[0]);
						point[1] = gTheScene->camCenter()[1] + t*(normalize3D(d)[1]);
						point[2] = gTheScene->camCenter()[2] + t*(normalize3D(d)[2]);
					}
					//Orthographic Projection
					else {
						point[0] = coordX+gTheScene->camCenter()[0]+t*(normalize3D(d)[0]);
						point[1] = coordY+gTheScene->camCenter()[1]+t*(normalize3D(d)[1]);
						point[2] = gTheScene->camCenter()[2]+t*(normalize3D(d)[2]);
					}
					//Compute Lighting and Color Pixels
					float diffuseColor[3] = { gTheScene->materials()[0+7*int(gTheScene->allTriangles()[0+10*i])],
															gTheScene->materials()[1+7*int(gTheScene->allTriangles()[0+10*i])],
															gTheScene->materials()[2+7*int(gTheScene->allTriangles()[0+10*i])]};
					float specularColor[3] ={gTheScene->materials()[3+7*int(gTheScene->allTriangles()[0+10*i])],
															 gTheScene->materials()[4+7*int(gTheScene->allTriangles()[0+10*i])],
															 gTheScene->materials()[5+7*int(gTheScene->allTriangles()[0+10*i])]};
					float exponent = gTheScene->materials()[6+7*int(gTheScene->allTriangles()[0+10*i])];

					lightObject(currentPixel, point, normal, cp, ip, d, diffuseColor, specularColor, exponent,2,i);
					tArray[currentPixel] = t;
					
				}
			}
		}

		//TRIANGLE MESH
		if(gInputModel!=0){
			//BOUNDING VOLUME TEST (SPHERE)
			float BVcenter[3] = {midpt[0],midpt[1], midpt[2]};
			float BVradius = (diff/2);

			////A = dNorm DOT dNorm
			float A =normalize3D(d)[0]*normalize3D(d)[0]+
					 normalize3D(d)[1]*normalize3D(d)[1]+
					 normalize3D(d)[2]*normalize3D(d)[2];

			////B = 2(eye-center) DOT dNorm
			float eyeMinusCenter[3] = {eye[0]-BVcenter[0],eye[1]-BVcenter[1],eye[2]-BVcenter[2]};

			float B = 2*(eyeMinusCenter[0]*(normalize3D(d)[0])+
			eyeMinusCenter[1]*(normalize3D(d)[1])+
			eyeMinusCenter[2]*(normalize3D(d)[2]));

			//C = (eye-center) DOT (eye-center) - radius^2
			float C = (eyeMinusCenter[0]*eyeMinusCenter[0]+
			eyeMinusCenter[1]*eyeMinusCenter[1]+
			eyeMinusCenter[2]*eyeMinusCenter[2]) - (BVradius*BVradius);

			//B^2 - 4AC
			float Q = (B*B) - 4*(A*C);

			if(Q >= 0){
				for(unsigned int i = 0; i < gInputModel->faceCount( ); i++) {
					float t = intersectTriangle(i, eye, d, true);
					if(t >= 1){
						if(t < tArray[currentPixel]){
							float point[3] = {0,0,0};

							//(B-A)X(C-A)
							float normal[3] = {gInputModel->faceNormals[i][0],gInputModel->faceNormals[i][1],gInputModel->faceNormals[i][2]};

							//Perspective Camera OR Simple Perspective Camera
							if( (gTheScene->camType() == 1) || (gTheScene->camType() == 2) ){
								point[0] = gTheScene->camCenter()[0] + t*(normalize3D(d)[0]);
								point[1] = gTheScene->camCenter()[1] + t*(normalize3D(d)[1]);
								point[2] = gTheScene->camCenter()[2] + t*(normalize3D(d)[2]);
							}
							//Orthographic Projection
							else {
								point[0] = coordX+gTheScene->camCenter()[0]+t*(normalize3D(d)[0]);
								point[1] = coordY+gTheScene->camCenter()[1]+t*(normalize3D(d)[1]);
								point[2] = gTheScene->camCenter()[2]+t*(normalize3D(d)[2]);
							}
							//Compute Lighting and Color Pixels
							float diffuseColor[3] = { gTheScene->materials()[0+7*gTheScene->materialIndex()[0]],
																	gTheScene->materials()[1+7*gTheScene->materialIndex()[0]],
																	gTheScene->materials()[2+7*gTheScene->materialIndex()[0]]};
							float specularColor[3] ={gTheScene->materials()[3+7*gTheScene->materialIndex()[0]],
																	 gTheScene->materials()[4+7*gTheScene->materialIndex()[0]],
																	 gTheScene->materials()[5+7*gTheScene->materialIndex()[0]]};
							float exponent = gTheScene->materials()[6+7*gTheScene->materialIndex()[0]];

							lightObject(currentPixel, point, normal, cp, ip, d, diffuseColor, specularColor, exponent,3,0);
							tArray[currentPixel] = t;
						}
					}
				}
			}
		}
		*cp++;
		*ip++;
		currentPixel++;
	}
	
}

void depth_buffer() {

	const char * ply_model = gTheScene->meshFile().c_str();

	gInputModel = readPlyModel( ply_model );
	if(gInputModel==0){
		if(gVerbose) cout << "No Ply Model Loaded" << endl;
	}
	else {
		float maxX = -100;
		float maxY = -100;
		float maxZ = -100;
		float minX = 100;
		float minY = 100;
		float minZ = 100;
		//COMPUTING BOUNDING SPHERE DIMENSIONS
		for( unsigned int i = 0; i < gInputModel->vertexCount(); i++ ){
			if(maxX < gInputModel->vertices[i][0]) maxX = gInputModel->vertices[i][0];
			if(minX > gInputModel->vertices[i][0]) minX = gInputModel->vertices[i][0];
			if(maxY < gInputModel->vertices[i][1]) maxY = gInputModel->vertices[i][1];
			if(minY > gInputModel->vertices[i][1]) minY = gInputModel->vertices[i][1];
			if(maxZ < gInputModel->vertices[i][2]) maxZ = gInputModel->vertices[i][2];
			if(minZ > gInputModel->vertices[i][2]) minZ = gInputModel->vertices[i][2];
		}
		diff = sqrt( (maxX-minX)*(maxX-minX)+(maxY-minY)*(maxY-minY)+(maxZ-minZ)*(maxZ-minZ) );
		midpt[0] = maxX-((maxX-minX)/2);
		midpt[1] = maxY-((maxY-minY)/2);
		midpt[2] = maxZ-((maxZ-minZ)/2);
		if(gVerbose) printf("Ply Model %s Loaded\n", ply_model);
	}

	//SET UP DEPTH IMAGE
	float black[3] = {0,0,0};
	depth_image = new Image(gResolution,gResolution,black);
	Pixel *dp = depth_image->pixels;

	//SET UP NORMAL IMAGE
	normal_image = new Image(gResolution,gResolution,black);
	Pixel *np = normal_image->pixels;

	//SET UP WORLD SPACE COORDINATE IMAGE
	coord_image = new Image(gResolution,gResolution,black);
	Pixel *wp = coord_image->pixels;

	//SET UP FLUX BUFFER IMAGE
	flux_image = new Image(gResolution,gResolution,black);
	Pixel *fp = flux_image->pixels;


	//SET UP CAMERA NORMAL IMAGE
	cam_normal_image = new Image(3*5,gNumDLights/5,black);
	Pixel *cnp = cam_normal_image->pixels;

	//SET UP CAMERA WORLD SPACE COORDINATE IMAGE
	cam_coord_image = new Image(gResolution,gResolution,black);
	Pixel *cwp = cam_coord_image->pixels;

	float W = 5;
	float H = 5;
	float resX = gResolution;
	float resY = gResolution;

	dArray = new float[gResolution*gResolution];
	for(int i = 0; i < gResolution*gResolution; i++) {
		dArray[i] = 20;
	}

	nArray = new float[3*gResolution*gResolution];
	for(int i = 0; i < gResolution*gResolution; i++) {
		nArray[i*3+0] = 0;
		nArray[i*3+1] = 0;
		nArray[i*3+2] = 0;
	}

	cArray = new float[3*gResolution*gResolution];
	for(int i = 0; i < gResolution*gResolution; i++) {
		cArray[i*3+0] = 0;
		cArray[i*3+1] = 0;
		cArray[i*3+2] = 0;
	}

	fArray = new float[3*gResolution*gResolution];
	for(int i = 0; i < gResolution*gResolution; i++) {
		fArray[i*3+0] = 0;
		fArray[i*3+1] = 0;
		fArray[i*3+2] = 0;
	}

	ccArray = new float[3*gResolution*gResolution];
	for(int i = 0; i < gResolution*gResolution; i++) {
		ccArray[i*3+0] = 0;
		ccArray[i*3+1] = 0;
		ccArray[i*3+2] = 0;
	}

	// ONLY 1 LIGHT SUPPORT currently
	int j = 0;
	float eye[3] = {0,0,0};
	eye[0] = gTheScene->lights()[0+9*j];
	eye[1] = gTheScene->lights()[1+9*j];
	eye[2] = gTheScene->lights()[2+9*j];

	int currentPixel = 0;
	while(currentPixel<resX*resY){

		float coordX = -(W/2)+(W/(resX-1))*(currentPixel%int(resX));
		float coordY = (H/2)-(H/(resY-1))*floor(currentPixel/resY);
		float coordZ = 0;

		coordZ = -coordY;
		coordY = 0;

		//Perspective Camera OR Simple Perspective FROM LIGHT POSITION
		float d[3] = {0,0,0};
		d[0] = coordX - eye[0];
		d[1] = coordY - eye[1];
		d[2] = coordZ - eye[2];

		//SPHERES
		for(int i = 0; i < gTheScene->numSpheres(); i++) {
			//Compute Intersection
			float tMinus = intersectSphere(i, eye, d);
			if((tMinus >= 0.1) && (tMinus < dArray[currentPixel])){

				float point[3] = {0,0,0};

				//Perspective Camera OR Simple Perspective Camera
				point[0] = eye[0] + tMinus*(normalize3D(d)[0]);
				point[1] = eye[1] + tMinus*(normalize3D(d)[1]);
				point[2] = eye[2] + tMinus*(normalize3D(d)[2]);
				
				float center[3] = {gTheScene->allSpheres()[1+5*i],gTheScene->allSpheres()[2+5*i],gTheScene->allSpheres()[3+5*i]};
				float normal[3] = {point[0] - center[0], point[1] - center[1], point[2] - center[2]};

				dArray[currentPixel] = tMinus;
				nArray[currentPixel*3+0] = normalize3D(normal)[0];
				nArray[currentPixel*3+1] = normalize3D(normal)[1];
				nArray[currentPixel*3+2] = normalize3D(normal)[2];
				cArray[currentPixel*3+0] = point[0];
				cArray[currentPixel*3+1] = point[1];
				cArray[currentPixel*3+2] = point[2];

				float refCoe = (-normalize3D(normal)[0])*(normalize3D(d)[0])-(normalize3D(normal)[1])*(normalize3D(d)[1])-
					(normalize3D(normal)[2])*(normalize3D(d)[2]);

				fArray[currentPixel*3+0] = max(0,gTheScene->materials()[0+7*int(gTheScene->allSpheres()[0+5*i])]*refCoe);
				fArray[currentPixel*3+1] = max(0,gTheScene->materials()[1+7*int(gTheScene->allSpheres()[0+5*i])]*refCoe);
				fArray[currentPixel*3+2] = max(0,gTheScene->materials()[2+7*int(gTheScene->allSpheres()[0+5*i])]*refCoe);
			}
		}

		//PLANES
		for(int i = 0; i < gTheScene->numPlanes(); i++) {
			float t = intersectPlane(i, eye, d);

			if((t >= 0.1) && (t < dArray[currentPixel])){
				float point[3] = {0,0,0};
				float normal[3] = {gTheScene->allPlanes()[1+5*i],
				gTheScene->allPlanes()[2+5*i],
				gTheScene->allPlanes()[3+5*i]};

				//Perspective Camera OR Simple Perspective Camera
				point[0] = eye[0] + t*(normalize3D(d)[0]);
				point[1] = eye[1] + t*(normalize3D(d)[1]);
				point[2] = eye[2] + t*(normalize3D(d)[2]);

				dArray[currentPixel] = t;
				nArray[currentPixel*3+0] = normalize3D(normal)[0];
				nArray[currentPixel*3+1] = normalize3D(normal)[1];
				nArray[currentPixel*3+2] = normalize3D(normal)[2];
				cArray[currentPixel*3+0] = point[0];
				cArray[currentPixel*3+1] = point[1];
				cArray[currentPixel*3+2] = point[2];

				float refCoe = (-normalize3D(normal)[0])*(normalize3D(d)[0])-(normalize3D(normal)[1])*(normalize3D(d)[1])-
					(normalize3D(normal)[2])*(normalize3D(d)[2]);
				
				fArray[currentPixel*3+0] = max(0,gTheScene->materials()[0+7*int(gTheScene->allPlanes()[0+5*i])]*refCoe);
				fArray[currentPixel*3+1] = max(0,gTheScene->materials()[1+7*int(gTheScene->allPlanes()[0+5*i])]*refCoe);
				fArray[currentPixel*3+2] = max(0,gTheScene->materials()[2+7*int(gTheScene->allPlanes()[0+5*i])]*refCoe);	 
			}
		}

		//TRIANGLE
		for(int i = 0; i < gTheScene->numTriangles(); i++) {
			float t = intersectTriangle(i, eye, d, false);
			if(t >= 0.1){
				if(t < dArray[currentPixel]){
					float point[3] = {0,0,0};
					//B-A
					float BMA[3] = { (gTheScene->allTriangles()[4+10*i])-(gTheScene->allTriangles()[1+10*i]),
												(gTheScene->allTriangles()[5+10*i])-(gTheScene->allTriangles()[2+10*i]),
												(gTheScene->allTriangles()[6+10*i])-(gTheScene->allTriangles()[3+10*i])};
					//C-A
					float CMA[3] = { (gTheScene->allTriangles()[7+10*i])-(gTheScene->allTriangles()[1+10*i]),
												(gTheScene->allTriangles()[8+10*i])-(gTheScene->allTriangles()[2+10*i]),
												(gTheScene->allTriangles()[9+10*i])-(gTheScene->allTriangles()[3+10*i])};
					//(B-A)X(C-A)
					float normal[3] = {BMA[1]*CMA[2]-BMA[2]*CMA[1],-BMA[0]*CMA[2]+BMA[2]*CMA[0],BMA[0]*CMA[1]-BMA[1]*CMA[0]};

					//Perspective Camera OR Simple Perspective Camera
					point[0] = eye[0] + t*(normalize3D(d)[0]);
					point[1] = eye[1] + t*(normalize3D(d)[1]);
					point[2] = eye[2] + t*(normalize3D(d)[2]);

					dArray[currentPixel] = t;
					nArray[currentPixel*3+0] = normalize3D(normal)[0];
					nArray[currentPixel*3+1] = normalize3D(normal)[1];
					nArray[currentPixel*3+2] = normalize3D(normal)[2];
					cArray[currentPixel*3+0] = point[0];
					cArray[currentPixel*3+1] = point[1];
					cArray[currentPixel*3+2] = point[2];

					float refCoe = (-normalize3D(normal)[0])*(normalize3D(d)[0])-(normalize3D(normal)[1])*(normalize3D(d)[1])-
								(normalize3D(normal)[2])*(normalize3D(d)[2]);

					fArray[currentPixel*3+0] = max(0,gTheScene->materials()[0+7*int(gTheScene->allTriangles()[0+10*i])]*refCoe);
					fArray[currentPixel*3+1] = max(0,gTheScene->materials()[1+7*int(gTheScene->allTriangles()[0+10*i])]*refCoe);
					fArray[currentPixel*3+2] = max(0,gTheScene->materials()[2+7*int(gTheScene->allTriangles()[0+10*i])]*refCoe);	
					
				}
			}
		}

		//TRIANGLE MESH
		if(gInputModel!=0){
			//BOUNDING VOLUME TEST (SPHERE)
			float BVcenter[3] = {midpt[0],midpt[1], midpt[2]};
			float BVradius = (diff/2);

			////A = dNorm DOT dNorm
			float A =normalize3D(d)[0]*normalize3D(d)[0]+
					 normalize3D(d)[1]*normalize3D(d)[1]+
					 normalize3D(d)[2]*normalize3D(d)[2];

			////B = 2(eye-center) DOT dNorm
			float eyeMinusCenter[3] = {eye[0]-BVcenter[0],eye[1]-BVcenter[1],eye[2]-BVcenter[2]};

			float B = 2*(eyeMinusCenter[0]*(normalize3D(d)[0])+
			eyeMinusCenter[1]*(normalize3D(d)[1])+
			eyeMinusCenter[2]*(normalize3D(d)[2]));

			//C = (eye-center) DOT (eye-center) - radius^2
			float C = (eyeMinusCenter[0]*eyeMinusCenter[0]+
			eyeMinusCenter[1]*eyeMinusCenter[1]+
			eyeMinusCenter[2]*eyeMinusCenter[2]) - (BVradius*BVradius);

			//B^2 - 4AC
			float Q = (B*B) - 4*(A*C);

			if(Q >= 0){
				for(unsigned int i = 0; i < gInputModel->faceCount( ); i++) {
					float t = intersectTriangle(i, eye, d, true);
					if(t >= 0.1){
						if(t < dArray[currentPixel]){
							float point[3] = {0,0,0};

							//(B-A)X(C-A)
							float normal[3] = {gInputModel->faceNormals[i][0],gInputModel->faceNormals[i][1],gInputModel->faceNormals[i][2]};

							//Perspective Camera OR Simple Perspective Camera
							point[0] = eye[0] + t*(normalize3D(d)[0]);
							point[1] = eye[1] + t*(normalize3D(d)[1]);
							point[2] = eye[2] + t*(normalize3D(d)[2]);


							dArray[currentPixel] = t;
							nArray[currentPixel*3+0] = normalize3D(normal)[0];
							nArray[currentPixel*3+1] = normalize3D(normal)[1];
							nArray[currentPixel*3+2] = normalize3D(normal)[2];
							cArray[currentPixel*3+0] = point[0];
							cArray[currentPixel*3+1] = point[1];
							cArray[currentPixel*3+2] = point[2];

							float refCoe = (-normalize3D(normal)[0])*(normalize3D(d)[0])-(normalize3D(normal)[1])*(normalize3D(d)[1])-
									(normalize3D(normal)[2])*(normalize3D(d)[2]);

							fArray[currentPixel*3+0] = max(0,gTheScene->materials()[0+7*gTheScene->materialIndex()[0]]*refCoe);
							fArray[currentPixel*3+1] = max(0,gTheScene->materials()[1+7*gTheScene->materialIndex()[0]]*refCoe);
							fArray[currentPixel*3+2] = max(0,gTheScene->materials()[2+7*gTheScene->materialIndex()[0]]*refCoe);
						}
					}
				}
			}
		}
		currentPixel++;
	}

	//CALCULATE DEPTH MAP RANGE
	float max = 0;
	float min = 20;
	currentPixel = 0;
	while(currentPixel<resX*resY){
		if(dArray[currentPixel] >= 20){
			//do nothing
		}
		else if(max < dArray[currentPixel]) {
			max = dArray[currentPixel]; 
		}
		else if(min > dArray[currentPixel]) {
			min = dArray[currentPixel]; 
		}
		currentPixel++;
	}

	float range  = max-min;
	if (range == 0){
		range = 0.01;
	}

	//DEPTH MAP CREATION
	currentPixel = 0;
	while(currentPixel<resX*resY){
		if(dArray[currentPixel] >= 20){
			*dp = Pixel(0,0,0);
		}
		else{
			*dp = Pixel(100+(155*(1-(dArray[currentPixel]-min)/range)),
								100+(155*(1-(dArray[currentPixel]-min)/range)),
								100+(155*(1-(dArray[currentPixel]-min)/range)));	
		}
		*dp++;
		currentPixel++;
	}
	
	//NORMAL MAP CREATION
	currentPixel = 0;
	while(currentPixel<resX*resY){
		float normal[3] = {nArray[3*currentPixel+0],nArray[3*currentPixel+1],nArray[3*currentPixel+2]};
		*np = Pixel(abs((normal[0])*255),
					abs((normal[1])*255),
					abs((normal[2])*255));	
		
		*np++;
		currentPixel++;
	}

	//WORLD SPACE COORDINATE MAP CREATION		
	currentPixel = 0;
	while(currentPixel<resX*resY){
		float point[3] = {cArray[3*currentPixel+0],cArray[3*currentPixel+1],cArray[3*currentPixel+2]};
		*wp = Pixel(abs((normalize3D(point)[0])*255),
					abs((normalize3D(point)[1])*255),
					abs((normalize3D(point)[2])*255));	
		
		*wp++;
		currentPixel++;
	}
	
	//FLUX BUFFER CREATION
	currentPixel = 0;
	while(currentPixel<resX*resY){
		*fp = Pixel(fArray[currentPixel*3+0]*255,
					fArray[currentPixel*3+1]*255,
					fArray[currentPixel*3+2]*255);	
		
		*fp++;
		currentPixel++;
	}
	/**************************************************************************************
	* Creating Camera Buffer with center(3: x,y,z), normal(3: x,y,z), attenuation(1)
	* 6485 Lights, 1297rays *5per ray
	* 72 horizontal (360 degrees/5) * 18 vertical (90/5) + 1 (<0,-1,0> ray) = 1297 Directional Lights
	* NOTE: DOES NOT INCLUDE PRIMARY DIRECTIONAL LIGHT (INDIRECT ONES ONLY)
	**************************************************************************************/
	//Support for 1 Light atm
	j = 0;
	cnArray = new float[7*gNumDLights];

	// First Ray (<0,-1,0> ray
	for(int i = 0; i <5; i++) {
		// Cam Center in World Space = center + normal direction* max distance *1/5
		cnArray[i*7+0] = gTheScene->lights()[0+9*j] + (gTheScene->dLight()[0+4*j])*8.0*((i+1.0)/5);
		cnArray[i*7+1] = gTheScene->lights()[1+9*j] + (gTheScene->dLight()[1+4*j])*8.0*((i+1.0)/5);
		cnArray[i*7+2] = gTheScene->lights()[2+9*j] + (gTheScene->dLight()[2+4*j])*8.0*((i+1.0)/5);
		// Cam Normal
		cnArray[i*7+3] = gTheScene->dLight()[0+4*j];
		cnArray[i*7+4] = gTheScene->dLight()[1+4*j];
		cnArray[i*7+5] = gTheScene->dLight()[2+4*j];
		// Cam Attenuation - only attenuating all colors of light evenly 90%,70%,50%,30%,10%
		cnArray[i*7+6] = (gTheScene->lights()[6+9*j])+ 0.1 +(0.2*i);
	}
	
	int i = 5;
	int interpolate = 1;

	for(int angleXZ = 0; angleXZ <360; angleXZ = angleXZ +5) {
		for(int angleXY = 275; angleXY <=360; angleXY = angleXY +5){
			for(int counter = 0; counter <5; counter++) {
				// Cam Center in World Space = center + normal direction* max distance *1/5
				cnArray[i*7+0] = gTheScene->lights()[0+9*j] + cos(angleXY*3.1416/180)*cos(angleXZ*3.1416/180)*(8.0-(4.0/interpolate))*(((counter%5)+1.0)/5);
				cnArray[i*7+1] = gTheScene->lights()[1+9*j] + sin(angleXY*3.1416/180)*(8.0-(4.0/interpolate))*(((counter%5)+1.0)/5);
				cnArray[i*7+2] = gTheScene->lights()[2+9*j] - sin(angleXZ*3.1416/180)*(8.0-(4.0/interpolate))*(((counter%5)+1.0)/5);
				// Cam Normal
				float normal[3] = {cos(angleXZ*3.1416/180)*cos(angleXY*3.1416/180),sin(angleXY*3.1416/180),-sin(angleXZ*3.1416/180)};
				cnArray[i*7+3] = (normalize3D(normal)[0]);
				cnArray[i*7+4] = (normalize3D(normal)[1]);
				cnArray[i*7+5] = (normalize3D(normal)[2]);
				// Cam Attenuation - only attenuating all colors of light evenly 90%,70%,50%,30%,10%
				cnArray[i*7+6] = (gTheScene->lights()[6+9*j])+ 0.1 +(0.2*(counter%5));
				i++;
			}
			interpolate++;
		}
		interpolate = 1;
	}




	////0-85 DEGREES VERTICAL
	//for(int angleXY = 270; angleXY <360; angleXY = angleXY +5){
	//	for(int counter = 0; counter <5; counter++) {
	//		// Cam Center in World Space = center + normal direction* max distance *1/5
	//		cnArray[i*7+0] = gTheScene->lights()[0+9*j] + cos(angleXY*3.1416/180)*max*(((i%5)+1.0)/5);
	//		cnArray[i*7+1] = gTheScene->lights()[1+9*j] + sin(angleXY*3.1416/180)*max*(((i%5)+1.0)/5);
	//		cnArray[i*7+2] = gTheScene->lights()[2+9*j] + 0*max*(((i%5)+1.0)/5);
	//		// Cam Normal
	//		cnArray[i*7+3] = cos(angleXY*3.1416/180);
	//		cnArray[i*7+4] = sin(angleXY*3.1416/180);
	//		cnArray[i*7+5] = 0;
	//		// Cam Attenuation - only attenuating all colors of light evenly 90%,70%,50%,30%,10%
	//		cnArray[i*7+6] = (gTheScene->lights()[6+9*j])+ 0.1 +(0.2*(i%5));
	//		i++;
	//	}
	//}

	//// 0-355 degrees horizontal
	//for(int angleXZ = 0; angleXZ <360; angleXZ = angleXZ +5) {
	//	for(int counter = 0; counter <5; counter++) {
	//		// Cam Center in World Space = center + normal direction* max distance *1/5
	//		cnArray[i*7+0] = gTheScene->lights()[0+9*j] + cos(angleXZ*3.1416/180)*max*(((i%5)+1.0)/5);
	//		cnArray[i*7+1] = gTheScene->lights()[1+9*j] + 0*max*(((i%5)+1.0)/5);
	//		cnArray[i*7+2] = gTheScene->lights()[2+9*j] - sin(angleXZ*3.1416/180)*max*(((i%5)+1.0)/5);
	//		// Cam Normal
	//		cnArray[i*7+3] = cos(angleXZ*3.1416/180);
	//		cnArray[i*7+4] = 0;
	//		cnArray[i*7+5] = -sin(angleXZ*3.1416/180);
	//		// Cam Attenuation - only attenuating all colors of light evenly 90%,70%,50%,30%,10%
	//		cnArray[i*7+6] = (gTheScene->lights()[6+9*j])+ 0.1 +(0.2*(i%5));
	//		i++;
	//	}
	//}

	//for(int i = 4; i <7; i++) {
	//	cout << cnArray[i*7+0] << endl;
	//	cout << cnArray[i*7+1] << endl;
	//	cout << cnArray[i*7+2] << endl<<endl;
	//	cout << cnArray[i*7+3] << endl;
	//	cout << cnArray[i*7+4] << endl;
	//	cout << cnArray[i*7+5] << endl<<endl;
	//	cout << cnArray[i*7+6] << endl << "*******" << endl;
	//}


	//CAMERA NORMAL MAP CREATION
	currentPixel = 0;
	while(currentPixel<gNumDLights){
		float normal[3] = {cnArray[7*currentPixel+0],cnArray[7*currentPixel+1],cnArray[7*currentPixel+2]};
		float point[3] = {cnArray[7*currentPixel+3],cnArray[7*currentPixel+4],cnArray[7*currentPixel+5]};
		float att = cnArray[7*currentPixel+6];
		*cnp = Pixel(abs((normal[0])*255),
					abs((normal[1])*255),
					abs((normal[2])*255));	
		
		*cnp++;
		*cnp = Pixel(abs((normalize3D(point)[0])*255),
					abs((normalize3D(point)[1])*255),
					abs((normalize3D(point)[2])*255));		
		
		*cnp++;
		if(att > 1.0) att = 1;
		*cnp = Pixel(att*255,
					att*255,
					att*255);		
		
		*cnp++;
		currentPixel++;
	}

	//CAMERA WORLD SPACE COORDINATE MAP CREATION		
	currentPixel = 0;
	while(currentPixel<resX*resY){
		float point[3] = {ccArray[3*currentPixel+0],ccArray[3*currentPixel+1],ccArray[3*currentPixel+2]};
		*cwp = Pixel(abs((normalize3D(point)[0])*255),
					abs((normalize3D(point)[1])*255),
					abs((normalize3D(point)[2])*255));	
		
		*cwp++;
		currentPixel++;
	}

}

int main( int argc, char **argv ){
	int timing = clock();
	string pathStr;
	gProgramName = argv[0];
	gInputModel = NULL;
	char *in = NULL;

	parseCommandLine( argc, argv );
	argc -= optind;
	argv += optind;
	if( gTheScene->hasInputSceneFilePath( ) &&
		 gTheScene->hasOutputFilePath( ) &&
		 gTheScene->hasDepthFilePath( ) ){
		gTheScene->parse( );	

		//OUTPUT THE PARSED DATA
		//if(gVerbose) cout << *gTheScene << endl;

		//Access Camera Type
		//if(gVerbose) cout << "Cam Type " << gTheScene->camType() << endl;

		//Convert String to const char
		string temp1 = gTheScene->inputSceneFile();
		string temp2 = gTheScene->inputSceneFile();
		string temp3 = gTheScene->inputSceneFile();
		string temp4 = gTheScene->inputSceneFile();
		string temp5 = gTheScene->inputSceneFile();
		string temp6 = gTheScene->inputSceneFile();
		const char * color_out = gTheScene->outputFile().c_str();
		const char * depth_out = gTheScene->depthFile().c_str();
		const char * normal_out =temp1.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_normal_map.ppm").c_str();
		const char * coord_out = temp2.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_world_coord_map.ppm").c_str();
		const char * flux_out = temp3.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_flux_buffer.ppm").c_str();
		const char * indirect_out = temp4.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_indirect_only.ppm").c_str();
		const char * cam_normal_out = temp5.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_cam_normal_map.ppm").c_str();
		const char * cam_coord_out = temp6.replace(gTheScene->inputSceneFile().length()-4,4,"").append("_cam_coord_map.ppm").c_str();
		
///////////////////////////////////////////////////////////////////////////////////////////////

		//CALCULATE DEPTH IMAGE, FLUX IMAGE, WORLD SPACE COORDS, NORMAL IMAGE
		depth_buffer();

		//CALCULATE COLOR IMAGE
		color_buffer();

////////////////////////////////////////////////////////////////////////////////


		if(depth_image->write(depth_out)){
			if(gVerbose) cout << "Depth Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Depth Image Creation Failed!" << endl;
		}
		if(normal_image->write(normal_out)){
			if(gVerbose) cout << "Normal Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Normal Image Creation Failed!" << endl;
		}
		if(coord_image->write(coord_out)){
			if(gVerbose) cout << "World Space Coordinate Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "World Space Coordinate Image Creation Failed!" << endl;
		}
		if(flux_image->write(flux_out)){
			if(gVerbose) cout << "Flux Buffer Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Flux Buffer Creation Failed!" << endl;
		}
		if(cam_normal_image->write(cam_normal_out)){
			if(gVerbose) cout << "Camera Normal Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Camera Normal Image Creation Failed!" << endl;
		}
		if(cam_coord_image->write(cam_coord_out)){
			if(gVerbose) cout << "Camera World Space Coordinate Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Camera World Space Coordinate Image Creation Failed!" << endl;
		}

		if(color_image->write(color_out)){
			if(gVerbose) cout << "Color Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Color Image Creation Failed!" << endl;
		}
		if(indirect_image->write(indirect_out)){
			if(gVerbose) cout << "Indirect Color Image Created Succuessfully!" << endl;
		}
		else {
			if(gVerbose) cout << "Indirect Color Image Creation Failed!" << endl;
		}
	}
	else{
		usage( "You specify an input scene file, an output file and a depth file." );
	}



	int newtime = clock();
	if(gVerbose) printf ("It took %d clicks.\n",newtime-timing);
	if(gVerbose) cout << "REACHED END" << endl;
	cin.get();
	return( 0 );
}
