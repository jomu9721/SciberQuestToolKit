#!/bin/bash

cmake \
    -DCMAKE_C_COMPILER=/opt/apps/intel/11.1/bin/intel64/icc \
    -DCMAKE_CXX_COMPILER=/opt/apps/intel/11.1/bin/intel64/icpc \
    -DCMAKE_LINKER=/opt/apps/intel/11.1/bin/intel64/icpc \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=OFF \
    -DPARAVIEW_BUILD_QT_GUI=OFF \
    -DPARAVIEW_BUILD_PLUGIN_Array=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ChartViewFrame=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientChartView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientGeoView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientGeoView2D=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientGraphView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientRecordView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientRichTextView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientTableView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientTreeView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_CommonToolbar=OFF \
    -DPARAVIEW_BUILD_PLUGIN_CosmoFilters=OFF \
    -DPARAVIEW_BUILD_PLUGIN_GraphLayoutFilterPanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_Infovis=OFF \
    -DPARAVIEW_BUILD_PLUGIN_Manta=OFF \
    -DPARAVIEW_BUILD_PLUGIN_Moments=OFF \
    -DPARAVIEW_BUILD_PLUGIN_NetDMFReader=OFF \
    -DPARAVIEW_BUILD_PLUGIN_Prism=OFF \
    -DPARAVIEW_BUILD_PLUGIN_PointSprite=ON \
    -DPARAVIEW_BUILD_PLUGIN_SierraPlotTools=OFF \
    -DPARAVIEW_BUILD_PLUGIN_SLACTools=OFF \
    -DPARAVIEW_BUILD_PLUGIN_SQLDatabaseGraphSourcePanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_SQLDatabaseTableSourcePanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_SplitTableFieldPanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_StatisticsToolbar=OFF \
    -DPARAVIEW_BUILD_PLUGIN_SurfaceLIC=ON \
    -DPARAVIEW_BUILD_PLUGIN_TableToGraphPanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_TableToSparseArrayPanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ThresholdTablePanel=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientGraphViewFrame=OFF \
    -DPARAVIEW_BUILD_PLUGIN_ClientTreeAreaView=OFF \
    -DPARAVIEW_BUILD_PLUGIN_VisItDatabaseBridge=OFF \
    -DPARAVIEW_BUILD_PLUGIN_H5PartReader=OFF \
    -DPARAVIEW_BUILD_PLUGIN_CoProcessingScriptGenerator=OFF \
    -DPARAVIEW_BUILD_PLUGIN_AnalyzeNIfTIReaderWriter=OFF \
    -DPARAVIEW_BUILD_PLUGIN_VisTrails=OFF \
    $* 
