################################
# Carnegie Mellon University
# M.S. Research
# Automatic Hex-Dominant meshing
# for complex flow problems
################################

Aim of this program is to create hex elements in the sharp corners of the geometry and subdivide them according to boundary layer specifications.
Currently 'hex_element_bl.cpp' code separates and stores boundary layer elements and edge elments they share at the sharp edge and 'boundary_subdivider.cpp'
stores the subdivision information and divides the prism elements on the boundary.
Future task is to utilize this information to create and subdivide hex elements.
This code is a part of my research project, which is being performed under the guidance of Professor Kenji Shimada, Professor Satbir Singh and 
Doctor Soji Yamakawa, Department of Mechanical Engineering, Carnegie Mellon University.



********************
Running the program
********************
Compile and run the 'hex_element_bl.cpp' to color the boundary layer faces and 'boundary_subdivider.cpp' to subdivide boundary layer elements.
Compiling the code will require libraries of the mesher and running it will require a '.ms3' mesh file as input.