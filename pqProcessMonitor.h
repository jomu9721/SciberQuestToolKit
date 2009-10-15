#ifndef pqProcessMonitor_h
#define pqProcessMonitor_h

#include "pqNamedObjectPanel.h"
#include "pqComponentsExport.h"// no comment
#include <vtkstd/vector>// no comment

#include "ui_pqProcessMonitorForm.h"//  no comment
using Ui::pqProcessMonitorForm;

#define pqProcessMonitorDEBUG

class pqProxy;
class vtkEventQtSlotConnect;
class QWidget;

class pqProcessMonitor : public pqNamedObjectPanel
{
  Q_OBJECT
public:
  pqProcessMonitor(pqProxy* proxy, QWidget* p = NULL);
  ~pqProcessMonitor();

protected slots:
  // Description:
  // Update the UI with values from the server.
  void PullServerConfig();
  // Description:
  // Update information events are generated by PV in many instances.
  // We need to watch for the ones coresponding to RequestInformation
  // on the server side where the new database view is stored in 
  // vtkInformation. This will take that information object
  // and load it in to the QTreeWidget.
  void UpdateInformationEvent();
  // Description:
  // This is where we have to communicate our state to the server.
  void accept();
  // Description:
  // Attach a debugger to the selected process.
  void ForkExec();
  void Signal();

private:



private:
  pqProcessMonitorForm *Form;
  vtkEventQtSlotConnect *VTKConnect;
};

#endif
