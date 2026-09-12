
#include <gtest/gtest.h>
#include <functional>
#include "pic.h"
#include "meshAMRcutcell.h"


void Orbiter::link_geomentry_test1() {}

TEST(SphereNormalTest, NormalsPointOutward) {
    // Loop through all boundary triangle faces
    for (int iFace = 0; iFace < CutCell::nBoundaryTriangleFaces; iFace++) {
        // Get the center position of the triangle
        double centerPos[3];
        CutCell::BoundaryTriangleFaces[iFace].GetCenterPosition(centerPos);

        // Create vector from origin to center position
        double* externalNormal = CutCell::BoundaryTriangleFaces[iFace].ExternalNormal;

        // Calculate dot product
        double dotProduct = Vector3D::DotProduct(centerPos, externalNormal);

        // Verify dot product is positive
        EXPECT_GT(dotProduct, 0.0) << "Face " << iFace << " normal points inward. "
                                  << "Center position: (" << centerPos[0] << ", "
                                  << centerPos[1] << ", " << centerPos[2] << ")";
    }
}

class CutCellNodeAssignmentTest : public ::testing::Test {
protected:
/**
 * Determines if a triangular face intersects with a block node.
 * This function checks three types of intersections:
 * 1. Any triangle vertex lies inside the block
 * 2. Any triangle edge intersects with block faces
 * 3. Any block edge intersects with the triangle face
 * 
 * @param face Pointer to triangle face containing three vertices
 * @param node Pointer to block node containing min/max bounds
 * @return true if any intersection is detected, false otherwise
 */
bool doesTriangleIntersectNode(CutCell::cTriangleFace* face, const cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {
    // Helper function to check if a 3D point lies inside an axis-aligned bounding box (AABB)
    // Uses simple min/max comparisons for each coordinate
    auto isPointInBlock = [&](const double* point, const double* xmin, const double* xmax) -> bool {
 //       return (point[0] >= xmin[0] && point[0] <= xmax[0] &&
 //               point[1] >= xmin[1] && point[1] <= xmax[1] &&
 //               point[2] >= xmin[2] && point[2] <= xmax[2]);

    for (int i = 0; i < 3; i++) {
        if (point[i] < xmin[i] - PIC::Mesh::mesh->EPS || point[i] > xmax[i] + PIC::Mesh::mesh->EPS) return false;
    }
    return true;

    };

    double triMin[3], triMax[3];
for (int i = 0; i < 3; i++) {
    triMin[i] = std::min(face->x0Face[i],std::min(face->x1Face[i],face->x2Face[i])) - PIC::Mesh::mesh->EPS;
    triMax[i] = std::max(face->x0Face[i],std::max(face->x1Face[i],face->x2Face[i])) + PIC::Mesh::mesh->EPS;
}

bool boxesOverlap = true;
for (int i = 0; i < 3; i++) {
    if (triMax[i] < node->xmin[i] - PIC::Mesh::mesh->EPS || triMin[i] > node->xmax[i] + PIC::Mesh::mesh->EPS) {
        boxesOverlap = false;
        break;
    }
}
if (!boxesOverlap) return false;

    //-------------------------------------------------------------------------
    // PART 1: Check if any triangle vertex is inside the block
    //-------------------------------------------------------------------------
    for (int i = 0; i < 3; i++) {
        if (isPointInBlock(face->node[i]->x, node->xmin, node->xmax)) {
            return true;
        }
    }

    // Helper function to check if a line segment intersects with an axis-aligned plane
    // A line segment intersects a plane if its endpoints lie on opposite sides of the plane
    auto lineIntersectsPlane = [](const double* p1, const double* p2, double planePos, int axis) -> bool {
        /*if ((p1[axis] <= planePos && p2[axis] >= planePos) ||
            (p1[axis] >= planePos && p2[axis] <= planePos)) {
            return true;
        }
        return false;
*/

    double min_val = std::min(p1[axis], p2[axis]);
    double max_val = std::max(p1[axis], p2[axis]);
    return (min_val <= planePos + PIC::Mesh::mesh->EPS && max_val >= planePos - PIC::Mesh::mesh->EPS);

    };

    // Store triangle edges for easier access
    // Each edge is defined by its two endpoint vertices
    const double* edges[3][2] = {
        {face->node[0]->x, face->node[1]->x},  // First edge
        {face->node[1]->x, face->node[2]->x},  // Second edge
        {face->node[2]->x, face->node[0]->x}   // Third edge
    };

    //-------------------------------------------------------------------------
    // PART 2: Check intersection of triangle edges with block faces
    //-------------------------------------------------------------------------
 /*   for (int axis = 0; axis < 3; axis++) {  // Check each axis (X, Y, Z)
        // For each edge of the triangle
        for (int e = 0; e < 3; e++) {
            // Check intersection with the minimum plane of current axis
            if (lineIntersectsPlane(edges[e][0], edges[e][1], node->xmin[axis], axis)) {
                // Calculate intersection point using parametric line equation
                // P(t) = P1 + t(P2 - P1), where t is calculated for the plane intersection
                double t = (node->xmin[axis] - edges[e][0][axis]) / 
                          (edges[e][1][axis] - edges[e][0][axis]);
                double intersectPoint[3];
                for (int i = 0; i < 3; i++) {
                    intersectPoint[i] = edges[e][0][i] + 
                                      t * (edges[e][1][i] - edges[e][0][i]);
                }
                // Check if intersection point lies within the bounds of the block face
                // Skip checking the current axis since we know it intersects at the plane
                if (axis != 0 && intersectPoint[0] >= node->xmin[0] && 
                    intersectPoint[0] <= node->xmax[0] &&
                    axis != 1 && intersectPoint[1] >= node->xmin[1] && 
                    intersectPoint[1] <= node->xmax[1] &&
                    axis != 2 && intersectPoint[2] >= node->xmin[2] && 
                    intersectPoint[2] <= node->xmax[2]) {
                    return true;
                }
            }

            // Same process for the maximum plane of current axis
            if (lineIntersectsPlane(edges[e][0], edges[e][1], node->xmax[axis], axis)) {
                double t = (node->xmax[axis] - edges[e][0][axis]) / 
                          (edges[e][1][axis] - edges[e][0][axis]);
                double intersectPoint[3];
                for (int i = 0; i < 3; i++) {
                    intersectPoint[i] = edges[e][0][i] + 
                                      t * (edges[e][1][i] - edges[e][0][i]);
                }
                if (axis != 0 && intersectPoint[0] >= node->xmin[0] && 
                    intersectPoint[0] <= node->xmax[0] &&
                    axis != 1 && intersectPoint[1] >= node->xmin[1] && 
                    intersectPoint[1] <= node->xmax[1] &&
                    axis != 2 && intersectPoint[2] >= node->xmin[2] && 
                    intersectPoint[2] <= node->xmax[2]) {
                    return true;
                }
            }
        }
    }
*/

for (int axis = 0; axis < 3; axis++) {
        for (int e = 0; e < 3; e++) {
            // Check both min and max planes with tolerance
    //        for (const double planePos : {xmin[axis], xmax[axis]}) {
     
    for (int ii=0;ii<2;ii++) {
	   double planePos =(ii==0) ?  node->xmin[axis] :  node->xmax[axis]; 
		if (lineIntersectsPlane(edges[e][0], edges[e][1], planePos, axis)) {
                    // Compute intersection point with tolerance
                    const double edge_vector[3] = {
                        edges[e][1][0] - edges[e][0][0],
                        edges[e][1][1] - edges[e][0][1],
                        edges[e][1][2] - edges[e][0][2]
                    };
                    
                    if (std::abs(edge_vector[axis]) < PIC::Mesh::mesh->EPS) continue;  // Edge parallel to plane

                    const double t = (planePos - edges[e][0][axis]) / edge_vector[axis];
                    if (t < -PIC::Mesh::mesh->EPS || t > 1.0 + PIC::Mesh::mesh->EPS) continue;  // Intersection outside edge

                    double intersectPoint[3];
                    for (int i = 0; i < 3; i++) {
                        intersectPoint[i] = edges[e][0][i] + t * edge_vector[i];
                    }

                    // Check if intersection point is within block face bounds (with tolerance)
                    bool isInFace = true;
                    for (int i = 0; i < 3; i++) {
                        if (i != axis) {  // Skip check for intersection axis
                            if (intersectPoint[i] < node->xmin[i] - PIC::Mesh::mesh->EPS || 
                                intersectPoint[i] > node->xmax[i] + PIC::Mesh::mesh->EPS) {
                                isInFace = false;
                                break;
                            }
                        }
                    }
                    if (isInFace) return true;
                }
            }
        }
    }


    // Helper function to check if a value lies between two bounds
    auto isBetween = [](double value, double min, double max) -> bool {
        return value >= min && value <= max;
    };

    /**
     * Helper function implementing Möller–Trumbore ray-triangle intersection algorithm
     * This algorithm avoids explicitly computing the plane equation of the triangle
     * Instead, it solves a system of equations using barycentric coordinates
     * 
     * @param orig Ray origin point
     * @param dir Ray direction vector
     * @param v0, v1, v2 Triangle vertices
     * @param t Output parameter: distance along ray to intersection point
     * @return true if ray intersects triangle, false otherwise
     */
    auto rayTriangleIntersect = [](const double* orig, const double* dir, 
                                  const double* v0, const double* v1, const double* v2,
                                  double& t) -> bool {
        const double EPSILON = 0.000001;  // Small value to handle floating point errors
        double edge1[3], edge2[3], h[3], s[3], q[3];
        double a, f, u, v;

        // Calculate triangle edges from v0
        for(int i = 0; i < 3; i++) {
            edge1[i] = v1[i] - v0[i];
            edge2[i] = v2[i] - v0[i];
        }

        // Calculate cross product of ray direction and edge2
        h[0] = dir[1] * edge2[2] - dir[2] * edge2[1];
        h[1] = dir[2] * edge2[0] - dir[0] * edge2[2];
        h[2] = dir[0] * edge2[1] - dir[1] * edge2[0];

        // Calculate determinant
        a = edge1[0] * h[0] + edge1[1] * h[1] + edge1[2] * h[2];

        // Check if ray is parallel to triangle
 //       if (a > -EPSILON && a < EPSILON) return false;
	if (std::abs(a) < PIC::Mesh::mesh->EPS) return false;

        f = 1.0 / a;
        
        // Calculate vector from v0 to ray origin
        for(int i = 0; i < 3; i++) {
            s[i] = orig[i] - v0[i];
        }

        // Calculate u parameter and test bounds
        u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
        if (u < -PIC::Mesh::mesh->EPS || u > 1.0+PIC::Mesh::mesh->EPS) return false;

        // Calculate q vector
        q[0] = s[1] * edge1[2] - s[2] * edge1[1];
        q[1] = s[2] * edge1[0] - s[0] * edge1[2];
        q[2] = s[0] * edge1[1] - s[1] * edge1[0];

        // Calculate v parameter and test bounds
        v = f * (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]);
        if (v < -PIC::Mesh::mesh->EPS || u + v > 1.0+PIC::Mesh::mesh->EPS) return false;

        // Calculate t (distance along ray to intersection)
        t = f * (edge2[0] * q[0] + edge2[1] * q[1] + edge2[2] * q[2]);

        return t > EPSILON;  // Intersection occurs on positive ray direction
    };

    //-------------------------------------------------------------------------
    // PART 3: Check intersection of block edges with triangle face
    //-------------------------------------------------------------------------
    // Iterate through all 12 edges of the block
    for (int axis = 0; axis < 3; axis++) {  // Primary axis of the edge (X, Y, or Z)
        for (int i = 0; i < 2; i++) {       // Position along primary axis (min/max)
            for (int j = 0; j < 2; j++) {   // Position along secondary axis (min/max)
                // Create edge start point and direction vector
                double start[3], dir[3] = {0, 0, 0};
                
                // Set start point coordinates
                // Primary axis position (either min or max)
                start[axis] = i == 0 ? node->xmin[axis] : node->xmax[axis];
                // Secondary axis position (either min or max)
                start[(axis + 1) % 3] = j == 0 ? node->xmin[(axis + 1) % 3] : node->xmax[(axis + 1) % 3];
                // Tertiary axis always starts at min
                start[(axis + 2) % 3] = node->xmin[(axis + 2) % 3];
                
                // Edge direction is along tertiary axis
                dir[(axis + 2) % 3] = node->xmax[(axis + 2) % 3] - node->xmin[(axis + 2) % 3];

                // Check intersection with triangle using Möller–Trumbore algorithm
                double t;
                if (rayTriangleIntersect(start, dir, 
                                       face->node[0]->x, 
                                       face->node[1]->x, 
                                       face->node[2]->x, t)) {
                    // Check if intersection point lies within edge bounds (0 ≤ t ≤ 1)
                    if (t >= 0.0 && t <= 1.0) {
                        return true;
                    }
                }
            }
        }
    }

    // No intersection found after all checks
    return false;
}
};

TEST_F(CutCellNodeAssignmentTest, VerifyNodeCutFaceAssignment) {
        std::stringstream errorLog;
        int totalNodesChecked=0;
        int totalFacesChecked=0;

        std::function<void(cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node)> NodeCutFaceVerifier;
        
        NodeCutFaceVerifier = [&](cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) { 
            totalNodesChecked++;
            
            // Set to store faces that should intersect the node
            std::set<CutCell::cTriangleFace*> expectedFaces;

            // Check all cut surfaces for intersection with this node
            for (int i = 0; i < CutCell::nBoundaryTriangleFaces; i++) {
                CutCell::cTriangleFace* face = &CutCell::BoundaryTriangleFaces[i];
                totalFacesChecked++;
                
                if (CutCellNodeAssignmentTest::doesTriangleIntersectNode(face, node)) {
                    expectedFaces.insert(face);
                }
            }

            // Collect actually assigned faces
            std::set<CutCell::cTriangleFace*> assignedFaces;
            int assignedCount = 0;
            
            for (auto t = node->FirstTriangleCutFace; t != NULL; t = t->next) {
                assignedFaces.insert(t->TriangleFace);
                assignedCount++;
            }

            // Verify assignments
            if (expectedFaces.size() != assignedFaces.size()) {
                errorLog << "Node at (" << node->xmin[0] << "," << node->xmin[1] 
                        << "," << node->xmin[2] << "): Expected " 
                        << expectedFaces.size() << " faces, found " 
                        << assignedFaces.size() << std::endl;
            }

            for (CutCell::cTriangleFace* face : assignedFaces) {
                if (expectedFaces.find(face) == expectedFaces.end()) {
                    errorLog << "Incorrectly assigned face in node at ("
                           << node->xmin[0] << "," << node->xmin[1] 
                           << "," << node->xmin[2] << ")" << std::endl;
                }
            }


            bool res1,res2;

            for (CutCell::cTriangleFace* face : expectedFaces) {
                if (assignedFaces.find(face) == assignedFaces.end()) {
                    errorLog << "Missing face assignment in node at ("
                           << node->xmin[0] << "," << node->xmin[1] 
                           << "," << node->xmin[2] << ")" << std::endl;


		    res1=face->BlockIntersection(node->xmin,node->xmax,PIC::Mesh::mesh->EPS);
		    res2=CutCellNodeAssignmentTest::doesTriangleIntersectNode(face, node); 

		    auto upNode=node->upNode;
		    bool found=false;

                    for (auto t = upNode->FirstTriangleCutFace; t != NULL; t = t->next) {
                      if (t->TriangleFace==face) { 
                        found=true;
                      }
		    }

		    res1=face->BlockIntersection(upNode->xmin,upNode->xmax,PIC::Mesh::mesh->EPS);

		    auto upupNode=upNode->upNode;

		    if (upupNode!=NULL) {
                    for (auto t = upupNode->FirstTriangleCutFace; t != NULL; t = t->next) {
                      if (t->TriangleFace==face) {
                        found=true;
                      }
                    }

		    res1=face->BlockIntersection(upupNode->xmin,upupNode->xmax,PIC::Mesh::mesh->EPS);


		    }
                }
            }

            // Check all assignments
            EXPECT_EQ(expectedFaces.size(), assignedFaces.size()) 
                << "Mismatch in number of assigned faces for node at "
                << node->xmin[0] << "," << node->xmin[1] << "," << node->xmin[2];

            for (CutCell::cTriangleFace* face : assignedFaces) {
                EXPECT_TRUE(expectedFaces.find(face) != expectedFaces.end())
                    << "Found incorrectly assigned face in node";
            }

            for (CutCell::cTriangleFace* face : expectedFaces) {
                EXPECT_TRUE(assignedFaces.find(face) != assignedFaces.end())
                    << "Missing face that should be assigned to node";
            }

            // Recursively check child nodes
           int nDownNode;

           for (nDownNode=0;nDownNode<(1<<_MESH_DIMENSION_);nDownNode++) if (node->downNode[nDownNode]!=NULL) {
              NodeCutFaceVerifier(node->downNode[nDownNode]);   
           }
       }; 

    // Create verifier and run check starting from root node
    NodeCutFaceVerifier(PIC::Mesh::mesh->rootTree);

    // Report statistics
    std::cout << "Nodes checked: " << totalNodesChecked << std::endl;
    std::cout << "Total face checks performed: " << totalFacesChecked << std::endl;
    if (!errorLog.str().empty()) {
       std::cout << "Errors found:\n" << errorLog.str() << std::endl;
    } 
}

