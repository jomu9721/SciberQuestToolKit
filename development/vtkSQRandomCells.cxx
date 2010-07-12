/*
   ____    _ __           ____               __    ____
  / __/___(_) /  ___ ____/ __ \__ _____ ___ / /_  /  _/__  ____
 _\ \/ __/ / _ \/ -_) __/ /_/ / // / -_|_-</ __/ _/ // _ \/ __/
/___/\__/_/_.__/\__/_/  \___\_\_,_/\__/___/\__/ /___/_//_/\__(_) 

Copyright 2008 SciberQuest Inc.
*/
/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSQRandomCells.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSQRandomCells.h"
 
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkMultiProcessController.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkCellArray.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkUnstructuredGrid.h"
#include "vtkType.h"
#include "vtkCellType.h"

#include "CellCopier.h"
#include "PolyDataCellCopier.h"
#include "UnstructuredGridCellCopier.h"
#include "SQMacros.h"
#include "IdBlock.h"
#include "Numerics.hxx"
#include "Tuple.hxx"

#include <set>
using std::set;
using std::pair;

typedef pair<set<unsigned long long>::iterator,bool> SetInsert;

#include <cstdlib>
#include <ctime>

#include <mpi.h>

// #define vtkSQRandomCellsDEBUG

vtkCxxRevisionMacro(vtkSQRandomCells, "$Revision: 0.0 $");
vtkStandardNewMacro(vtkSQRandomCells);


//*****************************************************************************
int findProcByCellId(unsigned long long cellId, IdBlock *bins, int s, int e)
{
  // binary search for rank who owns the given cell id.

  if (s>e || e<s)
    {
    sqErrorMacro(cerr,"Index error s=" << s << " e=" << e << ".");
    return -1;
    }

  int m=(s+e)/2;

  if (bins[m].contains(cellId))
    {
    cerr << "proc=" << m << " owns " << cellId << "." << endl;
    return m;
    }
  else
  if ((cellId<bins[m].first())&&(s!=e))
    {
    return findProcByCellId(cellId,bins,s,m-1);
    }
  else
  if ((cellId>(bins[m].last()-1))&&(s!=e))
    {
    return findProcByCellId(cellId,bins,m+1,e);
    }

  // not found
  sqErrorMacro(cerr,"Error: CellId=" << cellId << " was not found.");
  return -1;
}

//----------------------------------------------------------------------------
vtkSQRandomCells::vtkSQRandomCells()
{
  #ifdef vtkSQRandomCellsDEBUG
  cerr << "===============================vtkSQRandomCells::vtkSQRandomCells" << endl;
  #endif

  this->SampleSize=0;
  this->Seed=-1;

  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);

  int mpiOk=0;
  MPI_Initialized(&mpiOk);
  if (!mpiOk)
    {
    vtkErrorMacro("MPI has not been initialized. Restart ParaView using mpiexec.");
    }
}

//----------------------------------------------------------------------------
vtkSQRandomCells::~vtkSQRandomCells()
{
  #ifdef vtkSQRandomCellsDEBUG
  cerr << "===============================vtkSQRandomCells::~vtkSQRandomCells" << endl;
  #endif
}

//----------------------------------------------------------------------------
int vtkSQRandomCells::RequestInformation(
    vtkInformation */*req*/,
    vtkInformationVector **/*inInfos*/,
    vtkInformationVector *outInfos)
{
  #ifdef vtkSQRandomCellsDEBUG
  cerr << "===============================vtkSQRandomCells::RequestInformation" << endl;
  #endif

  // tell the excutive that we are handling our own paralelization.
  vtkInformation *outInfo=outInfos->GetInformationObject(0);
  outInfo->Set(vtkStreamingDemandDrivenPipeline::MAXIMUM_NUMBER_OF_PIECES(),-1);

  // TODO extract bounds and set if the input data set is present.

  return 1;
}

//----------------------------------------------------------------------------
int vtkSQRandomCells::RequestData(
    vtkInformation */*req*/,
    vtkInformationVector **inInfos,
    vtkInformationVector *outInfos)
{
  #ifdef vtkSQRandomCellsDEBUG
  cerr << "===============================vtkSQRandomCells::RequestData" << endl;
  #endif

  vtkInformation *inInfo=inInfos[0]->GetInformationObject(0);
  vtkDataSet *source
    = dynamic_cast<vtkDataSet*>(inInfo->Get(vtkDataObject::DATA_OBJECT()));

  // sanity -- empty input
  if (source==NULL)
    {
    vtkErrorMacro("Empty input.");
    return 1;
    }

  vtkInformation *outInfo=outInfos->GetInformationObject(0);
  vtkDataSet *output
    = dynamic_cast<vtkDataSet*>(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  // sasnity -- empty output
  if (output==NULL)
    {
    vtkErrorMacro("Empty output.");
    return 1;
    }

  // sanity - user set invalid number of cells.
  if (this->SampleSize<1)
    {
    vtkErrorMacro("Number of cells must be greater than 0.");
    output->Initialize();
    return 1;
    }

  // create the cell coppier.
  CellCopier *copier;
  if (dynamic_cast<vtkPolyData*>(source))
    {
    copier=new PolyDataCellCopier;
    }
  else
  if (dynamic_cast<vtkUnstructuredGrid*>(source))
    {
    copier=new UnstructuredGridCellCopier;
    }
  else
    {
    vtkErrorMacro("Unsupported dataset type " << source->GetClassName() << ".");
    return 1;
    }
  copier->Initialize(source,output);

  int worldSize=1;
  MPI_Comm_size(MPI_COMM_WORLD,&worldSize);

  int worldRank=0;
  MPI_Comm_rank(MPI_COMM_WORLD,&worldRank);

  // count of the cells we own.
  unsigned long long nLocalCells=source->GetNumberOfCells();

  // number and local cell ids of cells which we own which
  // are to be passed through to the output.
  unsigned long long nCellsToPass=0;
  vector<unsigned long long> cellsToPass;

  const int masterRank=(worldSize==1?0:1);

  // divi up the work, all process must participate.
  if (worldRank==masterRank)
    {
    // get counts of all cells.
    unsigned long long nRemoteCells[worldSize];
    MPI_Gather(
        &nLocalCells,
        1,
        MPI_UNSIGNED_LONG_LONG,
        &nRemoteCells[0],
        1,
        MPI_UNSIGNED_LONG_LONG,
        masterRank,
        MPI_COMM_WORLD);

    // construct the cell id block owned by each process.
    unsigned long long nCellsTotal=0;
    IdBlock remoteCellIds[worldSize];
    for (int i=0; i<worldSize; ++i)
      {
      remoteCellIds[i].first()=nCellsTotal;
      remoteCellIds[i].size()=nRemoteCells[i];
      nCellsTotal+=nRemoteCells[i];
      }

    // select cells to pass through. assigned to the process who
    // owns them.
    unsigned long long nAssigned[worldSize];
    for (int i=0; i<worldSize; ++i)
      {
      nAssigned[i]=0ll;
      }
    vector<unsigned long long> assignments[worldSize];

    // seed the number generator.
    int seed=(this->Seed<0?time(0):this->Seed);
    srand(seed);

    set<unsigned long long> usedCellIds;
    SetInsert ok;
    for (int i=0; i<this->SampleSize; ++i)
      {
      unsigned long long cellId=0;
      do
        {
        cellId=(unsigned long long)((double)nCellsTotal*(double)rand()/(double)RAND_MAX);
        ok=usedCellIds.insert(cellId);
        }
      while (!ok.second);
      int rank=findProcByCellId(cellId,remoteCellIds,0,worldSize-1);
      cellId-=remoteCellIds[rank].first();
      assignments[rank].push_back(cellId);
      ++nAssigned[rank];
      }

    // distribute the assignments
    for (int i=0; i<worldSize; ++i)
      {
      if (i==masterRank)
        {
        nCellsToPass=nAssigned[i];
        cellsToPass=assignments[i];
        continue;
        }
      MPI_Send(&nAssigned[i],1,MPI_UNSIGNED_LONG_LONG,i,0,MPI_COMM_WORLD);
      MPI_Send(&((assignments[i])[0]),nAssigned[i],MPI_UNSIGNED_LONG_LONG,i,1,MPI_COMM_WORLD);
      }
    }
  else
    {
    // send a count of the cells we own.
    MPI_Gather(
        &nLocalCells,
        1,
        MPI_UNSIGNED_LONG_LONG,
        0,
        0,
        MPI_UNSIGNED_LONG_LONG,
        masterRank,
        MPI_COMM_WORLD);

    // obtain our assignments
    MPI_Status stat;
    MPI_Recv(
          &nCellsToPass,
          1,
          MPI_UNSIGNED_LONG_LONG,
          masterRank,
          0,
          MPI_COMM_WORLD,
          &stat);

    cellsToPass.resize(nCellsToPass);
    MPI_Recv(
          &cellsToPass[0],
          nCellsToPass,
          MPI_UNSIGNED_LONG_LONG,
          masterRank,
          1,
          MPI_COMM_WORLD,
          &stat);
    }

  // copy cells, assoictaed points and data attributes to the output.
  for (int i=0; i<nCellsToPass; ++i)
    {
    copier->Copy(cellsToPass[i]);
    }

  delete copier;


  return 1;
}

//----------------------------------------------------------------------------
void vtkSQRandomCells::PrintSelf(ostream& os, vtkIndent indent)
{
  #ifdef vtkSQRandomCellsDEBUG
  cerr << "===============================vtkSQRandomCells::PrintSelf" << endl;
  #endif

  this->Superclass::PrintSelf(os,indent);

  // TODO
}