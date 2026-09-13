/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2025 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::meshSearch

Description
    Mesh object that implements searches within the local cells and faces

SourceFiles
    meshSearch.C

\*---------------------------------------------------------------------------*/

#ifndef meshSearch_H
#define meshSearch_H

#include "DemandDrivenMeshObject.H"
#include "indexedOctree.H"
#include "pointInCell.H"
#include "treeDataCell.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                         Class meshSearch Declaration
\*---------------------------------------------------------------------------*/

class meshSearch
:
    public DemandDrivenMeshObject
    <
        polyMesh,
        DeletableMeshObject,
        meshSearch
    >
{
    // Private Data

        //- Cell tree
        const indexedOctree<treeDataCell> cellTree_;


protected:

    friend class DemandDrivenMeshObject
    <
        polyMesh,
        DeletableMeshObject,
        meshSearch
    >;


    // Protected Constructors

        //- Construct from mesh
        explicit meshSearch(const polyMesh& mesh);


public:

    //- Run-time type information
    TypeName("meshSearch");


    // Constructors

        //- Lookup or construct from mesh and cell decomposition option
        static const meshSearch& New
        (
            const polyMesh& mesh,
            const pointInCellShapes = pointInCellShapes::tets
        );

        //- Disallow default bitwise copy construction
        meshSearch(const meshSearch&) = delete;


    //- Destructor
    virtual ~meshSearch();


    // Member Functions

        // Access

            //- Access the cell tree
            inline const indexedOctree<treeDataCell>& cellTree() const
            {
                return cellTree_;
            }


        // Queries

            //- Find the cell with centre closest to the given point
            label findNearestCell(const point& p) const;

            //- Find the cell with centre closest to the given point. Do a
            //  linear search through all the cells instead of using the tree.
            static label findNearestCellNoTree
            (
                const polyMesh& mesh,
                const point& p
            );

            //- Find the face with centre closest to the given point
            label findNearestFace(const point& p) const;

            //- Find the cell containing the given point
            label findCell
            (
                const point& p,
                const pointInCellShapes = pointInCellShapes::tets
            ) const;

            //- Find the cell containing the given point. Do a
            //  linear search through all the cells instead of using the tree.
            static label findCellNoTree
            (
                const polyMesh& mesh,
                const point& p,
                const pointInCellShapes = pointInCellShapes::tets
            );


    // Member Operators

        //- Disallow default bitwise assignment
        void operator=(const meshSearch&) = delete;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
