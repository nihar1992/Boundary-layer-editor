#include <stdio.h>
#include <ysmesh.h>
#include <ysmeshio.h>
#include <ysport.h>
#include <vector>

/*
 * boundary_subdivider.cpp
 * 
 * Objective:
 * To subdivide a single boundary layer into multiple divisions. 
 * This info will be used to divide the newly created hex element.
 *
 * Method:
 * First the boundary layer elments are identified and iterated through.
 * Points for subdivding the edges are created and associated with the edge
 * using a hashtable.
 * These points are then used to subdivide the 3D prism elements.
 * 
 * Future:
 * Currently the code only subdivides the 3D prism elements. When the hex
 * element creation code is complete the hastable data will be used to
 * subdivide the newly created hex elements.
 */

//Hashtable class
template <class KeyType,class ValueType>
class HashTable
{
public:
    class EnumHandle
    {
    public:
        long long int hashIdx;
        long long int arrayIdx;
    };
private:
    class Entry
    {
    public:
        KeyType hashKey;
        ValueType value;
    };
    enum 
    {
        MINIMUM_TABLE_SIZE=7
    };
    std::vector <std::vector <Entry> > table;
    long long int nElem;
public:
    HashTable();
    ~HashTable();
    void CleanUp(void);

    void Update(const KeyType &key,const ValueType &value);
	ValueType content(const KeyType key);
};

template <class KeyType,class ValueType>
HashTable<KeyType,ValueType>::HashTable()
{
    table.resize(MINIMUM_TABLE_SIZE);
    nElem=0;
}

template <class KeyType,class ValueType>
HashTable<KeyType,ValueType>::~HashTable()
{
}

template <class KeyType,class ValueType>
void HashTable<KeyType,ValueType>::CleanUp(void)
{
    table.resize(MINIMUM_TABLE_SIZE);
    for(auto &t : table)
    {
        t.clear();
    }
    nElem=0;
}

template <class KeyType,class ValueType>
void HashTable<KeyType,ValueType>::Update(const KeyType &key,const ValueType &value)
{
    auto idx=key%table.size();
    for(auto &e : table[idx])
    {
        if(e.hashKey==key)
        {
            e.value=value;
            return;
        }
    }
    Entry entry;
    entry.hashKey=key;
    entry.value=value;
    table[idx].push_back(entry);
    ++nElem;
}

template <class KeyType,class ValueType>
ValueType HashTable<KeyType,ValueType>::content(
    const KeyType key)
{
    auto idx=key%table.size();
    for(auto &e : table[idx])
    {
        if(e.hashKey==key)
        {
            return e.value;
        }
    }
    return nullptr;
}


int main(int ac, char *av[])
{
	YsMesh mesh;

	YsFileIO::File fp(av[1],"r");

	//Read mesh data
	YsMeshMs3Reader reader;
	YsString str;
	reader.BeginRead(mesh);
	while(nullptr!=str.Fgets(fp))
	{
		reader.ReadOneLine(str);
	}
	reader.EndRead();

	//Edge subdivision
	int num_divisions = 3;
	HashTable<long long int, long long int*> *hashtable = new HashTable<long long int, long long int*>();
	
	for(auto fgHd : mesh.AllFaceGroup()) //Iterate through all facegroups of the mesh
    {
        auto &blspec=mesh.GetBoundaryLayerSpec(fgHd);
        if(0<blspec.nLayer) //Check if facegroup has a boundary layer
        {
            auto &fgProp=mesh.GetFaceGroup(fgHd);
           
            auto fgEl2dHd=fgProp.GetFaceGroup(mesh);
            
			for(auto el2dHd : fgEl2dHd) //Iterate through all 2D faces of the facegroup
			{
				//Iterate through all 3D elements that contain the face
				for(auto el3dHD : mesh.FindElem3dFromElem2d(el2dHd))
				{
					auto prism = mesh.GetElem3d(el3dHD);
					long long int** nodearray = new long long int*[3];
					for(int j = 0; j < 3; j++)
					{
						// Create an array of nodes for all the edges of the 3D element
						nodearray[j] = new long long int[num_divisions + 1];
					}

					// Divide the edges and associate the newly created points for subdivision
					// with the corresponding edges using the hashtable
					for(int i=3; i<6; i++)
					{
						auto ei = prism->EdHd(mesh, i);
						auto edge = mesh.GetEdge(ei);

						YsVec3 pos0, pos1;
						
						// Check if face is external or internal & create the points accordingly
					    if(mesh.IsElem2dUsingNode(el2dHd, edge->NdHd(mesh,0))) 
						{
							pos0 = mesh.GetNodePos(edge->NdHd(mesh,0));
							pos1 = mesh.GetNodePos(edge->NdHd(mesh,1));
							nodearray[i - 3][0] = mesh.GetSearchKey(edge->NdHd(mesh,0));
							nodearray[i - 3][num_divisions] = mesh.GetSearchKey(edge->NdHd(mesh,1));
						}
						else
						{
							pos0 = mesh.GetNodePos(edge->NdHd(mesh,1));
							pos1 = mesh.GetNodePos(edge->NdHd(mesh,0));
							nodearray[i - 3][0] = mesh.GetSearchKey(edge->NdHd(mesh,1));
							nodearray[i - 3][num_divisions] = mesh.GetSearchKey(edge->NdHd(mesh,0));
						}

						for(int j = 0; j < (num_divisions - 1); j++)
						{
							// Create and store points interpolated between at equal distances
							// between the two endpoints of the edge
							YsVec3 posi(pos0 + (pos1 - pos0) * (j + 1)/ num_divisions);
							auto newNdHd=mesh.AddNode(posi);
							nodearray[i - 3][j + 1] = mesh.GetSearchKey(newNdHd);
						}
						hashtable->Update(mesh.GetSearchKey(ei), nodearray[i - 3]);
					}

					for(int i = 0; i < num_divisions; i++)
					{
						// Divide the 3D prism elements in the boundary layer using nodes created earlier
						YsMesh::NodeHandle prismnodes[6];
						for(int j = 0; j < 3; j++)
						{
							prismnodes[j] = mesh.FindNode(nodearray[j][i]);
							prismnodes[j + 3] = mesh.FindNode(nodearray[j][i + 1]);
						}
						mesh.AddElem3d(YsMesh::PRISM, prismnodes);
					}
					mesh.DeleteElem3d(el3dHD); //delete original element
					delete nodearray;
				}
			}
        }
    }

	{
		// Write the mesh to an '.ms3' output file
		YsMeshMs3Writer writer;
		YsFileIO::File fp("output.ms3","w");
		auto outStream=fp.OutStream();
		writer.BeginWrite(mesh);
		writer.Write(outStream);
		writer.EndWrite();
	}
	return 0;
}