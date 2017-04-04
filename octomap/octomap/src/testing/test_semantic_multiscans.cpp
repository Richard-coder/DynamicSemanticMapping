
#include <stdio.h>
#include <octomap/octomap.h>
#include <octomap/math/Utils.h>
#include <octomap/SemanticOcTree.h>
#include "testing.h"

using namespace std;
using namespace octomap;

void printUsage(char* self){
  std::cerr << "\nUSAGE: " << self << " 5 x point_cloud.txt  (point cloud file, required)\n\n";

  exit(1);
}

void print_query_info(point3d query, SemanticOcTreeNode* node) {
  if (node != NULL) {
    cout << "occupancy probability at " << query << ":\t " << node->getOccupancy() << endl;
    cout << "color of node is: " << node->getColor() << endl;
    cout << "Semantics of node is: " << node->getSemantics() << endl;
  }
  else
    cout << "occupancy probability at " << query << ":\t is unknown" << endl;
}


int main(int argc, char** argv) {
  if (argc < 2){
    printUsage(argv[0]);
  }

  // the global tree
  SemanticOcTree tree (0.05);


  // build a measurement local tree 
  SemanticOcTree localTree (0.05);
  float labels[5][5]= {{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}};
  int color[5][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 20, 147}, {0, 255, 255}};
 

  for ( int argn = 0; argn < argc-1; argn++) {
    // read frame pose from text file
    std::string filename = std::string(argv[argn+1]);
    std::ifstream infile(filename.c_str());
    
    // prepare label for current class
    std::vector<float> label;
    for (int c = 0; c < argc-1; c++) {
      label.push_back(labels[argn][c]);
    }

    // read point cloud with extrainfo
    Pointcloud* cloud = new Pointcloud();
    while (infile) {
      cloud->readExtraInfo(infile, 3);
    }

    // set point cloud origin pose for allignment
    pose6d origin(0, 0, 0, 0, 0, 0);
    
    // insert into OcTree  
    {
      // insert in global coordinates:
      localTree.insertPointCloud(*cloud, origin.trans());
      
      // fuse extra information
      for (int i=0; i < (int)cloud->size(); ++i) {
        const point3d& query = (*cloud)[i];
        //std::vector<float> extra_info = cloud->getExtraInfo(i);
        SemanticOcTreeNode* n = localTree.search (query);
        localTree.averageNodeSemantics(n, label);
        //print_query_info(query, n);  
      }
    }
  }//end for
  // end building local tree


  cout << "Expanded num. leafs: " << tree.getNumLeafNodes() << endl;
  // update the global tree according to local tree
  for (SemanticOcTree::leaf_iterator it = localTree.begin_leafs(),
      end = localTree.end_leafs(); it != end; ++it)
  {
    SemanticOcTreeNode* n = tree.search(it.getKey());
    if (n==NULL){
      // create a new node in global tree with the same coord
      point3d newCoord = it.getCoordinate();
      // set the occupancy log odds
      double pl = it->getOccupancy();
      float ol = octomap::logodds(pl);
      SemanticOcTreeNode* newNode = tree.updateNode(newCoord, ol);
      // set the semantics
      SemanticOcTreeNode::Semantics sl = it->getSemantics();
      newNode->setSemantics(sl);

    }
  }


  cout << "Expanded num. leafs: " << tree.getNumLeafNodes() << endl;

  // update the global tree according to local tree
  for (SemanticOcTree::leaf_iterator it = localTree.begin_leafs(),
      end = localTree.end_leafs(); it != end; ++it)
  {
    SemanticOcTreeNode* n = tree.search(it.getKey());
    if (n==NULL){
      // create a new node in global tree with the same coord
      point3d newCoord = it.getCoordinate();
      // set the occupancy log odds
      double pl = it->getOccupancy();
      float ol = octomap::logodds(pl);
      SemanticOcTreeNode* newNode = tree.updateNode(newCoord, ol);
      // set the semantics
      SemanticOcTreeNode::Semantics sl = it->getSemantics();
      newNode->setSemantics(sl);
      //OCTOMAP_ERROR("something wrong...\n");

    } else{
      // update occupancy prob according to Eq.(7) in paper
      double pl = it->getOccupancy();
      double npl = 1.0-pl;
      double pg = n->getOccupancy();
      double npg = 1.0-pg;

      // normalization
      double pg_new = pl*pg;
      double npg_new = npl*npg;
      pg_new = pg_new/(pg_new+npg_new);
      
      // set new value
      float ol = octomap::logodds(pg_new);
      n->setLogOdds(ol);
      
      // update semantics according to Eq.(7) in paper
      SemanticOcTreeNode::Semantics sl = it->getSemantics();
      SemanticOcTreeNode::Semantics sg = n->getSemantics();
      std::vector<float> sg_new;
      float factor = 0;
      for (int i = 0; i < (int)sl.label.size(); i++) {
        float s_new = sl.label[i] * sg.label[i];
        factor += s_new;
        sg_new.push_back(s_new);
      }
      // normalization
      for (int i = 0; i < (int)sg_new.size(); i++) {
        sg_new[i] /= factor;
      }

      // debug
      float sum = 0;
      for (int i = 0; i < (int)sg_new.size(); i++) {
        sum += sg_new[i];
      }
      //std::cout<< "sum" << sum << std::endl;
    
    }

  
  }


  cout << "Expanded num. leafs: " << tree.getNumLeafNodes() << endl;





  // traverse the whole tree, set color based on semantics to visualize
  for (SemanticOcTree::iterator it = tree.begin(); it != tree.end(); ++it) {
    if ( (&(*it))->isSemanticsSet() ) {
      SemanticOcTreeNode::Semantics s = (&(*it))->getSemantics();
      // Debug
      //print_query_info(point3d(0,0,0), &(*it));  
      for (int argn = 0; argn < argc-1; argn++) {
        if (s.label.size() && s.label[argn] > 0.3) {
          (&(*it))->setColor(color[argn][0], color[argn][1], color[argn][2]);
        }
      }
    }
  }//end for


  tree.write("semantic_color_scan.ot");
  

  cout << "Test done." << endl;
  exit(0);

}
