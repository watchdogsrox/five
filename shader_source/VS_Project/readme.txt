Instructions for adding a new shader:

Step 1: Open shaders_2010.vcxproj. Down at the end there's this comment:
	<!-- ADD SHADER FILES HERE -->
	That's where the new shader files go. Use the actual path to the shader file, relative to the project directory.
	If you want to add a new header, look for <!-- ADD HEADER FILES HERE -->
	
Step 2: Open shaders_2010.vcxproj.filters. This file defines the folder structure you see in VS. 
	Again look for <!-- ADD SHADER FILES HERE --> or <!-- ADD HEADER FILES HERE -->, copy one of the existing blocks and modify it
	Note that the "Include" attribute is the path to the file, and the <Filters> tag is which VS project folder the file should appear in.
