/*
   ____    _ __           ____               __    ____
  / __/___(_) /  ___ ____/ __ \__ _____ ___ / /_  /  _/__  ____
 _\ \/ __/ / _ \/ -_) __/ /_/ / // / -_|_-</ __/ _/ // _ \/ __/
/___/\__/_/_.__/\__/_/  \___\_\_,_/\__/___/\__/ /___/_//_/\__(_) 

Copyright 2008 SciberQuest Inc.

*/
#include "pqSQPlaneSource.h"

#include "pqApplicationCore.h"
#include "pqProxy.h"
#include "pqSettings.h"
#include "pqRenderView.h"

#include "vtkSMProxy.h"
#include "vtkSMRenderViewProxy.h"

#include "vtkSMProperty.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMDoubleVectorProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkMath.h"
#include "vtkSmartPointer.h"


#include "vtkEventQtSlotConnect.h"
#include "vtkProcessModule.h"

#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QPalette>
#include <QSettings>


#include <unistd.h>

#include "FsUtils.h"
#if defined pqSQPlaneSourceDEBUG
#include <PrintUtils.h>
#endif

#include "SQMacros.h"

#include <vector>
using vtkstd::vector;
#include <string>
using vtkstd::string;
#include <fstream>
using std::ofstream;
using std::ifstream;
using std::ios_base;
#include <sstream>
using std::ostringstream;
using std::istringstream;
#include <iostream>
using std::cerr;
using std::endl;

enum {
  PROCESS_TYPE=Qt::UserRole,
  PROCESS_TYPE_INVALID,
  PROCESS_TYPE_LOCAL,
  PROCESS_TYPE_REMOTE
};

//-----------------------------------------------------------------------------
pqSQPlaneSource::pqSQPlaneSource(
      pqProxy* proxy,
      QWidget* widget)
             :
      pqNamedObjectPanel(proxy, widget)
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::pqSQPlaneSource" << endl;
  #endif

  this->dims[0]=this->dims[1]=1.0;
  this->dx[0]=this->dx[1]=1.0;
  this->nx[0]=this->nx[1]=1;
  this->N[0]=this->N[1]=this->N[2]=0.0;

  // Construct Qt form.
  this->Form=new pqSQPlaneSourceForm;
  this->Form->setupUi(this);

  // set up coordinate line edits
  this->Form->o_x->setValidator(new QDoubleValidator(this->Form->o_x));
  this->Form->o_y->setValidator(new QDoubleValidator(this->Form->o_y));
  this->Form->o_z->setValidator(new QDoubleValidator(this->Form->o_z));
  this->Form->p1_x->setValidator(new QDoubleValidator(this->Form->p1_x));
  this->Form->p1_y->setValidator(new QDoubleValidator(this->Form->p1_y));
  this->Form->p1_z->setValidator(new QDoubleValidator(this->Form->p1_z));
  this->Form->p2_x->setValidator(new QDoubleValidator(this->Form->p2_x));
  this->Form->p2_y->setValidator(new QDoubleValidator(this->Form->p2_y));
  this->Form->p2_z->setValidator(new QDoubleValidator(this->Form->p2_z));
  this->Form->dx->setValidator(new QDoubleValidator(this->Form->dx));
  this->Form->dy->setValidator(new QDoubleValidator(this->Form->dy));

  this->SetSpacing(this->dx);
  this->SetResolution(this->nx);
  this->SetNormal(this->N);

  //   vtkSMProxy* pProxy=this->referenceProxy()->getProxy();
  //   
  //   // Connect to server side pipeline's UpdateInformation events.
  //   this->VTKConnect=vtkEventQtSlotConnect::New();
  //   this->VTKConnect->Connect(
  //       pProxy,
  //       vtkCommand::UpdateInformationEvent,
  //       this, SLOT(PullServerConfig()));

  // Set up configuration viewer
  this->PullServerConfig();

  // set up save/restore buttons
  QObject::connect(this->Form->save,SIGNAL(clicked()),this,SLOT(savePlaneState()));
  //QObject::connect(this->Form->restore,SIGNAL(clicked()),this,SLOT(loadPlaneState()));
  QObject::connect(this->Form->restore,SIGNAL(clicked()),this,SLOT(Restore()));
  QObject::connect(this->Form->snap,SIGNAL(clicked()),this,SLOT(SnapViewToNormal()));

  // set up the dimension calculator
  QObject::connect(
      this->Form->o_x,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->o_y,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->o_z,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  //
  QObject::connect(
      this->Form->p1_x,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->p1_y,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->p1_z,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  //
  QObject::connect(
      this->Form->p2_x,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->p2_y,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));
  QObject::connect(
      this->Form->p2_z,
      SIGNAL(editingFinished()),
      this, SLOT(DimensionsModified()));

  // link resolution to spacing
  QObject::connect(
      this->Form->res_x,
      SIGNAL(valueChanged(int)),
      this, SLOT(ResolutionModified()));
  //
  QObject::connect(
      this->Form->res_y,
      SIGNAL(valueChanged(int)),
      this, SLOT(ResolutionModified()));

  // link spacing to resolution
  QObject::connect(
      this->Form->dx,
      SIGNAL(editingFinished()),
      this, SLOT(SpacingModified()));
  //
  QObject::connect(
      this->Form->dy,
      SIGNAL(editingFinished()),
      this, SLOT(SpacingModified()));

  // make sure if uniform pixel aspect is changed the form is updated
  QObject::connect(
      this->Form->aspectLock,
      SIGNAL(toggled(bool)),
      this, SLOT(SpacingModified()));

  // These connection let PV know that we have changed, and makes the apply 
  // button activated.
  QObject::connect(
      this->Form->name,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  //
  QObject::connect(
      this->Form->o_x,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->o_y,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->o_z,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  //
  QObject::connect(
      this->Form->p1_x,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->p1_y,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->p1_z,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  //
  QObject::connect(
      this->Form->p2_x,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->p2_y,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->p2_z,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  //
  QObject::connect(
      this->Form->res_x,
      SIGNAL(valueChanged(int)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->res_y,
      SIGNAL(valueChanged(int)),
      this, SLOT(setModified()));
  //
  QObject::connect(
      this->Form->dx,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));
  QObject::connect(
      this->Form->dy,
      SIGNAL(textChanged(QString)),
      this, SLOT(setModified()));

  // Let the super class do the undocumented stuff that needs to hapen.
  pqNamedObjectPanel::linkServerManagerProperties();

}

//-----------------------------------------------------------------------------
pqSQPlaneSource::~pqSQPlaneSource()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::~pqSQPlaneSource" << endl;
  #endif

  delete this->Form;
  /*
  this->VTKConnect->Delete();
  this->VTKConnect=0;*/
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::Restore()
{
  QSettings settings("SciberQuest", "SciVisToolKit");
  QString lastUsedDir=settings.value("SQPlaneSource/lastUsedDir","").toString();

  QString fn=QFileDialog::getOpenFileName(this,"Open SQ Plane Source",lastUsedDir,"*.sqps");
  if (fn.size())
    {
    ifstream f(fn.toStdString().c_str(),ios_base::in);
    if (f.is_open())
      {
      char buf[1024];
      f.getline(buf,1024);
      if (string(buf).find("SQ Plane Source")!=string::npos)
        {
        // DescriptiveName
        f.getline(buf,1024);
        f.getline(buf,1024);
        this->Form->name->setText(buf);
        // Origin
        f.getline(buf,1024);
        f.getline(buf,1024);
        istringstream *is;
        is=new istringstream(buf);
        double p[3];
        *is >> p[0] >> p[1] >> p[2];
        delete is;
        this->Form->o_x->setText(QString("%1").arg(p[0]));
        this->Form->o_y->setText(QString("%1").arg(p[1]));
        this->Form->o_z->setText(QString("%1").arg(p[2]));
        // Point1
        f.getline(buf,1024);
        f.getline(buf,1024);
        is=new istringstream(buf);
        *is >> p[0] >> p[1] >> p[2];
        delete is;
        this->Form->p1_x->setText(QString("%1").arg(p[0]));
        this->Form->p1_y->setText(QString("%1").arg(p[1]));
        this->Form->p1_z->setText(QString("%1").arg(p[2]));
        // Point2
        f.getline(buf,1024);
        f.getline(buf,1024);
        is=new istringstream(buf);
        *is >> p[0] >> p[1] >> p[2];
        delete is;
        this->Form->p2_x->setText(QString("%1").arg(p[0]));
        this->Form->p2_y->setText(QString("%1").arg(p[1]));
        this->Form->p2_z->setText(QString("%1").arg(p[2]));
        // Resolution
        f.getline(buf,1024);
        f.getline(buf,1024);
        is=new istringstream(buf);
        int r[2];
        *is >> r[0] >> r[1];
        delete is;
        this->Form->res_x->setValue(r[0]);
        this->Form->res_y->setValue(r[0]);

        // update derived values.
        this->DimensionsModified();
        }
      else
        {
        QMessageBox::warning(this,"Open SQ Plane Source","Error: Bad format not a SQ plane source file.");
        }
      f.close();
      }
    else
      {
      QMessageBox::warning(this,"Save SQ Plane Source","Error: Could not open the file.");
      }
    }
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::Save()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::Save" << endl;
  #endif

  QString fn=QFileDialog::getSaveFileName(this,"Save SQ Plane Source","","*.sqps");
  if (fn.size())
    {
    QString lastUsedDir(StripFileNameFromPath(fn.toStdString()).c_str());
    QSettings settings("SciberQuest", "SciVisToolKit");
    settings.setValue("SQPlaneSource/lastUsedDir",lastUsedDir);

    ofstream f(fn.toStdString().c_str(),ios_base::out|ios_base::trunc);
    if (f.is_open())
      {
      f << "SQ Plane Source 1.0" << endl
        << "DescriptiveName" << endl
        << this->Form->name->text().toStdString() << endl
        << "Origin" << endl
        << this->Form->o_x->text().toDouble() << " "
        << this->Form->o_y->text().toDouble() << " "
        << this->Form->o_z->text().toDouble() << endl
        << "Point1" << endl
        << this->Form->p1_x->text().toDouble() << " "
        << this->Form->p1_y->text().toDouble() << " "
        << this->Form->p1_z->text().toDouble() << endl
        << "Point2" << endl
        << this->Form->p2_x->text().toDouble() << " "
        << this->Form->p2_y->text().toDouble() << " "
        << this->Form->p2_z->text().toDouble() << endl
        << "Resolution" << endl
        << this->Form->res_x->value() << " "
        << this->Form->res_y->value() << endl
        << endl;
      f.close();
      }
    else
      {
      QMessageBox::warning(this,"Save SQ Plane Source","Error: Failed to create the file.");
      }
    }
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::savePlaneState()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::savePlaneState" << endl;
  #endif

  pqSettings* settings=pqApplicationCore::instance()->settings();
  QString lastUsedPath=settings->value("SQPlaneSourceStateIO/lastUsedPath","").toString();

  char filters[]="SQ Plane State Files (*.sqps);;XML files (*.xml);;All Files (*.*)";

  QString filename
    = QFileDialog::getSaveFileName(this,"Save Plane Source State",lastUsedPath,filters);
  if (filename.size())
    {
    // add the default extension
    if (!filename.endsWith(".sqps"))
      {
      filename+=".sqps";
      }
    // Save the path as a convinience to put the dialog in the right spot
    // next time.
    QString path;
    path=QDir(filename).absolutePath();
    path=path.left(path.lastIndexOf("/"));
    settings->setValue("SQPlaneSourceStateIO/lastUsedPath",path);

    // Save the camer state
    vtkPVXMLElement* root=vtkPVXMLElement::New();
    root->SetName("SciberQuestPlaneSourceState");
    root->SetAttribute("version","1.0");

    vtkSMProxy *proxy=this->proxy();
    proxy->GetProperty("Name")->SaveConfiguration(root);
    proxy->GetProperty("Origin")->SaveConfiguration(root);
    proxy->GetProperty("Point1")->SaveConfiguration(root);
    proxy->GetProperty("Point2")->SaveConfiguration(root);
    proxy->GetProperty("XResolution")->SaveConfiguration(root);
    proxy->GetProperty("YResolution")->SaveConfiguration(root);

    ofstream os(filename.toStdString().c_str(),ios::out);
    root->PrintXML(os,vtkIndent());
    root->Delete();
    }
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::loadPlaneState()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::loadPlaneState" << endl;
  #endif

  pqSettings* settings=pqApplicationCore::instance()->settings();
  QString lastUsedPath=settings->value("SQPlaneSourceStateIO/lastUsedPath","").toString();

  char filters[]="SQ Plane State Files (*.sqps);;XML files (*.xml);;All Files (*.*)";

  QString filename
    = QFileDialog::getOpenFileName(this,"Load Plane Source State",lastUsedPath,filters);
  if (filename.size())
    {
    // Save the path as a convinience to put the dialog in the right spot
    // next time.
    QString path;
    path=QDir(filename).absolutePath();
    path=path.left(path.lastIndexOf("/"));
    settings->setValue("SQPlaneSourceStateIO/lastUsedPath",path);

    vtkSmartPointer<vtkPVXMLParser> parser=vtkSmartPointer<vtkPVXMLParser>::New();
    parser->SetFileName(filename.toStdString().c_str());
    if (parser->Parse()==0)
      {
      vtkGenericWarningMacro("Failed to load plane state. Aborting.");
      return;
      }

    vtkPVXMLElement *root=parser->GetRootElement();

    vtkstd::string requiredType("SciberQuestPlaneSourceState");
    vtkstd::string requiredVersion("1.0");
    const char *foundType=root->GetName();
    if (foundType==0 || foundType!=requiredType)
      {
      vtkGenericWarningMacro("This is not a valid SciberQuest Plane State file. Aborting.");
      return;
      }
    const char *foundVersion=root->GetAttribute("version");
    if (foundVersion==0)
      {
      vtkGenericWarningMacro("No \"version\" attribute was found. Aborting.");
      return;
      }
    if (foundVersion!=requiredVersion)
      {
      vtkGenericWarningMacro(
          << "Unsupported file version " << foundVersion
          << "!=" << requiredVersion << ". Aborting.");
      return;
      }

    vtkSMProxy *proxy=this->proxy();
    int ok=1;
    ok&=proxy->GetProperty("Name")->LoadConfiguration(root);
    ok&=proxy->GetProperty("Origin")->LoadConfiguration(root);
    ok&=proxy->GetProperty("Point1")->LoadConfiguration(root);
    ok&=proxy->GetProperty("Point2")->LoadConfiguration(root);
    ok&=proxy->GetProperty("XResolution")->LoadConfiguration(root);
    ok&=proxy->GetProperty("YResolution")->LoadConfiguration(root);
    if (!ok)
      {
      vtkGenericWarningMacro("There was an error loading a property. Aborting.");
      // TODO this->Internal->CameraLinks.reset();
      // return;
      }

    proxy->UpdateVTKObjects();

    this->PullServerConfig();
    }
}

//-----------------------------------------------------------------------------
int pqSQPlaneSource::ValidateCoordinates()
{
  double n[3]={0.0};
  int ok=this->CalculateNormal(n);
  if (ok)
    {
    // this->Form->coordStatus->setText("OK");
    // this->Form->coordStatus->setStyleSheet("color:green; background-color:white;");
    }
  else
    {
    // this->Form->coordStatus->setText("Error");
    // this->Form->coordStatus->setStyleSheet("color:red; background-color:lightyellow;");
    // this->Form->n_x->setText("");
    // this->Form->n_y->setText("");
    // this->Form->n_z->setText("");
    // this->Form->dim_x->setText("");
    // this->Form->dim_y->setText("");
    }
  return ok;
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetOrigin(double *o)
{
  o[0]=this->Form->o_x->text().toDouble();
  o[1]=this->Form->o_y->text().toDouble();
  o[2]=this->Form->o_z->text().toDouble();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetOrigin(double *o)
{
  this->Form->o_x->setText(QString("%1").arg(o[0]));
  this->Form->o_y->setText(QString("%1").arg(o[1]));
  this->Form->o_z->setText(QString("%1").arg(o[2]));
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetPoint1(double *p1)
{
  p1[0]=this->Form->p1_x->text().toDouble();
  p1[1]=this->Form->p1_y->text().toDouble();
  p1[2]=this->Form->p1_z->text().toDouble();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetPoint1(double *p1)
{
  this->Form->p1_x->setText(QString("%1").arg(p1[0]));
  this->Form->p1_y->setText(QString("%1").arg(p1[1]));
  this->Form->p1_z->setText(QString("%1").arg(p1[2]));
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetPoint2(double *p2)
{
  p2[0]=this->Form->p2_x->text().toDouble();
  p2[1]=this->Form->p2_y->text().toDouble();
  p2[2]=this->Form->p2_z->text().toDouble();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetPoint2(double *p2)
{
  this->Form->p2_x->setText(QString("%1").arg(p2[0]));
  this->Form->p2_y->setText(QString("%1").arg(p2[1]));
  this->Form->p2_z->setText(QString("%1").arg(p2[2]));
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetResolution(int *res)
{
  res[0]=this->Form->res_x->text().toInt();
  res[1]=this->Form->res_y->text().toInt();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetResolution(int *res)
{
  this->Form->res_x->setValue(res[0]);
  this->Form->res_y->setValue(res[1]);
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetSpacing(double *dx)
{
  dx[0]=this->Form->dx->text().toDouble();
  dx[1]=this->Form->dy->text().toDouble();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetSpacing(double *dx)
{
  this->Form->dx->setText(QString("%1").arg(dx[0]));
  this->Form->dy->setText(QString("%1").arg(dx[1]));
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::GetNormal(double *n)
{
  n[0]=this->Form->p2_x->text().toDouble();
  n[1]=this->Form->p2_y->text().toDouble();
  n[2]=this->Form->p2_z->text().toDouble();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SetNormal(double *n)
{
  this->Form->n_x->setText(QString("%1").arg(n[0]));
  this->Form->n_y->setText(QString("%1").arg(n[1]));
  this->Form->n_z->setText(QString("%1").arg(n[2]));
}

//-----------------------------------------------------------------------------
int pqSQPlaneSource::CalculateNormal(double *n)
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::CaclulateNormal" << endl;
  #endif

  double o[3];
  this->GetOrigin(o);

  double p1[3];
  this->GetPoint1(p1);

  double p2[3];
  this->GetPoint2(p2);

  double v1[3];
  v1[0]=p1[0]-o[0];
  v1[1]=p1[1]-o[1];
  v1[2]=p1[2]-o[2];

  double v2[3];
  v2[0]=p2[0]-o[0];
  v2[1]=p2[1]-o[1];
  v2[2]=p2[2]-o[2];

  vtkMath::Cross(v1,v2,n);
  int ok=vtkMath::Normalize(n);
  if (!ok)
    {
    pqErrorMacro("Invalid coordinate system.");
    return 0;
    }

  return 1;
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::DimensionsModified()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::DimensionsModified" << endl;
  #endif

  int ok=this->CalculateNormal(this->N);
  if (!ok)
    {
    this->N[0]=this->N[1]=this->N[2]=0.0;
    return;
    }
  this->SetNormal(this->N);


  double o[3];
  this->GetOrigin(o);

  double p1[3];
  this->GetPoint1(p1);

  double p2[3];
  this->GetPoint2(p2);

  double x,y,z,dim;
  x=p1[0]-o[0];
  y=p1[1]-o[1];
  z=p1[2]-o[2];
  dim=sqrt(x*x+y*y+z*z);
  this->dims[0]=dim;

  x=p2[0]-o[0];
  y=p2[1]-o[1];
  z=p2[2]-o[2];
  dim=sqrt(x*x+y*y+z*z);
  this->dims[1]=dim;

  this->Form->dim_x->setText(QString("%1").arg(this->dims[0]));
  this->Form->dim_y->setText(QString("%1").arg(this->dims[1]));

  // recompute resolution based on the current grid spacing
  // settings.
  this->SpacingModified();

  return;
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SpacingModified()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::SpacingModified" << endl;
  #endif

  // retreive the requested spacing.
  this->GetSpacing(this->dx);

  // enforce uniform pixel aspect ratio
  if (this->Form->aspectLock->isChecked())
    {
    this->dx[1]=this->dx[0];
    this->Form->dy->setText(QString("%1").arg(this->dx[0]));
    }

  // update the grid resolution to match the requested grid spacing
  // as closely as possible.
  this->nx[0]=ceil(this->dims[0]/this->dx[0]);
  this->nx[1]=ceil(this->dims[1]/this->dx[1]);
  this->SetResolution(nx);

  // compute the new number of cells.
  int nCells=this->nx[0]*this->nx[1];
  this->Form->nCells->setText(QString("%1").arg(nCells));

  // update the spacing to match the actual spacing that will be used.
  this->dx[0]=this->dims[0]/this->nx[0];
  this->dx[1]=this->dims[1]/this->nx[1];
  this->SetSpacing(this->dx);
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::ResolutionModified()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::ResolutionModified" << endl;
  #endif

  // retreive the requested resolution.
  this->GetResolution(this->nx);

  // enforce a uniform pixel aspect ratio
  if (this->Form->aspectLock->isChecked())
    {
    this->nx[1]=(this->dims[0]>1E-6?this->nx[0]*dims[1]/dims[0]:1);
    this->nx[1]=(this->nx[1]<1?1:this->nx[1]);
    this->SetResolution(this->nx);
    }

  // compute the new spacing
  this->dx[0]=this->dims[0]/this->nx[0];
  this->dx[1]=this->dims[1]/this->nx[1];

  this->SetSpacing(this->dx);

  // compute the new number of cells.
  int nCells=this->nx[0]*this->nx[1];
  this->Form->nCells->setText(QString("%1").arg(nCells));
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::SnapViewToNormal()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::SnapViewToNormal" << endl;
  #endif

  double o[3];
  this->GetOrigin(o);

  double p1[3];
  this->GetPoint1(p1);

  double p2[3];
  this->GetPoint2(p2);

  // compute the plane's center, this will become the camera focal point.
  double a[3];
  a[0]=o[0]+0.5*(p1[0]-o[0]);
  a[1]=o[1]+0.5*(p1[1]-o[1]);
  a[2]=o[2]+0.5*(p1[2]-o[2]);

  double b[3];
  b[0]=o[0]+0.5*(p2[0]-o[0]);
  b[1]=o[1]+0.5*(p2[1]-o[1]);
  b[2]=o[2]+0.5*(p2[2]-o[2]);

  double cen[3];
  cen[0]=a[0]+b[0]-o[0];
  cen[1]=a[1]+b[1]-o[1];
  cen[2]=a[2]+b[2]-o[2];

  // compute the camera center, 2 polane diagonals along its normal from
  // its center.
  double diag
    = sqrt(this->dims[0]*this->dims[0]+this->dims[1]*this->dims[1]);

  double pos[3];
  pos[0]=cen[0]+this->N[0]*2.0*diag;
  pos[1]=cen[1]+this->N[1]*2.0*diag;
  pos[2]=cen[2]+this->N[2]*2.0*diag;

  // compute the camera up from one of the planes axis.
  double up[3];
  if (this->Form->viewUp1->isChecked())
    {
    up[0]=p1[0]-o[0];
    up[1]=p1[1]-o[1];
    up[2]=p1[2]-o[2];
    }
  else
    {
    up[0]=p2[0]-o[0];
    up[1]=p2[1]-o[1];
    up[2]=p2[2]-o[2];
    }
  double mup=sqrt(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
  up[0]/=mup;
  up[1]/=mup;
  up[2]/=mup;


  pqRenderView *view=dynamic_cast<pqRenderView*>(this->view());
  if (!view)
    {
    pqErrorMacro("Failed to get the current view.");
    return;
    }

  vtkSMRenderViewProxy *proxy=view->getRenderViewProxy();

  vtkSMDoubleVectorProperty *prop;

  prop=dynamic_cast<vtkSMDoubleVectorProperty*>(proxy->GetProperty("CameraPosition"));
  prop->SetElements(pos);

  prop=dynamic_cast<vtkSMDoubleVectorProperty*>(proxy->GetProperty("CameraFocalPoint"));
  prop->SetElements(cen);

  prop=dynamic_cast<vtkSMDoubleVectorProperty*>(proxy->GetProperty("CameraViewUp"));
  prop->SetElements(up);

  prop=dynamic_cast<vtkSMDoubleVectorProperty*>(proxy->GetProperty("CenterOfRotation"));
  prop->SetElements(cen);

  proxy->UpdateVTKObjects();

  view->render();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::UpdateInformationEvent()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::UpdateInformationEvent" << endl;
  #endif
  // vtkSMProxy* pProxy=this->referenceProxy()->getProxy();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::PullServerConfig()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::PullServerConfig" << endl;
  #endif

  vtkSMProxy* pProxy=this->referenceProxy()->getProxy();

  // Name
  vtkSMStringVectorProperty *nameProp
    = dynamic_cast<vtkSMStringVectorProperty*>(pProxy->GetProperty("Name"));
  pProxy->UpdatePropertyInformation(nameProp);
  string name=nameProp->GetElement(0);
  if (!name.empty())
    {
    this->Form->name->setText(name.c_str());
    }

  // Origin
  vtkSMDoubleVectorProperty *oProp
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Origin"));
  pProxy->UpdatePropertyInformation(oProp);
  double *o=oProp->GetElements();
  this->SetOrigin(o);

  // Point 1
  vtkSMDoubleVectorProperty *p1Prop
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Point1"));
  pProxy->UpdatePropertyInformation(p1Prop);
  double *p1=p1Prop->GetElements();
  this->SetPoint1(p1);

  // Point 2
  vtkSMDoubleVectorProperty *p2Prop
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Point2"));
  pProxy->UpdatePropertyInformation(p2Prop);
  double *p2=p2Prop->GetElements();
  this->SetPoint2(p2);

  // Resolution
  vtkSMIntVectorProperty *rxProp
    = dynamic_cast<vtkSMIntVectorProperty*>(pProxy->GetProperty("XResolution"));
  pProxy->UpdatePropertyInformation(rxProp);
  vtkSMIntVectorProperty *ryProp
    = dynamic_cast<vtkSMIntVectorProperty*>(pProxy->GetProperty("YResolution"));
  pProxy->UpdatePropertyInformation(ryProp);
  int res[2]={rxProp->GetElement(0),ryProp->GetElement(0)};
  this->SetResolution(res);

  // update derived/computed values.
  this->DimensionsModified();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::PushServerConfig()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::PushServerConfig" << endl;
  #endif
  vtkSMProxy* pProxy=this->referenceProxy()->getProxy();

  // Name
  vtkSMStringVectorProperty *nameProp
    = dynamic_cast<vtkSMStringVectorProperty*>(pProxy->GetProperty("Name"));
  nameProp->SetElement(0,this->Form->name->text().toStdString().c_str());

  // Origin
  double o[3];
  this->GetOrigin(o);
  vtkSMDoubleVectorProperty *oProp
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Origin"));
  oProp->SetElements(o,3);

  // Point 1
  double p1[3];
  this->GetPoint1(p1);
  vtkSMDoubleVectorProperty *p1Prop
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Point1"));
  p1Prop->SetElements(p1,3);

  // Point 2
  double p2[3];
  this->GetPoint2(p2);
  vtkSMDoubleVectorProperty *p2Prop
    = dynamic_cast<vtkSMDoubleVectorProperty*>(pProxy->GetProperty("Point2"));
  p2Prop->SetElements(p2,3);

  // Resolution
  int nx[2];
  this->GetResolution(nx);
  vtkSMIntVectorProperty *rxProp
    = dynamic_cast<vtkSMIntVectorProperty*>(pProxy->GetProperty("XResolution"));
  rxProp->SetElement(0,nx[0]);
  vtkSMIntVectorProperty *ryProp
    = dynamic_cast<vtkSMIntVectorProperty*>(pProxy->GetProperty("YResolution"));
  ryProp->SetElement(0,nx[1]);

  // Let proxy send updated values.
  pProxy->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
void pqSQPlaneSource::accept()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::accept" << endl;
  #endif

  this->PushServerConfig();

  // Let our superclass do the undocumented stuff that needs to be done.
  pqNamedObjectPanel::accept();
}



//-----------------------------------------------------------------------------
void pqSQPlaneSource::reset()
{
  #if defined pqSQPlaneSourceDEBUG
  cerr << "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::reset" << endl;
  #endif

  this->PullServerConfig();

  // Let our superclass do the undocumented stuff that needs to be done.
  pqNamedObjectPanel::reset();
}

/// VTK stuffs
//   // Connect to server side pipeline's UpdateInformation events.
//   this->VTKConnect=vtkEventQtSlotConnect::New();
//   this->VTKConnect->Connect(
//       dbbProxy,
//       vtkCommand::UpdateInformationEvent,
//       this, SLOT(UpdateInformationEvent()));
//   // Get our initial state from the server side. In server side RequestInformation
//   // the database view is encoded in vtkInformationObject. We are relying on the 
//   // fact that there is a pending event waiting for us.
//   this->UpdateInformationEvent();

// 
//   // These connection let PV know that we have changed, and makes the apply button
//   // is activated.
//   QObject::connect(
//       this->Form->DatabaseView,
//       SIGNAL(itemChanged(QTreeWidgetItem*, int)),
//       this, SLOT(setModified()));

// vtkSMProxy* dbbProxy=this->referenceProxy()->getProxy();
/// Example of how to get stuff from the server side.
// vtkSMIntVectorProperty *serverDVMTimeProp
//   =dynamic_cast<vtkSMIntVectorProperty *>(dbbProxy->GetProperty("DatabaseViewMTime"));
// dbbProxy->UpdatePropertyInformation(serverDVMTimeProp);
// const int *serverDVMTime=dvmtProp->GetElement(0);
/// Example of how to get stuff from the XML configuration.
// vtkSMStringVectorProperty *ppProp
//   =dynamic_cast<vtkSMStringVectorProperty *>(dbbProxy->GetProperty("PluginPath"));
// const char *pp=ppProp->GetElement(0);
// this->Form->PluginPath->setText(pp);
/// Imediate update example.
// These are bad because it gets updated more than when you call
// modified, see the PV guide.
//  iuiProp->SetImmediateUpdate(1); set this in constructor
// vtkSMProperty *iuiProp=dbbProxy->GetProperty("InitializeUI");
// iuiProp->Modified();
/// Do some changes and force a push.
// vtkSMIntVectorProperty *meshProp
//   = dynamic_cast<vtkSMIntVectorProperty *>(dbbProxy->GetProperty("PushMeshId"));
// meshProp->SetElement(meshIdx,meshId);
// ++meshIdx;
// dbbProxy->UpdateProperty("PushMeshId");
/// Catch all, update anything else that may need updating.
// dbbProxy->UpdateVTKObjects();
/// How a wiget in custom panel tells PV that things need to be applied
// QObject::connect(
//     this->Form->DatabaseView,
//     SIGNAL(itemChanged(QTreeWidgetItem*, int)),
//     this, SLOT(setModified()));
  // Pull run time configuration from server. The values are transfered
  // in the form of an ascii stream.
//   vtkSMIntVectorProperty *pidProp
//     = dynamic_cast<vtkSMIntVectorProperty*>(pProxy->GetProperty("Pid"));
//   pProxy->UpdatePropertyInformation(pidProp);
//   pid_t p=pidProp->GetElement(0);
//   cerr << "PID=" << p << endl;
/*
  vtkSMProxy* reader = this->referenceProxy()->getProxy();
  reader->UpdatePropertyInformation(reader->GetProperty("Pid"));
  int stamp = vtkSMPropertyHelper(reader, "Pid").GetAsInt();
  cerr << stamp;*/




  /// NOTE how to get something from the server.
  /// dbbProxy->UpdatePropertyInformation(reader->GetProperty("SILUpdateStamp"));
  /// int stamp = vtkSMPropertyHelper(dbbProxy, "SILUpdateStamp").GetAsInt();