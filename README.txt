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
	
* The program computes and outputs the: (tentative)
	(from view of camera):
	* color image (with shadows by shadow maps) with indirect lighting

	(from view of the light source):
	* depth map
	* normal map (normals will be of the ~6125 directional lights)
	* world coordinate map (coords will be of directional lights centers)
	* flux buffer (unknown use currently)

Sample commandline(on Windows) :
			raytrace.exe -v -s -i scene5_01.txt -r 250

-r passes in the resolution (required)
-v turns on text output such as display parsed data, etc. (optional)
-s turns on shadow maps instead of light vector intersection tests for shadows

