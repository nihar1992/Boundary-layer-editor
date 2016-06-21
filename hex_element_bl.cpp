#include <fsgui3dapp.h>
#include <gui_extension/meshviewer_gui_extension.h>

#include "meshviewer.h"
#include <iostream>

/*
 * hex_element_bl.cpp
 * 
 * Objective:
 * To replace the prism elements in the boundary layer at the sharp edges of
 * the geometry with hex elements to avoid skewness and improve mesh quality.
 *
 * Method:
 * Prism elements at the sharp edges are pulled apart to newly created points 
 * a small distance away from the corners and the resulting gap is filled with 
 * a hex element.
 * Boundary layer elements on the opposite sides of the boundary edges are 
 * colored with different colors (red and green) to decide at which side the 
 * points should be created and how the prism elements should be modified.
 * Coloring is done only if both the faces sharing the boundary edge are
 * boundary layer faces, are convex and angle between them is sufficiently
 * close to 90 degrees.
 * The set of boundary edges and faces is stored in a class along with the 
 * coloring information which will later be used for modifying them and creating
 * the hex elements.
 *
 * Future code:
 * Currently the codes only successfully identifies and colors boundary layer 
 * elements. In future this information will be used to create hex elements.
 */

// Ring of edges
class Ring_node
{
public:
	YSHASHKEY key;
	Ring_node *next;
	Ring_node *prev;
};

class Ring
{
public:
	Ring();
	~Ring();
	void addEdge(YSHASHKEY key);
	Ring_node *root;
	int num_edges;
};

Ring::Ring()
{
	num_edges = 0;
	root = nullptr;
}

Ring::~Ring()
{
	if(root != nullptr)
	{
		num_edges = 0;
		root = nullptr;
	}
}

void Ring::addEdge(YSHASHKEY key)
{
	if(root == nullptr)
	{
		root = new Ring_node;
		root->key = key;
		root->next = nullptr;
		root->prev = nullptr;
	}
	else
	{
		Ring_node *temp = new Ring_node;
		root->next = temp;
		temp->key = key;
		temp->next = nullptr;
		temp->prev = root;
		root = temp;
	}
	num_edges++;
}

//Collection of boundary elements
class Prismlist_node
{
public:
	YSHASHKEY key_r;
	YSHASHKEY key_g;
	Prismlist_node *next;
	Prismlist_node *prev;
};

class Prismlist
{
public:
	Prismlist();
	~Prismlist();
	void addPrism(YSHASHKEY key1, YSHASHKEY key2);
	Prismlist_node *root;
	int num_prisms;
};

Prismlist::Prismlist()
{
	num_prisms = 0;
	root = nullptr;
}

Prismlist::~Prismlist()
{
	if(root != nullptr)
	{
		num_prisms = 0;
		root = nullptr;
	}
}

void Prismlist::addPrism(YSHASHKEY key1, YSHASHKEY key2)
{
	if(root == nullptr)
	{
		root = new Prismlist_node;
		root->key_r = key1;
		root->key_g = key2;
		root->next = nullptr;
		root->prev = nullptr;
	}
	else
	{
		Prismlist_node *temp = new Prismlist_node;
		root->next = temp;
		temp->key_r = key1;
		temp->key_g = key2;
		temp->next = nullptr;
		temp->prev = root;
		root = temp;
	}
	num_prisms++;
}

//Collection of boundary rings and faces
class BL_node
{
public:
	Ring *ring;
	Prismlist *prismlist;
	BL_node *next;
	BL_node *prev;
};

class BL
{
public:
	BL();
	~BL();
	void addBL(Ring *r, Prismlist *p);
	BL_node *root;
	int num_bl;
};

BL::BL()
{
	num_bl = 0;
	root = nullptr;
}

BL::~BL()
{
	if(root != nullptr)
	{
		num_bl = 0;
		root = nullptr;
	}
}

void BL::addBL(Ring *r, Prismlist *p)
{
	if(root == nullptr)
	{
		root = new BL_node;
		root->ring = r;
		root->prismlist = p;
		root->next = nullptr;
		root->prev = nullptr;
	}
	else
	{
		BL_node *temp = new BL_node;
		temp->ring = r;
		temp->prismlist = p;
		root->next = temp;
		temp->prev = root;
		root = temp;
	}
	num_bl++;
}

void MeshViewerGuiExtension::ColorElements(FsGuiPopUpMenuItem *)
{
	auto &canvas=*canvasPtr;
	auto meshAndDrawingPtr=canvas.meshStore.GetFirstMesh();

	if(NULL!=meshAndDrawingPtr)
	{
		auto &meshAndDrawing=*meshAndDrawingPtr;
		auto &mesh=meshAndDrawing.mesh;
		BL *bl = new BL();

		for(auto fgHd : mesh.AllFaceGroup()) // Iterate through all facegroups of the mesh
		{
			auto &blspec=mesh.GetBoundaryLayerSpec(fgHd);
			if(0<blspec.nLayer) // Check if facegroup has a boundary layer
			{
				auto fgHd_key = mesh.GetSearchKey(fgHd);
				auto &fgProp=mesh.GetFaceGroup(fgHd);
           
				auto fgEl2dHd=fgProp.GetFaceGroup(mesh);
				bool facegroup_is_valid = FALSE; // To check if faces were colored
				Ring *ring = new Ring(); // Stores edges at intersection of boundary layer faces
				Prismlist *prismlist = new Prismlist(); // Stores boundary layer faces adjacent to the ring

				for(auto el2dHd : fgEl2dHd) // Iterate through all 2D faces of the facegroup
				{
					auto face = mesh.GetElem2d(el2dHd);
					YsVec3 normal1 = YsMesh_CalculateExteriorElem2dNormal(mesh, el2dHd); // face normal
					normal1.Normalize(); // unit face normal
					YsVec3 centre1 = mesh.GetCenter(el2dHd); // center of the face

					for(auto edge : face->AllEdge(mesh)) // Iterate through all edges of the face
					{
						auto edge_elem = mesh.GetEdge(edge);
						auto node = edge_elem->NdHd(mesh,0); // extract a node of the edge
						
						// Iterate through all the faces containing the node.
						// This is a way to iterate through all the neighboring faces.
						for(auto neighbor : mesh.FindElem2dFromNode(node))
						{
							auto fg = mesh.FindFaceGroupFromElem2d(neighbor); //find facegroup of the neighbor
							auto fg_key = mesh.GetSearchKey(fg);

							// Check if the facegroup is valid.
							// This test will fail for an internal face.
							if(fg != nullptr)
							{
								auto blspec_neighbor = mesh.GetBoundaryLayerSpec(fg);
								auto num_layers = blspec_neighbor.nLayer;
								YsVec3 normal2 = YsMesh_CalculateExteriorElem2dNormal(mesh, neighbor); // neighbor normal
								normal2.Normalize();
								double cos_theta = abs(normal1 * normal2); // cos(angle between the face and its neighbor)

								// Check if the two faces are convex
								YsVec3 centre2 = mesh.GetCenter(neighbor);
								YsVec3 centre_avg = (centre1 + centre2) / 2;
								YsVec3 dir1 = centre_avg - centre1;
								YsVec3 dir2 = centre_avg - centre2;
								if(((normal1 * dir1) > 0) || ((normal2 * dir2) > 0)) { break; }
								
								// Perform the coloring only if both the face and its nieghbor are boundary layer faces
								// and the angle between them is sufficiently close to 90 degrees
								if((fg_key != fgHd_key) && (0 < num_layers) && (cos_theta < 0.5))
								{
									facegroup_is_valid = TRUE; // change this bool to true as faces are being colored
									YSHASHKEY key1 = 0, key2 = 0;

									// Iterate through all the 3D elements that share this face and color them red
									for(auto el3dHD : mesh.FindElem3dFromElem2d(el2dHd))
									{
										YsColor col;
										col=YsRed();
										key1 = mesh.GetSearchKey(el3dHD);
										meshAndDrawing.elem3dColorCode.Update(key1,col);
									}

									// Iterate through all the 3D elements that share its neighbor and color them green
									for(auto el3dHD : mesh.FindElem3dFromElem2d(neighbor))
									{
										YsColor col;
										col=YsGreen();
										key2 = mesh.GetSearchKey(el3dHD);
										meshAndDrawing.elem3dColorCode.Update(key2,col);
									}

									// Add the edge between the faces to the ring
									ring->addEdge(mesh.GetSearchKey(edge));
									prismlist->addPrism(key1, key2);
								}
							}
						}
					}
				}

				// If the face was create a bl element for the ring and list of boundary layer faces
				if(facegroup_is_valid)
				{
					bl->addBL(ring, prismlist);
				}
				else
				{
					delete ring;
					delete prismlist;
				}
			}
		}

		meshAndDrawing.needRemakeDrawingBuffer.TurnOnAll();
		canvasPtr->SetNeedRedraw(YSTRUE);
	}
}