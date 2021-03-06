/*
   ____    _ __           ____               __    ____
  / __/___(_) /  ___ ____/ __ \__ _____ ___ / /_  /  _/__  ____
 _\ \/ __/ / _ \/ -_) __/ /_/ / // / -_|_-</ __/ _/ // _ \/ __/
/___/\__/_/_.__/\__/_/  \___\_\_,_/\__/___/\__/ /___/_//_/\__(_)

Copyright 2012 SciberQuest Inc.
*/

//#define CUDAConvolutionDriverDEBUG

#include "CUDAConvolutionDriver.h"

#include "CartesianExtent.h"
#include "MemOrder.hxx"
#include "SQMacros.h"
#include "postream.h"

#if defined SQTK_CUDA
  #include "CUDAGlobalMemoryManager.hxx"
  #include "CUDAConstMemoryManager.hxx"
  #include "CUDANumerics.hxx"
  #include "CUDAMacros.h"
  #include <cuda.h>
  #include <cuda_runtime.h>
#endif

#include "vtkDataArray.h"

#include <iostream>
using std::cerr;
using std::endl;
#include <vector>
using std::vector;

//-----------------------------------------------------------------------------
CUDAConvolutionDriver::CUDAConvolutionDriver()
        :
     NDevices(0),
     DeviceId(0),
     MaxThreads(1),
     MaxBlocks(1),
     NThreads(1),
     NBlocks(1),
     KernelMemoryType(CUDA_MEM_TYPE_GLOBAL),
     InputMemoryType(CUDA_MEM_TYPE_GLOBAL),
     WarpSize(32),
     WarpsPerBlock(1),
     MaxWarpsPerBlock(1)
{
  int nDevs=0;
  #if defined SQTK_CUDA
  cudaGetDeviceCount(&nDevs);
  #endif
  this->NDevices=nDevs;
}

//-----------------------------------------------------------------------------
int CUDAConvolutionDriver::SetDeviceId(int deviceId)
{
  #ifdef CUDAConvolutionDriverDEBUG
  pCerr()
    << "===============CUDAConvolutionDriver::SetDeviceId" << endl
    << deviceId << endl;
  #endif
  #if defined SQTK_CUDA
  if (deviceId>=this->NDevices)
    {
    sqErrorMacro(
      pCerr(),
      << "Attempt to select invalid device "
      << deviceId << " of " << this->NDevices);
    }

  this->DeviceId=deviceId;
  cudaError_t ierr=cudaSetDevice(deviceId);
  if (ierr)
    {
    CUDAErrorMacro(pCerr(),ierr,"Failed to select device " << deviceId);
    return -1;
    }

  cudaDeviceProp props;
  cudaGetDeviceProperties(&props, deviceId);

  this->MaxThreads=props.maxThreadsPerBlock;
  this->NThreads=props.maxThreadsPerBlock/2;

  this->MaxBlocks=props.maxGridSize[0];
  this->SetNumberOfBlocks(1024);

  this->BlockGridMax[0]=props.maxGridSize[0];
  this->BlockGridMax[1]=props.maxGridSize[1];
  this->BlockGridMax[2]=props.maxGridSize[2];
  this->WarpSize=props.warpSize;
  this->MaxWarpsPerBlock=props.maxThreadsPerBlock/props.warpSize;
  #endif

  return 0;
}

//-----------------------------------------------------------------------------
int CUDAConvolutionDriver::Convolution(
    CartesianExtent &extV,
    CartesianExtent &extW,
    CartesianExtent &extK,
    int nGhost,
    int mode,
    vtkDataArray *V,
    vtkDataArray *W,
    float *K)
{
  // TODO - make sure nothing is leaked if an error occurs!

  #ifdef CUDAConvolutionDriverDEBUG
  pCerr()
    << "===============CUDAConvolutionDriver::Convolution" << endl;
  #endif

  #if defined SQTK_CUDA
  int nV[3];
  extV.Size(nV);
  unsigned long vnijk=extV.Size();

  int nW[3];
  extW.Size(nW);
  unsigned long wnijk=extW.Size();

  int nK[3];
  extK.Size(nK);
  unsigned long knijk=extK.Size();

  CUDAMemoryManager<float> *devK;
  if ( (this->KernelMemoryType==CUDA_MEM_TYPE_CONST)
    && (knijk*sizeof(float)<65536) )
    {
    #ifdef CUDAConvolutionDriverDEBUG
    pCerr() << "Using constant memory for kernel" << endl;
    #endif
    devK=CUDAConstMemoryManager<float>::New("gK",K,knijk);
    }
  else
  if (this->KernelMemoryType==CUDA_MEM_TYPE_TEX)
    {
    // TODO
    sqErrorMacro(cerr,"Kernel texture memory is not implemented!");
    return -1;
    }
  else
    {
    #ifdef CUDAConvolutionDriverDEBUG
    pCerr() << "Using global memory for kernel" << endl;
    #endif
    devK=CUDAGlobalMemoryManager<float>::New(K,knijk);
    }

  unsigned long nComp=W->GetNumberOfComponents();

  dim3 blockGrid(1,1,1);
  dim3 threadGrid(1,1,1);
  unsigned long nBlocks;
  int ierr=PartitionBlocks(
        wnijk,
        this->WarpsPerBlock,
        this->WarpSize,
        this->BlockGridMax,
        blockGrid,
        nBlocks,
        threadGrid);
  if (ierr)
    {
    sqErrorMacro(pCerr(),"Failed to decompose domain for the GPU");
    return -1;
    }


  int fastDim=0;
  int slowDim=1;
  switch (mode)
    {
    case CartesianExtent::DIM_MODE_2D_XY:
      fastDim=0;
      slowDim=1;
      break;
    case CartesianExtent::DIM_MODE_2D_XZ:
      fastDim=0;
      slowDim=2;
      break;
    case CartesianExtent::DIM_MODE_2D_YZ:
      fastDim=1;
      slowDim=2;
      break;
    case CartesianExtent::DIM_MODE_3D:
      fastDim=0;
      slowDim=1;
      break;
    default:
      sqErrorMacro(cerr,"Bad dim mode.");
      return -1;
    }

  #ifdef CUDAConvolutionDriverDEBUG
  pCerr() << "wnijk=" << wnijk << endl;
  pCerr() << "WarpsPerBlock=" << this->WarpsPerBlock << endl;
  pCerr() << "WarpSize=" << this->WarpSize << endl;
  pCerr() << "blockGridMaxMax=(" << this->BlockGridMax[0] << ", " << this->BlockGridMax[1] << ", " << this->BlockGridMax[2] << ")" << endl;
  pCerr() << "blockGrid=(" << blockGrid.x << ", " << blockGrid.y << ", " << blockGrid.z << ")" << endl;
  pCerr() << "nBlocks=" << nBlocks << endl;
  pCerr() << "threadGrid=(" << threadGrid.x << ", " << threadGrid.y << ", " << threadGrid.z << ")" << endl;
  pCerr() << "fastDim=" << fastDim << endl;
  pCerr() << "slowDim=" << slowDim << endl;
  pCerr() << "extV=" << extV << endl;
  pCerr() << "nV=(" << nV[fastDim] <<  ", " << nV[slowDim] << ")" << endl;
  pCerr() << "extW=" << extW << endl;
  pCerr() << "nW=(" << nW[fastDim] <<  ", " << nW[slowDim] << ")" << endl;
  #endif

  switch (V->GetDataType())
    {
    // TODO -- replace with vtkTemplateMacro
    case VTK_FLOAT:
      {
      //int worldRank;
      //MPI_Comm_rank(MPI_COMM_WORLD,&worldRank);
      cudaError_t uerr;
      // allocate device memory for vector components
      vector<float*> sV(nComp,0);
      vector<CUDAGlobalMemoryManager<float>*> devV(nComp,0);
      vector<CUDAGlobalMemoryManager<float>*> devW(nComp,0);
      vector<float*> sW(nComp,0);
      for (int q=0; q<nComp; ++q)
        {
        // input arrays
        cudaHostAlloc(
              &sV[q],
              vnijk*sizeof(float),
              cudaHostAllocDefault);
        if (this->InputMemoryType==CUDA_MEM_TYPE_TEX)
          {
          // use texture memory for input
          if ((mode==CartesianExtent::DIM_MODE_2D_XY)
            ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
            ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
            {
            devV[q]
              = CUDAGlobalMemoryManager<float>::New(
                  nV[fastDim],
                  nV[slowDim]);
            }
          else
            {
            // TODO
            sqErrorMacro(cerr,"3D Texture kernel is not implemented!");
            return -1;
            }
          }
        else
          {
          // use global memory for input
          devV[q]=CUDAGlobalMemoryManager<float>::New(vnijk);
          }

        // output array
        devW[q]=CUDAGlobalMemoryManager<float>::New(wnijk);
        cudaHostAlloc(
            &sW[q],
            wnijk*sizeof(float),
            cudaHostAllocDefault);

        /*
        // fill device data with falt index for lookup validation
        for (int i=0; i<vnijk; ++i)
          {
          sV[q][i]=i;
          }
        */
        }

      // convert vtk vectors/tensors into scalar component arrays
      float *hV=(float*)V->GetVoidPointer(0);
      Split<float>(vnijk,hV,sV);

      // copy the input arrays to the device
      for (int q=0; q<nComp; ++q)
        {
        // TODO-could this be streamed to overlap com w/ comp?
        devV[q]->Push(sV[q]);

        // execute the kernel
        if (this->InputMemoryType==CUDA_MEM_TYPE_TEX)
          {
          #ifdef CUDAConvolutionDriverDEBUG
          pCerr() << "Using texture memory for input" << endl;
          #endif
            if ((mode==CartesianExtent::DIM_MODE_2D_XY)
              ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
              ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
              {
              // setup texture
              ierr=devV[q]->BindToTexture(&gV2);
              CUDAScalarConvolution2D<<<blockGrid,threadGrid>>>(
                    //worldRank,
                    nV[fastDim],
                    nW[fastDim],
                    wnijk,
                    nK[fastDim],
                    nGhost,
                    devW[q]->GetDevicePointer(),
                    devK->GetDevicePointer());
              }
            else
              {
              // TODO
              sqErrorMacro(cerr,"3D Texture kernel is not implemented!");
              return -1;
              }
            uerr=cudaGetLastError();
            if (uerr)
              {
              CUDAErrorMacro(pCerr(),uerr,"Error invoking 2D kernel.");
              return -1;
              }
            // release texture
            cudaDeviceSynchronize();
            //cudaUnbindTexture(gV2);
          }
        else
          {
          #ifdef CUDAConvolutionDriverDEBUG
          pCerr() << "Using global memory for input" << endl;
          #endif
          if ((mode==CartesianExtent::DIM_MODE_2D_XY)
            ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
            ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
            {
            CUDAScalarConvolution2D<<<blockGrid,threadGrid>>>(
                  //worldRank,
                  nV[fastDim],
                  nW[fastDim],
                  wnijk,
                  nK[fastDim],
                  nGhost,
                  devV[q]->GetDevicePointer(),
                  devW[q]->GetDevicePointer(),
                  devK->GetDevicePointer());
            }
          else
            {
            CUDAScalarConvolution3D<<<blockGrid,threadGrid>>>(
                  nV[fastDim],
                  nV[fastDim]*nV[slowDim],
                  nW[fastDim],
                  nW[fastDim]*nW[slowDim],
                  wnijk,
                  nK[fastDim],
                  nK[fastDim]*nK[slowDim],
                  nGhost,
                  devV[q]->GetDevicePointer(),
                  devW[q]->GetDevicePointer(),
                  devK->GetDevicePointer());
            }
          uerr=cudaGetLastError();
          if (uerr)
            {
            CUDAErrorMacro(pCerr(),uerr,"Error invoking global memory kernel.");
            return -1;
            }
          }
        // retreive the output/result
        devW[q]->Pull(sW[q]);
        }

      // put results in vtk order
      float *hW=(float*)W->GetVoidPointer(0);
      Interleave(wnijk,sW,hW);

      // clean up
      for (int q=0; q<nComp; ++q)
        {
        delete devW[q];
        cudaFreeHost(sW[q]);
        delete devV[q];
        cudaFreeHost(sV[q]);
        }
      cudaUnbindTexture(gV2);
      }
      break;
    default:
      // TODO 
      sqErrorMacro(cerr,"Not currently using vtkTemplateMacro");
      return -1;
    }

  delete devK;
  // TODO if kernel is in texture mem, unbind
  #endif

  return 0;
}

//-----------------------------------------------------------------------------
int CUDAConvolutionDriver::Convolution(
    CartesianExtent &extV,
    CartesianExtent &extW,
    CartesianExtent &extK,
    CartesianExtent &extB,
    int blockSide,
    int nGhost,
    int mode,
    vtkDataArray *V,
    vtkDataArray *W,
    float *K)
{
  // TODO - make sure nothing is leaked if an error occurs!

  #ifdef CUDAConvolutionDriverDEBUG
  pCerr()
    << "===============CUDAConvolutionDriver::Convolution" << endl;
  #endif

  #if defined SQTK_CUDA

  int nV[3];
  extV.Size(nV);
  unsigned long vnijk=extV.Size();
  int V0[3];
  extV.GetStartIndex(V0);

  int nW[3];
  extW.Size(nW);
  unsigned long wnijk=extW.Size();
  unsigned long nComp=W->GetNumberOfComponents();
  int W0[3];
  extW.GetStartIndex(W0);

  int nK[3];
  extK.Size(nK);
  unsigned long knijk=extK.Size();

  int nB[3];
  extB.Size(nB);

  int nBlocks[3];
  nBlocks[0]=nW[0]/nB[0]+(nW[0]%nB[0]?1:0);
  nBlocks[1]=nW[1]/nB[1]+(nW[0]%nB[0]?1:0);
  nBlocks[2]=nW[2]/nB[2]+(nW[0]%nB[0]?1:0);

  CUDAMemoryManager<float> *devK;
  if ( (this->KernelMemoryType==CUDA_MEM_TYPE_CONST)
    && (knijk*sizeof(float)<65536) )
    {
    #ifdef CUDAConvolutionDriverDEBUG
    pCerr() << "Using constant memory for kernel" << endl;
    #endif
    devK=CUDAConstMemoryManager<float>::New("gK",K,knijk);
    }
  else
  if (this->KernelMemoryType==CUDA_MEM_TYPE_TEX)
    {
    // TODO
    sqErrorMacro(cerr,"Kernel texture memory is not implemented!");
    return -1;
    }
  else
    {
    #ifdef CUDAConvolutionDriverDEBUG
    pCerr() << "Using global memory for kernel" << endl;
    #endif
    devK=CUDAGlobalMemoryManager<float>::New(K,knijk);
    }


  for (int r=0; r<nBlocks[2]; ++r)
    {
    for (int q=0; q<nBlocks[1]; ++q)
      {
      for (int p=0; p<nBlocks[0]; ++p)
        {
        CartesianExtent extBw(
              p*nB[0]+W0[0],(p+1)*nB[0]-1+W0[0],
              p*nB[1]+W0[1],(p+1)*nB[1]-1+W0[1],
              p*nB[2]+W0[2],(p+1)*nB[2]-1+W0[2]);


        extBw&=extW;

        CartesianExtent extBv(extBw);
        CartesianExtent::Grow(extBv,nGhost,mode);


        }
      }
    }




  dim3 blockGrid(1,1,1);
  dim3 threadGrid(1,1,1);
  unsigned long nBlocks;
  int ierr=PartitionBlocks(
        wnijk,
        this->WarpsPerBlock,
        this->WarpSize,
        this->BlockGridMax,
        blockGrid,
        nBlocks,
        threadGrid);
  if (ierr)
    {
    sqErrorMacro(pCerr(),"Failed to decompose domain for the GPU");
    return -1;
    }


  int fastDim=0;
  int slowDim=1;
  switch (mode)
    {
    case CartesianExtent::DIM_MODE_2D_XY:
      fastDim=0;
      slowDim=1;
      break;
    case CartesianExtent::DIM_MODE_2D_XZ:
      fastDim=0;
      slowDim=2;
      break;
    case CartesianExtent::DIM_MODE_2D_YZ:
      fastDim=1;
      slowDim=2;
      break;
    case CartesianExtent::DIM_MODE_3D:
      fastDim=0;
      slowDim=1;
      break;
    default:
      sqErrorMacro(cerr,"Bad dim mode.");
      return -1;
    }

  #ifdef CUDAConvolutionDriverDEBUG
  pCerr() << "wnijk=" << wnijk << endl;
  pCerr() << "WarpsPerBlock=" << this->WarpsPerBlock << endl;
  pCerr() << "WarpSize=" << this->WarpSize << endl;
  pCerr() << "blockGridMaxMax=(" << this->BlockGridMax[0] << ", " << this->BlockGridMax[1] << ", " << this->BlockGridMax[2] << ")" << endl;
  pCerr() << "blockGrid=(" << blockGrid.x << ", " << blockGrid.y << ", " << blockGrid.z << ")" << endl;
  pCerr() << "nBlocks=" << nBlocks << endl;
  pCerr() << "threadGrid=(" << threadGrid.x << ", " << threadGrid.y << ", " << threadGrid.z << ")" << endl;
  pCerr() << "fastDim=" << fastDim << endl;
  pCerr() << "slowDim=" << slowDim << endl;
  pCerr() << "extV=" << extV << endl;
  pCerr() << "nV=(" << nV[fastDim] <<  ", " << nV[slowDim] << ")" << endl;
  pCerr() << "extW=" << extW << endl;
  pCerr() << "nW=(" << nW[fastDim] <<  ", " << nW[slowDim] << ")" << endl;
  #endif

  switch (V->GetDataType())
    {
    // TODO -- replace with vtkTemplateMacro
    case VTK_FLOAT:
      {
      //int worldRank;
      //MPI_Comm_rank(MPI_COMM_WORLD,&worldRank);
      cudaError_t uerr;
      // allocate device memory for vector components
      vector<float*> sV(nComp,0);
      vector<CUDAGlobalMemoryManager<float>*> devV(nComp,0);
      vector<CUDAGlobalMemoryManager<float>*> devW(nComp,0);
      vector<float*> sW(nComp,0);
      for (int q=0; q<nComp; ++q)
        {
        // input arrays
        cudaHostAlloc(
              &sV[q],
              vnijk*sizeof(float),
              cudaHostAllocDefault);
        if (this->InputMemoryType==CUDA_MEM_TYPE_TEX)
          {
          // use texture memory for input
          if ((mode==CartesianExtent::DIM_MODE_2D_XY)
            ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
            ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
            {
            devV[q]
              = CUDAGlobalMemoryManager<float>::New(
                  nV[fastDim],
                  nV[slowDim]);
            }
          else
            {
            // TODO
            sqErrorMacro(cerr,"3D Texture kernel is not implemented!");
            return -1;
            }
          }
        else
          {
          // use global memory for input
          devV[q]=CUDAGlobalMemoryManager<float>::New(vnijk);
          }

        // output array
        devW[q]=CUDAGlobalMemoryManager<float>::New(wnijk);
        cudaHostAlloc(
            &sW[q],
            wnijk*sizeof(float),
            cudaHostAllocDefault);

        /*
        // fill device data with falt index for lookup validation
        for (int i=0; i<vnijk; ++i)
          {
          sV[q][i]=i;
          }
        */
        }

      // convert vtk vectors/tensors into scalar component arrays
      float *hV=(float*)V->GetVoidPointer(0);
      Split<float>(vnijk,hV,sV);

      // copy the input arrays to the device
      for (int q=0; q<nComp; ++q)
        {
        // TODO-could this be streamed to overlap com w/ comp?
        devV[q]->Push(sV[q]);

        // execute the kernel
        if (this->InputMemoryType==CUDA_MEM_TYPE_TEX)
          {
          #ifdef CUDAConvolutionDriverDEBUG
          pCerr() << "Using texture memory for input" << endl;
          #endif
            if ((mode==CartesianExtent::DIM_MODE_2D_XY)
              ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
              ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
              {
              // setup texture
              ierr=devV[q]->BindToTexture(&gV2);
              CUDAScalarConvolution2D<<<blockGrid,threadGrid>>>(
                    //worldRank,
                    nV[fastDim],
                    nW[fastDim],
                    wnijk,
                    nK[fastDim],
                    nGhost,
                    devW[q]->GetDevicePointer(),
                    devK->GetDevicePointer());
              }
            else
              {
              // TODO
              sqErrorMacro(cerr,"3D Texture kernel is not implemented!");
              return -1;
              }
            uerr=cudaGetLastError();
            if (uerr)
              {
              CUDAErrorMacro(pCerr(),uerr,"Error invoking 2D kernel.");
              return -1;
              }
            // release texture
            cudaDeviceSynchronize();
            //cudaUnbindTexture(gV2);
          }
        else
          {
          #ifdef CUDAConvolutionDriverDEBUG
          pCerr() << "Using global memory for input" << endl;
          #endif
          if ((mode==CartesianExtent::DIM_MODE_2D_XY)
            ||(mode==CartesianExtent::DIM_MODE_2D_XZ)
            ||(mode==CartesianExtent::DIM_MODE_2D_YZ))
            {
            CUDAScalarConvolution2D<<<blockGrid,threadGrid>>>(
                  //worldRank,
                  nV[fastDim],
                  nW[fastDim],
                  wnijk,
                  nK[fastDim],
                  nGhost,
                  devV[q]->GetDevicePointer(),
                  devW[q]->GetDevicePointer(),
                  devK->GetDevicePointer());
            }
          else
            {
            CUDAScalarConvolution3D<<<blockGrid,threadGrid>>>(
                  nV[fastDim],
                  nV[fastDim]*nV[slowDim],
                  nW[fastDim],
                  nW[fastDim]*nW[slowDim],
                  wnijk,
                  nK[fastDim],
                  nK[fastDim]*nK[slowDim],
                  nGhost,
                  devV[q]->GetDevicePointer(),
                  devW[q]->GetDevicePointer(),
                  devK->GetDevicePointer());
            }
          uerr=cudaGetLastError();
          if (uerr)
            {
            CUDAErrorMacro(pCerr(),uerr,"Error invoking global memory kernel.");
            return -1;
            }
          }
        // retreive the output/result
        devW[q]->Pull(sW[q]);
        }

      // put results in vtk order
      float *hW=(float*)W->GetVoidPointer(0);
      Interleave(wnijk,sW,hW);

      // clean up
      for (int q=0; q<nComp; ++q)
        {
        delete devW[q];
        cudaFreeHost(sW[q]);
        delete devV[q];
        cudaFreeHost(sV[q]);
        }
      cudaUnbindTexture(gV2);
      }
      break;
    default:
      // TODO 
      sqErrorMacro(cerr,"Not currently using vtkTemplateMacro");
      return -1;
    }

  delete devK;
  // TODO if kernel is in texture mem, unbind
  #endif

  return 0;
}




/*
//-----------------------------------------------------------------------------
void CUDAConvolutionDriver::Convolution(
    CartesianExtent &extV,
    CartesianExtent &extW,
    CartesianExtent &extK,
    int ghostV,
    int mode,
    vtkDataArray *V,
    vtkDataArray *W,
    float *K)
{
  pCerr()
    << "===============CUDAConvolutionDriver::Convolution" << endl
    << "NBlocks=" << this->NBlocks << endl
    << "NThreads=" << this->NThreads << endl;

  CUDAFlatIndex  idxV(extV,mode);
  CUDATupleIndex tupW(extW,ghostV,mode);
  CUDAFlatIndex  idxK(extK,mode);

  int compW=W->GetNumberOfComponents();
  unsigned long sizeW=W->GetNumberOfTuples();

  CUDAMemoryManager<int> *devExtK
    = CUDAMemoryManager<int>::New(extK.GetData(),6);
  devExtK->Push();

  CUDAMemoryManager<float> *devK
    = CUDAMemoryManager<float>::New(K,extK.Size());
  devK->Push();

  switch (V->GetDataType())
    {
    case VTK_FLOAT:
      {
      CUDAMemoryManager<float> *devV
        = CUDAMemoryManager<float>::New(V);
      devV->Push();

      CUDAMemoryManager<float> *devW
        = CUDAMemoryManager<float>::New(W);
      //devW->Zero(); bug in cuda!
      //devW->Push();

      cerr << "Calling" << endl;
      ::CUDAConvolution<float><<<this->NBlocks,this->NThreads>>>(
          idxV.GetDevicePointer(),
          devV->GetDevicePointer(),
          tupW.GetDevicePointer(),
          sizeW,
          compW,
          devW->GetDevicePointer(),
          devExtK->GetDevicePointer(),
          idxK.GetDevicePointer(),
          devK->GetDevicePointer());
      cudaError_t ierr=cudaGetLastError();
      if (ierr)
      {
        CUDAErrorMacro(pCerr(),ierr,"Kernel fialed to run.");
      }

      devW->Pull();

      delete devW;
      delete devV;
      }
      break;
    };

  delete devExtK;
  delete devK;
  cerr << "Finished." << endl;
}

//-----------------------------------------------------------------------------
void CUDAConvolutionDriver::Convolution3D(
    CartesianExtent &extV,
    CartesianExtent &extW,
    CartesianExtent &extK,
    int nGhost,
    int mode,
    vtkDataArray *V,
    vtkDataArray *W,
    float *K)
{
  // block decomp
  int wSize[3];
  extW.Size(wSize);
  dim3 blockDecomp(wSize[0],wSize[1],wSize[2]);

  // thread decomp
  int nCompV=V->GetNumberOfComponents();

  // translation between output and input index tuple
  int nGhostV[3]={0};
  CartesianExtent::Shift(nGhostV,nGhost,mode);
  CUDAMemoryManager<int> *devNGhostV
    = CUDAMemoryManager<int>::New(nGhostV,3);
  devNGhostV->Push();

  //
  CUDAFlatIndex idxV(extV,mode);
  CUDAFlatIndex idxW(extW,mode);
  CUDAFlatIndex idxK(extK,mode);

  CUDAMemoryManager<int> *devExtK
    = CUDAMemoryManager<int>::New(extK.GetData(),6);
  devExtK->Push();

  CUDAMemoryManager<float> *devK
    = CUDAMemoryManager<float>::New(K,extK.Size());
  devK->Push();

  pCerr()
    << "===============CUDAConvolutionDriver::Convolution3D" << endl
    << "NBlocks=" << wSize[0] << ", " << wSize[1] << ", " << wSize[2] << endl
    << "NThreads=" << nCompV << endl;

  switch (V->GetDataType())
    {
    case VTK_FLOAT:
      {
      CUDAMemoryManager<float> *devV
        = CUDAMemoryManager<float>::New(V);
      devV->Push();

      CUDAMemoryManager<float> *devW
        = CUDAMemoryManager<float>::New(W);
      //devW->Zero(); bug in cuda! not all bytes are zerod
      //devW->Push();

      cerr << "Calling" << endl;
      ::CUDAConvolution<float><<<blockDecomp,nCompV>>>(
          idxV.GetDevicePointer(),
          nCompV,
          devNGhostV->GetDevicePointer(),
          devV->GetDevicePointer(),
          idxW.GetDevicePointer(),
          devW->GetDevicePointer(),
          devExtK->GetDevicePointer(),
          idxK.GetDevicePointer(),
          devK->GetDevicePointer());
      cudaThreadSynchronize();
      cudaError_t ierr=cudaGetLastError();
      if (ierr!=cudaSuccess)
      {
        CUDAErrorMacro(pCerr(),ierr,"Kernel fialed.");
      }

      devW->Pull();

      delete devW;
      delete devV;
      }
      break;
    };

    delete devK;
    delete devExtK;
    cerr << "Finished." << endl;
}

*/

