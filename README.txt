Michael Robertson
mirob2005@gmail.com, miro2005@csu.fullerton.edu
SID: 892-32-2629
Thesis Test Implementation
Start Date: 5/23/12


Assignment was completed using Visual Studio 2008 Pro on Windows 7 64-bit.
Included is all .cpp, .h, and .txt to independently run using any compiler.

All files must be in the same directory to be found.

* The project extends my CPSC 566 Final Project Reflective Shadow Map extension with a test implementation

* This test implementation will attempt to model the global illumination in the scene more like a wave rather than a point light source

*This is done by using 6485 virtual directional lights positioned outward from the primary directional light.
They are distributed evenly using angles of 5 degrees around the Z axis of the primary light (360/5 = 72)
Angles of 5 degrees around the Y axis (90/5 = 18)
And the vertical ray <0,-1,0> (1)
72*18+1 = 1297 rays of lights with 5 lights distributed down each ray with increasingly higher attenuation
1297*5 = 6485

Therefore, performance could be increased and quality (number of lights) decreased by decreasing the number of lights on each ray
OR by increasing the angles betweens lights from 5 to >5.
This has not been tested to find an optimal number of lights with performance gains considered.

TODO: 
1) Decrease power of indirect lighting, images are currently looking overexposed.
2) Test performance gains with quality tradeoff by decreasing the number of lights either by increasing the angles or decreasing the number of lights per ray.


* The program computes and outputs the: (tentative)
	(from view of camera):
	* color image (with shadows by shadow maps) with direct and indirect lighting
	* color image - indirect only
	* color image - direct only

	(from view of the light source):
	* depth map
	(commented out)* normal map (normals will be of the ~6125 directional lights)
	(commented out)* world coordinate map (coords will be of directional lights centers)
	(commented out)* flux buffer (unknown use currently)


Current Runtimes:

250x250 -> 47seconds
500x500 -> 187seconds




Sample commandline(on Windows) :
			raytrace.exe -v -i scene5_01.txt -r 250

-r passes in the resolution (required)
-v turns on text output such as display parsed data, etc. (optional)

